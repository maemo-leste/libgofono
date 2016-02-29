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

#ifndef GOFONO_NETREG_H
#define GOFONO_NETREG_H

#include "gofono_modemintf.h"

G_BEGIN_DECLS

typedef struct ofono_netreg_priv OfonoNetRegPriv;

typedef enum ofono_netreg_status {
    OFONO_NETREG_STATUS_UNKNOWN = -1,
    OFONO_NETREG_STATUS_NONE,
    OFONO_NETREG_STATUS_UNREGISTERED,   /* unregistered */
    OFONO_NETREG_STATUS_REGISTERED,     /* registered */
    OFONO_NETREG_STATUS_SEARCHING,      /* searching */
    OFONO_NETREG_STATUS_DENIED,         /* denied */
    OFONO_NETREG_STATUS_ROAMING         /* roaming */
} OFONO_NETREG_STATUS;

typedef enum ofono_netreg_mode {
    OFONO_NETREG_MODE_UNKNOWN = -1,
    OFONO_NETREG_MODE_NONE,
    OFONO_NETREG_MODE_AUTO,             /* auto */
    OFONO_NETREG_MODE_AUTO_ONLY,        /* auto-only */
    OFONO_NETREG_MODE_MANUAL            /* manual */
} OFONO_NETREG_MODE;

typedef enum ofono_netreg_tech {
    OFONO_NETREG_TECH_UNKNOWN = -1,
    OFONO_NETREG_TECH_NONE,
    OFONO_NETREG_TECH_GSM,              /* gsm */
    OFONO_NETREG_TECH_EDGE,             /* edge */
    OFONO_NETREG_TECH_UMTS,             /* umts */
    OFONO_NETREG_TECH_HSPA,             /* hspa */
    OFONO_NETREG_TECH_LTE,              /* lte */
} OFONO_NETREG_TECH;

struct ofono_netreg {
    OfonoModemInterface intf;
    OfonoNetRegPriv* priv;
    OFONO_NETREG_STATUS status;         /* Status */
    OFONO_NETREG_MODE mode;             /* Mode */
    OFONO_NETREG_TECH tech;             /* Technology */
    const char* mcc;                    /* MobileCountryCode */
    const char* mnc;                    /* MobileNetworkCode */
    const char* name;                   /* Name */
    guint cell;                         /* CellId */
    guint areacode;                     /* LocationAreaCode */
    guint strength;                     /* Strength */
};

GType ofono_netreg_get_type(void);
#define OFONO_TYPE_NETREG (ofono_netreg_get_type())
#define OFONO_NETREG(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
        OFONO_TYPE_NETREG, OfonoNetReg))

typedef
void
(*OfonoNetRegHandler)(
    OfonoNetReg* sender,
    void* arg);

typedef
void
(*OfonoNetRegPropertyHandler)(
    OfonoNetReg* sender,
    const char* name,
    GVariant* value,
    void* arg);

OfonoNetReg*
ofono_netreg_new(
    const char* path);

OfonoNetReg*
ofono_netreg_ref(
    OfonoNetReg* netreg);

void
ofono_netreg_unref(
    OfonoNetReg* netreg);

const char*
ofono_netreg_country(
    OfonoNetReg* netreg);

const char*
ofono_netreg_status_string(
    OFONO_NETREG_STATUS status);

const char*
ofono_netreg_mode_string(
    OFONO_NETREG_MODE mode);

const char*
ofono_netreg_tech_string(
    OFONO_NETREG_TECH tech);

gulong
ofono_netreg_add_valid_changed_handler(
    OfonoNetReg* netreg,
    OfonoNetRegHandler handler,
    void* arg);

/* Properties */

gulong
ofono_netreg_add_property_changed_handler(
    OfonoNetReg* netreg,
    OfonoNetRegPropertyHandler handler,
    const char* name,
    void* arg);

gulong
ofono_netreg_add_status_changed_handler(
    OfonoNetReg* netreg,
    OfonoNetRegHandler handler,
    void* arg);

gulong
ofono_netreg_add_mode_changed_handler(
    OfonoNetReg* netreg,
    OfonoNetRegHandler handler,
    void* arg);

gulong
ofono_netreg_add_tech_changed_handler(
    OfonoNetReg* netreg,
    OfonoNetRegHandler handler,
    void* arg);

gulong
ofono_netreg_add_mcc_changed_handler(
    OfonoNetReg* netreg,
    OfonoNetRegHandler handler,
    void* arg);

gulong
ofono_netreg_add_mnc_changed_handler(
    OfonoNetReg* netreg,
    OfonoNetRegHandler handler,
    void* arg);

gulong
ofono_netreg_add_name_changed_handler(
    OfonoNetReg* netreg,
    OfonoNetRegHandler handler,
    void* arg);

gulong
ofono_netreg_add_cell_changed_handler(
    OfonoNetReg* netreg,
    OfonoNetRegHandler handler,
    void* arg);

gulong
ofono_netreg_add_areacode_changed_handler(
    OfonoNetReg* netreg,
    OfonoNetRegHandler handler,
    void* arg);

gulong
ofono_netreg_add_strength_changed_handler(
    OfonoNetReg* netreg,
    OfonoNetRegHandler handler,
    void* arg);

void
ofono_netreg_remove_handler(
    OfonoNetReg* netreg,
    gulong id);

/* Inline wrappers */

OFONO_INLINE OfonoObject*
ofono_netreg_object(OfonoNetReg* netreg)
    { return &netreg->intf.object; }

OFONO_INLINE const char*
ofono_netreg_path(OfonoNetReg* netreg)
    { return G_LIKELY(netreg) ? netreg->intf.object.path : NULL; }

OFONO_INLINE gboolean
ofono_netreg_valid(OfonoNetReg* netreg)
    { return G_LIKELY(netreg) && netreg->intf.object.valid; }

OFONO_INLINE void
ofono_netreg_remove_handlers(OfonoNetReg* netreg, gulong* ids, guint n)
    { ofono_object_remove_handlers(ofono_netreg_object(netreg), ids, n); }

G_END_DECLS

#endif /* GOFONO_NETREG_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
