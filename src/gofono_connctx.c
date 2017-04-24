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

#include "gofono_connctx.h"
#include "gofono_modem.h"
#include "gofono_error.h"
#include "gofono_names.h"
#include "gofono_util.h"
#include "gofono_log.h"

#include <gutil_strv.h>
#include <gutil_misc.h>

/* Generated headers */
#define OFONO_OBJECT_PROXY OrgOfonoConnectionContext
#include "org.ofono.ConnectionContext.h"
#include "org.ofono.ConnectionManager.h"
#include "gofono_object_p.h"

/* Retry configuration */
#define RETRY_DELAY_SEC (1)
#define MAX_RETRY_COUNT (30)

/* Object definition */
typedef struct ofono_connctx_settings_priv {
    OfonoConnCtxSettings pub;
    char* ifname;
    char* address;
    char* netmask;
    char* gateway;
    char** dns;
} OfonoConnCtxSettingsPriv;

enum modem_event {
    MODEM_EVENT_VALID,
    MODEM_EVENT_INTERFACES,
    MODEM_EVENT_COUNT
};

enum connmgr_event {
    CONNMGR_EVENT_CONTEXT_ADDED,
    CONNMGR_EVENT_CONTEXT_REMOVED,
    CONNMGR_EVENT_COUNT
};

typedef enum connctx_action {
    CONNCTX_ACTION_NONE,
    CONNCTX_ACTION_ACTIVATE,
    CONNCTX_ACTION_DEACTIVATE
} CONNCTX_ACTION;

struct ofono_connctx_priv {
    CONNCTX_ACTION next_action;
    CONNCTX_ACTION current_action;
    guint retry_id;
    guint retry_count;
    char* apn;
    char* name;
    char* username;
    char* password;
    char* mms_proxy;
    char* mms_center;
    OfonoConnCtxSettingsPriv settings;
    OfonoConnCtxSettingsPriv ipv6_settings;
    OfonoModem* modem;
    gulong modem_event_id[MODEM_EVENT_COUNT];
    OrgOfonoConnectionManager* connmgr;
    gulong connmgr_event_id[CONNMGR_EVENT_COUNT];
    gboolean removed;
};

typedef OfonoObjectClass OfonoConnCtxClass;
G_DEFINE_TYPE(OfonoConnCtx, ofono_connctx, OFONO_TYPE_OBJECT)
#define SUPER_CLASS ofono_connctx_parent_class

enum ofono_connctx_signal {
    CONNCTX_SIGNAL_ACTIVATE_FAILED,
    CONNCTX_SIGNAL_INTERFACE_CHANGED,
    CONNCTX_SIGNAL_COUNT
};

#define CONNCTX_SIGNAL_ACTIVATE_FAILED_NAME         "activate-failed"
#define CONNCTX_SIGNAL_INTERFACE_CHANGED_NAME       "interface-changed"
#define CONNCTX_SIGNAL_ACTIVE_CHANGED_NAME          "active-changed"
#define CONNCTX_SIGNAL_APN_CHANGED_NAME             "apn-changed"
#define CONNCTX_SIGNAL_TYPE_CHANGED_NAME            "type-changed"
#define CONNCTX_SIGNAL_NAME_CHANGED_NAME            "name-changed"
#define CONNCTX_SIGNAL_AUTH_CHANGED_NAME            "auth-changed"
#define CONNCTX_SIGNAL_USERNAME_CHANGED_NAME        "username-changed"
#define CONNCTX_SIGNAL_PASSWORD_CHANGED_NAME        "password-changed"
#define CONNCTX_SIGNAL_PROTOCOL_CHANGED_NAME        "protocol-changed"
#define CONNCTX_SIGNAL_MMS_PROXY_CHANGED_NAME       "mms-proxy-changed"
#define CONNCTX_SIGNAL_MMS_CENTER_CHANGED_NAME      "mms-center-changed"
#define CONNCTX_SIGNAL_SETTINGS_CHANGED_NAME        "settings-changed"
#define CONNCTX_SIGNAL_IPV6_SETTINGS_CHANGED_NAME   "ipv6-settings-changed"

static guint ofono_connctx_signals[CONNCTX_SIGNAL_COUNT] = {0};

