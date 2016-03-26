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

#ifndef GOFONO_OBJECT_PRIVATE_H
#define GOFONO_OBJECT_PRIVATE_H

#include "gofono_object.h"

#ifndef OFONO_OBJECT_PROXY
#define OFONO_OBJECT_PROXY GDBusProxy
#endif

typedef struct ofono_object_property OfonoObjectProperty;

typedef struct ofono_object_pending_call {
    OfonoObject* object;
    GCancellable* cancellable;
    OfonoObjectCallFinishedCallback callback;
    void* arg;
} OfonoObjectPendingCall;

typedef
void
(*OfonoObjectProxyCallFinishedCallback)(
    OFONO_OBJECT_PROXY* object,
    GAsyncResult* result,
    const OfonoObjectPendingCall* call);

typedef struct ofono_object_class {
    GObjectClass object;
    OfonoObjectProperty* properties;
    guint nproperties;
    void (*fn_proxy_created)(
        OfonoObject* object,
        OFONO_OBJECT_PROXY* proxy);
    gboolean (*fn_is_ready)(
        OfonoObject* object);
    void (*fn_ready_changed)(
        OfonoObject* object,
        gboolean ready);
    gboolean (*fn_is_valid)(
        OfonoObject* object);
    void (*fn_valid_changed)(
        OfonoObject* object);
    /* Functions below point to generated stubs */
    void (*fn_proxy_new)(
        GDBusConnection* bus,
        GDBusProxyFlags flags,
        const gchar* name,
        const gchar* path,
        GCancellable* cancellable,
        GAsyncReadyCallback callback,
        gpointer data);
    OFONO_OBJECT_PROXY* (*fn_proxy_new_finish)(
        GAsyncResult* result,
        GError** error);
    void(*fn_proxy_call_get_properties)(
        OFONO_OBJECT_PROXY* proxy,
        GCancellable* cancellable,
        GAsyncReadyCallback callback,
        gpointer data);
    void (*fn_proxy_call_set_property)(
        OFONO_OBJECT_PROXY* proxy,
        const gchar* name,
        GVariant* value,
        GCancellable* cancellable,
        GAsyncReadyCallback callback,
        gpointer data);
    gboolean (*fn_proxy_call_get_properties_finish)(
        OFONO_OBJECT_PROXY* proxy,
        GVariant** properties,
        GAsyncResult* res,
        GError** error);
    gboolean (*fn_proxy_call_set_property_finish)(
        OFONO_OBJECT_PROXY* proxy,
        GAsyncResult* res,
        GError** error);
} OfonoObjectClass;

struct ofono_object_property {
    const char* name;
    const char* signal_name;
    guint signal;
    void* (*fn_priv)(OfonoObject* object, const OfonoObjectProperty* property);
    void (*fn_reset)(OfonoObject* object, const OfonoObjectProperty* property);
    void (*fn_apply)(OfonoObject* object, const OfonoObjectProperty* property,
        GVariant* value);
    goffset off_pub;
    goffset off_priv;
    const void* ext;
};

GType ofono_object_get_type();
#define OFONO_TYPE_OBJECT (ofono_object_get_type())
#define OFONO_OBJECT(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
        OFONO_TYPE_OBJECT, OfonoObject))
#define OFONO_OBJECT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), \
        OFONO_TYPE_OBJECT, OfonoObjectClass))
#define OFONO_OBJECT_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
        OFONO_TYPE_OBJECT, OfonoObjectClass))
#define OFONO_OBJECT_CLASS_SET_PROXY_CALLBACKS(k,p) \
    (k)->fn_proxy_new = p##_proxy_new; \
    (k)->fn_proxy_new_finish = p##_proxy_new_finish; \
    (k)->fn_proxy_call_set_property = p##_call_set_property; \
    (k)->fn_proxy_call_get_properties = p##_call_get_properties; \
    (k)->fn_proxy_call_set_property_finish = p##_call_set_property_finish; \
    (k)->fn_proxy_call_get_properties_finish = p##_call_get_properties_finish;
