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

#include "gofono_connmgr.h"
#include "gofono_connctx.h"
#include "gofono_modem_p.h"
#include "gofono_error_p.h"
#include "gofono_util_p.h"
#include "gofono_names.h"
#include "gofono_log.h"

#include <gutil_misc.h>

/* Generated headers */
#define OFONO_OBJECT_PROXY OrgOfonoConnectionManager
#include "org.ofono.ConnectionManager.h"
#include "gofono_modemintf_p.h"

/* Object definition */
enum proxy_handler_id {
    PROXY_HANDLER_CONTEXT_ADDED,
    PROXY_HANDLER_CONTEXT_REMOVED,
    PROXY_HANDLER_COUNT
};

typedef struct ofono_connmgr_context_data {
    OfonoConnCtx* context;
    gulong valid_handler_id;
} OfonoConnMgrContextData;

typedef struct ofono_connmgr_get_contexts_call {
    OFONO_OBJECT_PROXY* proxy;
    GCancellable* cancel;
    OfonoConnMgr* self;
} OfonoConnMgrGetContextsCall;

struct ofono_connmgr_priv {
    const char* name;
    gulong proxy_handler_id[PROXY_HANDLER_COUNT];
    GUtilIdlePool* pool;
    OfonoConnMgrGetContextsCall* get_contexts_pending;
    GHashTable* all_contexts;
    GPtrArray* valid_contexts;
    gboolean get_contexts_ok;
};

typedef OfonoModemInterfaceClass OfonoConnMgrClass;
G_DEFINE_TYPE(OfonoConnMgr, ofono_connmgr, OFONO_TYPE_MODEM_INTERFACE)
#define SUPER_CLASS ofono_connmgr_parent_class

enum ofono_connmgr_signal {
    CONNMGR_SIGNAL_CONTEXT_ADDED,
    CONNMGR_SIGNAL_CONTEXT_REMOVED,
    CONNMGR_SIGNAL_COUNT
};

#define CONNMGR_SIGNAL_CONTEXT_ADDED_NAME           "context-added"
#define CONNMGR_SIGNAL_CONTEXT_REMOVED_NAME         "context-removed"
#define CONNMGR_SIGNAL_ATTACHED_CHANGED_NAME        "attached-changed"
#define CONNMGR_SIGNAL_ROAMING_ALLOWED_CHANGED_NAME "roaming-allowed-changed"
#define CONNMGR_SIGNAL_POWERED_CHANGED_NAME         "powered-changed"

static guint ofono_connmgr_signals[CONNMGR_SIGNAL_COUNT] = { 0 };