#define CONNCTX_SIGNAL_NEW(klass,name) \
    ofono_connctx_signals[CONNCTX_SIGNAL_##name##_CHANGED] = \
        g_signal_new(CONNCTX_SIGNAL_##name##_CHANGED_NAME, \
            G_OBJECT_CLASS_TYPE(klass), G_SIGNAL_RUN_FIRST, \
            0, NULL, NULL, NULL, G_TYPE_NONE, 0)
#define CONNCTX_SIGNAL_EMIT(self,name) \
    g_signal_emit(self, ofono_connctx_signals[\
    CONNCTX_SIGNAL_##name##_CHANGED], 0)

static GHashTable* ofono_connctx_table = NULL;

/* Enum <-> string mappings */
static const OfonoNameIntPair ofono_connctx_type_values[] = {
    { "internet", OFONO_CONNCTX_TYPE_INTERNET },
    { "mms",      OFONO_CONNCTX_TYPE_MMS },
    { "wap",      OFONO_CONNCTX_TYPE_WAP },
    { "ims",      OFONO_CONNCTX_TYPE_IMS }
};

static const OfonoNameIntMap ofono_connctx_type_map = {
    "context type",
    OFONO_NAME_INT_MAP_ENTRIES(ofono_connctx_type_values),
    { NULL, OFONO_CONNCTX_TYPE_UNKNOWN }
};

static const OfonoNameIntPair ofono_connctx_protocol_values[] = {
    { "ip",   OFONO_CONNCTX_PROTOCOL_IP },
    { "ipv6", OFONO_CONNCTX_PROTOCOL_IPV6 },
    { "dual", OFONO_CONNCTX_PROTOCOL_DUAL }
};

static const OfonoNameIntMap ofono_connctx_protocol_map = {
    "protocol",
    OFONO_NAME_INT_MAP_ENTRIES(ofono_connctx_protocol_values),
    { NULL,  OFONO_CONNCTX_PROTOCOL_UNKNOWN}
};

static const OfonoNameIntPair ofono_connctx_auth_values[] = {
    { "pap",  OFONO_CONNCTX_AUTH_PAP },
    { "chap", OFONO_CONNCTX_AUTH_CHAP }
};

static const OfonoNameIntMap ofono_connctx_auth_map = {
    "auth method",
    OFONO_NAME_INT_MAP_ENTRIES(ofono_connctx_auth_values),
    { NULL,  OFONO_CONNCTX_AUTH_UNKNOWN}
};

static const OfonoNameIntPair ofono_connctx_method_values[] = {
    { "static", OFONO_CONNCTX_METHOD_STATIC },
    { "dhcp",   OFONO_CONNCTX_METHOD_DHCP }
};

static const OfonoNameIntMap ofono_connctx_method_map = {
    "configuration method",
    OFONO_NAME_INT_MAP_ENTRIES(ofono_connctx_method_values),
    { NULL,  OFONO_CONNCTX_METHOD_UNKNOWN}
};

/* Forward declarations */
static
void
ofono_connctx_perform_next_action(
    OfonoConnCtx* self);

static
gboolean
ofono_connctx_set_active_retry(
    gpointer user_data);

/*==========================================================================*
 * Implementation
 *==========================================================================*/

OFONO_INLINE
void
ofono_connctx_update_ready(
    OfonoConnCtx* self)
{
    ofono_object_update_ready(ofono_connctx_object(self));
}

static
void
ofono_connctx_settings_init(
    OfonoConnCtxSettingsPriv* settings)
{
    /* The struct is assumed to have been zeroed */
    settings->pub.method = OFONO_CONNCTX_METHOD_UNKNOWN;
}

static
void
ofono_connctx_settings_clear(
    OfonoConnCtxSettingsPriv* settings)
{
    g_free(settings->ifname);
    g_free(settings->address);
    g_free(settings->netmask);
    g_free(settings->gateway);
    g_strfreev(settings->dns);
    memset(settings, 0, sizeof(*settings));
    ofono_connctx_settings_init(settings);
};

static
gboolean
ofono_connctx_settings_decode(
    OfonoConnCtxSettingsPriv* settings,
    GVariant* dict)
{
    GVariant* value;
    const char* s;
    guchar y;
    gboolean empty = TRUE;

    memset(settings, 0, sizeof(*settings));
    ofono_connctx_settings_init(settings);

    /* Interface */
    s = NULL;
    if (g_variant_lookup(dict, OFONO_CONNCTX_SETTINGS_INTERFACE, "&s", &s) &&
        s && s[0]) {
        settings->pub.ifname = settings->ifname = g_strdup(s);
        empty = FALSE;
    }

    /* Address */
    s = NULL;
    if (g_variant_lookup(dict, OFONO_CONNCTX_SETTINGS_ADDRESS, "&s", &s) &&
        s && s[0]) {
        settings->pub.address = settings->address = g_strdup(s);
        empty = FALSE;
    }

    /* Netmask */
    s = NULL;
    if (g_variant_lookup(dict, OFONO_CONNCTX_SETTINGS_NETMASK, "&s", &s) &&
        s && s[0]) {
        settings->pub.netmask = settings->netmask = g_strdup(s);
        empty = FALSE;
    }

    /* Gateway */
    s = NULL;
    if (g_variant_lookup(dict, OFONO_CONNCTX_SETTINGS_GATEWAY, "&s", &s) &&
        s && s[0]) {
        settings->pub.gateway = settings->gateway = g_strdup(s);
        empty = FALSE;
    }

    /* Method */
    s = NULL;
    if (g_variant_lookup(dict, OFONO_CONNCTX_SETTINGS_METHOD, "&s", &s) &&
        s && s[0]) {
        settings->pub.method = ofono_name_to_int(&ofono_connctx_method_map, s);
        empty = FALSE;
    }

    /* PrefixLength */
    y = 0;
    if (g_variant_lookup(dict, OFONO_CONNCTX_SETTINGS_PREFIX_LENGTH, "y", &y)) {
        settings->pub.prefix = y;
        empty = FALSE;
    }

    /* DomainNameServers */
    value = g_variant_lookup_value(dict, OFONO_CONNCTX_SETTINGS_DNS,
        G_VARIANT_TYPE_ARRAY);
    if (value) {
        const int n = g_variant_n_children(value);
        if (n > 0) {
            int i;
            settings->pub.dns = settings->dns = g_new(char*, n+1);
            for (i=0; i<n; i++) {
                GVariant* string = g_variant_get_child_value(value, i);
                settings->dns[i] = g_strdup(g_variant_get_string(string, NULL));
                g_variant_unref(string);
            }
            settings->dns[i] = NULL;
            empty = FALSE;
        }
        g_variant_unref(value);
    }

    return !empty;
}

static
void
ofono_connctx_set_active_done(
    OfonoObject* object,
    const GError* error,
    void* arg)
{
    OfonoConnCtx* self = OFONO_CONNCTX(object);
    OfonoConnCtxPriv* priv = self->priv;
    GASSERT(!priv->retry_id);
    GASSERT(priv->current_action != CONNCTX_ACTION_NONE);
    if (error) {
        if (error->domain == OFONO_ERROR &&
            error->code == OFONO_ERROR_BUSY &&
            priv->retry_count < MAX_RETRY_COUNT) {
            priv->retry_count++;
            GDEBUG("Retry %d in %d sec", priv->retry_count, RETRY_DELAY_SEC);
            priv->retry_id = g_timeout_add_seconds(RETRY_DELAY_SEC,
                ofono_connctx_set_active_retry, self);
        } else {
            GDEBUG("Giving up on %s", ofono_connctx_path(self));
            if (priv->current_action == CONNCTX_ACTION_ACTIVATE) {
                priv->current_action = CONNCTX_ACTION_NONE;
                /* Need to reset current_action before emitting the signal */
                g_signal_emit(self, ofono_connctx_signals[
                    CONNCTX_SIGNAL_ACTIVATE_FAILED], 0, error);
            } else {
                priv->current_action = CONNCTX_ACTION_NONE;
            }
        }
    } else {
        /* Success */
        priv->current_action = CONNCTX_ACTION_NONE;
    }
    ofono_connctx_perform_next_action(self);
}

static
gboolean
ofono_connctx_set_active_retry(
    gpointer user_data)
{
    OfonoConnCtx* self = OFONO_CONNCTX(user_data);
    OfonoConnCtxPriv* priv = self->priv;
    const gboolean on = (priv->current_action == CONNCTX_ACTION_ACTIVATE);
    GASSERT(priv->current_action != CONNCTX_ACTION_NONE);
    GASSERT(priv->retry_id);
    priv->retry_id = 0;
    GDEBUG("%sctivating %s again", on ? "A" : "Dea", ofono_connctx_path(self));
    ofono_object_set_boolean(ofono_connctx_object(self),
        OFONO_CONNCTX_PROPERTY_ACTIVE, on, ofono_connctx_set_active_done,
        NULL);
    return G_SOURCE_REMOVE;
}

static
void
ofono_connctx_perform_next_action(
    OfonoConnCtx* self)
{
    OfonoObject* object = ofono_connctx_object(self);
    OfonoConnCtxPriv* priv = self->priv;
    if (object->valid) {
        if (priv->current_action != CONNCTX_ACTION_NONE &&
            priv->next_action != CONNCTX_ACTION_NONE &&
            priv->next_action != priv->current_action &&
            priv->retry_id) {
            /* Cancel retry, start the next action */
            priv->current_action = CONNCTX_ACTION_NONE;
            g_source_remove(priv->retry_id);
            priv->retry_id = 0;
        }
        if (priv->current_action == CONNCTX_ACTION_NONE &&
            priv->next_action != CONNCTX_ACTION_NONE) {
            const gboolean on = (priv->next_action == CONNCTX_ACTION_ACTIVATE);
            GASSERT(!priv->retry_id);
            priv->retry_count = 0;
            GDEBUG("%sctivating %s", on ? "A" : "Dea", object->path);
            ofono_object_set_boolean(object, OFONO_CONNCTX_PROPERTY_ACTIVE, on,
                ofono_connctx_set_active_done, self);
            priv->current_action = priv->next_action;
            priv->next_action = CONNCTX_ACTION_NONE;
        }
    }
}

static
void
ofono_connctx_modem_changed(
    OfonoModem* modem,
    void* arg)
{
    ofono_connctx_update_ready(OFONO_CONNCTX(arg));
}

static
void
ofono_connctx_added(
    OrgOfonoConnectionManager* proxy,
    const char* path,
    GVariant* properties,
    OfonoConnCtx* self)
{
    if (!g_strcmp0(path, ofono_connctx_path(self))) {
        OfonoConnCtxPriv* priv = self->priv;
        GVERBOSE_("%s added", path);
        if (!priv->removed) {
            /*
             * If priv->removed is TRUE, then this object is already
             * invalid; otherwise we need to make is invalid to ensure
             * that setup gets repeated.
             */
            priv->removed = TRUE;
            ofono_connctx_update_ready(self);
        }
        priv->removed = FALSE;
        ofono_connctx_update_ready(self);
    }
}

static
void
ofono_connctx_removed(
    OrgOfonoConnectionManager* proxy,
    const char* path,
    OfonoConnCtx* self)
{
    if (!g_strcmp0(path, ofono_connctx_path(self))) {
        OfonoConnCtxPriv* priv = self->priv;
        GVERBOSE_("%s removed", path);
        priv->removed = TRUE;
        ofono_connctx_update_ready(self);
    }
}

static
void
ofono_connctx_connection_manager_proxy_created(
    GObject* source,
    GAsyncResult* res,
    gpointer data)
{
    OfonoConnCtx* self = OFONO_CONNCTX(data);
    OfonoConnCtxPriv* priv = self->priv;
    GError* err = NULL;
    priv->connmgr = org_ofono_connection_manager_proxy_new_finish(res, &err);
    if (priv->connmgr) {
        priv->connmgr_event_id[CONNMGR_EVENT_CONTEXT_ADDED] =
            g_signal_connect(priv->connmgr, "context-added",
            G_CALLBACK(ofono_connctx_added), self);
        priv->connmgr_event_id[CONNMGR_EVENT_CONTEXT_REMOVED] =
            g_signal_connect(priv->connmgr, "context-removed",
            G_CALLBACK(ofono_connctx_removed), self);
        ofono_connctx_update_ready(self);
    } else {
        GERR("%s", GERRMSG(err));
    }
    if (err) g_error_free(err);
    ofono_connctx_unref(self);
}

static
void
ofono_connctx_destroyed(
    gpointer key,
    GObject* ctx)
{
    GASSERT(ofono_connctx_table);
    GVERBOSE_("%s", (char*)key);
    if (ofono_connctx_table) {
        GASSERT(g_hash_table_lookup(ofono_connctx_table,key) == ctx);
        g_hash_table_remove(ofono_connctx_table, key);
        if (g_hash_table_size(ofono_connctx_table) == 0) {
            g_hash_table_unref(ofono_connctx_table);
            ofono_connctx_table = NULL;
        }
    }
}

static
OfonoConnCtx*
ofono_connctx_create(
    const char* path)
{
    OfonoConnCtx* self = g_object_new(OFONO_TYPE_CONNCTX, NULL);
    OfonoObject* object = ofono_connctx_object(self);
    const char* sep = NULL;
    ofono_object_initialize(object, OFONO_CONNCTX_INTERFACE_NAME, path);
    if (path && path[0] == '/') sep = strchr(path + 1, '/');
    if (sep) {
        OfonoConnCtxPriv* priv = self->priv;
        char* modem_path = strndup(path, sep - path);

        /* Initialize modem */
        priv->modem = ofono_modem_new(modem_path);
        priv->modem_event_id[MODEM_EVENT_VALID] =
            ofono_modem_add_valid_changed_handler(priv->modem,
                ofono_connctx_modem_changed, self);
        priv->modem_event_id[MODEM_EVENT_INTERFACES] =
            ofono_modem_add_interfaces_changed_handler(priv->modem,
                ofono_connctx_modem_changed, self);

        /* Initialize ConnectionManager proxy */
        org_ofono_connection_manager_proxy_new(ofono_object_bus(object),
            G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES, OFONO_SERVICE,
            modem_path, NULL, ofono_connctx_connection_manager_proxy_created,
            ofono_connctx_ref(self));

        g_free(modem_path);
    }
    ofono_connctx_update_ready(self);
    return self;
}

/*==========================================================================*
 * API
 *==========================================================================*/

OfonoConnCtx*
ofono_connctx_new(
    const char* path)
{
    OfonoConnCtx* context = NULL;
    if (path && ofono_connctx_table) {
        context = ofono_connctx_ref(g_hash_table_lookup(
            ofono_connctx_table, path));
    }
    if (!context && path) {
        char* key = g_strdup(path);
        context = ofono_connctx_create(path);
        if (!ofono_connctx_table) {
            ofono_connctx_table = g_hash_table_new_full(g_str_hash,
                g_str_equal, g_free, NULL);
        }
        g_hash_table_replace(ofono_connctx_table, key, context);
        g_object_weak_ref(G_OBJECT(context), ofono_connctx_destroyed, key);
    }
    return context;
}

OfonoConnCtx*
ofono_connctx_ref(
    OfonoConnCtx* self)
{
    if (G_LIKELY(self)) {
        g_object_ref(OFONO_CONNCTX(self));
        return self;
    } else {
        return NULL;
    }
}

void
ofono_connctx_unref(
    OfonoConnCtx* self)
{
    if (G_LIKELY(self)) {
        g_object_unref(OFONO_CONNCTX(self));
    }
}

const char*
ofono_connctx_type_string(
    OFONO_CONNCTX_TYPE type)
{
    return ofono_int_to_name(&ofono_connctx_type_map, type);
}

const char*
ofono_connctx_protocol_string(
    OFONO_CONNCTX_PROTOCOL protocol)

{
    return ofono_int_to_name(&ofono_connctx_protocol_map, protocol);
}

const char*
ofono_connctx_auth_string(
    OFONO_CONNCTX_AUTH auth)
{
    return ofono_int_to_name(&ofono_connctx_auth_map, auth);
}

const char*
ofono_connctx_method_string(
    OFONO_CONNCTX_METHOD method)
{
    return ofono_int_to_name(&ofono_connctx_method_map, method);
}

gulong
ofono_connctx_add_valid_changed_handler(
    OfonoConnCtx* self,
    OfonoConnCtxHandler fn,
    void* arg)
{
    return G_LIKELY(self) ? ofono_object_add_valid_changed_handler(
        &self->object, (OfonoObjectHandler)fn, arg) : 0;
}

gulong
ofono_connctx_add_property_changed_handler(
    OfonoConnCtx* self,
    OfonoConnCtxPropertyHandler fn,
    const char* name,
    void* arg)
{
    return G_LIKELY(self) ? ofono_object_add_property_changed_handler(
        &self->object, (OfonoObjectPropertyHandler)fn, name, arg) :  0;
}

gulong
ofono_connctx_add_name_changed_handler(
    OfonoConnCtx* self,
    OfonoConnCtxHandler fn,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(fn)) ? g_signal_connect(self,
        CONNCTX_SIGNAL_NAME_CHANGED_NAME, G_CALLBACK(fn), arg) : 0;
}

gulong
ofono_connctx_add_apn_changed_handler(
    OfonoConnCtx* self,
    OfonoConnCtxHandler fn,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(fn)) ? g_signal_connect(self,
        CONNCTX_SIGNAL_APN_CHANGED_NAME, G_CALLBACK(fn), arg) : 0;
}

gulong
ofono_connctx_add_type_changed_handler(
    OfonoConnCtx* self,
    OfonoConnCtxHandler fn,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(fn)) ? g_signal_connect(self,
        CONNCTX_SIGNAL_TYPE_CHANGED_NAME, G_CALLBACK(fn), arg) : 0;
}

