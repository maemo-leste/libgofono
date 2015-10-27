/*
 * Copyright (C) 2014-2015 Jolla Ltd.
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
#include "gofono_connctx_p.h"
#include "gofono_modem_p.h"
#include "gofono_names.h"
#include "gofono_log.h"

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

struct ofono_connmgr_priv {
    const char* name;
    gulong proxy_handler_id[PROXY_HANDLER_COUNT];
    GCancellable* cancel_setup;
    GPtrArray* contexts;
    guint flags;

#define CONNMGR_FLAG_SETUP_DONE         (0x01)
#define CONNMGR_FLAG_GET_CONTEXTS_DONE  (0x02)
};

typedef OfonoModemInterfaceClass OfonoConnMgrClass;
G_DEFINE_TYPE(OfonoConnMgr, ofono_connmgr, OFONO_TYPE_MODEM_INTERFACE)

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

#define CONNMGR_OBJECT_SIGNAL_NEW(klass,name) \
    ofono_connmgr_signals[CONNMGR_SIGNAL_##name] = \
        g_signal_new(CONNMGR_SIGNAL_##name##_NAME, \
            G_OBJECT_CLASS_TYPE(klass), G_SIGNAL_RUN_FIRST, \
            0, NULL, NULL, g_cclosure_marshal_VOID__OBJECT, \
            G_TYPE_NONE, 1, G_TYPE_OBJECT)
#define CONNMGR_OBJECT_SIGNAL_EMIT(self,name,object) \
    g_signal_emit(self, ofono_connmgr_signals[\
    CONNMGR_SIGNAL_##name], 0, object)

#define CONNMGR_DEFINE_PROPERTY_BOOL(NAME,var) \
    OFONO_OBJECT_DEFINE_PROPERTY_BOOL(CONNMGR,NAME,OfonoConnMgr,var)

/*==========================================================================*
 * Implementation
 *==========================================================================*/

static
void
ofono_connmgr_context_free(
    gpointer data)
{
    ofono_connctx_unref(data);
}

static
gint
ofono_connmgr_context_compare(
    gconstpointer a,
    gconstpointer b)
{
    OfonoConnCtx** ctx1 = a;
    OfonoConnCtx** ctx2 = b;
    return g_strcmp0(ofono_connctx_path(*ctx1), ofono_connctx_path(*ctx2));
}

