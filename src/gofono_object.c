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

#include "gofono_object_p.h"
#include "gofono_error_p.h"
#include "gofono_util_p.h"
#include "gofono_names.h"
#include "gofono_log.h"

#include <gutil_misc.h>

#define OFONO_BUSY_RETRY_DELAY (200) /* ms */

typedef struct ofono_object_get_properties_call {
    GDBusProxy* proxy;
    GCancellable* cancel;
    OfonoObject* object;
    gboolean (*fn_finish)(
        GDBusProxy* proxy,
        GVariant** props,
        GAsyncResult* res,
        GError** error);
} OfonoObjectGetPropertiesCall;

/* Object definition */
struct ofono_object_priv {
    char* intf;
    char* path;
    GDBusConnection* bus;
    GDBusProxy* proxy;
    gboolean ready;
    gboolean get_properties_ok;
    OfonoObjectGetPropertiesCall* get_properties_pending;
    guint get_properties_retry_id;
    gulong property_changed_signal_id;
    GUtilIdlePool* pool;
    GHashTable* properties;
    GList* pending_calls;
};

G_DEFINE_TYPE(OfonoObject, ofono_object, G_TYPE_OBJECT)
static OfonoObjectClass* ofono_object_class = NULL;

enum ofono_object_signal {
    OFONO_OBJECT_SIGNAL_VALID_CHANGED,
    OFONO_OBJECT_SIGNAL_PROPERTY_CHANGED,
    OFONO_OBJECT_SIGNAL_COUNT
};

#define OFONO_OBJECT_SIGNAL_VALID_CHANGED_NAME     "valid-changed"
#define OFONO_OBJECT_SIGNAL_PROPERTY_CHANGED_NAME  "property-changed"

static guint ofono_object_signals[OFONO_OBJECT_SIGNAL_COUNT] = { 0 };

typedef struct ofono_object_pending_call_priv {
    OfonoObjectPendingCall call;
    OfonoObjectProxyCallFinishedCallback finished;
} OfonoObjectPendingCallPriv;

OFONO_INLINE OfonoObjectPendingCallPriv*
ofono_object_pending_call_cast(OfonoObjectPendingCall* call)
    { return G_CAST(call, OfonoObjectPendingCallPriv, call); }

#ifdef DEBUG
OFONO_INLINE OfonoObject* ofono_object_check(OfonoObject* obj) { return obj; }
#else
#  define ofono_object_check(obj) obj
#endif

/* Forward declarations */
static
gboolean
ofono_object_get_properties_retry(
    gpointer user_data);

static
void
ofono_object_property_changed(
    OFONO_OBJECT_PROXY* proxy,
    const char* name,
    GVariant* variant,
    gpointer data);

/*==========================================================================*
 * Default client proxy.
 *
 * The default proxy implements GetProperties and SetProperty functionality
 * that most Ofono objects support. This makes the raw (not subclassed)
 * OfonoObject usable for the purpose of getting, settings and tracking
 * its properties.
 *==========================================================================*/

typedef struct ofono_object_proxy {
    GDBusProxy proxy;
} OfonoObjectProxy;

typedef struct ofono_object_proxy_class {
  GDBusProxyClass proxy;
} OfonoObjectProxyClass;

#define OFONO_TYPE_PROXY (ofono_object_proxy_get_type())
G_DEFINE_TYPE(OfonoObjectProxy, ofono_object_proxy, G_TYPE_DBUS_PROXY)

enum ofono_object_proxy_signal_ids {
    PROXY_SIGNAL_PROPERTY_CHANGED,
    PROXY_SIGNAL_COUNT
};

#define PROXY_SIGNAL_PROPERTY_CHANGED_NAME "property-changed"

static guint ofono_object_proxy_signals[PROXY_SIGNAL_COUNT] = { 0 };

static
void
ofono_object_proxy_new(
    GDBusConnection* bus,
    GDBusProxyFlags flags,
    const gchar* name,
    const gchar* path,
    GCancellable* cancellable,
    GAsyncReadyCallback callback,
    gpointer data)
{
    OfonoObject* self = OFONO_OBJECT(data);
    g_async_initable_new_async(OFONO_TYPE_PROXY, G_PRIORITY_DEFAULT,
       cancellable, callback, data, "g-flags", flags, "g-name", name,
       "g-connection", bus, "g-object-path", path,
       "g-interface-name", self->priv->intf, NULL);
}

static
GDBusProxy*
ofono_object_default_proxy_new_finish(
    GAsyncResult* res,
    GError** error)
{
  GObject* src = g_async_result_get_source_object(res);
  GObject* ret = g_async_initable_new_finish(G_ASYNC_INITABLE(src), res, error);
  g_object_unref(src);
  return ret ? G_DBUS_PROXY(ret) : NULL;
}

static
void
ofono_object_default_proxy_call_get_properties(
    GDBusProxy* proxy,
    GCancellable* cancellable,
    GAsyncReadyCallback callback,
    gpointer data)
{
    g_dbus_proxy_call(proxy, "GetProperties", g_variant_new("()"),
        G_DBUS_CALL_FLAGS_NONE, -1, cancellable, callback, data);
}

