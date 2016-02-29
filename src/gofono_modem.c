/*
 * Copyright (C) 2014-2016 Jolla Ltd.
 * Contact: Slava Monich <slava.monich@jolla.com>
 *
 * You may use this file under the terms of BSD license as follows:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   3. Neither the name of the Jolla Ltd nor the names of its contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "gofono_modem_p.h"
#include "gofono_modemintf.h"
#include "gofono_manager.h"
#include "gofono_names.h"
#include "gofono_util.h"
#include "gofono_log.h"

/* Generated headers */
#define OFONO_OBJECT_PROXY OrgOfonoModem
#include "org.ofono.Modem.h"
#include "gofono_object_p.h"

/* Object definition */
enum modem_handler_id {
    MANAGER_HANDLER_VALID_CHANGED,
    MANAGER_HANDLER_MODEM_REMOVED,
    MANAGER_HANDLER_COUNT
};

struct ofono_modem_priv {
    const char* id;
    char* name;
    char* manufacturer;
    char* model;
    char* revision;
    char* serial;
    char* type;
    GHashTable* intf_objects;
    OfonoManager* manager;
    gulong manager_handler_id[MANAGER_HANDLER_COUNT];
};

typedef OfonoObjectClass OfonoModemClass;
G_DEFINE_TYPE(OfonoModem, ofono_modem, OFONO_TYPE_OBJECT)

#define MODEM_SIGNAL_POWERED_CHANGED_NAME       "powered-changed"
#define MODEM_SIGNAL_ONLINE_CHANGED_NAME        "online-changed"
#define MODEM_SIGNAL_LOCKDOWN_CHANGED_NAME      "lockdown-changed"
#define MODEM_SIGNAL_EMERGENCY_CHANGED_NAME     "emergency-changed"
#define MODEM_SIGNAL_NAME_CHANGED_NAME          "name-changed"
#define MODEM_SIGNAL_MANUFACTURER_CHANGED_NAME  "manufacturer-changed"
#define MODEM_SIGNAL_MODEL_CHANGED_NAME         "model-changed"
#define MODEM_SIGNAL_REVISION_CHANGED_NAME      "revision-changed"
#define MODEM_SIGNAL_SERIAL_CHANGED_NAME        "serial-changed"
#define MODEM_SIGNAL_FEATURES_CHANGED_NAME      "features-changed"
#define MODEM_SIGNAL_INTERFACES_CHANGED_NAME    "interfaces-changed"
#define MODEM_SIGNAL_TYPE_CHANGED_NAME          "type-changed"

static GHashTable* ofono_modem_table = NULL;

/*==========================================================================*
 * Implementation
 *==========================================================================*/

#if GUTIL_LOG_DEBUG
static
gboolean
ofono_modem_initialized(
    OfonoObject* object)
{
    OfonoModem* self = OFONO_MODEM(object);
    GDEBUG("%s: Modem %sline", self->priv->id, self->online ? "on" : "off");
    return OFONO_OBJECT_CLASS(ofono_modem_parent_class)->fn_initialized(object);
}
#endif /* GUTIL_LOG_DEBUG */

static
void
ofono_modem_interface_destroyed(
    gpointer arg,
    GObject* intf)
{
    OfonoModem* self = OFONO_MODEM(arg);
    OfonoModemPriv* priv = self->priv;
    GHashTableIter it;
    gpointer key, value;
    GASSERT(priv->intf_objects);
    g_hash_table_iter_init(&it, priv->intf_objects);
    while (g_hash_table_iter_next(&it, &key, &value)) {
        if (value == intf) {
            GVERBOSE_("%s", (char*)key);
            g_hash_table_iter_remove(&it);
            break;
        }
    }
}

static
void
ofono_modem_destroyed(
    gpointer key,
    GObject* modem)
{
    GASSERT(ofono_modem_table);
    GVERBOSE_("%s", (char*)key);
    if (ofono_modem_table) {
        GASSERT(g_hash_table_lookup(ofono_modem_table, key) == modem);
        g_hash_table_remove(ofono_modem_table, key);
        if (g_hash_table_size(ofono_modem_table) == 0) {
            g_hash_table_unref(ofono_modem_table);
            ofono_modem_table = NULL;
        }
    }
}