static
int
ofono_connmgr_find_context(
    OfonoConnMgr* self,
    const char* path)
{
    if (path) {
        GPtrArray* list = self->priv->contexts;
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
ofono_connmgr_add_context(
    OfonoConnMgr* self,
    const char* path,
    GVariant* props)
{
    OfonoConnMgrPriv* priv = self->priv;
    if (path && props && ofono_connmgr_find_context(self, path) < 0) {
        OfonoConnCtx* ctx = ofono_connctx_new_internal(path, props);
        g_ptr_array_add(priv->contexts, ctx);
        g_ptr_array_sort(priv->contexts, ofono_connmgr_context_compare);
        CONNMGR_OBJECT_SIGNAL_EMIT(self, CONTEXT_ADDED, ctx);
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
    ofono_connmgr_add_context(self, path, properties);
}

static
void
ofono_connmgr_context_removed(
    OrgOfonoConnectionManager* proxy,
    const char* path,
    gpointer data)
{
    OfonoConnMgr* self = OFONO_CONNMGR(data);
    int i = ofono_connmgr_find_context(self, path);
    GVERBOSE_("%s", path);
    if (i >= 0) {
        OfonoConnMgrPriv* priv = self->priv;
        OfonoConnCtx* ctx = ofono_connctx_ref(priv->contexts->pdata[i]);
        g_ptr_array_remove_index(priv->contexts, i);
        CONNMGR_OBJECT_SIGNAL_EMIT(self, CONTEXT_REMOVED, ctx);
        ofono_connctx_unref(ctx);
    }
}

static
void
ofono_connmgr_get_contexts_done(
    GObject* proxy,
    GAsyncResult* result,
    gpointer data)
{
    OfonoConnMgr* self = OFONO_CONNMGR(data);
    GVariant* contexts = NULL;
    GError* error = NULL;
    gboolean ok = org_ofono_connection_manager_call_get_contexts_finish(
        ORG_OFONO_CONNECTION_MANAGER(proxy), &contexts, result, &error);
    if (ok) {
        GVariantIter iter;
        GVariant* child;
        GVERBOSE("  %d context(s)", (guint)g_variant_n_children(contexts));
        for (g_variant_iter_init(&iter, contexts);
             (child = g_variant_iter_next_value(&iter)) != NULL;
             g_variant_unref(child)) {

            const char* path = NULL;
            GVariant* props = NULL;
            g_variant_get(child, "(&o@a{sv})", &path, &props);
            ofono_connmgr_add_context(self, path, props);
            if (props) g_variant_unref(props);
        }

        g_variant_unref(contexts);
    } else {
        GERR("%s", GERRMSG(error));
        g_error_free(error);
    }

    /* The object can be marked valid on success */
    if (ok) {
        ofono_object_set_initialized(&self->intf.object);
    } else {
        ofono_object_set_invalid(&self->intf.object, TRUE);
    }

    /* Release the reference we have been holding during the call */
    ofono_connmgr_unref(self);
}

static
gboolean
ofono_connmgr_setup(
    OfonoObject* object)
{
    OfonoConnMgr* self = OFONO_CONNMGR(object);
    OfonoConnMgrPriv* priv = self->priv;
    OFONO_OBJECT_PROXY* proxy = ofono_object_proxy(object);
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

    /* We have to query the list of contexts before this object can be
     * marked as valid */
    org_ofono_connection_manager_call_get_contexts(proxy, NULL,
        ofono_connmgr_get_contexts_done, ofono_connmgr_ref(self));
    return FALSE;
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
        GPtrArray* list = self->priv->contexts;
        guint i, n = list->len;
        contexts = g_ptr_array_new_full(n, ofono_connmgr_context_free);
        for (i=0; i<n; i++) {
            g_ptr_array_add(contexts, ofono_connctx_ref(list->pdata[i]));
        }
    }
    return contexts;
}

OfonoConnCtx*
ofono_connmgr_get_context_for_type(
    OfonoConnMgr* self,
    OFONO_CONNCTX_TYPE type)
{
    if (G_LIKELY(self) && G_LIKELY(type >= OFONO_CONNCTX_TYPE_NONE)) {
        GPtrArray* list = self->priv->contexts;
        int i, n = list->len;
        for (i=0; i<n; i++) {
            OfonoConnCtx* context = OFONO_CONNCTX(list->pdata[i]);
            if (context->type == type || type == OFONO_CONNCTX_TYPE_NONE) {
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
    if (G_LIKELY(self)) {
        GPtrArray* list = self->priv->contexts;
        if (path) {
            int i = ofono_connmgr_find_context(self, path);
            if (i >= 0) {
                return OFONO_CONNCTX(list->pdata[i]);
            }
        } else if (list->len > 0) {
            return OFONO_CONNCTX(list->pdata[0]);
        }
    }
    return NULL;
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
    OfonoConnMgrContextHandler fn,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(fn)) ? g_signal_connect(self,
        CONNMGR_SIGNAL_CONTEXT_ADDED_NAME, G_CALLBACK(fn), arg) : 0;
}

gulong
ofono_connmgr_add_context_removed_handler(
    OfonoConnMgr* self,
    OfonoConnMgrContextHandler fn,
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

/**
 * Reset the object after it has become invalid.
 */
static
void
ofono_connmgr_invalidate(
    OfonoObject* object)
{
    OFONO_OBJECT_PROXY* proxy = ofono_object_proxy(object);
    if (proxy) {
        OfonoConnMgr* self = OFONO_CONNMGR(object);
        OfonoConnMgrPriv* priv = self->priv;
        unsigned int i;
        for (i=0; i<G_N_ELEMENTS(priv->proxy_handler_id); i++) {
            if (priv->proxy_handler_id[i]) {
                g_signal_handler_disconnect(proxy, priv->proxy_handler_id[i]);
                priv->proxy_handler_id[i] = 0;
            }
        }
    }
    OFONO_OBJECT_CLASS(ofono_connmgr_parent_class)->fn_invalidate(object);
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
    priv->contexts = g_ptr_array_new_with_free_func(ofono_connmgr_context_free);
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
    g_ptr_array_free(self->priv->contexts, TRUE);
    G_OBJECT_CLASS(ofono_connmgr_parent_class)->finalize(object);
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
    ofono->fn_invalidate = ofono_connmgr_invalidate;
    ofono->fn_initialized = ofono_connmgr_setup;
    ofono->properties = ofono_connmgr_properties;
    ofono->nproperties = G_N_ELEMENTS(ofono_connmgr_properties);
    OFONO_OBJECT_CLASS_SET_PROXY_CALLBACKS(ofono, org_ofono_connection_manager);
    CONNMGR_OBJECT_SIGNAL_NEW(klass,CONTEXT_ADDED);
    CONNMGR_OBJECT_SIGNAL_NEW(klass,CONTEXT_REMOVED);
    ofono_class_initialize(ofono);
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