#define OFONO_OBJECT_CLASS_SET_PROXY_CALLBACKS_RO(k,p) \
    (k)->fn_proxy_new = p##_proxy_new; \
    (k)->fn_proxy_new_finish = p##_proxy_new_finish; \
    (k)->fn_proxy_call_set_property = NULL; \
    (k)->fn_proxy_call_get_properties = p##_call_get_properties; \
    (k)->fn_proxy_call_set_property_finish = NULL; \
    (k)->fn_proxy_call_get_properties_finish = p##_call_get_properties_finish;

void
ofono_class_initialize(
    OfonoObjectClass* klass);

void
ofono_object_initialize(
    OfonoObject* object,
    const char* intf,
    const char* path);

void
ofono_object_apply_properties(
    OfonoObject* self,
    GVariant* properties);

GVariant*
ofono_object_get_properties(
    OfonoObject* self);

void
ofono_object_update_ready(
    OfonoObject* object);

void
ofono_object_update_valid(
    OfonoObject* object);

GDBusConnection*
ofono_object_bus(
    OfonoObject* object);

const char*
ofono_object_name(
    OfonoObject* object);

OFONO_OBJECT_PROXY*
ofono_object_proxy(
    OfonoObject* object);

OfonoObjectPendingCall*
ofono_object_pending_call_new(
    OfonoObject* object,
    OfonoObjectProxyCallFinishedCallback finished,
    OfonoObjectCallFinishedCallback callback,
    void* arg);

void
ofono_object_pending_call_finished(
    GObject* proxy,
    GAsyncResult* result,
    gpointer data);

/* Properties */

void
ofono_object_property_boolean_reset(
    OfonoObject* self,
    const OfonoObjectProperty* prop);

void
ofono_object_property_boolean_apply(
    OfonoObject* object,
    const OfonoObjectProperty* prop,
    GVariant* value);

void
ofono_object_property_uint_reset(
    OfonoObject* self,
    const OfonoObjectProperty* prop);

void
ofono_object_property_byte_apply(
    OfonoObject* object,
    const OfonoObjectProperty* prop,
    GVariant* value);

void
ofono_object_property_uint16_apply(
    OfonoObject* object,
    const OfonoObjectProperty* prop,
    GVariant* value);

void
ofono_object_property_uint32_apply(
    OfonoObject* object,
    const OfonoObjectProperty* prop,
    GVariant* value);

void
ofono_object_property_enum_reset(
    OfonoObject* self,
    const OfonoObjectProperty* prop);

void
ofono_object_property_enum_apply(
    OfonoObject* self,
    const OfonoObjectProperty* prop,
    GVariant* value);

void
ofono_object_property_string_array_reset(
    OfonoObject* object,
    const OfonoObjectProperty* prop);

void
ofono_object_property_string_array_apply(
    OfonoObject* object,
    const OfonoObjectProperty* prop,
    GVariant* value);

void
ofono_object_property_string_apply(
    OfonoObject* self,
    const OfonoObjectProperty* prop,
    GVariant* value);

void
ofono_object_property_string_reset(
    OfonoObject* self,
    const OfonoObjectProperty* prop);

OFONO_INLINE void
ofono_object_emit_property_changed_signal(OfonoObject* object,
    const OfonoObjectProperty* property)
    { if (property->signal) g_signal_emit(object, property->signal, 0); }

/* NOTE: public fields for both BYTE and UINT16 are assumed to be guint */
#define ofono_object_property_byte_reset ofono_object_property_uint_reset
#define ofono_object_property_uint16_reset ofono_object_property_uint_reset
#define ofono_object_property_uint32_reset ofono_object_property_uint_reset

#define OFONO_OBJECT_OFFSET_NONE ((goffset)-1)