static
void
ofono_object_default_proxy_call_set_property(
    GDBusProxy* proxy,
    const gchar* name,
    GVariant* value,
    GCancellable* cancellable,
    GAsyncReadyCallback callback,
    gpointer data)
{
    g_dbus_proxy_call(proxy, "SetProperty", g_variant_new("(s@v)", name, value),
        G_DBUS_CALL_FLAGS_NONE, -1, cancellable, callback, data);
}

static
gboolean
ofono_object_default_proxy_call_get_properties_finish(
    GDBusProxy* proxy,
    GVariant** properties,
    GAsyncResult* res,
    GError** error)
{
    GVariant* ret = g_dbus_proxy_call_finish(proxy, res, error);
    if (ret) {
        g_variant_get(ret, "(@a{sv})", properties);
        g_variant_unref(ret);
        return TRUE;
    }
    return FALSE;
}

static
gboolean
ofono_object_default_proxy_call_set_property_finish(
    GDBusProxy* proxy,
    GAsyncResult* res,
    GError** error)
{
    GVariant* ret = g_dbus_proxy_call_finish(proxy, res, error);
    if (ret) {
        g_variant_get(ret, "()");
        g_variant_unref(ret);
        return TRUE;
    }
    return FALSE;
}

static
void
ofono_object_proxy_signal(
  GDBusProxy* proxy,
  const gchar* sender G_GNUC_UNUSED,
  const gchar* signal,
  GVariant* parameters)
{
    if (signal && !strcmp(signal, "PropertyChanged")) {
        const guint num = g_variant_n_children(parameters);
        if (G_LIKELY(num == 2)) {
            const char* name = NULL;
            GVariant* value = NULL;
            g_variant_get(parameters, "(&s@v)", &name, &value);
            if (G_LIKELY(name)) {
                if (G_LIKELY(value)) {
                    g_signal_emit(proxy, ofono_object_proxy_signals[
                        PROXY_SIGNAL_PROPERTY_CHANGED], 0, name, value);
                    g_variant_unref(value);
                }
            }
        } else {
            GWARN_("Unexpected number of parameters for %s (%u)", signal, num);
        }
    }
}

static
void
ofono_object_proxy_init(
    OfonoObjectProxy* self)
{
}

static
void
ofono_object_proxy_class_init(
    OfonoObjectProxyClass* klass)
{
    klass->proxy.g_signal = ofono_object_proxy_signal;
    ofono_object_proxy_signals[PROXY_SIGNAL_PROPERTY_CHANGED] =
        g_signal_new(PROXY_SIGNAL_PROPERTY_CHANGED_NAME,
            G_OBJECT_CLASS_TYPE(klass), G_SIGNAL_RUN_FIRST,
            0, NULL, NULL, NULL, G_TYPE_NONE,
            2, G_TYPE_STRING, G_TYPE_VARIANT);
}

/*==========================================================================*
 * Initialization
 *==========================================================================*/

static
void
ofono_object_setup_finished(
    GObject* proxy,
    GAsyncResult* result,
    gpointer data)
{
    OfonoObjectGetPropertiesCall* call = data;
    OfonoObject* object = call->object;
    GError* error = NULL;
    GVariant* props = NULL;
    int retry_ms = -1;
    gboolean ok = call->fn_finish(G_DBUS_PROXY(proxy), &props, result, &error);

    if (ok) {
        /* Success */
        if (object) {
            ofono_object_apply_properties(object, props);
        }
        g_variant_unref(props);
    } else if (object) {
        if (ofono_error_is_generic_timeout(error)) {
            /* Retry immediately */
            GWARN("%s.GetProperties %s", object->priv->intf, GERRMSG(error));
            retry_ms = 0;
        } else if (ofono_error_is_busy(error)) {
            /* Retry after delay */
            GWARN("%s.GetProperties %s", object->priv->intf, GERRMSG(error));
            retry_ms = OFONO_BUSY_RETRY_DELAY;
        } else if (error->code != G_IO_ERROR_CANCELLED) {
            /* Something unrecoverable */
            GERR("%s.GetProperties %s", object->priv->intf, GERRMSG(error));
        }
    }

    if (retry_ms >= 0) {
        OfonoObjectPriv* priv = object->priv;
        GASSERT(!priv->get_properties_retry_id);
        priv->get_properties_retry_id = g_timeout_add(retry_ms,
            ofono_object_get_properties_retry, object);
    } else {
        if (call->object) {
            OfonoObjectPriv* priv = call->object->priv;
            priv->get_properties_ok = ok;
            priv->get_properties_pending = NULL;
            ofono_object_update_valid(call->object);
        }
        g_object_unref(call->proxy);
        g_object_unref(call->cancel);
        g_slice_free(OfonoObjectGetPropertiesCall, call);
   }
    if (error) g_error_free(error);
}

static
void
ofono_object_cancel_get_properties(
    OfonoObject* self)
{
    OfonoObjectPriv* priv = self->priv;
    if (priv->get_properties_pending) {
        g_cancellable_cancel(priv->get_properties_pending->cancel);
        priv->get_properties_pending->object = NULL;
        priv->get_properties_pending = NULL;
    }
    if (priv->get_properties_retry_id) {
        g_source_remove(priv->get_properties_retry_id);
        priv->get_properties_retry_id = 0;
    }
}

