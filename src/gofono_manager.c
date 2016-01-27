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

#include "gofono_manager.h"
#include "gofono_names.h"
#include "gofono_util_p.h"
#include "gofono_object_p.h"
#include "gofono_modem_p.h"
#include "gofono_error.h"
#include "gofono_log.h"

/* Generated headers */
#include "org.ofono.Manager.h"

/* Log module */
GLOG_MODULE_DEFINE("ofono");

/* Object definition */
enum proxy_handler_id {
    PROXY_HANDLER_MODEM_ADDED,
    PROXY_HANDLER_MODEM_REMOVED,
    PROXY_HANDLER_COUNT
};
struct ofono_manager_priv {
    GDBusConnection* bus;
    OrgOfonoManager* proxy;
    GCancellable* cancel;
    guint ofono_watch_id;
    gulong proxy_handler_id[PROXY_HANDLER_COUNT];
    GPtrArray* modems;
};

typedef GObjectClass OfonoManagerClass;
G_DEFINE_TYPE(OfonoManager, ofono_manager, G_TYPE_OBJECT)
#define OFONO_TYPE_MANAGER (ofono_manager_get_type())
#define OFONO_MANAGER(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
        OFONO_TYPE_MANAGER, OfonoManager))

enum ofono_manager_signal {
    MANAGER_SIGNAL_VALID_CHANGED,
    MANAGER_SIGNAL_MODEM_ADDED,
    MANAGER_SIGNAL_MODEM_REMOVED,
    MANAGER_SIGNAL_COUNT
};

#define MANAGER_SIGNAL_VALID_CHANGED_NAME   "gofono-valid-changed"
#define MANAGER_SIGNAL_MODEM_ADDED_NAME     "gofono-modem-added"
#define MANAGER_SIGNAL_MODEM_REMOVED_NAME   "gofono-modem-removed"

static guint ofono_manager_signals[MANAGER_SIGNAL_COUNT] = { 0 };

/* Weak reference to the single instance of OfonoManager */
static OfonoManager* ofono_manager_instance = NULL;

/*==========================================================================*
 * Implementation
 *==========================================================================*/

static
gint
ofono_manager_sort_modems(
    gconstpointer s1,
    gconstpointer s2)
{
    return strcmp(s1, s2);
}

static
int
ofono_manager_modem_index(
    OfonoManager* self,
    const char* path)
{
    OfonoManagerPriv* priv = self->priv;
    guint i;
    for (i=0; i<priv->modems->len; i++) {
        if (!strcmp(priv->modems->pdata[i], path)) {
            return i;
        }
    }
    return -1;
}

static
void
ofono_manager_reset(
    OfonoManager* self)
{
    OfonoManagerPriv* priv = self->priv;
    g_ptr_array_set_size(priv->modems, 0);
    if (priv->cancel) {
        g_cancellable_cancel(priv->cancel);
        g_object_unref(priv->cancel);
        priv->cancel = NULL;
    }
    if (priv->proxy) {
        int i;
        for (i=0; i<G_N_ELEMENTS(priv->proxy_handler_id); i++) {
            if (priv->proxy_handler_id[i]) {
                g_signal_handler_disconnect(priv->proxy,
                    priv->proxy_handler_id[i]);
                priv->proxy_handler_id[i] = 0;
            }
        }
        g_object_unref(priv->proxy);
        priv->proxy = NULL;
    }
}

static
void
ofono_manager_add_modem(
    OfonoManager* self,
    const char* path,
    GVariant* properties)
{
    OfonoManagerPriv* priv = self->priv;
    GASSERT(path);
    GASSERT(properties);
    if (path && properties) {
        GASSERT(ofono_manager_modem_index(self, path) < 0);
        if (ofono_manager_modem_index(self, path) < 0) {
            OfonoModem* modem = ofono_modem_added(path, properties);
            g_ptr_array_add(priv->modems, g_strdup(path));
            g_ptr_array_sort(priv->modems, ofono_manager_sort_modems);
            ofono_object_set_invalid(&modem->object, !self->valid);
            if (self->valid) {
                g_signal_emit(self, ofono_manager_signals[
                    MANAGER_SIGNAL_MODEM_ADDED], 0, modem);
            }
            ofono_modem_unref(modem);
        }
    }
}

