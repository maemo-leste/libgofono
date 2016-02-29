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

#include "gofono_netreg.h"
#include "gofono_modem_p.h"
#include "gofono_names.h"
#include "gofono_util.h"
#include "gofono_log.h"

/* Generated headers */
#define OFONO_OBJECT_PROXY OrgOfonoNetworkRegistration
#include "org.ofono.NetworkRegistration.h"
#include "gofono_modemintf_p.h"

/* Object definition */
struct ofono_netreg_priv {
    char* mcc;
    char* mnc;
    char* name;
};

typedef OfonoModemInterfaceClass OfonoNetRegClass;
G_DEFINE_TYPE(OfonoNetReg, ofono_netreg, OFONO_TYPE_MODEM_INTERFACE)

#define NETREG_SIGNAL_STATUS_CHANGED_NAME             "status-changed"
#define NETREG_SIGNAL_MODE_CHANGED_NAME               "mode-changed"
#define NETREG_SIGNAL_TECHNOLOGY_CHANGED_NAME         "tech-changed"
#define NETREG_SIGNAL_MCC_CHANGED_NAME                "mcc-changed"
#define NETREG_SIGNAL_MNC_CHANGED_NAME                "mnc-changed"
#define NETREG_SIGNAL_NAME_CHANGED_NAME               "name-changed"
#define NETREG_SIGNAL_CELL_ID_CHANGED_NAME            "cell-changed"
#define NETREG_SIGNAL_LOCATION_AREA_CODE_CHANGED_NAME "areacode-changed"
#define NETREG_SIGNAL_STRENGTH_CHANGED_NAME           "strength-changed"

/* Enum <-> string mappings */
static const OfonoNameIntPair ofono_netreg_status_values[] = {
    { "unregistered", OFONO_NETREG_STATUS_UNREGISTERED },
    { "registered",   OFONO_NETREG_STATUS_REGISTERED },
    { "searching",    OFONO_NETREG_STATUS_SEARCHING },
    { "denied",       OFONO_NETREG_STATUS_DENIED },
    { "unknown",      OFONO_NETREG_STATUS_UNKNOWN },
    { "roaming",      OFONO_NETREG_STATUS_ROAMING }
};

static const OfonoNameIntMap ofono_netreg_status_map = {
    "registration status",
    OFONO_NAME_INT_MAP_ENTRIES(ofono_netreg_status_values),
    { NULL, OFONO_NETREG_STATUS_UNKNOWN }
};

static const OfonoNameIntPair ofono_netreg_mode_values[] = {
    { "auto",      OFONO_NETREG_MODE_AUTO },
    { "auto-only", OFONO_NETREG_MODE_AUTO_ONLY },
    { "manual",    OFONO_NETREG_MODE_MANUAL }
};

static const OfonoNameIntMap ofono_netreg_mode_map = {
    "registration mode",
    OFONO_NAME_INT_MAP_ENTRIES(ofono_netreg_mode_values),
    { NULL, OFONO_NETREG_MODE_UNKNOWN }
};

static const OfonoNameIntPair ofono_netreg_tech_values[] = {
    { "gsm",  OFONO_NETREG_TECH_GSM },
    { "edge", OFONO_NETREG_TECH_EDGE },
    { "umts", OFONO_NETREG_TECH_UMTS },
    { "hspa", OFONO_NETREG_TECH_HSPA },
    { "lte",  OFONO_NETREG_TECH_LTE }
};

static const OfonoNameIntMap ofono_netreg_tech_map = {
    "technology",
    OFONO_NAME_INT_MAP_ENTRIES(ofono_netreg_tech_values),
    { NULL, OFONO_NETREG_TECH_UNKNOWN }
};

/*==========================================================================*
 * API
 *==========================================================================*/

OfonoNetReg*
ofono_netreg_new(
    const char* path)
{
    const char* ifname = OFONO_NETREG_INTERFACE_NAME;
    OfonoModem* modem = ofono_modem_new(path);
    OfonoNetReg* netreg;
    OfonoModemInterface* intf = ofono_modem_get_interface(modem, ifname);
    if (G_TYPE_CHECK_INSTANCE_TYPE(intf, OFONO_TYPE_NETREG)) {
        /* Reuse the existing object */
        netreg = ofono_netreg_ref(OFONO_NETREG(intf));
    } else {
        netreg = g_object_new(OFONO_TYPE_NETREG, NULL);
        intf = &netreg->intf;
        GVERBOSE_("%s", path);
        ofono_modem_interface_initialize(intf, ifname, path);
        ofono_modem_set_interface(modem, intf);
        GASSERT(intf->modem == modem);
    }
    ofono_modem_unref(modem);
    return netreg;
}