static
OfonoModem*
ofono_modem_create(
    const char* path,
    GVariant* properties)
{
    OfonoModem* modem = (path && ofono_modem_table) ?
        ofono_modem_ref(g_hash_table_lookup(ofono_modem_table, path)) : NULL;
    if (!modem) {
        OfonoObject* object;
        OfonoModemPriv* priv;
        char* key = g_strdup(path);
        modem = g_object_new(OFONO_TYPE_MODEM, NULL);
        priv = modem->priv;
        object = &modem->object;

        ofono_object_initialize(object, OFONO_MODEM_INTERFACE_NAME, path);
        priv->id = ofono_object_name(object);
        if (!ofono_modem_table) {
            ofono_modem_table = g_hash_table_new_full(g_str_hash,
                g_str_equal, g_free, NULL);
        }
        g_hash_table_replace(ofono_modem_table, key, modem);
        g_object_weak_ref(G_OBJECT(modem), ofono_modem_destroyed, key);
        ofono_object_set_invalid(&modem->object, !priv->manager->valid ||
            !ofono_manager_has_modem(priv->manager, path));
        GDEBUG("Modem '%s'", path);
    }
    if (properties) ofono_object_apply_properties(&modem->object, properties);
    return modem;
}

static
void
ofono_modem_removed(
    OfonoManager* manager,
    const char* path,
    void* arg)
{
    OfonoModem* modem = arg;
    GASSERT(modem->priv->manager == manager);
    if (!g_strcmp0(path, modem->object.path)) {
        GDEBUG("Modem '%s' is gone", path);
        ofono_object_set_invalid(&modem->object, TRUE);
    }
}

static
void
ofono_modem_manager_valid_changed(
    OfonoManager* manager,
    void* arg)
{
    OfonoModem* modem = arg;
    GASSERT(modem->priv->manager == manager);
    ofono_object_set_invalid(&modem->object, !manager->valid ||
        !ofono_manager_has_modem(manager, modem->object.path));
}

OFONO_INLINE
GCancellable*
ofono_modem_set_boolean_full(
    OfonoModem* self,
    const char* name,
    gboolean value,
    OfonoModemCallHandler callback,
    void* arg)
{
    return ofono_object_set_boolean(ofono_modem_object(self), name, value,
        (OfonoObjectCallFinishedCallback)callback, arg);
}

/*==========================================================================*
 * API
 *==========================================================================*/

OfonoModem*
ofono_modem_ref(
    OfonoModem* self)
{
    if (G_LIKELY(self)) {
        g_object_ref(OFONO_MODEM(self));
        return self;
    } else {
        return NULL;
    }
}

void
ofono_modem_unref(
    OfonoModem* self)
{
    if (G_LIKELY(self)) {
        g_object_unref(OFONO_MODEM(self));
    }
}

OfonoModem*
ofono_modem_new(
    const char* path)
{
    return ofono_modem_create(path, NULL);
}

OfonoModem*
ofono_modem_added(
    const char* path,
    GVariant* properties)
{
    return ofono_modem_create(path, properties);
}

gboolean
ofono_modem_equal(
    OfonoModem* modem1,
    OfonoModem* modem2)
{
    if (modem1 == modem2) {
        return TRUE;
    } else if (modem1 && modem2) {
        return !g_strcmp0(modem1->object.path, modem2->object.path);
    } else {
        return FALSE;
    }
}

OfonoModemInterface*
ofono_modem_get_interface(
    OfonoModem* self,
    const char* intf)
{
    if (G_LIKELY(self) && G_LIKELY(intf)) {
        OfonoModemPriv* priv = self->priv;
        if (priv->intf_objects) {
            return g_hash_table_lookup(priv->intf_objects, intf);
        }
    }
    return NULL;
}

gboolean
ofono_modem_has_interface(
    OfonoModem* self,
    const char* intf)
{
    if (G_LIKELY(self) && G_LIKELY(intf)) {
        GPtrArray* interfaces = self->interfaces;
        if (interfaces) {
            guint i;
            for (i=0; i<interfaces->len; i++) {
                if (!strcmp(interfaces->pdata[i], intf)) {
                    return TRUE;
                }
            }
        }
    }
    return FALSE;
}