#define CONNMGR_OBJECT_SIGNAL_EMIT(self,name,param) \
    g_signal_emit(self, ofono_connmgr_signals[\
    CONNMGR_SIGNAL_##name], 0, param)

#define CONNMGR_DEFINE_PROPERTY_BOOL(NAME,var) \
    OFONO_OBJECT_DEFINE_PROPERTY_BOOL(CONNMGR,NAME,OfonoConnMgr,var)

/*==========================================================================*
 * Implementation
 *==========================================================================*/

OFONO_INLINE
OFONO_OBJECT_PROXY*
ofono_connmgr_proxy(
    OfonoConnMgr* self)
{
    return ofono_object_proxy(ofono_connmgr_object(self));
}

OFONO_INLINE
void
ofono_connmgr_update_ready(
    OfonoConnMgr* self)
{
    ofono_object_update_ready(ofono_connmgr_object(self));
}

OFONO_INLINE
void
ofono_connmgr_update_valid(
    OfonoConnMgr* self)
{
    ofono_object_update_valid(ofono_connmgr_object(self));
}

static
void
ofono_connmgr_context_data_destroy(
    gpointer value)
{
    OfonoConnMgrContextData* data = value;
    ofono_connctx_remove_handler(data->context, data->valid_handler_id);
    ofono_connctx_unref(data->context);
    g_slice_free(OfonoConnMgrContextData, data);
}

static
gint
ofono_connmgr_context_compare(
    gconstpointer a,
    gconstpointer b)
{
    OfonoConnCtx* const* ctx1 = a;
    OfonoConnCtx* const* ctx2 = b;
    return g_strcmp0(ofono_connctx_path(*ctx1), ofono_connctx_path(*ctx2));
}

static
int
ofono_connmgr_find_valid_context(
    OfonoConnMgr* self,
    const char* path)
{
    if (path) {
        GPtrArray* list = self->priv->valid_contexts;
        int i, n = list->len;
        for (i=0; i<n; i++) {
            OfonoConnCtx* ctx = OFONO_CONNCTX(list->pdata[i]);
            if (!g_strcmp0(path, ofono_connctx_path(ctx))) {
                return i;
            }
        }
    }
    return -1;
}

static
void
ofono_connmgr_add_valid_context(
    OfonoConnMgr* self,
    OfonoConnCtx* ctx)
{
    GASSERT(ofono_connctx_valid(ctx));
    if (ofono_connmgr_find_valid_context(self, ofono_connctx_path(ctx)) < 0) {
        OfonoConnMgrPriv* priv = self->priv;
        g_ptr_array_add(priv->valid_contexts, ofono_connctx_ref(ctx));
        g_ptr_array_sort(priv->valid_contexts, ofono_connmgr_context_compare);
        if (ofono_connmgr_valid(self)) {
            CONNMGR_OBJECT_SIGNAL_EMIT(self, CONTEXT_ADDED, ctx);
        }
    }
}

static
void
ofono_connmgr_remove_valid_context(
    OfonoConnMgr* self,
    const char* path)
{
    const int index = ofono_connmgr_find_valid_context(self, path);
    if (index >= 0) {
        OfonoConnMgrPriv* priv = self->priv;
        g_ptr_array_remove_index(priv->valid_contexts, index);
        if (ofono_connmgr_valid(self)) {
            CONNMGR_OBJECT_SIGNAL_EMIT(self, CONTEXT_REMOVED, path);
        }
    }
}

static
void
ofono_connmgr_context_valid_changed(
    OfonoConnCtx* ctx,
    void* arg)
{
    OfonoConnMgr* self = OFONO_CONNMGR(arg);
    const gboolean valid = ofono_connctx_valid(ctx);
    const char* path = ofono_connctx_path(ctx);
    GVERBOSE_("%s %svalid", path, valid ? "" : "in");
    if (valid) {
        ofono_connmgr_add_valid_context(self, ctx);
    } else {
        ofono_connmgr_remove_valid_context(self, path);
    }
    ofono_connmgr_update_valid(self);
}

static
void
ofono_connmgr_add_context(
    OfonoConnMgr* self,
    const char* path)
{
    if (path && path[0] == '/') {
        OfonoConnMgrPriv* priv = self->priv;
        OfonoConnCtx* ctx = ofono_connctx_new(path);
        OfonoConnMgrContextData* data = g_slice_new0(OfonoConnMgrContextData);
        gpointer key = (gpointer)ofono_connctx_path(ctx);

        data->context = ctx;
        data->valid_handler_id =
            ofono_connctx_add_valid_changed_handler(ctx,
                ofono_connmgr_context_valid_changed, self);

        GASSERT(!g_hash_table_lookup(priv->all_contexts, path));
        g_hash_table_replace(priv->all_contexts, key, data);

        if (ofono_connctx_valid(ctx)) {
            ofono_connmgr_add_valid_context(self, ctx);
        }

        ofono_connmgr_update_valid(self);
    }
}

static
void
ofono_connmgr_context_added(
    OrgOfonoConnectionManager* proxy,
    const char* path,
    GVariant* properties,
    gpointer data)
{
    OfonoConnMgr* self = OFONO_CONNMGR(data);
    GVERBOSE_("%s", path);
    ofono_connmgr_add_context(self, path);
}

static
void
ofono_connmgr_context_removed(
    OrgOfonoConnectionManager* proxy,
    const char* path,
    gpointer data)
{
    OfonoConnMgr* self = OFONO_CONNMGR(data);
    OfonoConnMgrPriv* priv = self->priv;
    GVERBOSE_("%s", path);
    ofono_connmgr_remove_valid_context(self, path);
    g_hash_table_remove(priv->all_contexts, path);
}

static
void
ofono_connmgr_get_contexts_finished(
    GObject* proxy_object,
    GAsyncResult* result,
    gpointer data)
{
    OfonoConnMgrGetContextsCall* call = data;
    GVariant* contexts = NULL;
    GError* error = NULL;
    OFONO_OBJECT_PROXY* proxy = ORG_OFONO_CONNECTION_MANAGER(proxy_object);
    gboolean ok = org_ofono_connection_manager_call_get_contexts_finish(proxy,
        &contexts, result, &error);
    GASSERT(!call->self || call->self->priv->get_contexts_pending == call);
    if (ok) {
        if (call->self) {
            GVariantIter iter;
            GVariant* child;
            GVERBOSE("  %d context(s)", (int)g_variant_n_children(contexts));
            for (g_variant_iter_init(&iter, contexts);
                 (child = g_variant_iter_next_value(&iter)) != NULL;
                 g_variant_unref(child)) {
                const char* path = NULL;
                GVariant* props = NULL;
                g_variant_get(child, "(&o@a{sv})", &path, &props);
                ofono_connmgr_add_context(call->self, path);
                if (props) g_variant_unref(props);
            }
        }
        g_variant_unref(contexts);
    } else if (call->self) {
        if (ofono_error_is_generic_timeout(error)) {
            GWARN("%s.GetContexts - %s", OFONO_CONNMGR_INTERFACE_NAME,
                GERRMSG(error));
            GDEBUG("Retrying %s.GetContexts", OFONO_MANAGER_INTERFACE_NAME);
            org_ofono_connection_manager_call_get_contexts(proxy,
                call->cancel, ofono_connmgr_get_contexts_finished, call);
            call = NULL;
        } else {
            GERR("%s.GetContexts %s", OFONO_CONNMGR_INTERFACE_NAME,
                GERRMSG(error));
        }
    }

    if (error) g_error_free(error);
    if (call) {
        if (call->self) {
            OfonoConnMgrPriv* priv = call->self->priv;
            priv->get_contexts_ok = ok;
            priv->get_contexts_pending = NULL;
            ofono_connmgr_update_valid(call->self);
        }
        g_object_unref(call->proxy);
        g_object_unref(call->cancel);
        g_slice_free(OfonoConnMgrGetContextsCall, call);
    }
}

static
void
ofono_connmgr_cancel_get_contexts(
    OfonoConnMgr* self)
{
    OfonoConnMgrPriv* priv = self->priv;
    if (priv->get_contexts_pending) {
        g_cancellable_cancel(priv->get_contexts_pending->cancel);
        priv->get_contexts_pending->self = NULL;
        priv->get_contexts_pending = NULL;
    }
}

static
void
ofono_connmgr_start_get_contexts(
    OfonoConnMgr* self,
    OFONO_OBJECT_PROXY* proxy)
{
    OfonoConnMgrPriv* priv = self->priv;
    OfonoConnMgrGetContextsCall* call;

    call = g_slice_new0(OfonoConnMgrGetContextsCall);
    call->cancel = g_cancellable_new();
    call->self = self;
    g_object_ref(call->proxy = proxy);

    ofono_connmgr_cancel_get_contexts(self);
    priv->get_contexts_ok = FALSE;
    priv->get_contexts_pending = call;
    org_ofono_connection_manager_call_get_contexts(proxy, call->cancel,
        ofono_connmgr_get_contexts_finished, call);
}

static
void
ofono_connmgr_proxy_created(
    OfonoObject* object,
    OFONO_OBJECT_PROXY* proxy)
{
    OfonoConnMgr* self = OFONO_CONNMGR(object);
    OfonoConnMgrPriv* priv = self->priv;
    GDEBUG("%s: %sattached", priv->name, self->attached ? "" : "not ");

    /* Subscribe for notifications */
    GASSERT(!priv->proxy_handler_id[PROXY_HANDLER_CONTEXT_ADDED]);
    GASSERT(!priv->proxy_handler_id[PROXY_HANDLER_CONTEXT_REMOVED]);
    priv->proxy_handler_id[PROXY_HANDLER_CONTEXT_ADDED] =
        g_signal_connect(proxy, "context-added",
        G_CALLBACK(ofono_connmgr_context_added), self);
    priv->proxy_handler_id[PROXY_HANDLER_CONTEXT_REMOVED] =
        g_signal_connect(proxy, "context-removed",
        G_CALLBACK(ofono_connmgr_context_removed), self);

    OFONO_OBJECT_CLASS(SUPER_CLASS)->fn_proxy_created(object, proxy);

    if (OFONO_OBJECT_GET_CLASS(self)->fn_is_ready(object)) {
        GVERBOSE_("Fetching contexts...");
        ofono_connmgr_start_get_contexts(self, proxy);
    }
}

/*==========================================================================*
 * API
 *==========================================================================*/

OfonoConnMgr*
ofono_connmgr_new(
    const char* path)
{
    const char* ifname = OFONO_CONNMGR_INTERFACE_NAME;
    OfonoModem* modem = ofono_modem_new(path);
    OfonoConnMgr* connmgr;
    OfonoModemInterface* intf = ofono_modem_get_interface(modem, ifname);
    if (G_TYPE_CHECK_INSTANCE_TYPE(intf, OFONO_TYPE_CONNMGR)) {
        /* Reuse the existing object */
        connmgr = ofono_connmgr_ref(OFONO_CONNMGR(intf));
    } else {
        connmgr = g_object_new(OFONO_TYPE_CONNMGR, NULL);
        intf = &connmgr->intf;
        GVERBOSE_("%s", path);
        ofono_modem_interface_initialize(intf, ifname, path);
        ofono_modem_set_interface(modem, intf);
        connmgr->priv->name = ofono_object_name(&intf->object);
        ofono_connmgr_update_ready(connmgr);
        GASSERT(!ofono_connmgr_proxy(connmgr));
        GASSERT(intf->modem == modem);
    }
    ofono_modem_unref(modem);
    return connmgr;
}

OfonoConnMgr*
ofono_connmgr_ref(
    OfonoConnMgr* self)
{
    if (G_LIKELY(self)) {
        g_object_ref(OFONO_CONNMGR(self));
        return self;
    }
    return NULL;
}

void
ofono_connmgr_unref(
    OfonoConnMgr* self)
{
    if (G_LIKELY(self)) {
        g_object_unref(OFONO_CONNMGR(self));
    }
}

GPtrArray*
ofono_connmgr_get_contexts(
    OfonoConnMgr* self)
{
    GPtrArray* contexts = NULL;
    if (G_LIKELY(self)) {
        OfonoConnMgrPriv* priv = self->priv;
        GPtrArray* list = priv->valid_contexts;
        guint i, n = list->len;
        contexts = g_ptr_array_new_full(n, g_object_unref);
        for (i=0; i<n; i++) {
            g_ptr_array_add(contexts, ofono_connctx_ref(list->pdata[i]));
        }
        gutil_idle_pool_add_ptr_array(priv->pool, contexts);
    }
    return contexts;
}

OfonoConnCtx*
ofono_connmgr_get_context_for_type(
    OfonoConnMgr* self,
    OFONO_CONNCTX_TYPE type)
{
    if (G_LIKELY(self) && G_LIKELY(type >= OFONO_CONNCTX_TYPE_NONE)) {
        OfonoConnMgrPriv* priv = self->priv;
        GPtrArray* list = priv->valid_contexts;
        int i, n = list->len;
        for (i=0; i<n; i++) {
            OfonoConnCtx* context = OFONO_CONNCTX(list->pdata[i]);
            if (context->type == type || type == OFONO_CONNCTX_TYPE_NONE) {
                gutil_idle_pool_add_object_ref(priv->pool, context);
                return context;
            }
        }
    }
    return NULL;
}

OfonoConnCtx*
ofono_connmgr_get_context_for_path(
    OfonoConnMgr* self,
    const char* path)
{
    OfonoConnCtx* context = NULL;
    if (G_LIKELY(self)) {
        OfonoConnMgrPriv* priv = self->priv;
        GPtrArray* list = priv->valid_contexts;
        if (path) {
            int i = ofono_connmgr_find_valid_context(self, path);
            if (i >= 0) {
                context = OFONO_CONNCTX(list->pdata[i]);
            }
        } else if (list->len > 0) {
            context = OFONO_CONNCTX(list->pdata[0]);
        }
        if (context) {
            gutil_idle_pool_add_object_ref(priv->pool, context);
        }
    }
    return context;
}

gulong
ofono_connmgr_add_valid_changed_handler(
    OfonoConnMgr* self,
    OfonoConnMgrHandler fn,
    void* arg)
{
    return G_LIKELY(self) ? ofono_object_add_valid_changed_handler(
        &self->intf.object, (OfonoObjectHandler)fn, arg) : 0;
}

gulong
ofono_connmgr_add_property_changed_handler(
    OfonoConnMgr* self,
    OfonoConnMgrPropertyHandler fn,
    const char* name,
    void* arg)
{
    return G_LIKELY(self) ? ofono_object_add_property_changed_handler(
        &self->intf.object, (OfonoObjectPropertyHandler)fn, name, arg) : 0;
}

gulong
ofono_connmgr_add_context_added_handler(
    OfonoConnMgr* self,
    OfonoConnMgrContextAddedHandler fn,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(fn)) ? g_signal_connect(self,
        CONNMGR_SIGNAL_CONTEXT_ADDED_NAME, G_CALLBACK(fn), arg) : 0;
}

gulong
ofono_connmgr_add_context_removed_handler(
    OfonoConnMgr* self,
    OfonoConnMgrContextRemovedHandler fn,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(fn)) ? g_signal_connect(self,
        CONNMGR_SIGNAL_CONTEXT_REMOVED_NAME, G_CALLBACK(fn), arg) : 0;
}

