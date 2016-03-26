/*
 * Copyright (C) 2016 Jolla Ltd.
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

#include "gofono_manager_proxy.h"
#include "gofono_modem_p.h"
#include "gofono_error_p.h"
#include "gofono_names.h"
#include "gofono_log.h"

#include <gutil_misc.h>

/*
 * OfonoManagerProxy exists mostly to break two-way dependency between
 * OfonoManager and OfonoModem.
 */

/* Generated headers */
#include "org.ofono.Manager.h"

/* Object definition */
enum proxy_handler_id {
    PROXY_HANDLER_MODEM_ADDED,
    PROXY_HANDLER_MODEM_REMOVED,
    PROXY_HANDLER_COUNT
};

struct ofono_manager_proxy_priv {
    GDBusConnection* bus;
    OrgOfonoManager* proxy;
    GCancellable* cancel;
    guint ofono_watch_id;
    gulong proxy_handler_id[PROXY_HANDLER_COUNT];
};

typedef GObjectClass OfonoManagerProxyClass;
G_DEFINE_TYPE(OfonoManagerProxy, ofono_manager_proxy, G_TYPE_OBJECT)
#define OFONO_TYPE_MANAGER_PROXY (ofono_manager_proxy_get_type())
#define OFONO_MANAGER_PROXY(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
        OFONO_TYPE_MANAGER_PROXY, OfonoManagerProxy))

enum ofono_proxy_manager_signal {
    SIGNAL_VALID_CHANGED,
    SIGNAL_MODEM_ADDED,
    SIGNAL_MODEM_REMOVED,
    SIGNAL_COUNT
};

#define SIGNAL_VALID_CHANGED_NAME       "gofono-valid-changed"
#define SIGNAL_MODEM_ADDED_NAME         "gofono-modem-added"
#define SIGNAL_MODEM_REMOVED_NAME       "gofono-modem-removed"

static guint ofono_manager_proxy_signals[SIGNAL_COUNT] = { 0 };

/*==========================================================================*
 * Implementation
 *==========================================================================*/

static
gint
ofono_manager_proxy_sort_modems(
    gconstpointer a,
    gconstpointer b)
{
    const char* const* s1 = a;
    const char* const* s2 = b;
    return g_strcmp0(*s1, *s2);
}

static
int
ofono_manager_proxy_modem_index(
    OfonoManagerProxy* self,
    const char* path)
{
    if (path) {
        guint i;
        GPtrArray* list = self->modem_paths;
        for (i=0; i<list->len; i++) {
            if (!g_strcmp0(list->pdata[i], path)) {
                return i;
            }
        }
    }
    return -1;
}

static
void
ofono_manager_proxy_reset(
    OfonoManagerProxy* self)
{
    OfonoManagerProxyPriv* priv = self->priv;
    g_ptr_array_set_size(self->modem_paths, 0);
    if (priv->cancel) {
        g_cancellable_cancel(priv->cancel);
        g_object_unref(priv->cancel);
        priv->cancel = NULL;
    }
    if (priv->proxy) {
        gutil_disconnect_handlers(priv->proxy, priv->proxy_handler_id,
            G_N_ELEMENTS(priv->proxy_handler_id));
        g_object_unref(priv->proxy);
        priv->proxy = NULL;
    }
    if (self->valid) {
        self->valid = FALSE;
        g_signal_emit(self, ofono_manager_proxy_signals[
            SIGNAL_VALID_CHANGED], 0);
    }
}

static
void
ofono_manager_proxy_add_modem(
    OfonoManagerProxy* self,
    const char* path)
{
    if (ofono_manager_proxy_modem_index(self, path) < 0) {
        g_ptr_array_add(self->modem_paths, g_strdup(path));
        g_ptr_array_sort(self->modem_paths, ofono_manager_proxy_sort_modems);
        g_signal_emit(self, ofono_manager_proxy_signals[
            SIGNAL_MODEM_ADDED], 0, path);
    }
}

static
void
ofono_manager_proxy_modem_added(
    OrgOfonoManager* proxy,
    const char* path,
    GVariant* properties,
    gpointer data)
{
    OfonoManagerProxy* self = OFONO_MANAGER_PROXY(data);
    GVERBOSE_("%s", path);
    GASSERT(proxy == self->priv->proxy);
    ofono_manager_proxy_add_modem(self, path);
}

static
void
ofono_manager_proxy_modem_removed(
    OrgOfonoManager* proxy,
    const char* path,
    gpointer data)
{
    OfonoManagerProxy* self = OFONO_MANAGER_PROXY(data);
    int index = ofono_manager_proxy_modem_index(self, path);
    GVERBOSE_("%s", path);
    GASSERT(index >= 0);
    if (index >= 0) {
        g_ptr_array_remove_index(self->modem_paths, index);
        g_signal_emit(self, ofono_manager_proxy_signals[
            SIGNAL_MODEM_REMOVED], 0, path);
    }
}

