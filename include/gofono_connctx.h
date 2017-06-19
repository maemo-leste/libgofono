/*
 * Copyright (C) 2014-2017 Jolla Ltd.
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
 *   3. Neither the name of Jolla Ltd nor the names of its contributors may
 *      be used to endorse or promote products derived from this software
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

#ifndef GOFONO_CONNCTX_H
#define GOFONO_CONNCTX_H

#include "gofono_object.h"

G_BEGIN_DECLS

typedef struct ofono_connctx_priv OfonoConnCtxPriv;

typedef enum ofono_connctx_protocol {
    OFONO_CONNCTX_PROTOCOL_UNKNOWN = -1,
    OFONO_CONNCTX_PROTOCOL_NONE,
    OFONO_CONNCTX_PROTOCOL_IP,                  /* ip */
    OFONO_CONNCTX_PROTOCOL_IPV6,                /* ipv6 */
    OFONO_CONNCTX_PROTOCOL_DUAL                 /* dual */
} OFONO_CONNCTX_PROTOCOL;

typedef enum ofono_connctx_auth {
    OFONO_CONNCTX_AUTH_UNKNOWN = -1,
    OFONO_CONNCTX_AUTH_NONE,
    OFONO_CONNCTX_AUTH_PAP,                     /* pap */
    OFONO_CONNCTX_AUTH_CHAP,                    /* chap */
    OFONO_CONNCTX_AUTH_ANY                      /* pap or chap */
} OFONO_CONNCTX_AUTH;

typedef enum ofono_connctx_method {
    OFONO_CONNCTX_METHOD_UNKNOWN = -1,
    OFONO_CONNCTX_METHOD_NONE,
    OFONO_CONNCTX_METHOD_STATIC,                /* static */
    OFONO_CONNCTX_METHOD_DHCP                   /* dhcp */
} OFONO_CONNCTX_METHOD;

typedef struct ofono_connctx_settings {
    const char* ifname;                         /* Interface */
    OFONO_CONNCTX_METHOD method;                /* Method */
    const char* address;                        /* Address */
    const char* netmask;                        /* Netmask */
    const char* gateway;                        /* Gateway */
    char* const* dns;                           /* DomainNameServers */
    guint prefix;                               /* PrefixLength */
} OfonoConnCtxSettings;

struct ofono_connctx {
    OfonoObject object;
    OfonoConnCtxPriv* priv;
    const char* ifname;
    gboolean active;                            /* Active */
    const char* apn;                            /* AccessPointName */
    OFONO_CONNCTX_TYPE type;                    /* Type */
    OFONO_CONNCTX_AUTH auth;                    /* AuthenticationMethod */
    const char* username;                       /* Username */
    const char* password;                       /* Password */
    OFONO_CONNCTX_PROTOCOL protocol;            /* Protocol */
    const char* name;                           /* Name */
    const OfonoConnCtxSettings* settings;       /* Settings */
    const OfonoConnCtxSettings* ipv6_settings;  /* IPv6.Settings */
    const char* mms_proxy;                      /* MessageProxy */
    const char* mms_center;                     /* MessageCenter */
};

GType ofono_connctx_get_type(void);
#define OFONO_TYPE_CONNCTX (ofono_connctx_get_type())
#define OFONO_CONNCTX(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
        OFONO_TYPE_CONNCTX, OfonoConnCtx))

typedef
void
(*OfonoConnCtxHandler)(
    OfonoConnCtx* sender,
    void* arg);

typedef
void
(*OfonoConnCtxErrorHandler)(
    OfonoConnCtx* sender,
    const GError* error,
    void* arg);

typedef
void
(*OfonoConnCtxCallHandler)(
    OfonoConnCtx* sender,
    const GError* error,
    void* arg);

typedef
void
(*OfonoConnCtxPropertyHandler)(
    OfonoConnCtx* sender,
    const char* name,
    GVariant* value,
    void* arg);

OfonoConnCtx*
ofono_connctx_new(
    const char* path);

OfonoConnCtx*
ofono_connctx_ref(
    OfonoConnCtx* context);

void
ofono_connctx_unref(
    OfonoConnCtx* context);

const char*
ofono_connctx_type_string(
    OFONO_CONNCTX_TYPE type);

const char*
ofono_connctx_protocol_string(
    OFONO_CONNCTX_PROTOCOL protocol);

const char*
ofono_connctx_auth_string(
    OFONO_CONNCTX_AUTH auth);

const char*
ofono_connctx_method_string(
    OFONO_CONNCTX_METHOD method);

gulong
ofono_connctx_add_valid_changed_handler(
    OfonoConnCtx* context,
    OfonoConnCtxHandler handler,
    void* arg);