gulong
ofono_connmgr_add_attached_changed_handler(
    OfonoConnMgr* self,
    OfonoConnMgrHandler fn,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(fn)) ? g_signal_connect(self,
        CONNMGR_SIGNAL_ATTACHED_CHANGED_NAME, G_CALLBACK(fn), arg) : 0;
}

gulong
ofono_connmgr_add_roaming_allowed_changed_handler(
    OfonoConnMgr* self,
    OfonoConnMgrHandler fn,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(fn)) ? g_signal_connect(self,
        CONNMGR_SIGNAL_ROAMING_ALLOWED_CHANGED_NAME, G_CALLBACK(fn), arg) : 0;
}

gulong
ofono_connmgr_add_powered_changed_handler(
    OfonoConnMgr* self,
    OfonoConnMgrHandler fn,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(fn)) ? g_signal_connect(self,
        CONNMGR_SIGNAL_POWERED_CHANGED_NAME, G_CALLBACK(fn), arg) : 0;
}

void
ofono_connmgr_remove_handler(
    OfonoConnMgr* self,
    gulong id)
{
    if (G_LIKELY(self) && G_LIKELY(id)) {
        g_signal_handler_disconnect(self, id);
    }
}

/*==========================================================================*
 * Internals
 *==========================================================================*/