static
void
ofono_object_start_get_properties(
    OfonoObject* self)
{
    OfonoObjectPriv* priv = self->priv;
    OfonoObjectClass* klass = OFONO_OBJECT_GET_CLASS(self);
    GASSERT(priv->proxy);
    if (klass->fn_proxy_call_get_properties) {
        OfonoObjectGetPropertiesCall* call;

        call = g_slice_new0(OfonoObjectGetPropertiesCall);
        call->cancel = g_cancellable_new();
        call->object = self;
        g_object_ref(call->proxy = priv->proxy);
        call->fn_finish = klass->fn_proxy_call_get_properties_finish;
        GASSERT(call->fn_finish);

        GASSERT(priv->property_changed_signal_id);
        ofono_object_cancel_get_properties(self);
        priv->get_properties_ok = FALSE;
        priv->get_properties_pending = call;
        klass->fn_proxy_call_get_properties(priv->proxy, call->cancel,
            ofono_object_setup_finished, call);
    } else {
        /* No properties to query */
        priv->get_properties_ok = TRUE;
    }
}

static
gboolean
ofono_object_get_properties_retry(
    gpointer data)
{
    OfonoObject* self = OFONO_OBJECT(data);
    OfonoObjectPriv* priv = self->priv;
    priv->get_properties_retry_id = 0;
    GDEBUG("Retrying %s.GetProperties", priv->intf);
    GASSERT(priv->get_properties_pending);
    OFONO_OBJECT_GET_CLASS(self)->fn_proxy_call_get_properties(priv->proxy,
        priv->get_properties_pending->cancel, ofono_object_setup_finished,
        priv->get_properties_pending);
    return G_SOURCE_REMOVE;
}

static
void
ofono_object_proxy_created(
    OfonoObject* self,
    GDBusProxy* proxy)
{
    OfonoObjectPriv* priv = self->priv;
    GASSERT(!priv->proxy);
    g_object_ref(priv->proxy = proxy);
    GASSERT(!priv->property_changed_signal_id);
    priv->property_changed_signal_id = g_signal_connect(proxy,
        PROXY_SIGNAL_PROPERTY_CHANGED_NAME,
        G_CALLBACK(ofono_object_property_changed), self);
    ofono_object_update_ready(self);
    ofono_object_update_valid(self);
}

static
void
ofono_object_create_proxy_finished(
    GObject* source,
    GAsyncResult* result,
    gpointer data)
{
    GError* error = NULL;
    OfonoObject* self = OFONO_OBJECT(data);
    OfonoObjectClass* klass = OFONO_OBJECT_GET_CLASS(self);
    OFONO_OBJECT_PROXY* proxy;

    /* Retrieve the result */
    GASSERT(klass->fn_proxy_new_finish);
    proxy = klass->fn_proxy_new_finish(result, &error);
    if (proxy) {
        klass->fn_proxy_created(self, proxy);
    } else {
        GERR("%s", GERRMSG(error));
    }

    if (error) g_error_free(error);
    ofono_object_unref(self);
}

/*==========================================================================*
 * Implementation
 *==========================================================================*/

OfonoObjectPendingCall*
ofono_object_pending_call_new(
    OfonoObject* self,
    OfonoObjectProxyCallFinishedCallback finished,
    OfonoObjectCallFinishedCallback callback,
    void* arg)
{
    OfonoObjectPendingCallPriv* call = g_new(OfonoObjectPendingCallPriv, 1);
    OfonoObjectPriv* priv = self->priv;
    g_object_ref(call->call.object = self);
    call->call.cancellable = g_cancellable_new();
    call->call.callback = callback;
    call->call.arg = arg;
    call->finished = finished;
    priv->pending_calls = g_list_prepend(priv->pending_calls, call);
    return &call->call;
}

static
void
ofono_object_pending_call_free(
    OfonoObjectPendingCallPriv* call)
{
    OfonoObjectPriv* priv = call->call.object->priv;
    priv->pending_calls = g_list_remove(priv->pending_calls, call);
    ofono_object_unref(call->call.object);
    g_object_unref(call->call.cancellable);
    g_free(call);
}

void
ofono_object_pending_call_finished(
    GObject* proxy,
    GAsyncResult* res,
    gpointer data)
{
    OfonoObjectPendingCallPriv* call = data;
    GASSERT(G_DBUS_PROXY(proxy) == call->call.object->priv->proxy);
    call->finished(G_DBUS_PROXY(proxy), res, &call->call);
    ofono_object_pending_call_free(call);
}

static
void
ofono_object_apply_property_r(
    OfonoObject* self,
    OfonoObjectClass* klass,
    const char* name,
    GVariant* value)
{
    guint i;
    const OfonoObjectProperty* property;
    for (i=0, property=klass->properties;
         i<klass->nproperties;
         i++, property++) {
        if (!strcmp(property->name, name)) {
            property->fn_apply(self, property, value);
            return;
        }
    }
    if (klass != ofono_object_class) {
        ofono_object_apply_property_r(self, OFONO_OBJECT_CLASS(
            g_type_class_peek_parent(klass)), name, value);
    }
}