static
void
ofono_manager_modem_added(
    OrgOfonoManager* proxy,
    const char* path,
    GVariant* properties,
    gpointer data)
{
    OfonoManager* self = OFONO_MANAGER(data);
    GVERBOSE_("%s", path);
    GASSERT(proxy == self->priv->proxy);
    ofono_manager_add_modem(self, path, properties);
}

static
void
ofono_manager_modem_removed(
    OrgOfonoManager* proxy,
    const char* path,
    gpointer data)
{
    OfonoManager* self = OFONO_MANAGER(data);
    OfonoManagerPriv* priv = self->priv;
    const int i = ofono_manager_modem_index(self, path);
    GVERBOSE_("%s", path);
    GASSERT(i >= 0);
    if (i >= 0) {
        g_ptr_array_remove_index(priv->modems, i);
        g_signal_emit(self, ofono_manager_signals[
            MANAGER_SIGNAL_MODEM_REMOVED], 0, path);
    }
}

static
void
ofono_manager_get_modems_done(
    GObject* proxy,
    GAsyncResult* result,
    gpointer data)
{
    GError* error = NULL;
    GVariant* modems = NULL;
    OfonoManager* self = OFONO_MANAGER(data);
    OfonoManagerPriv* priv = self->priv;

    GASSERT(ORG_OFONO_MANAGER(proxy) == priv->proxy);
    if (priv->cancel) {
        g_object_unref(priv->cancel);
        priv->cancel = NULL;
    }

    if (org_ofono_manager_call_get_modems_finish(ORG_OFONO_MANAGER(proxy),
        &modems, result, &error)) {
        GVariantIter iter;
        GVariant* child;
        GDEBUG("%u modem(s) found", (guint)g_variant_n_children(modems));

        for (g_variant_iter_init(&iter, modems);
             (child = g_variant_iter_next_value(&iter)) != NULL;
             g_variant_unref(child)) {
            const char* path = NULL;
            GVariant* properties = NULL;
            g_variant_get(child, "(&o@a{sv})", &path, &properties);
            ofono_manager_add_modem(self, path, properties);
            g_variant_unref(properties);
        }
        g_variant_unref(modems);

        GASSERT(!self->valid);
        self->valid = TRUE;
        g_signal_emit(self, ofono_manager_signals[
            MANAGER_SIGNAL_VALID_CHANGED], 0);
    } else {
        GERR("%s", GERRMSG(error));
        g_error_free(error);
    }

    /* Release the reference we have been holding during initialization */
    ofono_manager_unref(self);
}

static
void
ofono_manager_proxy_created(
    GObject* bus,
    GAsyncResult* result,
    gpointer data)
{
    OfonoManager* self = OFONO_MANAGER(data);
    OfonoManagerPriv* priv = self->priv;
    GError* error = NULL;
    OrgOfonoManager* proxy = org_ofono_manager_proxy_new_finish(result, &error);
    if (proxy) {
        GASSERT(!priv->proxy);
        priv->proxy = proxy;

        /* Request the list of modems */
        org_ofono_manager_call_get_modems(priv->proxy, priv->cancel,
            ofono_manager_get_modems_done, self);

        /* Subscribe for ModemAdded/Removed notifications */
        priv->proxy_handler_id[PROXY_HANDLER_MODEM_ADDED] =
            g_signal_connect(proxy, "modem-added",
            G_CALLBACK(ofono_manager_modem_added), self);
        priv->proxy_handler_id[PROXY_HANDLER_MODEM_REMOVED] =
            g_signal_connect(proxy, "modem-removed",
            G_CALLBACK(ofono_manager_modem_removed), self);
    } else {
        GERR("%s", GERRMSG(error));
        g_error_free(error);
        ofono_manager_unref(self);
    }
}

static
void
ofono_manager_name_appeared(
    GDBusConnection* bus,
    const gchar* name,
    const gchar* owner,
    gpointer arg)
{
    OfonoManager* self = OFONO_MANAGER(arg);
    OfonoManagerPriv* priv = self->priv;
    GDEBUG("Name '%s' is owned by %s", name, owner);

    /* Start the initialization sequence */
    GASSERT(!priv->cancel);
    GASSERT(!priv->modems->len);
    priv->cancel = g_cancellable_new();
    org_ofono_manager_proxy_new(bus, G_DBUS_PROXY_FLAGS_NONE,
        OFONO_SERVICE, "/", priv->cancel, ofono_manager_proxy_created,
        ofono_manager_ref(self));
}