static
gboolean
ofono_connmgr_is_valid(
    OfonoObject* object)
{
    OfonoConnMgr* self = OFONO_CONNMGR(object);
    OfonoConnMgrPriv* priv = self->priv;
    return priv->get_contexts_ok && !priv->get_contexts_pending &&
        g_hash_table_size(priv->all_contexts) == priv->valid_contexts->len &&
        OFONO_OBJECT_CLASS(SUPER_CLASS)->fn_is_valid(object);
}

static
void
ofono_connmgr_ready_changed(
    OfonoObject* object,
    gboolean ready)
{
    OfonoConnMgr* self = OFONO_CONNMGR(object);
    OFONO_OBJECT_PROXY* proxy = ofono_object_proxy(object);
    if (proxy && ready) {
        GVERBOSE_("Fetching contexts...");
        ofono_connmgr_start_get_contexts(self, proxy);
    } else {
        ofono_connmgr_cancel_get_contexts(self);
        if (!ready) {
            OfonoConnMgrPriv* priv = self->priv;
            g_hash_table_remove_all(priv->all_contexts);
            g_ptr_array_set_size(priv->valid_contexts, 0);
        }
    }
    OFONO_OBJECT_CLASS(SUPER_CLASS)->fn_ready_changed(object, ready);
}