static
void
ofono_object_apply_property(
    OfonoObject* self,
    const char* name,
    GVariant* v)
{
    ofono_object_apply_property_r(self, OFONO_OBJECT_GET_CLASS(self), name, v);
}

static
void
ofono_object_reset_properties_r(
    OfonoObject* self,
    OfonoObjectClass* klass)
{
    guint i;
    const OfonoObjectProperty* property;
    for (i=0, property=klass->properties;
         i<klass->nproperties;
         i++, property++) {
        if (property->fn_reset) {
            property->fn_reset(self, property);
        }
    }
    if (klass != ofono_object_class) {
        ofono_object_reset_properties_r(self, OFONO_OBJECT_CLASS(
            g_type_class_peek_parent(klass)));
    }
}

static
void
ofono_object_reset_properties(
    OfonoObject* self)
{
    ofono_object_reset_properties_r(self, OFONO_OBJECT_GET_CLASS(self));
}

static
void
ofono_object_property_changed(
    OFONO_OBJECT_PROXY* proxy,
    const char* name,
    GVariant* variant,
    gpointer data)
{
    OfonoObject* self = OFONO_OBJECT(data);
    OfonoObjectPriv* priv = self->priv;
    GQuark detail = g_quark_from_string(name);

    /* Hash table keeps the value reference */
    GVariant* value = (g_variant_is_of_type(variant, G_VARIANT_TYPE_VARIANT)) ?
        g_variant_get_variant(variant) : g_variant_ref(variant);
    g_hash_table_replace(priv->properties, g_strdup(name), value);

#if GUTIL_LOG_VERBOSE
    if (GLOG_ENABLED(GLOG_LEVEL_VERBOSE)) {
        gchar* text = g_variant_print(value, FALSE);
        GVERBOSE_("%s %s %s: %s", priv->path, self->intf, name, text);
        g_free(text);
    }
#endif /* GUTIL_LOG_VERBOSE */

    g_variant_ref(value);
    ofono_object_apply_property(self, name, value);
    g_signal_emit(self, ofono_object_signals[
        OFONO_OBJECT_SIGNAL_PROPERTY_CHANGED], detail, name, value);
    g_variant_unref(value);
}

static
void
ofono_object_set_property_finished(
    GDBusProxy* proxy,
    GAsyncResult* result,
    const OfonoObjectPendingCall* call)
{
    GError* error = NULL;
    OfonoObject* self = call->object;
    OfonoObjectClass* klass = OFONO_OBJECT_GET_CLASS(self);

    /* Retrieve the result */
    GASSERT(klass->fn_proxy_call_set_property_finish);
    if (!klass->fn_proxy_call_set_property_finish(proxy, result, &error)) {
        GERR("%s", GERRMSG(error));
    }

    /* Notify the derived class if necessary */
    if (call->callback) call->callback(self, error, call->arg);

    /* Cleanup */
    if (error) g_error_free(error);
}

/*==========================================================================*
 * API
 *==========================================================================*/

OfonoObject*
ofono_object_new(
    const char* intf,
    const char* path)
{
    OfonoObject* self = g_object_new(OFONO_TYPE_OBJECT, NULL);
    ofono_object_initialize(self, intf, path);
    return self;
}

OfonoObject*
ofono_object_ref(
    OfonoObject* self)
{
    if (G_LIKELY(self)) {
        g_object_ref(OFONO_OBJECT(self));
        return self;
    } else {
        return NULL;
    }
}

void
ofono_object_unref(
    OfonoObject* self)
{
    if (G_LIKELY(self)) {
        g_object_unref(OFONO_OBJECT(self));
    }
}

void
ofono_object_initialize(
    OfonoObject* self,
    const char* intf,
    const char* path)
{
    OfonoObjectPriv* priv = self->priv;
    GASSERT(!priv->path);
    self->intf = priv->intf = g_strdup(intf);
    self->path = priv->path = g_strdup(path);
    if (priv->bus) {
        OFONO_OBJECT_GET_CLASS(self)->fn_proxy_new(priv->bus,
            G_DBUS_PROXY_FLAGS_NONE, OFONO_SERVICE, self->path,
            NULL, ofono_object_create_proxy_finished,
            ofono_object_ref(self));
    }
}

void
ofono_object_update_ready(
    OfonoObject* self)
{
    OfonoObjectPriv* priv = self->priv;
    OfonoObjectClass* klass = OFONO_OBJECT_GET_CLASS(self);
    const gboolean ready = klass->fn_is_ready(self);
    if (priv->ready != ready) {
        priv->ready = ready;
        klass->fn_ready_changed(self, ready);
    }
}

void
ofono_object_update_valid(
    OfonoObject* self)
{
    OfonoObjectClass* klass = OFONO_OBJECT_GET_CLASS(self);
    const gboolean valid = klass->fn_is_valid(self);
    if (self->valid != valid) {
        self->valid = valid;
        klass->fn_valid_changed(self);
    }
}