gulong
ofono_connctx_add_mms_proxy_changed_handler(
    OfonoConnCtx* self,
    OfonoConnCtxHandler fn,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(fn)) ? g_signal_connect(self,
        CONNCTX_SIGNAL_MMS_PROXY_CHANGED_NAME, G_CALLBACK(fn), arg) : 0;
}

gulong
ofono_connctx_add_mms_center_changed_handler(
    OfonoConnCtx* self,
    OfonoConnCtxHandler fn,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(fn)) ? g_signal_connect(self,
        CONNCTX_SIGNAL_MMS_CENTER_CHANGED_NAME, G_CALLBACK(fn), arg) : 0;
}

gulong
ofono_connctx_add_interface_changed_handler(
    OfonoConnCtx* self,
    OfonoConnCtxHandler fn,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(fn)) ? g_signal_connect(self,
        CONNCTX_SIGNAL_INTERFACE_CHANGED_NAME, G_CALLBACK(fn), arg) : 0;
}

gulong
ofono_connctx_add_settings_changed_handler(
    OfonoConnCtx* self,
    OfonoConnCtxHandler fn,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(fn)) ? g_signal_connect(self,
        CONNCTX_SIGNAL_SETTINGS_CHANGED_NAME, G_CALLBACK(fn), arg) : 0;
}