gulong
ofono_connctx_add_property_changed_handler(
    OfonoConnCtx* context,
    OfonoConnCtxPropertyHandler handler,
    const char* name,
    void* arg);

gulong
ofono_connctx_add_name_changed_handler(
    OfonoConnCtx* context,
    OfonoConnCtxHandler handler,
    void* arg);

gulong
ofono_connctx_add_apn_changed_handler(
    OfonoConnCtx* context,
    OfonoConnCtxHandler handler,
    void* arg);

gulong
ofono_connctx_add_type_changed_handler(
    OfonoConnCtx* context,
    OfonoConnCtxHandler handler,
    void* arg);

gulong
ofono_connctx_add_mms_proxy_changed_handler(
    OfonoConnCtx* context,
    OfonoConnCtxHandler handler,
    void* arg);

gulong
ofono_connctx_add_mms_center_changed_handler(
    OfonoConnCtx* context,
    OfonoConnCtxHandler handler,
    void* arg);

gulong
ofono_connctx_add_interface_changed_handler(
    OfonoConnCtx* context,
    OfonoConnCtxHandler handler,
    void* arg);

gulong
ofono_connctx_add_settings_changed_handler(
    OfonoConnCtx* context,
    OfonoConnCtxHandler handler,
    void* arg);

gulong
ofono_connctx_add_ipv6_settings_changed_handler(
    OfonoConnCtx* context,
    OfonoConnCtxHandler handler,
    void* arg);

gulong
ofono_connctx_add_activate_failed_handler(
    OfonoConnCtx* context,
    OfonoConnCtxErrorHandler handler,
    void* arg);

gulong
ofono_connctx_add_active_changed_handler(
    OfonoConnCtx* context,
    OfonoConnCtxHandler handler,
    void* arg);

void
ofono_connctx_remove_handler(
    OfonoConnCtx* context,
    gulong id);

void
ofono_connctx_activate(
    OfonoConnCtx* context);

void
ofono_connctx_deactivate(
    OfonoConnCtx* context);

gboolean
ofono_connctx_set_string(
    OfonoConnCtx* context,
    const char* name,
    const char* value);

gboolean
ofono_connctx_set_type(
    OfonoConnCtx* context,
    OFONO_CONNCTX_TYPE type);

gboolean
ofono_connctx_set_protocol(
    OfonoConnCtx* context,
    OFONO_CONNCTX_PROTOCOL protocol);

gboolean
ofono_connctx_set_auth(
    OfonoConnCtx* context,
    OFONO_CONNCTX_AUTH auth);

gboolean
ofono_connctx_provision(
    OfonoConnCtx* context);

GCancellable*
ofono_connctx_set_string_full(
    OfonoConnCtx* self,
    const char* name,
    const char* value,
    OfonoConnCtxCallHandler callback,
    void* arg);

GCancellable*
ofono_connctx_set_type_full(
    OfonoConnCtx* context,
    OFONO_CONNCTX_TYPE type,
    OfonoConnCtxCallHandler callback,
    void* arg);

GCancellable*
ofono_connctx_set_protocol_full(
    OfonoConnCtx* context,
    OFONO_CONNCTX_PROTOCOL protocol,
    OfonoConnCtxCallHandler callback,
    void* arg);

GCancellable*
ofono_connctx_set_auth_full(
    OfonoConnCtx* context,
    OFONO_CONNCTX_AUTH auth,
    OfonoConnCtxCallHandler callback,
    void* arg);

GCancellable*
ofono_connctx_provision_full(
    OfonoConnCtx* context,
    OfonoConnCtxCallHandler callback,
    void* arg);

/* Inline wrappers */

OFONO_INLINE OfonoObject*
ofono_connctx_object(OfonoConnCtx* ctx)
    { return G_LIKELY(ctx) ? &ctx->object : NULL; }

OFONO_INLINE const char*
ofono_connctx_path(const OfonoConnCtx* ctx)
    { return G_LIKELY(ctx) ? ctx->object.path : NULL; }

OFONO_INLINE gboolean
ofono_connctx_valid(const OfonoConnCtx* ctx)
    { return G_LIKELY(ctx) && ctx->object.valid; }

OFONO_INLINE gboolean
ofono_connctx_wait_valid(OfonoConnCtx* ctx, int msec, GError** error)
    { return ofono_object_wait_valid(ofono_connctx_object(ctx), msec, error); }

OFONO_INLINE void
ofono_connctx_remove_handlers(OfonoConnCtx* ctx, gulong* ids, guint n)
    { ofono_object_remove_handlers(ofono_connctx_object(ctx), ids, n); }

G_END_DECLS

#endif /* GOFONO_CONNCTX_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