void
ofono_object_apply_properties(
    OfonoObject* self,
    GVariant* dictionary)
{
    OfonoObjectPriv* priv = self->priv;
    if (G_LIKELY(dictionary)) {
        GASSERT(g_variant_is_of_type(dictionary, G_VARIANT_TYPE("a{s*}")) ||
                g_variant_is_of_type(dictionary, G_VARIANT_TYPE ("a{o*}")));
        GVariantIter it;
        GVariant* entry;
        for (g_variant_iter_init(&it, dictionary);
             (entry = g_variant_iter_next_value(&it)) != NULL;
             g_variant_unref(entry)) {
            GVariant* key = g_variant_get_child_value(entry, 0);
            GVariant* value = g_variant_get_child_value(entry, 1);
            const char* name = g_variant_get_string(key, NULL);
            if (g_variant_is_of_type(value, G_VARIANT_TYPE_VARIANT)) {
                GVariant* tmp = g_variant_get_variant(value);
                g_variant_unref(value);
                value = tmp;
            }

            g_hash_table_replace(priv->properties, g_strdup(name), value);
            ofono_object_apply_property(self, name, value);

            /* Hash table keeps the value reference */
            g_variant_unref(key);
        }
    } else {
        ofono_object_reset_properties(self);
    }
}

GVariant*
ofono_object_get_properties(
    OfonoObject* self)
{
    OfonoObjectPriv* priv = self->priv;
    GHashTableIter it;
    gpointer key, value;
    GVariantBuilder builder;
    GVariant* properties;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
    g_hash_table_iter_init(&it, self->priv->properties);
    while (g_hash_table_iter_next(&it, &key, &value)) {
        g_variant_builder_add(&builder, "{sv}", key, value);
    }
    properties = g_variant_take_ref(g_variant_builder_end(&builder));
    gutil_idle_pool_add_variant(priv->pool, properties);
    return properties;
}

GVariant*
ofono_object_get_property(
    OfonoObject* self,
    const char* name,
    const GVariantType* type)
{
    OfonoObjectPriv* priv = self->priv;
    GVariant* value = g_hash_table_lookup(priv->properties, name);
    if (value && (!type || g_variant_is_of_type(value, type))) {
        gutil_idle_pool_add_variant_ref(priv->pool, value);
        return value;
    } else {
        return NULL;
    }
}

const char*
ofono_object_get_string(
    OfonoObject* self,
    const char* name)
{
    GVariant* v = ofono_object_get_property(self, name, G_VARIANT_TYPE_STRING);
    return v ? g_variant_get_string(v, NULL) : NULL;
}

gboolean
ofono_object_get_boolean(
    OfonoObject* self,
    const char* name,
    gboolean default_value)
{
    GVariant* v = ofono_object_get_property(self, name, G_VARIANT_TYPE_BOOLEAN);
    return v ? g_variant_get_boolean(v) : default_value;
}

static
void
ofono_object_get_property_keys_callback(
    gpointer key,
    gpointer value,
    gpointer data)
{
    g_ptr_array_add(data, strdup(key));
}

GPtrArray*
ofono_object_get_property_keys(
    OfonoObject* self)
{
    OfonoObjectPriv* priv = self->priv;
    GPtrArray* keys = g_ptr_array_new_with_free_func(g_free);
    g_hash_table_foreach(priv->properties,
        ofono_object_get_property_keys_callback, keys);
    gutil_idle_pool_add_ptr_array(priv->pool, keys);
    return keys;
}

GCancellable*
ofono_object_set_property(
    OfonoObject* self,
    const char* name,
    GVariant* value,
    OfonoObjectCallFinishedCallback callback,
    void* arg)
{
    GCancellable* cancellable = NULL;
    g_variant_ref_sink(value);
    if (G_LIKELY(self) && G_LIKELY(name) && G_LIKELY(value)) {
        OfonoObjectClass* klass = OFONO_OBJECT_GET_CLASS(self);
        if (G_LIKELY(klass->fn_proxy_call_set_property)) {
            OfonoObjectPriv* priv = self->priv;
            GASSERT(priv->proxy);
            if (G_LIKELY(priv->proxy)) {
                OfonoObjectPendingCall* pc = ofono_object_pending_call_new(self,
                    ofono_object_set_property_finished, callback, arg);
                klass->fn_proxy_call_set_property(priv->proxy, name, value,
                    pc->cancellable, ofono_object_pending_call_finished, pc);
                cancellable = pc->cancellable;
            }
        }
    }
    g_variant_unref(value);
    return cancellable;
}

GCancellable*
ofono_object_set_string(
    OfonoObject* self,
    const char* name,
    const char* value,
    OfonoObjectCallFinishedCallback callback,
    void* arg)
{
    if (G_LIKELY(self) && G_LIKELY(name) && G_LIKELY(value)) {
        return ofono_object_set_property(self, name,
            g_variant_new_variant(g_variant_new_string(value)),
            callback, arg);
    }
    return NULL;
}

GCancellable*
ofono_object_set_boolean(
    OfonoObject* self,
    const char* name,
    gboolean value,
    OfonoObjectCallFinishedCallback callback,
    void* arg)
{
    if (G_LIKELY(self) && G_LIKELY(name)) {
        return ofono_object_set_property(self, name,
            g_variant_new_variant(g_variant_new_boolean(value)),
            callback, arg);
    }
    return NULL;
}