gulong
ofono_connctx_add_ipv6_settings_changed_handler(
    OfonoConnCtx* self,
    OfonoConnCtxHandler fn,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(fn)) ? g_signal_connect(self,
        CONNCTX_SIGNAL_IPV6_SETTINGS_CHANGED_NAME, G_CALLBACK(fn), arg) : 0;
}

gulong
ofono_connctx_add_active_changed_handler(
    OfonoConnCtx* self,
    OfonoConnCtxHandler fn,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(fn)) ? g_signal_connect(self,
        CONNCTX_SIGNAL_ACTIVE_CHANGED_NAME, G_CALLBACK(fn), arg) : 0;
}

gulong
ofono_connctx_add_activate_failed_handler(
    OfonoConnCtx* self,
    OfonoConnCtxErrorHandler fn,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(fn)) ? g_signal_connect(self,
        CONNCTX_SIGNAL_ACTIVATE_FAILED_NAME, G_CALLBACK(fn), arg) : 0;
}

void
ofono_connctx_remove_handler(
    OfonoConnCtx* self,
    gulong id)
{
    if (G_LIKELY(self) && G_LIKELY(id)) {
        g_signal_handler_disconnect(self, id);
    }
}

static
void
ofono_connctx_perform_action(
    OfonoConnCtx* self,
    CONNCTX_ACTION action)
{
    if (G_LIKELY(self)) {
        OfonoConnCtxPriv* priv = self->priv;
        if (priv->current_action != action) {
            priv->next_action = action;
            ofono_connctx_perform_next_action(self);
        } else {
            /* It's already being performed */
            priv->next_action = CONNCTX_ACTION_NONE;
        }
    }
}

