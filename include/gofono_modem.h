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

#ifndef GOFONO_MODEM_H
#define GOFONO_MODEM_H

#include "gofono_object.h"

G_BEGIN_DECLS

typedef struct ofono_modem_priv OfonoModemPriv;

struct ofono_modem {
    OfonoObject object;
    OfonoModemPriv* priv;
    gboolean powered;                   /* Powered */
    gboolean online;                    /* Online */
    gboolean lockdown;                  /* Lockdown */
    gboolean emergency;                 /* Emergency */
    const char* name;                   /* Name */
    const char* manufacturer;           /* Manufacturer */
    const char* model;                  /* Model */
    const char* revision;               /* Revision */
    const char* serial;                 /* Serial */
    GPtrArray* features;                /* Features */
    GPtrArray* interfaces;              /* Interfaces */
    const char* type;                   /* Type */
};

GType ofono_modem_get_type(void);
#define OFONO_TYPE_MODEM (ofono_modem_get_type())
#define OFONO_MODEM(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
        OFONO_TYPE_MODEM, OfonoModem))

typedef
void
(*OfonoModemHandler)(
    OfonoModem* sender,
    void* arg);

typedef
void
(*OfonoModemCallHandler)(
    OfonoModem* sender,
    const GError* error,
    void* arg);

typedef
void
(*OfonoModemPropertyHandler)(
    OfonoModem* sender,
    const char* name,
    GVariant* value,
    void* arg);

OfonoModem*
ofono_modem_new(
    const char* path);

OfonoModem*
ofono_modem_ref(
    OfonoModem* modem);

void
ofono_modem_unref(
    OfonoModem* modem);

gboolean
ofono_modem_equal(
    OfonoModem* modem1,
    OfonoModem* modem2);

gboolean
ofono_modem_has_interface(
    OfonoModem* modem,
    const char* intf);

gulong
ofono_modem_add_valid_changed_handler(
    OfonoModem* modem,
    OfonoModemHandler handler,
    void* arg);

/* Properties */

gulong
ofono_modem_add_property_changed_handler(
    OfonoModem* modem,
    OfonoModemPropertyHandler handler,
    const char* name,
    void* arg);

gulong
ofono_modem_add_powered_changed_handler(
    OfonoModem* modem,
    OfonoModemHandler handler,
    void* arg);

gulong
ofono_modem_add_online_changed_handler(
    OfonoModem* modem,
    OfonoModemHandler handler,
    void* arg);

gulong
ofono_modem_add_lockdown_changed_handler(
    OfonoModem* modem,
    OfonoModemHandler handler,
    void* arg);

gulong
ofono_modem_add_emergency_changed_handler(
    OfonoModem* modem,
    OfonoModemHandler handler,
    void* arg);

gulong
ofono_modem_add_interfaces_changed_handler(
    OfonoModem* modem,
    OfonoModemHandler handler,
    void* arg);

void
ofono_modem_remove_handler(
    OfonoModem* modem,
    gulong id);

gboolean
ofono_modem_set_powered(
    OfonoModem* modem,
    gboolean powered);

gboolean
ofono_modem_set_online(
    OfonoModem* modem,
    gboolean online);

GCancellable*
ofono_modem_set_powered_full(
    OfonoModem* modem,
    gboolean powered,
    OfonoModemCallHandler callback,
    void* arg);

GCancellable*
ofono_modem_set_online_full(
    OfonoModem* modem,
    gboolean online,
    OfonoModemCallHandler callback,
    void* arg);

/* Inline wrappers */

OFONO_INLINE OfonoObject*
ofono_modem_object(OfonoModem* modem)
    { return G_LIKELY(modem) ? &modem->object : NULL; }

OFONO_INLINE const char*
ofono_modem_path(OfonoModem* modem)
    { return G_LIKELY(modem) ? modem->object.path : NULL; }

OFONO_INLINE gboolean
ofono_modem_valid(OfonoModem* modem)
    { return G_LIKELY(modem) && modem->object.valid; }

OFONO_INLINE gboolean
ofono_modem_wait_valid(OfonoModem* modem, int msec, GError** error)
    { return ofono_object_wait_valid(ofono_modem_object(modem), msec, error); }

OFONO_INLINE void
ofono_modem_remove_handlers(OfonoModem* modem, gulong* ids, guint n)
    { ofono_object_remove_handlers(ofono_modem_object(modem), ids, n); }

G_END_DECLS

#endif /* GOFONO_MODEM_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