gulong
ofono_object_add_valid_changed_handler(
    OfonoObject* self,
    OfonoObjectHandler handler,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(handler)) ? g_signal_connect(self,
        OFONO_OBJECT_SIGNAL_VALID_CHANGED_NAME, G_CALLBACK(handler), arg) : 0;
}

gulong
ofono_object_add_property_changed_handler(
    OfonoObject* self,
    OfonoObjectPropertyHandler fn,
    const char* name,
    void* arg)
{
    gulong id = 0;
    if (G_LIKELY(self) && G_LIKELY(fn)) {
        char* tmp;
        const char* signal_name;
        if (name) {
            tmp = g_strconcat(OFONO_OBJECT_SIGNAL_PROPERTY_CHANGED_NAME, "::",
                name, NULL);
            signal_name = tmp;
        } else {
            tmp = NULL;
            signal_name = OFONO_OBJECT_SIGNAL_PROPERTY_CHANGED_NAME;
        }
        id = g_signal_connect(self, signal_name, G_CALLBACK(fn), arg);
        g_free(tmp);
    }
    return id;
}

void
ofono_object_remove_handler(
    OfonoObject* self,
    gulong id)
{
    if (G_LIKELY(self) && G_LIKELY(id)) {
        g_signal_handler_disconnect(self, id);
    }
}

void
ofono_object_remove_handlers(
    OfonoObject* self,
    gulong* ids,
    unsigned int count)
{
    gutil_disconnect_handlers(self, ids, count);
}

GDBusConnection*
ofono_object_bus(
    OfonoObject* self)
{
    return (G_LIKELY(self) && G_LIKELY(self->priv)) ? self->priv->bus : NULL;
}

const char*
ofono_object_name(
    OfonoObject* self)
{
    const char* name = self->path + strlen(self->path);
    while (name > self->path && name[-1] != '/') name--;
    return name;
}

OFONO_OBJECT_PROXY*
ofono_object_proxy(
    OfonoObject* self)
{
    return (G_LIKELY(self) && G_LIKELY(self->priv)) ? self->priv->proxy : NULL;
}

void
ofono_class_initialize(
    OfonoObjectClass* klass)
{
    guint i;
    OfonoObjectProperty* property;
    for (i=0, property=klass->properties;
         i<klass->nproperties;
         i++, property++) {
        if (property->signal_name) {
            GASSERT(!property->signal);
            property->signal = g_signal_new(property->signal_name,
                G_OBJECT_CLASS_TYPE(klass), G_SIGNAL_RUN_FIRST,
                0, NULL, NULL, NULL, G_TYPE_NONE, 0);
        }
    }
}

static
gboolean
ofono_object_wait_valid_check(
    GObject* object)
{
    return OFONO_OBJECT(object)->valid;
}

static
gulong
ofono_object_wait_valid_add_handler(
    GObject* object,
    OfonoConditionHandler handler,
    void* arg)
{
    return ofono_object_add_valid_changed_handler(OFONO_OBJECT(object),
        (OfonoObjectHandler)handler, arg);
}

static
void
ofono_object_wait_valid_remove_handler(
    GObject* object,
    gulong id)
{
    ofono_object_remove_handler(OFONO_OBJECT(object), id);
}

gboolean
ofono_object_wait_valid(
    OfonoObject* self,
    int timeout_msec,
    GError** error)
{
    return ofono_condition_wait(&self->object,
        ofono_object_wait_valid_check,
        ofono_object_wait_valid_add_handler,
        ofono_object_wait_valid_remove_handler,
        timeout_msec, error);
}

/*==========================================================================*
 * Properties
 *==========================================================================*/

#define OFONO_OBJECT_INT(obj,prop) \
    G_STRUCT_MEMBER(int, ofono_object_check(obj), (prop)->off_pub)
#define OFONO_OBJECT_UINT(obj,prop) \
    G_STRUCT_MEMBER(guint, ofono_object_check(obj), (prop)->off_pub)
#define OFONO_OBJECT_BOOL(obj,prop) \
    G_STRUCT_MEMBER(gboolean, ofono_object_check(obj), (prop)->off_pub)
#define OFONO_OBJECT_PTR_ARRAY(obj,prop) \
    G_STRUCT_MEMBER(GPtrArray*, ofono_object_check(obj), (prop)->off_pub)
#define OFONO_OBJECT_STRING_PUB(obj,prop) G_STRUCT_MEMBER(const char*, \
    ofono_object_check(obj), (prop)->off_pub)
#define OFONO_OBJECT_STRING_PRIV(priv,prop) G_STRUCT_MEMBER(char*, \
    priv, (prop)->off_priv)

void
ofono_object_property_boolean_reset(
    OfonoObject* self,
    const OfonoObjectProperty* prop)
{
    OFONO_OBJECT_BOOL(self,prop) = FALSE;
}