void
ofono_connctx_activate(
    OfonoConnCtx* self)
{
    ofono_connctx_perform_action(self, CONNCTX_ACTION_ACTIVATE);
}

void
ofono_connctx_deactivate(
    OfonoConnCtx* self)
{
    ofono_connctx_perform_action(self, CONNCTX_ACTION_DEACTIVATE);
}

static
void
ofono_connctx_provision_finished(
    OFONO_OBJECT_PROXY* proxy,
    GAsyncResult* result,
    const OfonoObjectPendingCall* call)
{
    GError* error = NULL;
    if (!org_ofono_connection_context_call_provision_context_finish(proxy,
        result, &error)) {
        GERR("%s", GERRMSG(error));
    }
    if (call->callback) {
        ((OfonoConnCtxCallHandler)call->callback)(
             OFONO_CONNCTX(call->object), error, call->arg);
    }
    if (error) g_error_free(error);
}

GCancellable*
ofono_connctx_set_string_full(
    OfonoConnCtx* self,
    const char* name,
    const char* value,
    OfonoConnCtxCallHandler callback,
    void* arg)
{
    return ofono_object_set_string(ofono_connctx_object(self), name, value,
        (OfonoObjectCallFinishedCallback)callback, arg);
}

GCancellable*
ofono_connctx_set_type_full(
    OfonoConnCtx* self,
    OFONO_CONNCTX_TYPE type,
    OfonoConnCtxCallHandler callback,
    void* arg)
{
    return ofono_connctx_set_string_full(self, OFONO_CONNCTX_PROPERTY_TYPE,
        ofono_int_to_name(&ofono_connctx_type_map, type), callback, arg);
}

GCancellable*
ofono_connctx_set_protocol_full(
    OfonoConnCtx* self,
    OFONO_CONNCTX_PROTOCOL prot,
    OfonoConnCtxCallHandler callback,
    void* arg)
{
    return ofono_connctx_set_string_full(self, OFONO_CONNCTX_PROPERTY_PROTOCOL,
        ofono_int_to_name(&ofono_connctx_protocol_map, prot), callback, arg);
}