void
ofono_modem_set_interface(
    OfonoModem* self,
    OfonoModemInterface* intf)
{
    if (G_LIKELY(self) && G_LIKELY(intf)) {
        OfonoModemPriv* priv = self->priv;
        char* key = g_strdup(intf->object.intf);
        if (!priv->intf_objects) {
            priv->intf_objects = g_hash_table_new_full(g_str_hash, g_str_equal,
                g_free, NULL);
        }
        GASSERT(!g_hash_table_lookup(priv->intf_objects, key));
        g_hash_table_replace(priv->intf_objects, key, intf);
        g_object_weak_ref(G_OBJECT(intf), ofono_modem_interface_destroyed,
            self);
    }
}

gulong
ofono_modem_add_property_changed_handler(
    OfonoModem* self,
    OfonoModemPropertyHandler fn,
    const char* name,
    void* arg)
{
    return G_LIKELY(self) ? ofono_object_add_property_changed_handler(
        &self->object, (OfonoObjectPropertyHandler)fn, name, arg) :  0;
}

gulong
ofono_modem_add_valid_changed_handler(
    OfonoModem* self,
    OfonoModemHandler fn,
    void* arg)
{
    return G_LIKELY(self) ? ofono_object_add_valid_changed_handler(
        &self->object, (OfonoObjectHandler)fn, arg) : 0;
}

gulong
ofono_modem_add_powered_changed_handler(
    OfonoModem* self,
    OfonoModemHandler fn,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(fn)) ? g_signal_connect(self,
        MODEM_SIGNAL_POWERED_CHANGED_NAME, G_CALLBACK(fn), arg) : 0;
}

gulong
ofono_modem_add_online_changed_handler(
    OfonoModem* self,
    OfonoModemHandler fn,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(fn)) ? g_signal_connect(self,
        MODEM_SIGNAL_ONLINE_CHANGED_NAME, G_CALLBACK(fn), arg) : 0;
}

gulong
ofono_modem_add_lockdown_changed_handler(
    OfonoModem* self,
    OfonoModemHandler fn,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(fn)) ? g_signal_connect(self,
        MODEM_SIGNAL_LOCKDOWN_CHANGED_NAME, G_CALLBACK(fn), arg) : 0;
}

gulong
ofono_modem_add_emergency_changed_handler(
    OfonoModem* self,
    OfonoModemHandler fn,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(fn)) ? g_signal_connect(self,
        MODEM_SIGNAL_EMERGENCY_CHANGED_NAME, G_CALLBACK(fn), arg) : 0;
}

gulong
ofono_modem_add_interfaces_changed_handler(
    OfonoModem* self,
    OfonoModemHandler fn,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(fn)) ? g_signal_connect(self,
        MODEM_SIGNAL_INTERFACES_CHANGED_NAME, G_CALLBACK(fn), arg) : 0;
}

void
ofono_modem_remove_handler(
    OfonoModem* self,
    gulong id)
{
    if (G_LIKELY(self) && G_LIKELY(id)) {
        g_signal_handler_disconnect(self, id);
    }
}

gboolean
ofono_modem_set_powered(
    OfonoModem* modem,
    gboolean powered)
{
    return ofono_modem_set_powered_full(modem, powered, NULL, NULL) != NULL;
}

gboolean
ofono_modem_set_online(
    OfonoModem* modem,
    gboolean online)
{
    return ofono_modem_set_online_full(modem, online, NULL, NULL) != NULL;
}

GCancellable*
ofono_modem_set_powered_full(
    OfonoModem* modem,
    gboolean powered,
    OfonoModemCallHandler callback,
    void* arg)
{
    return ofono_modem_set_boolean_full(modem, OFONO_MODEM_PROPERTY_POWERED,
        powered, callback, arg);
}

GCancellable*
ofono_modem_set_online_full(
    OfonoModem* modem,
    gboolean online,
    OfonoModemCallHandler callback,
    void* arg)
{
    return ofono_modem_set_boolean_full(modem, OFONO_MODEM_PROPERTY_ONLINE,
        online, callback, arg);
}

/*==========================================================================*
 * Properties
 *==========================================================================*/

#define MODEM_DEFINE_PROPERTY_BOOL(NAME,var) \
    OFONO_OBJECT_DEFINE_PROPERTY_BOOL(MODEM,NAME,OfonoModem,var)

#define MODEM_DEFINE_PROPERTY_STRING_ARRAY(NAME,var) \
    OFONO_OBJECT_DEFINE_PROPERTY_STRING_ARRAY(MODEM,NAME,OfonoModem,var)