static
void
ofono_manager_proxy_get_modems_finished(
    GObject* proxy,
    GAsyncResult* result,
    gpointer data)
{
    GError* error = NULL;
    GVariant* modems = NULL;
    OfonoManagerProxy* self = OFONO_MANAGER_PROXY(data);
    OfonoManagerProxyPriv* priv = self->priv;
    gboolean need_cancel = FALSE;

    GASSERT(ORG_OFONO_MANAGER(proxy) == priv->proxy);

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
            ofono_manager_proxy_add_modem(self, path);
            g_variant_unref(properties);
        }
        g_variant_unref(modems);

        GASSERT(!self->valid);
        self->valid = TRUE;
        g_signal_emit(self, ofono_manager_proxy_signals[
            SIGNAL_VALID_CHANGED], 0);
    } else if (ofono_error_is_generic_timeout(error)) {
        GWARN("%s.GetModems %s", OFONO_MANAGER_INTERFACE_NAME, GERRMSG(error));
        /* If priv->cancel is NULL, it must have been cancelled,
         * don't retry then */
        if (priv->cancel) {
            need_cancel = TRUE;
            GDEBUG("Retrying %s.GetModems", OFONO_MANAGER_INTERFACE_NAME);
            org_ofono_manager_call_get_modems(priv->proxy, priv->cancel,
                ofono_manager_proxy_get_modems_finished, g_object_ref(self));
        }
    } else {
        GERR("%s.GetModems %s", OFONO_MANAGER_INTERFACE_NAME, GERRMSG(error));
    }

    if (!need_cancel && priv->cancel) {
        g_object_unref(priv->cancel);
        priv->cancel = NULL;
    }

    if (error) g_error_free(error);
    g_object_unref(self);
}

static
void
ofono_manager_proxy_created(
    GObject* bus,
    GAsyncResult* result,
    gpointer data)
{
    OfonoManagerProxy* self = OFONO_MANAGER_PROXY(data);
    OfonoManagerProxyPriv* priv = self->priv;
    GError* error = NULL;
    OrgOfonoManager* proxy = org_ofono_manager_proxy_new_finish(result, &error);
    if (proxy) {
        GASSERT(!priv->proxy);
        priv->proxy = proxy;

        /* Subscribe for ModemAdded/Removed notifications */
        priv->proxy_handler_id[PROXY_HANDLER_MODEM_ADDED] =
            g_signal_connect(proxy, "modem-added",
            G_CALLBACK(ofono_manager_proxy_modem_added), self);
        priv->proxy_handler_id[PROXY_HANDLER_MODEM_REMOVED] =
            g_signal_connect(proxy, "modem-removed",
            G_CALLBACK(ofono_manager_proxy_modem_removed), self);

        /* Request the list of modems */
        org_ofono_manager_call_get_modems(priv->proxy, priv->cancel,
            ofono_manager_proxy_get_modems_finished, self);
    } else {
        GERR("%s", GERRMSG(error));
        g_error_free(error);
        g_object_unref(self);
    }
}

static
void
ofono_manager_proxy_appeared(
    GDBusConnection* bus,
    const gchar* name,
    const gchar* owner,
    gpointer arg)
{
    OfonoManagerProxy* self = OFONO_MANAGER_PROXY(arg);
    OfonoManagerProxyPriv* priv = self->priv;
    GDEBUG("Name '%s' is owned by %s", name, owner);

    /* Start the initialization sequence */
    GASSERT(!priv->cancel);
    GASSERT(!self->modem_paths->len);
    priv->cancel = g_cancellable_new();
    org_ofono_manager_proxy_new(bus, G_DBUS_PROXY_FLAGS_NONE,
        OFONO_SERVICE, "/", priv->cancel, ofono_manager_proxy_created,
        g_object_ref(self));
}

static
void
ofono_manager_proxy_vanished(
    GDBusConnection* bus,
    const gchar* name,
    gpointer arg)
{
    OfonoManagerProxy* self = OFONO_MANAGER_PROXY(arg);
    GDEBUG("Name '%s' has disappeared", name);
    ofono_manager_proxy_reset(self);
    if (self->valid) {
        self->valid = FALSE;
        g_signal_emit(self, ofono_manager_proxy_signals[
            SIGNAL_VALID_CHANGED], 0);
    }
}

/*==========================================================================*
 * API
 *==========================================================================*/