GCancellable*
ofono_connctx_set_auth_full(
    OfonoConnCtx* self,
    OFONO_CONNCTX_AUTH auth,
    OfonoConnCtxCallHandler callback,
    void* arg)
{
    return ofono_connctx_set_string_full(self, OFONO_CONNCTX_PROPERTY_AUTH,
        ofono_int_to_name(&ofono_connctx_auth_map, auth), callback, arg);
}

GCancellable*
ofono_connctx_provision_full(
    OfonoConnCtx* self,
    OfonoConnCtxCallHandler fn,
    void* arg)
{
    if (G_LIKELY(self)) {
        OfonoObject* object = &self->object;
        OFONO_OBJECT_PROXY* proxy = ofono_object_proxy(object);
        if (G_LIKELY(proxy)) {
            OfonoObjectPendingCall* call = ofono_object_pending_call_new(object,
                ofono_connctx_provision_finished,
                (OfonoObjectCallFinishedCallback)fn, arg);
            org_ofono_connection_context_call_provision_context(proxy,
                call->cancellable, ofono_object_pending_call_finished, call);
            return call->cancellable;
        }
    }
    return NULL;
}

gboolean
ofono_connctx_set_string(
    OfonoConnCtx* self,
    const char* name,
    const char* value)
{
    return ofono_connctx_set_string_full(self, name, value, NULL, NULL) != NULL;
}

gboolean
ofono_connctx_set_type(
    OfonoConnCtx* self,
    OFONO_CONNCTX_TYPE type)
{
    return ofono_connctx_set_type_full(self, type, NULL, NULL) != NULL;
}

gboolean
ofono_connctx_set_protocol(
    OfonoConnCtx* self,
    OFONO_CONNCTX_PROTOCOL protocol)
{
    return ofono_connctx_set_protocol_full(self, protocol, NULL, NULL) != NULL;
}

gboolean
ofono_connctx_set_auth(
    OfonoConnCtx* self,
    OFONO_CONNCTX_AUTH auth)
{
    return ofono_connctx_set_auth_full(self, auth, NULL, NULL) != NULL;
}

gboolean
ofono_connctx_provision(
    OfonoConnCtx* self)
{
    return ofono_connctx_provision_full(self, NULL, NULL) != NULL;
}

/*==========================================================================*
 * Properties
 *==========================================================================*/

#define CONNCTX_SETTINGS_PUB(obj,prop) \
    G_STRUCT_MEMBER(OfonoConnCtxSettings*, obj, (prop)->off_pub)

#define CONNCTX_SETTINGS_PRIV_P(priv,prop) \
    ((OfonoConnCtxSettingsPriv*) G_STRUCT_MEMBER_P(priv, (prop)->off_priv))

#define CONNCTX_DEFINE_PROPERTY_STRING(NAME,var) \
    OFONO_OBJECT_DEFINE_PROPERTY_STRING(CONNCTX,connctx,NAME,OfonoConnCtx,var)

