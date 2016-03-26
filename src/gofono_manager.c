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
#include "gofono_manager_proxy.h"
#include "gofono_util_p.h"
#include "gofono_modem.h"
#include "gofono_names.h"
#include "gofono_log.h"

#include <gutil_misc.h>

/* Log module */
GLOG_MODULE_DEFINE("ofono");

/* Object definition */
enum proxy_handler_id {
    PROXY_HANDLER_VALID_CHANGED,
    PROXY_HANDLER_MODEM_ADDED,
    PROXY_HANDLER_MODEM_REMOVED,
    PROXY_HANDLER_COUNT
};

typedef struct ofono_manager_modem_data {
    OfonoModem* modem;
    gulong valid_handler_id;
} OfonoManagerModemData;

struct ofono_manager_priv {
    OfonoManagerProxy* proxy;
    gulong proxy_handler_id[PROXY_HANDLER_COUNT];
    GUtilIdlePool* pool;
    GHashTable* all_modems;
    GPtrArray* valid_modems;
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

#define MANAGER_SIGNAL_VALID_CHANGED_NAME       "gofono-valid-changed"
#define MANAGER_SIGNAL_MODEM_ADDED_NAME         "gofono-modem-added"
#define MANAGER_SIGNAL_MODEM_REMOVED_NAME       "gofono-modem-removed"

static guint ofono_manager_signals[MANAGER_SIGNAL_COUNT] = { 0 };

/*==========================================================================*
 * Implementation
 *==========================================================================*/

static
void
ofono_manager_modem_data_destroy(
    gpointer value)
{
    OfonoManagerModemData* data = value;
    ofono_modem_remove_handler(data->modem, data->valid_handler_id);
    ofono_modem_unref(data->modem);
    g_slice_free(OfonoManagerModemData, data);
}

static
gint
ofono_manager_sort_modems(
    gconstpointer s1,
    gconstpointer s2)
{
    OfonoModem* const* m1 = s1;
    OfonoModem* const* m2 = s2;
    return g_strcmp0(ofono_modem_path(*m1), ofono_modem_path(*m2));
}

static
int
ofono_manager_find_valid_modem(
    OfonoManager* self,
    const char* path)
{
    if (path) {
        OfonoManagerPriv* priv = self->priv;
        GPtrArray* modems = priv->valid_modems;
        guint i;
        for (i=0; i<modems->len; i++) {
            if (!g_strcmp0(ofono_modem_path(modems->pdata[i]), path)) {
                return i;
            }
        }
    }
    return -1;
}

static
void
ofono_manager_update_valid(
    OfonoManager* self)
{
    OfonoManagerPriv* priv = self->priv;
    gboolean valid = priv->proxy->valid &&
        priv->valid_modems->len == g_hash_table_size(priv->all_modems);
    if (self->valid != valid) {
        self->valid = valid;
        g_signal_emit(self, ofono_manager_signals[
            MANAGER_SIGNAL_VALID_CHANGED], 0);
    }
}

static
void
ofono_manager_add_valid_modem(
    OfonoManager* self,
    OfonoModem* modem)
{
    GASSERT(ofono_modem_valid(modem));
    if (!ofono_manager_has_modem(self, ofono_modem_path(modem))) {
        OfonoManagerPriv* priv = self->priv;
        g_ptr_array_add(priv->valid_modems, ofono_modem_ref(modem));
        g_ptr_array_sort(priv->valid_modems, ofono_manager_sort_modems);
        if (self->valid) {
            g_signal_emit(self, ofono_manager_signals[
                MANAGER_SIGNAL_MODEM_ADDED], 0, modem);
        }
    }
}

static
void
ofono_manager_remove_valid_modem(
    OfonoManager* self,
    const char* path)
{
    const int index = ofono_manager_find_valid_modem(self, path);
    if (index >= 0) {
        OfonoManagerPriv* priv = self->priv;
        g_ptr_array_remove_index(priv->valid_modems, index);
        g_signal_emit(self, ofono_manager_signals[
            MANAGER_SIGNAL_MODEM_REMOVED], 0, path);
    }
}

static
void
ofono_manager_modem_valid_changed(
    OfonoModem* modem,
    void* arg)
{
    OfonoManager* self = OFONO_MANAGER(arg);
    const gboolean valid = ofono_modem_valid(modem);
    const char* path = ofono_modem_path(modem);
    GVERBOSE_("%s %svalid", path, valid ? "" : "in");
    if (valid) {
        ofono_manager_add_valid_modem(self, modem);
    } else {
        ofono_manager_remove_valid_modem(self, path);
    }
    ofono_manager_update_valid(self);
}

static
void
ofono_manager_add_modem(
    OfonoManager* self,
    const char* path)
{
    OfonoManagerPriv* priv = self->priv;
    GASSERT(path);
    if (path && path[0] == '/') {
        OfonoModem* modem = ofono_modem_new(path);
        OfonoManagerModemData* data = g_slice_new0(OfonoManagerModemData);
        gpointer key = (gpointer)ofono_modem_path(modem);

        data->modem = modem;
        data->valid_handler_id =
            ofono_modem_add_valid_changed_handler(modem,
                ofono_manager_modem_valid_changed, self);

        GASSERT(!g_hash_table_lookup(priv->all_modems, path));
        g_hash_table_replace(priv->all_modems, key, data);

        if (ofono_modem_valid(modem)) {
            ofono_manager_add_valid_modem(self, modem);
        }
    }
}

static
void
ofono_manager_proxy_valid_changed(
    OfonoManagerProxy* proxy,
    gpointer data)
{
    ofono_manager_update_valid(OFONO_MANAGER(data));
}

static
void
ofono_manager_modem_added(
    OfonoManagerProxy* proxy,
    const char* path,
    gpointer data)
{
    OfonoManager* self = OFONO_MANAGER(data);
    GVERBOSE_("%s", path);
    ofono_manager_add_modem(self, path);
}

static
void
ofono_manager_modem_removed(
    OfonoManagerProxy* proxy,
    const char* path,
    gpointer data)
{
    OfonoManager* self = OFONO_MANAGER(data);
    OfonoManagerPriv* priv = self->priv;
    GVERBOSE_("%s", path);
    g_hash_table_remove(priv->all_modems, path);
    ofono_manager_remove_valid_modem(self, path);
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
        guint i;
        priv->proxy = ofono_manager_proxy_new();
        priv->proxy_handler_id[PROXY_HANDLER_VALID_CHANGED] =
            ofono_manager_proxy_add_valid_changed_handler(priv->proxy,
                ofono_manager_proxy_valid_changed, self);
        priv->proxy_handler_id[PROXY_HANDLER_MODEM_ADDED] =
            ofono_manager_proxy_add_modem_added_handler(priv->proxy,
                ofono_manager_modem_added, self);
        priv->proxy_handler_id[PROXY_HANDLER_MODEM_REMOVED] =
            ofono_manager_proxy_add_modem_removed_handler(priv->proxy,
                ofono_manager_modem_removed, self);
        for (i=0; i<priv->proxy->modem_paths->len; i++) {
            ofono_manager_add_modem(self, priv->proxy->modem_paths->pdata[i]);
        }
        ofono_manager_update_valid(self);
        return self;
    } else {
        GERR("%s", GERRMSG(error));
        g_error_free(error);
    }
    return NULL;
}