void
ofono_object_property_boolean_apply(
    OfonoObject* self,
    const OfonoObjectProperty* prop,
    GVariant* value)
{
    const gboolean b = value && g_variant_get_boolean(value);
    if (OFONO_OBJECT_BOOL(self,prop) != b) {
        OFONO_OBJECT_BOOL(self,prop) = b;
        ofono_object_emit_property_changed_signal(self, prop);
    }
}

void
ofono_object_property_uint_reset(
    OfonoObject* self,
    const OfonoObjectProperty* prop)
{
    OFONO_OBJECT_UINT(self,prop) = 0;
}

static
void
ofono_object_property_uint_apply(
    OfonoObject* self,
    const OfonoObjectProperty* prop,
    guint value)
{
    if (OFONO_OBJECT_UINT(self,prop) != value) {
        OFONO_OBJECT_UINT(self,prop) = value;
        ofono_object_emit_property_changed_signal(self, prop);
    }
}

void
ofono_object_property_byte_apply(
    OfonoObject* self,
    const OfonoObjectProperty* prop,
    GVariant* value)
{
    ofono_object_property_uint_apply(self, prop, g_variant_get_byte(value));
}

void
ofono_object_property_uint16_apply(
    OfonoObject* self,
    const OfonoObjectProperty* prop,
    GVariant* value)
{
    ofono_object_property_uint_apply(self, prop, g_variant_get_uint16(value));
}

void
ofono_object_property_uint32_apply(
    OfonoObject* self,
    const OfonoObjectProperty* prop,
    GVariant* value)
{
    ofono_object_property_uint_apply(self, prop, g_variant_get_uint32(value));
}

void
ofono_object_property_enum_reset(
    OfonoObject* self,
    const OfonoObjectProperty* prop)
{
    OFONO_OBJECT_INT(self,prop) = 0;
}

void
ofono_object_property_enum_apply(
    OfonoObject* self,
    const OfonoObjectProperty* prop,
    GVariant* value)
{
    const OfonoNameIntMap* map = prop->ext;
    int i = ofono_name_to_int(map, g_variant_get_string(value, NULL));
    if (OFONO_OBJECT_INT(self,prop) != i) {
        OFONO_OBJECT_INT(self,prop) = i;
        ofono_object_emit_property_changed_signal(self, prop);
    }
}

void
ofono_object_property_string_array_reset(
    OfonoObject* self,
    const OfonoObjectProperty* prop)
{
    if (OFONO_OBJECT_PTR_ARRAY(self,prop)) {
        g_ptr_array_unref(OFONO_OBJECT_PTR_ARRAY(self,prop));
        OFONO_OBJECT_PTR_ARRAY(self,prop) = NULL;
    }
}

void
ofono_object_property_string_array_apply(
    OfonoObject* self,
    const OfonoObjectProperty* prop,
    GVariant* v)
{
    GPtrArray* a = ofono_string_array_sort(ofono_string_array_from_variant(v));
    if (!ofono_string_array_equal(a, OFONO_OBJECT_PTR_ARRAY(self,prop))) {
        if (OFONO_OBJECT_PTR_ARRAY(self,prop)) {
            g_ptr_array_unref(OFONO_OBJECT_PTR_ARRAY(self,prop));
        }
        OFONO_OBJECT_PTR_ARRAY(self,prop) = a;
        ofono_object_emit_property_changed_signal(self, prop);
    } else {
        GDEBUG("%s: %s unchanged", ofono_object_name(self), prop->name);
        g_ptr_array_unref(a);
    }
}

void
ofono_object_property_string_apply(
    OfonoObject* self,
    const OfonoObjectProperty* prop,
    GVariant* value)
{
    void* priv = prop->fn_priv(self, prop);
    const char* str = g_variant_get_string(value, NULL);
    if (g_strcmp0(OFONO_OBJECT_STRING_PRIV(priv,prop), str)) {
        g_free(OFONO_OBJECT_STRING_PRIV(priv,prop));
        OFONO_OBJECT_STRING_PUB(self,prop) =
        OFONO_OBJECT_STRING_PRIV(priv,prop) = g_strdup(str);
        ofono_object_emit_property_changed_signal(self, prop);
    }
}

void
ofono_object_property_string_reset(
    OfonoObject* self,
    const OfonoObjectProperty* prop)
{
    void* priv = prop->fn_priv(self, prop);
    g_free(OFONO_OBJECT_STRING_PRIV(priv,prop));
    OFONO_OBJECT_STRING_PUB(self,prop) =
    OFONO_OBJECT_STRING_PRIV(priv,prop) = NULL;
}

/*==========================================================================*
 * Internals
 *==========================================================================*/

static
gboolean
ofono_object_is_ready(
    OfonoObject* self)
{
    OfonoObjectPriv* priv = self->priv;
    return priv->proxy != NULL;
}

static
gboolean
ofono_object_is_valid(
    OfonoObject* self)
{
    OfonoObjectPriv* priv = self->priv;
    return ofono_object_is_ready(self) && !priv->get_properties_pending &&
        !priv->get_properties_retry_id && priv->get_properties_ok;
}

static
void
ofono_object_cancel_call(
    gpointer list_data,
    gpointer user_data)
{
    OfonoObjectPendingCall* call = list_data;
    g_cancellable_cancel(call->cancellable);
}