#define CONNCTX_DEFINE_PROPERTY_ENUM(NAME,var) \
    OFONO_OBJECT_DEFINE_PROPERTY_ENUM(CONNCTX,NAME,OfonoConnCtx,var, \
    &ofono_connctx_##var##_map)

G_STATIC_ASSERT(sizeof(OFONO_CONNCTX_TYPE) == sizeof(int));
G_STATIC_ASSERT(sizeof(OFONO_CONNCTX_AUTH) == sizeof(int));
G_STATIC_ASSERT(sizeof(OFONO_CONNCTX_PROTOCOL) == sizeof(int));

static
void*
ofono_connctx_property_priv(
    OfonoObject* object,
    const OfonoObjectProperty* prop)
{
    return OFONO_CONNCTX(object)->priv;
}

#if GUTIL_LOG_DEBUG
static
void
ofono_connctx_property_active_apply(
    OfonoObject* object,
    const OfonoObjectProperty* prop,
    GVariant* value)
{
    GDEBUG("Context %s is %sactive", object->path,
        g_variant_get_boolean(value) ? "" : "not ");
    ofono_object_property_boolean_apply(object, prop, value);
}
#else
#  define ofono_connctx_property_active_apply \
          ofono_object_property_boolean_apply
#endif

static
void
ofono_connctx_property_settings_reset(
    OfonoObject* object,
    const OfonoObjectProperty* prop)
{
    OfonoConnCtx* self = OFONO_CONNCTX(object);
    OfonoConnCtxPriv* priv = self->priv;
    OfonoConnCtxSettingsPriv* settings = CONNCTX_SETTINGS_PRIV_P(priv,prop);
    gboolean ifname_changed = FALSE;
    if (self->ifname == settings->ifname) {
        self->ifname = (settings == &priv->settings) ? 
            priv->ipv6_settings.ifname : priv->settings.ifname;
        if (g_strcmp0(self->ifname, settings->ifname)) {
            GDEBUG("Interface: %s", self->ifname ? self->ifname : "<none>");
            ifname_changed = TRUE;
        }
    }
    ofono_connctx_settings_clear(CONNCTX_SETTINGS_PRIV_P(self->priv,prop));
    CONNCTX_SETTINGS_PUB(self,prop) = NULL;
    if (ifname_changed) {
        CONNCTX_SIGNAL_EMIT(self, INTERFACE);
    }
}

static
void
ofono_connctx_property_settings_apply(
    OfonoObject* object,
    const OfonoObjectProperty* prop,
    GVariant* value)
{
    OfonoConnCtx* self = OFONO_CONNCTX(object);
    OfonoConnCtxPriv* priv = self->priv;
    gboolean changed = FALSE;
    OfonoConnCtxSettingsPriv decoded;
    const gboolean empty = !ofono_connctx_settings_decode(&decoded, value);
    OfonoConnCtxSettingsPriv* settings = CONNCTX_SETTINGS_PRIV_P(priv,prop);
    OfonoConnCtxSettingsPriv old = *settings;
    const char* ifname;

    *settings = decoded;

    if (empty) {
        if (CONNCTX_SETTINGS_PUB(self,prop)) {
            CONNCTX_SETTINGS_PUB(self,prop) = NULL;
            changed = TRUE;
        }
    } else if (!CONNCTX_SETTINGS_PUB(self,prop)) {
        CONNCTX_SETTINGS_PUB(self,prop) = &settings->pub;
        changed = TRUE;
    }

    /* Need to signal these properties individually? */
    if (g_strcmp0(old.ifname, decoded.ifname)) {
        changed = TRUE;
        if (decoded.ifname) {
            GDEBUG("%s.%s: %s", prop->name, OFONO_CONNCTX_SETTINGS_INTERFACE,
                decoded.ifname);
        }
    }

    if (old.pub.method != decoded.pub.method) {
        changed = TRUE;
        if (decoded.pub.method != OFONO_CONNCTX_METHOD_UNKNOWN) {
            GDEBUG("%s.%s: %s", prop->name, OFONO_CONNCTX_SETTINGS_METHOD,
                ofono_connctx_method_string(decoded.pub.method));
        }
    }

    if (g_strcmp0(old.address, decoded.address)) {
        changed = TRUE;
        if (decoded.address) {
            GDEBUG("%s.%s: %s", prop->name, OFONO_CONNCTX_SETTINGS_ADDRESS,
                decoded.address);
        }
    }

    if (g_strcmp0(old.netmask, decoded.netmask)) {
        changed = TRUE;
        if (decoded.netmask) {
            GDEBUG("%s.%s: %s", prop->name, OFONO_CONNCTX_SETTINGS_NETMASK,
                decoded.netmask);
        }
    }

    if (g_strcmp0(old.gateway, decoded.gateway)) {
        changed = TRUE;
        if (decoded.gateway) {
            GDEBUG("%s.%s: %s", prop->name, OFONO_CONNCTX_SETTINGS_GATEWAY,
                decoded.gateway);
        }
    }

    if (old.pub.prefix != decoded.pub.prefix) {
        changed = TRUE;
        GDEBUG("%s.%s: %u", prop->name, OFONO_CONNCTX_SETTINGS_PREFIX_LENGTH,
            decoded.pub.prefix);
    }

    if (!gutil_strv_equal(old.dns, decoded.dns)) {
        changed = TRUE;
        if (GLOG_ENABLED(GLOG_LEVEL_DEBUG) && decoded.dns) {
            char* dns = g_strjoinv(" ", decoded.dns);
            GDEBUG("%s.%s: %s", prop->name, OFONO_CONNCTX_SETTINGS_DNS, dns);
            g_free(dns);
        }
    }

    ifname = priv->settings.ifname ? priv->settings.ifname :
        priv->ipv6_settings.ifname;
    if (g_strcmp0(self->ifname, ifname)) {
        GDEBUG("Interface: %s", ifname ? ifname : "<none>");
        self->ifname = ifname;
        CONNCTX_SIGNAL_EMIT(self, INTERFACE);
    } else {
        /*
         * In any case update the public pointer because it may point to the
         * string which ofono_connctx_settings_clear is about to deallocate
         */
        self->ifname = ifname;
    }

    ofono_connctx_settings_clear(&old);
    if (changed) {
        ofono_object_emit_property_changed_signal(object, prop);
    }
}

/*==========================================================================*
 * Internals
 *==========================================================================*/

static
gboolean
ofono_connctx_is_present(
    OfonoConnCtx* self)
{
    OfonoConnCtxPriv* priv = self->priv;
    return priv->connmgr && ofono_modem_valid(priv->modem) && !priv->removed &&
        ofono_modem_has_interface(priv->modem, OFONO_CONNMGR_INTERFACE_NAME);
}

static
gboolean
ofono_connctx_is_ready(
    OfonoObject* object)
{
    return ofono_connctx_is_present(OFONO_CONNCTX(object)) &&
        OFONO_OBJECT_CLASS(SUPER_CLASS)->fn_is_ready(object);
}

static
gboolean
ofono_connctx_is_valid(
    OfonoObject* object)
{
    return ofono_connctx_is_present(OFONO_CONNCTX(object)) &&
        OFONO_OBJECT_CLASS(SUPER_CLASS)->fn_is_valid(object);
}

static
void
ofono_connctx_valid_changed(
    OfonoObject* object)
{
    OfonoConnCtx* self = OFONO_CONNCTX(object);
    if (object->valid) ofono_connctx_perform_next_action(self);
    OFONO_OBJECT_CLASS(SUPER_CLASS)->fn_valid_changed(object);
}

/**
 * Per instance initializer
 */
static
void
ofono_connctx_init(
    OfonoConnCtx* self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
        OFONO_TYPE_CONNCTX, OfonoConnCtxPriv);
    ofono_connctx_settings_init(&self->priv->settings);
    ofono_connctx_settings_init(&self->priv->ipv6_settings);
}