OfonoNetReg*
ofono_netreg_ref(
    OfonoNetReg* self)
{
    if (G_LIKELY(self)) {
        g_object_ref(OFONO_NETREG(self));
        return self;
    } else {
        return NULL;
    }
}

void
ofono_netreg_unref(
    OfonoNetReg* self)
{
    if (G_LIKELY(self)) {
        g_object_unref(OFONO_NETREG(self));
    }
}

const char*
ofono_netreg_country(
    OfonoNetReg* self)
{
    const char* country = NULL;
    if (ofono_netreg_valid(self)) {
        country = ofono_country_code(self->mcc, self->mnc);
        if (!country && self->mnc) {
            country = ofono_country_code(self->mcc, self->mnc);
        }
    }
    return country;
}

const char*
ofono_netreg_status_string(
    OFONO_NETREG_STATUS status)
{
    return ofono_int_to_name(&ofono_netreg_status_map, status);
}

const char*
ofono_netreg_mode_string(
    OFONO_NETREG_MODE mode)
{
    return ofono_int_to_name(&ofono_netreg_mode_map, mode);
}

const char*
ofono_netreg_tech_string(
    OFONO_NETREG_TECH tech)
{
    return ofono_int_to_name(&ofono_netreg_tech_map, tech);
}

gulong
ofono_netreg_add_property_changed_handler(
    OfonoNetReg* self,
    OfonoNetRegPropertyHandler fn,
    const char* name,
    void* arg)
{
    return G_LIKELY(self) ? ofono_object_add_property_changed_handler(
        &self->intf.object, (OfonoObjectPropertyHandler)fn, name, arg) :  0;
}

gulong
ofono_netreg_add_valid_changed_handler(
    OfonoNetReg* self,
    OfonoNetRegHandler fn,
    void* arg)
{
    return G_LIKELY(self) ? ofono_object_add_valid_changed_handler(
        &self->intf.object, (OfonoObjectHandler)fn, arg) : 0;
}

gulong
ofono_netreg_add_status_changed_handler(
    OfonoNetReg* self,
    OfonoNetRegHandler fn,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(fn)) ? g_signal_connect(self,
        NETREG_SIGNAL_STATUS_CHANGED_NAME, G_CALLBACK(fn), arg) : 0;
}

gulong
ofono_netreg_add_mode_changed_handler(
    OfonoNetReg* self,
    OfonoNetRegHandler fn,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(fn)) ? g_signal_connect(self,
        NETREG_SIGNAL_MODE_CHANGED_NAME, G_CALLBACK(fn), arg) : 0;
}

gulong
ofono_netreg_add_tech_changed_handler(
    OfonoNetReg* self,
    OfonoNetRegHandler fn,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(fn)) ? g_signal_connect(self,
        NETREG_SIGNAL_TECHNOLOGY_CHANGED_NAME, G_CALLBACK(fn), arg) : 0;
}

gulong
ofono_netreg_add_mcc_changed_handler(
    OfonoNetReg* self,
    OfonoNetRegHandler fn,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(fn)) ? g_signal_connect(self,
        NETREG_SIGNAL_MCC_CHANGED_NAME, G_CALLBACK(fn), arg) : 0;
}

gulong
ofono_netreg_add_mnc_changed_handler(
    OfonoNetReg* self,
    OfonoNetRegHandler fn,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(fn)) ? g_signal_connect(self,
        NETREG_SIGNAL_MNC_CHANGED_NAME, G_CALLBACK(fn), arg) : 0;
}

gulong
ofono_netreg_add_name_changed_handler(
    OfonoNetReg* self,
    OfonoNetRegHandler fn,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(fn)) ? g_signal_connect(self,
        NETREG_SIGNAL_NAME_CHANGED_NAME, G_CALLBACK(fn), arg) : 0;
}

gulong
ofono_netreg_add_cell_changed_handler(
    OfonoNetReg* self,
    OfonoNetRegHandler fn,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(fn)) ? g_signal_connect(self,
        NETREG_SIGNAL_CELL_ID_CHANGED_NAME, G_CALLBACK(fn), arg) : 0;
}

gulong
ofono_netreg_add_areacode_changed_handler(
    OfonoNetReg* self,
    OfonoNetRegHandler fn,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(fn)) ? g_signal_connect(self,
        NETREG_SIGNAL_LOCATION_AREA_CODE_CHANGED_NAME, G_CALLBACK(fn), arg) : 0;
}