#define MODEM_DEFINE_PROPERTY_STRING(NAME,var) \
    OFONO_OBJECT_DEFINE_PROPERTY_STRING(MODEM,modem,NAME,OfonoModem,var)

static
void*
ofono_modem_property_priv(
    OfonoObject* object,
    const OfonoObjectProperty* prop)
{
    return OFONO_MODEM(object)->priv;
}

/*==========================================================================*
 * Internals
 *==========================================================================*/

/**
 * Per instance initializer
 */
static
void
ofono_modem_init(
    OfonoModem* self)
{
    OfonoModemPriv* priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
        OFONO_TYPE_MODEM, OfonoModemPriv);
    self->priv = priv;
    priv->manager = ofono_manager_new();
    priv->manager_handler_id[MANAGER_HANDLER_VALID_CHANGED] =
        ofono_manager_add_valid_changed_handler(priv->manager,
            ofono_modem_manager_valid_changed, self);
    priv->manager_handler_id[MANAGER_HANDLER_MODEM_REMOVED] =
        ofono_manager_add_modem_removed_handler(priv->manager,
            ofono_modem_removed, self);
}

/**
 * Final stage of deinitialization
 */
static
void
ofono_modem_finalize(
    GObject* object)
{
    OfonoModem* self = OFONO_MODEM(object);
    OfonoModemPriv* priv = self->priv;
    unsigned int i;
    for (i=0; i<G_N_ELEMENTS(priv->manager_handler_id); i++) {
        ofono_manager_remove_handler(priv->manager,
            priv->manager_handler_id[i]);
    }
    ofono_manager_unref(priv->manager);
    if (priv->intf_objects) {
        GHashTableIter it;
        gpointer key, value;
        g_hash_table_iter_init(&it, priv->intf_objects);
        while (g_hash_table_iter_next(&it, &key, &value)) {
            g_object_weak_unref(G_OBJECT(value),
                ofono_modem_interface_destroyed, self);
            g_hash_table_iter_remove(&it);
        }
        g_hash_table_unref(priv->intf_objects);
    }
    g_free(priv->name);
    g_free(priv->manufacturer);
    g_free(priv->model);
    g_free(priv->revision);
    g_free(priv->serial);
    g_free(priv->type);
    if (self->features) g_ptr_array_unref(self->features);
    if (self->interfaces) g_ptr_array_unref(self->interfaces);
    G_OBJECT_CLASS(ofono_modem_parent_class)->finalize(object);
}

/**
 * Per class initializer
 */
static
void
ofono_modem_class_init(
    OfonoModemClass* klass)
{
    static OfonoObjectProperty ofono_modem_properties[] = {
        MODEM_DEFINE_PROPERTY_BOOL(POWERED,powered),
        MODEM_DEFINE_PROPERTY_BOOL(ONLINE,online),
        MODEM_DEFINE_PROPERTY_BOOL(LOCKDOWN,lockdown),
        MODEM_DEFINE_PROPERTY_BOOL(EMERGENCY,emergency),
        MODEM_DEFINE_PROPERTY_STRING(NAME,name),
        MODEM_DEFINE_PROPERTY_STRING(MANUFACTURER,manufacturer),
        MODEM_DEFINE_PROPERTY_STRING(MODEL,model),
        MODEM_DEFINE_PROPERTY_STRING(REVISION,revision),
        MODEM_DEFINE_PROPERTY_STRING(SERIAL,serial),
        MODEM_DEFINE_PROPERTY_STRING(TYPE,type),
        MODEM_DEFINE_PROPERTY_STRING_ARRAY(FEATURES,features),
        MODEM_DEFINE_PROPERTY_STRING_ARRAY(INTERFACES,interfaces)
    };

    klass->properties = ofono_modem_properties;
    klass->nproperties = G_N_ELEMENTS(ofono_modem_properties);
    G_OBJECT_CLASS(klass)->finalize = ofono_modem_finalize;
    g_type_class_add_private(klass, sizeof(OfonoModemPriv));
    OFONO_OBJECT_CLASS_SET_PROXY_CALLBACKS(klass, org_ofono_modem);
#if GUTIL_LOG_DEBUG
    klass->fn_initialized = ofono_modem_initialized;
#endif /* GUTIL_LOG_DEBUG */
    ofono_class_initialize(klass);
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