static
void
ofono_manager_name_vanished(
    GDBusConnection* bus,
    const gchar* name,
    gpointer arg)
{
    OfonoManager* self = OFONO_MANAGER(arg);
    GDEBUG("Name '%s' has disappeared", name);
    ofono_manager_reset(self);
    if (self->valid) {
        self->valid = FALSE;
        g_signal_emit(self, ofono_manager_signals[
            MANAGER_SIGNAL_VALID_CHANGED], 0);
    }
}

/*==========================================================================*
 * API
 *==========================================================================*/

static
OfonoManager*
ofono_manager_create()
{
    GError* error = NULL;
    GDBusConnection* bus = g_bus_get_sync(OFONO_BUS_TYPE, NULL, &error);
    if (bus) {
        OfonoManager* self = g_object_new(OFONO_TYPE_MANAGER, NULL);
        OfonoManagerPriv* priv = self->priv;
        priv->bus = bus;
        priv->ofono_watch_id = g_bus_watch_name_on_connection(bus,
            OFONO_SERVICE, G_BUS_NAME_WATCHER_FLAGS_NONE,
            ofono_manager_name_appeared, ofono_manager_name_vanished,
            self, NULL);
        return self;
    } else {
        GERR("%s", GERRMSG(error));
        g_error_free(error);
    }
    return NULL;
}

static
void
ofono_manager_destroyed(
    gpointer arg,
    GObject* object)
{
    GASSERT(object == (GObject*)ofono_manager_instance);
    ofono_manager_instance = NULL;
    GVERBOSE_("%p", object);
}

OfonoManager*
ofono_manager_new()
{
    OfonoManager* manager;
    if (ofono_manager_instance) {
        g_object_ref(manager = ofono_manager_instance);
    } else {
        manager = ofono_manager_create();
        ofono_manager_instance = manager;
        g_object_weak_ref(G_OBJECT(manager), ofono_manager_destroyed, manager);
    }
    return manager;
}

OfonoManager*
ofono_manager_ref(
    OfonoManager* self)
{
    if (G_LIKELY(self)) {
        g_object_ref(OFONO_MANAGER(self));
        return self;
    } else {
        return NULL;
    }
}

void
ofono_manager_unref(
    OfonoManager* self)
{
    if (G_LIKELY(self)) {
        g_object_unref(OFONO_MANAGER(self));
    }
}

static
void
ofono_manager_modem_list_free(
    gpointer data)
{
    ofono_modem_unref(data);
}

GPtrArray*
ofono_manager_get_modems(
    OfonoManager* self)
{
    GPtrArray* modems = NULL;
    if (G_LIKELY(self)) {
        guint i;
        OfonoManagerPriv* priv = self->priv;
        modems = g_ptr_array_new_full(priv->modems->len,
            ofono_manager_modem_list_free);
        for (i=0; i<priv->modems->len; i++) {
            g_ptr_array_add(modems, ofono_modem_new(priv->modems->pdata[i]));
        }
    }
    return modems;
}

gboolean
ofono_manager_has_modem(
    OfonoManager* self,
    const char* path)
{
    return self && path && ofono_manager_modem_index(self, path) >= 0;
}

gulong
ofono_manager_add_valid_changed_handler(
    OfonoManager* self,
    OfonoManagerHandler handler,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(handler)) ? g_signal_connect(self,
        MANAGER_SIGNAL_VALID_CHANGED_NAME, G_CALLBACK(handler), arg) : 0;
}

gulong
ofono_manager_add_modem_added_handler(
    OfonoManager* self,
    OfonoManagerModemAddedHandler handler,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(handler)) ? g_signal_connect(self,
        MANAGER_SIGNAL_MODEM_ADDED_NAME, G_CALLBACK(handler), arg) : 0;
}

gulong
ofono_manager_add_modem_removed_handler(
    OfonoManager* self,
    OfonoManagerModemRemovedHandler handler,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(handler)) ? g_signal_connect(self,
        MANAGER_SIGNAL_MODEM_REMOVED_NAME, G_CALLBACK(handler), arg) : 0;
}