gulong
ofono_netreg_add_strength_changed_handler(
    OfonoNetReg* self,
    OfonoNetRegHandler fn,
    void* arg)
{
    return (G_LIKELY(self) && G_LIKELY(fn)) ? g_signal_connect(self,
        NETREG_SIGNAL_STRENGTH_CHANGED_NAME, G_CALLBACK(fn), arg) : 0;
}

void
ofono_netreg_remove_handler(
    OfonoNetReg* self,
    gulong id)
{
    if (G_LIKELY(self) && G_LIKELY(id)) {
        g_signal_handler_disconnect(self, id);
    }
}

/*==========================================================================*
 * Properties
 *==========================================================================*/

#define NETREG_DEFINE_PROPERTY_BYTE(NAME,var) \
    OFONO_OBJECT_DEFINE_PROPERTY_BYTE(NETREG,NAME,OfonoNetReg,var)

#define NETREG_DEFINE_PROPERTY_UINT16(NAME,var) \
    OFONO_OBJECT_DEFINE_PROPERTY_UINT16(NETREG,NAME,OfonoNetReg,var)

#define NETREG_DEFINE_PROPERTY_UINT32(NAME,var) \
    OFONO_OBJECT_DEFINE_PROPERTY_UINT32(NETREG,NAME,OfonoNetReg,var)

#define NETREG_DEFINE_PROPERTY_STRING(NAME,var) \
    OFONO_OBJECT_DEFINE_PROPERTY_STRING(NETREG,netreg,NAME,OfonoNetReg,var)

#define NETREG_DEFINE_PROPERTY_ENUM(NAME,var) \
    OFONO_OBJECT_DEFINE_PROPERTY_ENUM(NETREG,NAME,OfonoNetReg,var, \
    &ofono_netreg_##var##_map)

G_STATIC_ASSERT(sizeof(OFONO_NETREG_STATUS) == sizeof(int));
G_STATIC_ASSERT(sizeof(OFONO_NETREG_MODE) == sizeof(int));
G_STATIC_ASSERT(sizeof(OFONO_NETREG_TECH) == sizeof(int));

static
void*
ofono_netreg_property_priv(
    OfonoObject* object,
    const OfonoObjectProperty* prop)
{
    return OFONO_NETREG(object)->priv;
}

/*==========================================================================*
 * Internals
 *==========================================================================*/

/**
 * Per instance initializer
 */
static
void
ofono_netreg_init(
    OfonoNetReg* self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self, OFONO_TYPE_NETREG,
        OfonoNetRegPriv);
}

/**
 * Final stage of deinitialization
 */
static
void
ofono_netreg_finalize(
    GObject* object)
{
    OfonoNetReg* self = OFONO_NETREG(object);
    OfonoNetRegPriv* priv = self->priv;
    g_free(priv->mcc);
    g_free(priv->mnc);
    g_free(priv->name);
    G_OBJECT_CLASS(ofono_netreg_parent_class)->finalize(object);
}

/**
 * Per class initializer
 */
static
void
ofono_netreg_class_init(
    OfonoNetRegClass* klass)
{
    static OfonoObjectProperty ofono_netreg_properties[] = {
        NETREG_DEFINE_PROPERTY_ENUM(STATUS,status),
        NETREG_DEFINE_PROPERTY_ENUM(MODE,mode),
        NETREG_DEFINE_PROPERTY_ENUM(TECHNOLOGY,tech),
        NETREG_DEFINE_PROPERTY_STRING(MCC,mcc),
        NETREG_DEFINE_PROPERTY_STRING(MNC,mnc),
        NETREG_DEFINE_PROPERTY_STRING(NAME,name),
        NETREG_DEFINE_PROPERTY_UINT32(CELL_ID,cell),
        NETREG_DEFINE_PROPERTY_UINT16(LOCATION_AREA_CODE,areacode),
        NETREG_DEFINE_PROPERTY_BYTE(STRENGTH,strength)
    };

    OfonoObjectClass* ofono = &klass->object;
    G_OBJECT_CLASS(klass)->finalize = ofono_netreg_finalize;
    g_type_class_add_private(klass, sizeof(OfonoNetRegPriv));
    ofono->properties = ofono_netreg_properties;
    ofono->nproperties = G_N_ELEMENTS(ofono_netreg_properties);
    OFONO_OBJECT_CLASS_SET_PROXY_CALLBACKS_RO(ofono,
        org_ofono_network_registration);
    ofono_class_initialize(ofono);
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