OfonoManager*
ofono_manager_new()
{
    static OfonoManager* ofono_manager_instance = NULL;
    if (ofono_manager_instance) {
        ofono_manager_ref(ofono_manager_instance);
    } else {
        ofono_manager_instance = ofono_manager_create();
        g_object_add_weak_pointer(G_OBJECT(ofono_manager_instance),
            (gpointer*)&ofono_manager_instance);
    }
    return ofono_manager_instance;
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

GPtrArray*
ofono_manager_get_modems(
    OfonoManager* self)
{
    GPtrArray* modems = NULL;
    if (G_LIKELY(self)) {
        guint i;
        OfonoManagerPriv* priv = self->priv;
        GPtrArray* list = priv->valid_modems;
        modems = g_ptr_array_new_full(list->len, g_object_unref);
        for (i=0; i<list->len; i++) {
            g_ptr_array_add(modems, ofono_modem_ref(list->pdata[i]));
        }
        gutil_idle_pool_add_ptr_array(priv->pool, modems);
    }
    return modems;
}

gboolean
ofono_manager_has_modem(
    OfonoManager* self,
    const char* path)
{
    return self && ofono_manager_find_valid_modem(self, path) >= 0;
}

gulong
ofono_manager_add_valid_changed_handler(
    OfonoManager* self,
    OfonoManagerHandler cb,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(cb)) ? g_signal_connect(self,
        MANAGER_SIGNAL_VALID_CHANGED_NAME, G_CALLBACK(cb), arg) : 0;
}

gulong
ofono_manager_add_modem_added_handler(
    OfonoManager* self,
    OfonoManagerModemAddedHandler cb,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(cb)) ? g_signal_connect(self,
        MANAGER_SIGNAL_MODEM_ADDED_NAME, G_CALLBACK(cb), arg) : 0;
}

gulong
ofono_manager_add_modem_removed_handler(
    OfonoManager* self,
    OfonoManagerModemRemovedHandler cb,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(cb)) ? g_signal_connect(self,
        MANAGER_SIGNAL_MODEM_REMOVED_NAME, G_CALLBACK(cb), arg) : 0;
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
    gutil_disconnect_handlers(self, ids, count);
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
    priv->pool = gutil_idle_pool_ref(ofono_idle_pool());
    priv->valid_modems = g_ptr_array_new_with_free_func(g_object_unref);
    priv->all_modems = g_hash_table_new_full(g_str_hash, g_str_equal, NULL,
        ofono_manager_modem_data_destroy);
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
    self->valid = FALSE;
    g_ptr_array_set_size(priv->valid_modems, 0);
    g_hash_table_remove_all(priv->all_modems);
    gutil_disconnect_handlers(priv->proxy, priv->proxy_handler_id,
        G_N_ELEMENTS(priv->proxy_handler_id));
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
    gutil_idle_pool_unref(priv->pool);
    g_ptr_array_unref(priv->valid_modems);
    g_hash_table_destroy(priv->all_modems);
    g_object_unref(priv->proxy);
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
    GType class_type = G_OBJECT_CLASS_TYPE(klass);
    object_class->dispose = ofono_manager_dispose;
    object_class->finalize = ofono_manager_finalize;
    g_type_class_add_private(klass, sizeof(OfonoManagerPriv));
    ofono_manager_signals[MANAGER_SIGNAL_VALID_CHANGED] =
        g_signal_new(MANAGER_SIGNAL_VALID_CHANGED_NAME, class_type,
            G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL,
            G_TYPE_NONE, 0);
    ofono_manager_signals[MANAGER_SIGNAL_MODEM_ADDED] =
        g_signal_new(MANAGER_SIGNAL_MODEM_ADDED_NAME, class_type,
            G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL,
            G_TYPE_NONE, 1, G_TYPE_OBJECT);
    ofono_manager_signals[MANAGER_SIGNAL_MODEM_REMOVED] =
        g_signal_new(MANAGER_SIGNAL_MODEM_REMOVED_NAME, class_type,
            G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL,
            G_TYPE_NONE, 1, G_TYPE_STRING);
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