static
void
ofono_object_ready_changed(
    OfonoObject* self,
    gboolean ready)
{
    OfonoObjectPriv* priv = self->priv;
    GASSERT(priv->ready == ready);
    if (priv->ready) {
        if (priv->proxy) {
            ofono_object_cancel_get_properties(self);
            ofono_object_start_get_properties(self);
            ofono_object_update_valid(self);
        }
    } else {
        priv->get_properties_ok = FALSE;
        ofono_object_cancel_get_properties(self);
        ofono_object_reset_properties(self);
        g_list_foreach(priv->pending_calls, ofono_object_cancel_call, NULL);
        ofono_object_update_valid(self);
    }
}

static
void
ofono_object_valid_changed(
    OfonoObject* self)
{
    g_signal_emit(self, ofono_object_signals[
        OFONO_OBJECT_SIGNAL_VALID_CHANGED], 0);
}

/**
 * Callback to free property value
 */
static
void
ofono_object_cleanup_property(
    gpointer value)
{
    g_variant_unref(value);
}

/**
 * First stage of deinitialization (release all references).
 * May be called more than once in the lifetime of the object.
 */
static
void
ofono_object_dispose(
    GObject* object)
{
    OfonoObject* self = OFONO_OBJECT(object);
    OfonoObjectPriv* priv = self->priv;
    ofono_object_cancel_get_properties(self);
    if (priv->proxy) {
        gutil_disconnect_handlers(priv->proxy,
            &priv->property_changed_signal_id, 1);
        g_object_unref(priv->proxy);
        priv->proxy = NULL;
    }
    if (priv->bus) {
        g_object_unref(priv->bus);
        priv->bus = NULL;
    }
    G_OBJECT_CLASS(ofono_object_parent_class)->dispose(object);
}

/**
 * Final stage of deinitialization
 */
static
void
ofono_object_finalize(
    GObject* object)
{
    OfonoObject* self = OFONO_OBJECT(object);
    OfonoObjectPriv* priv = self->priv;
    GASSERT(!priv->pending_calls);
    gutil_idle_pool_unref(priv->pool);
    g_hash_table_unref(priv->properties);
    g_free(priv->intf);
    g_free(priv->path);
    G_OBJECT_CLASS(ofono_object_parent_class)->finalize(object);
}

/**
 * Per instance initializer
 */
static
void
ofono_object_init(
    OfonoObject* self)
{
    GError* error = NULL;
    OfonoObjectPriv* priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
        OFONO_TYPE_OBJECT, OfonoObjectPriv);
    self->priv = priv;
    priv->pool = gutil_idle_pool_ref(ofono_idle_pool());
    priv->bus = g_bus_get_sync(OFONO_BUS_TYPE, NULL, &error);
    if (priv->bus) {
        g_dbus_connection_set_exit_on_close(priv->bus, FALSE);
    } else {
        GERR("%s", GERRMSG(error));
        g_error_free(error);
    }
    priv->properties = g_hash_table_new_full(g_str_hash, g_str_equal,
        g_free, ofono_object_cleanup_property);
}

/**
 * Per class initializer
 */
static
void
ofono_object_class_init(
    OfonoObjectClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    ofono_object_class = klass;
    g_type_class_add_private(klass, sizeof(OfonoObjectPriv));
    klass->fn_is_ready = ofono_object_is_ready;
    klass->fn_is_valid = ofono_object_is_valid;
    klass->fn_ready_changed = ofono_object_ready_changed;
    klass->fn_valid_changed = ofono_object_valid_changed;
    klass->fn_proxy_created = ofono_object_proxy_created;

    /* Default proxy callbacks */
    klass->fn_proxy_new = ofono_object_proxy_new;
    klass->fn_proxy_new_finish = ofono_object_default_proxy_new_finish;
    klass->fn_proxy_call_get_properties =
        ofono_object_default_proxy_call_get_properties;
    klass->fn_proxy_call_set_property =
        ofono_object_default_proxy_call_set_property;
    klass->fn_proxy_call_get_properties_finish =
        ofono_object_default_proxy_call_get_properties_finish;
    klass->fn_proxy_call_set_property_finish =
        ofono_object_default_proxy_call_set_property_finish;

    object_class->dispose = ofono_object_dispose;
    object_class->finalize = ofono_object_finalize;
    ofono_object_signals[OFONO_OBJECT_SIGNAL_VALID_CHANGED] =
        g_signal_new(OFONO_OBJECT_SIGNAL_VALID_CHANGED_NAME,
            G_OBJECT_CLASS_TYPE(klass), G_SIGNAL_RUN_FIRST,
            0, NULL, NULL, NULL, G_TYPE_NONE, 0);
    ofono_object_signals[OFONO_OBJECT_SIGNAL_PROPERTY_CHANGED] =
        g_signal_new(OFONO_OBJECT_SIGNAL_PROPERTY_CHANGED_NAME,
            G_OBJECT_CLASS_TYPE(klass), G_SIGNAL_RUN_FIRST | G_SIGNAL_DETAILED,
            0, NULL, NULL, NULL, G_TYPE_NONE,
            2, G_TYPE_STRING, G_TYPE_VARIANT);

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