void
ofono_manager_remove_handler(
    OfonoManager* self,
    gulong id)
{
    if (G_LIKELY(self) && G_LIKELY(id)) {
        g_signal_handler_disconnect(self, id);
    }
}

void
ofono_manager_remove_handlers(
    OfonoManager* self,
    gulong* ids,
    unsigned int count)
{
    if (G_LIKELY(self)) {
        unsigned int i;
        for (i=0; i<count; i++) {
            if (G_LIKELY(ids[i])) {
                g_signal_handler_disconnect(self, ids[i]);
                ids[i] = 0;
            }
        }
    }
}

static
gboolean
ofono_manager_wait_valid_check(
    GObject* object)
{
    return OFONO_MANAGER(object)->valid;
}

static
gulong
ofono_manager_wait_valid_add_handler(
    GObject* object,
    OfonoConditionHandler handler,
    void* arg)
{
    return ofono_manager_add_valid_changed_handler(OFONO_MANAGER(object),
        (OfonoManagerHandler)handler, arg);
}

static
void
ofono_manager_wait_valid_remove_handler(
    GObject* object,
    gulong id)
{
    ofono_manager_remove_handler(OFONO_MANAGER(object), id);
}

gboolean
ofono_manager_wait_valid(
    OfonoManager* self,
    int timeout_msec,
    GError** error)
{
    return ofono_condition_wait(&self->object,
        ofono_manager_wait_valid_check,
        ofono_manager_wait_valid_add_handler,
        ofono_manager_wait_valid_remove_handler,
        timeout_msec, error);
}

/*==========================================================================*
 * Internals
 *==========================================================================*/

/**
 * Per instance initializer
 */
static
void
ofono_manager_init(
    OfonoManager* self)
{
    OfonoManagerPriv* priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
        OFONO_TYPE_MANAGER, OfonoManagerPriv);
    self->priv = priv;
    priv->modems = g_ptr_array_new_with_free_func(g_free);
}

/**
 * First stage of deinitialization (release all references).
 * May be called more than once in the lifetime of the object.
 */
static
void
ofono_manager_dispose(
    GObject* object)
{
    OfonoManager* self = OFONO_MANAGER(object);
    OfonoManagerPriv* priv = self->priv;
    ofono_manager_reset(self);
    if (priv->ofono_watch_id) {
        g_bus_unwatch_name(priv->ofono_watch_id);
        priv->ofono_watch_id = 0;
    }
    G_OBJECT_CLASS(ofono_manager_parent_class)->dispose(object);
}

/**
 * Final stage of deinitialization
 */
static
void
ofono_manager_finalize(
    GObject* object)
{
    OfonoManager* self = OFONO_MANAGER(object);
    OfonoManagerPriv* priv = self->priv;
    g_ptr_array_unref(priv->modems);
    g_object_unref(priv->bus);
    G_OBJECT_CLASS(ofono_manager_parent_class)->finalize(object);
}

/**
 * Per class initializer
 */
static
void
ofono_manager_class_init(
    OfonoManagerClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = ofono_manager_dispose;
    object_class->finalize = ofono_manager_finalize;
    g_type_class_add_private(klass, sizeof(OfonoManagerPriv));
    ofono_manager_signals[MANAGER_SIGNAL_VALID_CHANGED] =
        g_signal_new(MANAGER_SIGNAL_VALID_CHANGED_NAME,
            G_OBJECT_CLASS_TYPE(klass), G_SIGNAL_RUN_FIRST,
            0, NULL, NULL, NULL, G_TYPE_NONE, 0);
    ofono_manager_signals[MANAGER_SIGNAL_MODEM_ADDED] =
        g_signal_new(MANAGER_SIGNAL_MODEM_ADDED_NAME,
            G_OBJECT_CLASS_TYPE(klass), G_SIGNAL_RUN_FIRST,
            0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_OBJECT);
    ofono_manager_signals[MANAGER_SIGNAL_MODEM_REMOVED] =
        g_signal_new(MANAGER_SIGNAL_MODEM_REMOVED_NAME,
            G_OBJECT_CLASS_TYPE(klass), G_SIGNAL_RUN_FIRST,
            0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING);

    /* Register ofono errors with gio */
    ofono_error_quark();
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
