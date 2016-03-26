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

#ifndef GOFONO_CONNMGR_H
#define GOFONO_CONNMGR_H

#include "gofono_modemintf.h"

G_BEGIN_DECLS

typedef struct ofono_connmgr_priv OfonoConnMgrPriv;

struct ofono_connmgr {
    OfonoModemInterface intf;
    OfonoConnMgrPriv* priv;
    gboolean attached;                  /* Attached */
    gboolean roaming_allowed;           /* RoamingAllowed */
    gboolean powered;                   /* Powered */
};

GType ofono_connmgr_get_type(void);
#define OFONO_TYPE_CONNMGR (ofono_connmgr_get_type())
#define OFONO_CONNMGR(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
        OFONO_TYPE_CONNMGR, OfonoConnMgr))

typedef
void
(*OfonoConnMgrHandler)(
    OfonoConnMgr* sender,
    void* arg);

typedef
void
(*OfonoConnMgrContextAddedHandler)(
    OfonoConnMgr* sender,
    OfonoConnCtx* context,
    void* arg);

typedef
void
(*OfonoConnMgrContextRemovedHandler)(
    OfonoConnMgr* sender,
    const char* path,
    void* arg);

typedef
void
(*OfonoConnMgrPropertyHandler)(
    OfonoConnMgr* sender,
    const char* name,
    GVariant* value,
    void* arg);

OfonoConnMgr*
ofono_connmgr_new(
    const char* path);

OfonoConnMgr*
ofono_connmgr_ref(
    OfonoConnMgr* mgr);

void
ofono_connmgr_unref(
    OfonoConnMgr* mgr);

GPtrArray*
ofono_connmgr_get_contexts(
    OfonoConnMgr* mgr);

OfonoConnCtx*
ofono_connmgr_get_context_for_type(
    OfonoConnMgr* mgr,
    OFONO_CONNCTX_TYPE type);

OfonoConnCtx*
ofono_connmgr_get_context_for_path(
    OfonoConnMgr* mgr,
    const char* path);

gulong
ofono_connmgr_add_valid_changed_handler(
    OfonoConnMgr* mgr,
    OfonoConnMgrHandler handler,
    void* arg);

gulong
ofono_connmgr_add_context_added_handler(
    OfonoConnMgr* mgr,
    OfonoConnMgrContextAddedHandler handler,
    void* arg);

gulong
ofono_connmgr_add_context_removed_handler(
    OfonoConnMgr* mgr,
    OfonoConnMgrContextRemovedHandler handler,
    void* arg);

/* Properties */

gulong
ofono_connmgr_add_property_changed_handler(
    OfonoConnMgr* mgr,
    OfonoConnMgrPropertyHandler handler,
    const char* name,
    void* arg);

gulong
ofono_connmgr_add_attached_changed_handler(
    OfonoConnMgr* mgr,
    OfonoConnMgrHandler handler,
    void* arg);

gulong
ofono_connmgr_add_roaming_allowed_changed_handler(
    OfonoConnMgr* mgr,
    OfonoConnMgrHandler handler,
    void* arg);

gulong
ofono_connmgr_add_powered_changed_handler(
    OfonoConnMgr* mgr,
    OfonoConnMgrHandler handler,
    void* arg);

void
ofono_connmgr_remove_handler(
    OfonoConnMgr* mgr,
    gulong id);

/* Inline wrappers */

OFONO_INLINE OfonoObject*
ofono_connmgr_object(OfonoConnMgr* mgr)
    { return G_LIKELY(mgr) ? &mgr->intf.object : NULL; }

OFONO_INLINE const char*
ofono_connmgr_path(OfonoConnMgr* mgr)
    { return G_LIKELY(mgr) ? mgr->intf.object.path : NULL; }

OFONO_INLINE gboolean
ofono_connmgr_valid(OfonoConnMgr* mgr)
    { return G_LIKELY(mgr) && mgr->intf.object.valid; }

OFONO_INLINE gboolean
ofono_connmgr_wait_valid(OfonoConnMgr* mgr, int msec, GError** error)
    { return ofono_object_wait_valid(ofono_connmgr_object(mgr), msec, error); }

OFONO_INLINE void
ofono_connmgr_remove_handlers(OfonoConnMgr* mgr, gulong* ids, guint n)
    { ofono_object_remove_handlers(ofono_connmgr_object(mgr), ids, n); }

G_END_DECLS

#endif /* GOFONO_CONNMGR_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