/**
 * Per instance initializer
 */
static
void
ofono_connmgr_init(
    OfonoConnMgr* self)
{
    OfonoConnMgrPriv* priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
        OFONO_TYPE_CONNMGR, OfonoConnMgrPriv);
    self->priv = priv;
    priv->pool = gutil_idle_pool_ref(ofono_idle_pool());
    priv->valid_contexts = g_ptr_array_new_with_free_func(g_object_unref);
    priv->all_contexts = g_hash_table_new_full(g_str_hash, g_str_equal, NULL,
        ofono_connmgr_context_data_destroy);
}

/**
 * Final stage of deinitialization
 */
static
void
ofono_connmgr_finalize(
    GObject* object)
{
    OfonoConnMgr* self = OFONO_CONNMGR(object);
    OfonoConnMgrPriv* priv = self->priv;
    ofono_connmgr_cancel_get_contexts(self);
    gutil_idle_pool_unref(priv->pool);
    gutil_disconnect_handlers(ofono_connmgr_proxy(self),
        priv->proxy_handler_id, G_N_ELEMENTS(priv->proxy_handler_id));
    g_ptr_array_unref(priv->valid_contexts);
    g_hash_table_destroy(priv->all_contexts);
    G_OBJECT_CLASS(SUPER_CLASS)->finalize(object);
}