static
OfonoManagerProxy*
ofono_manager_proxy_create()
{
    GError* error = NULL;
    GDBusConnection* bus = g_bus_get_sync(OFONO_BUS_TYPE, NULL, &error);
    if (bus) {
        OfonoManagerProxy* self = g_object_new(OFONO_TYPE_MANAGER_PROXY, NULL);
        OfonoManagerProxyPriv* priv = self->priv;
        priv->bus = bus;
        priv->ofono_watch_id = g_bus_watch_name_on_connection(bus,
            OFONO_SERVICE, G_BUS_NAME_WATCHER_FLAGS_NONE,
            ofono_manager_proxy_appeared, ofono_manager_proxy_vanished,
            self, NULL);
        return self;
    } else {
        GERR("%s", GERRMSG(error));
        g_error_free(error);
    }
    return NULL;
}

OfonoManagerProxy*
ofono_manager_proxy_new()
{
    static OfonoManagerProxy* ofono_manager_proxy_instance = NULL;
    if (ofono_manager_proxy_instance) {
        g_object_ref(ofono_manager_proxy_instance);
    } else {
        ofono_manager_proxy_instance = ofono_manager_proxy_create();
        g_object_add_weak_pointer(G_OBJECT(ofono_manager_proxy_instance),
            (gpointer*)(&ofono_manager_proxy_instance));
    }
    return ofono_manager_proxy_instance;
}

gboolean
ofono_manager_proxy_has_modem(
    OfonoManagerProxy* self,
    const char* path)
{
    return self && ofono_manager_proxy_modem_index(self, path) >= 0;
}

gulong
ofono_manager_proxy_add_valid_changed_handler(
    OfonoManagerProxy* self,
    OfonoManagerProxyHandler cb,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(cb)) ? g_signal_connect(self,
        SIGNAL_VALID_CHANGED_NAME, G_CALLBACK(cb), arg) : 0;
}

gulong
ofono_manager_proxy_add_modem_added_handler(
    OfonoManagerProxy* self,
    OfonoManagerProxyModemHandler cb,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(cb)) ? g_signal_connect(self,
        SIGNAL_MODEM_ADDED_NAME, G_CALLBACK(cb), arg) : 0;
}

gulong
ofono_manager_proxy_add_modem_removed_handler(
    OfonoManagerProxy* self,
    OfonoManagerProxyModemHandler cb,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(cb)) ? g_signal_connect(self,
        SIGNAL_MODEM_REMOVED_NAME, G_CALLBACK(cb), arg) : 0;
}

/*==========================================================================*
 * Internals
 *==========================================================================*/

/**
 * Per instance initializer
 */
static
void
ofono_manager_proxy_init(
    OfonoManagerProxy* self)
{
    self->modem_paths = g_ptr_array_new_with_free_func(g_free);
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self, OFONO_TYPE_MANAGER_PROXY,
        OfonoManagerProxyPriv);
}

/**
 * First stage of deinitialization (release all references).
 * May be called more than once in the lifetime of the object.
 */
static
void
ofono_manager_proxy_dispose(
    GObject* object)
{
    OfonoManagerProxy* self = OFONO_MANAGER_PROXY(object);
    OfonoManagerProxyPriv* priv = self->priv;
    ofono_manager_proxy_reset(self);
    if (priv->ofono_watch_id) {
        g_bus_unwatch_name(priv->ofono_watch_id);
        priv->ofono_watch_id = 0;
    }
    G_OBJECT_CLASS(ofono_manager_proxy_parent_class)->dispose(object);
}

/**
 * Final stage of deinitialization
 */
static
void
ofono_manager_proxy_finalize(
    GObject* object)
{
    OfonoManagerProxy* self = OFONO_MANAGER_PROXY(object);
    OfonoManagerProxyPriv* priv = self->priv;
    GVERBOSE_("");
    g_ptr_array_unref(self->modem_paths);
    g_object_unref(priv->bus);
    G_OBJECT_CLASS(ofono_manager_proxy_parent_class)->finalize(object);
}

/**
 * Per class initializer
 */
static
void
ofono_manager_proxy_class_init(
    OfonoManagerProxyClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    GType class_type = G_OBJECT_CLASS_TYPE(klass);
    object_class->dispose = ofono_manager_proxy_dispose;
    object_class->finalize = ofono_manager_proxy_finalize;
    g_type_class_add_private(klass, sizeof(OfonoManagerProxyPriv));
    ofono_manager_proxy_signals[SIGNAL_VALID_CHANGED] =
        g_signal_new(SIGNAL_VALID_CHANGED_NAME, class_type,
            G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL,
            G_TYPE_NONE, 0);
    ofono_manager_proxy_signals[SIGNAL_MODEM_ADDED] =
        g_signal_new(SIGNAL_MODEM_ADDED_NAME, class_type,
            G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL,
            G_TYPE_NONE, 1, G_TYPE_STRING);
    ofono_manager_proxy_signals[SIGNAL_MODEM_REMOVED] =
        g_signal_new(SIGNAL_MODEM_REMOVED_NAME, class_type,
            G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL,
            G_TYPE_NONE, 1, G_TYPE_STRING);

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