/**
 * First stage of deinitialization (release all references).
 * May be called more than once in the lifetime of the object.
 */
static
void
ofono_connctx_dispose(
    GObject* object)
{
    OfonoConnCtx* self = OFONO_CONNCTX(object);
    OfonoConnCtxPriv* priv = self->priv;
    /* OfonoObjectPendingCall maintains a reference to the ofono object while
     * the call is pending (i.e. while current_action is being performed),
     * meaning that we can't get here until the current action is finished. */
    GASSERT(priv->current_action == CONNCTX_ACTION_NONE);
    priv->next_action = CONNCTX_ACTION_NONE;
    if (priv->retry_id) {
        g_source_remove(priv->retry_id);
        priv->retry_id = 0;
    }
    gutil_disconnect_handlers(priv->connmgr, priv->connmgr_event_id,
        G_N_ELEMENTS(priv->connmgr_event_id));
    ofono_modem_remove_handlers(priv->modem, priv->modem_event_id,
        G_N_ELEMENTS(priv->modem_event_id));
    G_OBJECT_CLASS(SUPER_CLASS)->dispose(object);
}

/**
 * Final stage of deinitialization
 */
static
void
ofono_connctx_finalize(
    GObject* object)
{
    OfonoConnCtx* self = OFONO_CONNCTX(object);
    OfonoConnCtxPriv* priv = self->priv;
    g_free(priv->apn);
    g_free(priv->name);
    g_free(priv->username);
    g_free(priv->password);
    g_free(priv->mms_proxy);
    g_free(priv->mms_center);
    ofono_modem_unref(priv->modem);
    ofono_connctx_settings_clear(&priv->settings);
    ofono_connctx_settings_clear(&priv->ipv6_settings);
    if (priv->connmgr) g_object_unref(priv->connmgr);
    G_OBJECT_CLASS(SUPER_CLASS)->finalize(object);
}

/**
 * Per class initializer
 */
static
void
ofono_connctx_class_init(
    OfonoConnCtxClass* klass)
{
    static OfonoObjectProperty ofono_connctx_properties[] = {
        CONNCTX_DEFINE_PROPERTY_ENUM(TYPE,type),
        CONNCTX_DEFINE_PROPERTY_ENUM(AUTH,auth),
        CONNCTX_DEFINE_PROPERTY_ENUM(PROTOCOL,protocol),
        CONNCTX_DEFINE_PROPERTY_STRING(APN,apn),
        CONNCTX_DEFINE_PROPERTY_STRING(NAME,name),
        CONNCTX_DEFINE_PROPERTY_STRING(USERNAME,username),
        CONNCTX_DEFINE_PROPERTY_STRING(PASSWORD,password),
        CONNCTX_DEFINE_PROPERTY_STRING(MMS_PROXY,mms_proxy),
        CONNCTX_DEFINE_PROPERTY_STRING(MMS_CENTER,mms_center),
        {
            OFONO_CONNCTX_PROPERTY_ACTIVE,
            CONNCTX_SIGNAL_ACTIVE_CHANGED_NAME, 0,
            ofono_connctx_property_priv,
            ofono_object_property_boolean_reset,
            ofono_connctx_property_active_apply,
            G_STRUCT_OFFSET(OfonoConnCtx,active),
            OFONO_OBJECT_OFFSET_NONE
        },{
            OFONO_CONNCTX_PROPERTY_SETTINGS,
            CONNCTX_SIGNAL_SETTINGS_CHANGED_NAME, 0,
            ofono_connctx_property_priv,
            ofono_connctx_property_settings_reset,
            ofono_connctx_property_settings_apply,
            G_STRUCT_OFFSET(OfonoConnCtx,settings),
            G_STRUCT_OFFSET(OfonoConnCtxPriv,settings)
        },{
            OFONO_CONNCTX_PROPERTY_IPV6_SETTINGS,
            CONNCTX_SIGNAL_IPV6_SETTINGS_CHANGED_NAME, 0,
            ofono_connctx_property_priv,
            ofono_connctx_property_settings_reset,
            ofono_connctx_property_settings_apply,
            G_STRUCT_OFFSET(OfonoConnCtx,ipv6_settings),
            G_STRUCT_OFFSET(OfonoConnCtxPriv,ipv6_settings)
        }
    };

    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    klass->properties = ofono_connctx_properties;
    klass->nproperties = G_N_ELEMENTS(ofono_connctx_properties);
    klass->fn_is_ready = ofono_connctx_is_ready;
    klass->fn_is_valid = ofono_connctx_is_valid;
    klass->fn_valid_changed = ofono_connctx_valid_changed;
    object_class->dispose = ofono_connctx_dispose;
    object_class->finalize = ofono_connctx_finalize;
    g_type_class_add_private(klass, sizeof(OfonoConnCtxPriv));
    OFONO_OBJECT_CLASS_SET_PROXY_CALLBACKS(klass, org_ofono_connection_context);
    ofono_connctx_signals[CONNCTX_SIGNAL_ACTIVATE_FAILED] =
        g_signal_new(CONNCTX_SIGNAL_ACTIVATE_FAILED_NAME,
        G_OBJECT_CLASS_TYPE(klass), G_SIGNAL_RUN_FIRST,
        0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_ERROR);
    CONNCTX_SIGNAL_NEW(klass, INTERFACE);
    ofono_class_initialize(klass);
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
