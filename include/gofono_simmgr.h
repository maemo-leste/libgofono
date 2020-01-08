/*
 * Copyright (C) 2014-2019 Jolla Ltd.
 * Copyright (C) 2014-2019 Slava Monich <slava.monich@jolla.com>
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
 *   3. Neither the names of the copyright holders nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
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

#ifndef GOFONO_SIMMGR_H
#define GOFONO_SIMMGR_H

#include "gofono_modemintf.h"

G_BEGIN_DECLS

typedef enum ofono_simmgr_pin {
    OFONO_SIMMGR_PIN_UNKNOWN = -1,
    OFONO_SIMMGR_PIN_NONE,              /* none */
    OFONO_SIMMGR_PIN_PIN,               /* pin */
    OFONO_SIMMGR_PIN_PHONE,             /* phone */
    OFONO_SIMMGR_PIN_FIRSTPHONE,        /* firstphone */
    OFONO_SIMMGR_PIN_PIN2,              /* pin2 */
    OFONO_SIMMGR_PIN_NETWORK,           /* network */
    OFONO_SIMMGR_PIN_NETSUB,            /* netsub */
    OFONO_SIMMGR_PIN_SERVICE,           /* service */
    OFONO_SIMMGR_PIN_CORP,              /* corp */
    OFONO_SIMMGR_PIN_PUK,               /* puk */
    OFONO_SIMMGR_PIN_FIRSTPHONEPUK,     /* firstphonepuk */
    OFONO_SIMMGR_PIN_PUK2,              /* puk2 */
    OFONO_SIMMGR_PIN_NETWORKPUK,        /* networkpuk */
    OFONO_SIMMGR_PIN_NETSUBPUK,         /* netsubpuk */
    OFONO_SIMMGR_PIN_SERVICEPUK,        /* servicepuk */
    OFONO_SIMMGR_PIN_CORPPUK            /* corppuk */
} OFONO_SIMMGR_PIN;

typedef struct ofono_simmgr_priv OfonoSimMgrPriv;

struct ofono_simmgr {
    OfonoModemInterface intf;
    OfonoSimMgrPriv* priv;
    gboolean present;                   /* Present */
    const char* imsi;                   /* SubscriberIdentity */
    const char* mcc;                    /* MobileCountryCode */
    const char* mnc;                    /* MobileNetworkCode */
    const char* spn;                    /* ServiceProviderName */
    /* Since 2.0.8 */
    OFONO_SIMMGR_PIN  pin_required;     /* PinRequired */
};

GType ofono_simmgr_get_type(void);
#define OFONO_TYPE_SIMMGR (ofono_simmgr_get_type())
#define OFONO_SIMMGR(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
        OFONO_TYPE_SIMMGR, OfonoSimMgr))

typedef
void
(*OfonoSimMgrHandler)(
    OfonoSimMgr* sender,
    void* arg);

typedef
void
(*OfonoSimMgrPropertyHandler)(
    OfonoSimMgr* sender,
    const char* name,
    GVariant* value,
    void* arg);

OfonoSimMgr*
ofono_simmgr_new(
    const char* path);

OfonoSimMgr*
ofono_simmgr_ref(
    OfonoSimMgr* sim);

void
ofono_simmgr_unref(
    OfonoSimMgr* sim);

/* Properties */

gulong
ofono_simmgr_add_property_changed_handler(
    OfonoSimMgr* sim,
    OfonoSimMgrPropertyHandler handler,
    const char* name,
    void* arg);

gulong
ofono_simmgr_add_valid_changed_handler(
    OfonoSimMgr* sim,
    OfonoSimMgrHandler handler,
    void* arg);

gulong
ofono_simmgr_add_imsi_changed_handler(
    OfonoSimMgr* sim,
    OfonoSimMgrHandler handler,
    void* arg);

gulong
ofono_simmgr_add_mcc_changed_handler(
    OfonoSimMgr* sim,
    OfonoSimMgrHandler handler,
    void* arg);

gulong
ofono_simmgr_add_mnc_changed_handler(
    OfonoSimMgr* sim,
    OfonoSimMgrHandler handler,
    void* arg);

gulong
ofono_simmgr_add_spn_changed_handler(
    OfonoSimMgr* sim,
    OfonoSimMgrHandler handler,
    void* arg);

gulong
ofono_simmgr_add_present_changed_handler(
    OfonoSimMgr* sim,
    OfonoSimMgrHandler handler,
    void* arg);

void
ofono_simmgr_remove_handler(
    OfonoSimMgr* sim,
    gulong id);

/* Inline wrappers */

OFONO_INLINE OfonoObject*
ofono_simmgr_object(OfonoSimMgr* sim)
    { return G_LIKELY(sim) ? &sim->intf.object : NULL; }

OFONO_INLINE const char*
ofono_simmgr_path(OfonoSimMgr* sim)
    { return G_LIKELY(sim) ? sim->intf.object.path : NULL; }

OFONO_INLINE gboolean
ofono_simmgr_valid(OfonoSimMgr* sim)
    { return G_LIKELY(sim) && sim->intf.object.valid; }

OFONO_INLINE gboolean
ofono_simmgr_wait_valid(OfonoSimMgr* sim, int msec, GError** error)
    { return ofono_object_wait_valid(ofono_simmgr_object(sim), msec, error); }

OFONO_INLINE void
ofono_simmgr_remove_handlers(OfonoSimMgr* sim, gulong* ids, guint n)
    { ofono_object_remove_handlers(ofono_simmgr_object(sim), ids, n); }

G_END_DECLS

#endif /* GOFONO_SIMMGR_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