/**
 * Per class initializer
 */
static
void
ofono_connmgr_class_init(
    OfonoConnMgrClass* klass)
{
    static OfonoObjectProperty ofono_connmgr_properties[] = {
        CONNMGR_DEFINE_PROPERTY_BOOL(ATTACHED,attached),
        CONNMGR_DEFINE_PROPERTY_BOOL(ROAMING_ALLOWED,roaming_allowed),
        CONNMGR_DEFINE_PROPERTY_BOOL(POWERED,powered)
    };

    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    OfonoObjectClass* ofono = &klass->object;
    object_class->finalize = ofono_connmgr_finalize;
    g_type_class_add_private(klass, sizeof(OfonoConnMgrPriv));
    ofono->fn_is_valid = ofono_connmgr_is_valid;
    ofono->fn_proxy_created = ofono_connmgr_proxy_created;
    ofono->fn_ready_changed = ofono_connmgr_ready_changed;
    ofono->properties = ofono_connmgr_properties;
    ofono->nproperties = G_N_ELEMENTS(ofono_connmgr_properties);
    OFONO_OBJECT_CLASS_SET_PROXY_CALLBACKS(ofono, org_ofono_connection_manager);
    ofono_connmgr_signals[CONNMGR_SIGNAL_CONTEXT_ADDED] =
        g_signal_new(CONNMGR_SIGNAL_CONTEXT_ADDED_NAME,
            G_OBJECT_CLASS_TYPE(klass), G_SIGNAL_RUN_FIRST,
            0, NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
            G_TYPE_NONE, 1, G_TYPE_OBJECT);
    ofono_connmgr_signals[CONNMGR_SIGNAL_CONTEXT_REMOVED] =
        g_signal_new(CONNMGR_SIGNAL_CONTEXT_REMOVED_NAME,
            G_OBJECT_CLASS_TYPE(klass), G_SIGNAL_RUN_FIRST,
            0, NULL, NULL, g_cclosure_marshal_VOID__STRING,
            G_TYPE_NONE, 1, G_TYPE_STRING);
    ofono_class_initialize(ofono);
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