#define OFONO_OBJECT_DEFINE_PROPERTY_BOOL(PREFIX,NAME,T,var) {          \
    OFONO_##PREFIX##_PROPERTY_##NAME,                                   \
    PREFIX##_SIGNAL_##NAME##_CHANGED_NAME, 0, NULL,                     \
    ofono_object_property_boolean_reset,                                \
    ofono_object_property_boolean_apply,                                \
    G_STRUCT_OFFSET(T,var), OFONO_OBJECT_OFFSET_NONE, NULL }

#define OFONO_OBJECT_DEFINE_PROPERTY_BYTE(PREFIX,NAME,T,var) {          \
    OFONO_##PREFIX##_PROPERTY_##NAME,                                   \
    PREFIX##_SIGNAL_##NAME##_CHANGED_NAME, 0, NULL,                     \
    ofono_object_property_byte_reset,                                   \
    ofono_object_property_byte_apply,                                   \
    G_STRUCT_OFFSET(T,var), OFONO_OBJECT_OFFSET_NONE, NULL }

#define OFONO_OBJECT_DEFINE_PROPERTY_UINT16(PREFIX,NAME,T,var) {        \
    OFONO_##PREFIX##_PROPERTY_##NAME,                                   \
    PREFIX##_SIGNAL_##NAME##_CHANGED_NAME, 0, NULL,                     \
    ofono_object_property_uint16_reset,                                 \
    ofono_object_property_uint16_apply,                                 \
    G_STRUCT_OFFSET(T,var), OFONO_OBJECT_OFFSET_NONE, NULL }

#define OFONO_OBJECT_DEFINE_PROPERTY_UINT32(PREFIX,NAME,T,var) {        \
    OFONO_##PREFIX##_PROPERTY_##NAME,                                   \
    PREFIX##_SIGNAL_##NAME##_CHANGED_NAME, 0, NULL,                     \
    ofono_object_property_uint32_reset,                                 \
    ofono_object_property_uint32_apply,                                 \
    G_STRUCT_OFFSET(T,var), OFONO_OBJECT_OFFSET_NONE, NULL }

/* Last parameter must point to OfonoNameIntMap. Also, the size of the
 * enum must equal sizeof(int) and it's not always the case. The most
 * portable way to ensure that is to have -1 as one of the enum values.
 * The reset function resets the enum value to zero. */
#define OFONO_OBJECT_DEFINE_PROPERTY_ENUM(PREFIX,NAME,T,var,map) {      \
    OFONO_##PREFIX##_PROPERTY_##NAME,                                   \
    PREFIX##_SIGNAL_##NAME##_CHANGED_NAME, 0, NULL,                     \
    ofono_object_property_enum_reset,                                   \
    ofono_object_property_enum_apply,                                   \
    G_STRUCT_OFFSET(T,var), OFONO_OBJECT_OFFSET_NONE, map }

#define OFONO_OBJECT_DEFINE_PROPERTY_STRING_ARRAY(PREFIX,NAME,T,var) {  \
    OFONO_##PREFIX##_PROPERTY_##NAME,                                   \
    PREFIX##_SIGNAL_##NAME##_CHANGED_NAME, 0, NULL,                     \
    ofono_object_property_string_array_reset,                           \
    ofono_object_property_string_array_apply,                           \
    G_STRUCT_OFFSET(T,var), OFONO_OBJECT_OFFSET_NONE, NULL }

#define OFONO_OBJECT_DEFINE_PROPERTY_STRING(PREFIX,prefix,NAME,T,var) { \
    OFONO_##PREFIX##_PROPERTY_##NAME,                                   \
    PREFIX##_SIGNAL_##NAME##_CHANGED_NAME, 0,                           \
    ofono_##prefix##_property_priv,                                     \
    ofono_object_property_string_reset,                                 \
    ofono_object_property_string_apply,                                 \
    G_STRUCT_OFFSET(T,var), G_STRUCT_OFFSET(T##Priv,var), NULL}

#endif /* GOFONO_OBJECT_PRIVATE_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
