/*
 * Copyright (C) 2016 Jolla Ltd.
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

#include "gofono_error_p.h"
#include "gofono_names.h"

#define OFONO_ERROR_(error) OFONO_SERVICE ".Error." error

static const GDBusErrorEntry ofono_errors[] = {
    {OFONO_ERROR_INVALID_ARGS,          OFONO_ERROR_("InvalidArguments")},
    {OFONO_ERROR_INVALID_FORMAT,        OFONO_ERROR_("InvalidFormat")},
    {OFONO_ERROR_NOT_IMPLEMENTED,       OFONO_ERROR_("NotImplemented")},
    {OFONO_ERROR_FAILED,                OFONO_ERROR_("Failed")},
    {OFONO_ERROR_BUSY,                  OFONO_ERROR_("InProgress")},
    {OFONO_ERROR_NOT_FOUND,             OFONO_ERROR_("NotFound")},
    {OFONO_ERROR_NOT_ACTIVE,            OFONO_ERROR_("NotActive")},
    {OFONO_ERROR_NOT_SUPPORTED,         OFONO_ERROR_("NotSupported")},
    {OFONO_ERROR_NOT_AVAILABLE,         OFONO_ERROR_("NotAvailable")},
    {OFONO_ERROR_TIMED_OUT,             OFONO_ERROR_("Timedout")},
    {OFONO_ERROR_SIM_NOT_READY,         OFONO_ERROR_("SimNotReady")},
    {OFONO_ERROR_IN_USE,                OFONO_ERROR_("InUse")},
    {OFONO_ERROR_NOT_ATTACHED,          OFONO_ERROR_("NotAttached")},
    {OFONO_ERROR_ATTACH_IN_PROGRESS,    OFONO_ERROR_("AttachInProgress")},
    {OFONO_ERROR_NOT_REGISTERED,        OFONO_ERROR_("NotRegistered")},
    {OFONO_ERROR_CANCELED,              OFONO_ERROR_("Canceled")},
    {OFONO_ERROR_ACCESS_DENIED,         OFONO_ERROR_("AccessDenied")},
    {OFONO_ERROR_EMERGENCY_ACTIVE,      OFONO_ERROR_("EmergencyActive")},
    {OFONO_ERROR_INCORRECT_PASSWORD,    OFONO_ERROR_("IncorrectPassword")},
    {OFONO_ERROR_NOT_ALLOWED,           OFONO_ERROR_("NotAllowed")},
    {OFONO_ERROR_NOT_RECOGNIZED,        OFONO_ERROR_("NotRecognized")},
    {OFONO_ERROR_NETWORK_TERMINATED,    OFONO_ERROR_("Terminated")}
};

G_STATIC_ASSERT(G_N_ELEMENTS(ofono_errors) == OFONO_NUM_ERRORS);

GQuark
ofono_error_quark()
{
    static volatile gsize ofono_error_quark_value = 0;
    g_dbus_error_register_error_domain("ofono-error-quark",
        &ofono_error_quark_value, ofono_errors, G_N_ELEMENTS(ofono_errors));
    return (GQuark)ofono_error_quark_value;
}

gboolean
ofono_error_is_generic_timeout(
    const GError* error)
{
    if (error) {
        if (error->domain == G_IO_ERROR) {
            switch (error->code) {
            case G_IO_ERROR_TIMED_OUT:
                return TRUE;
            default:
                return FALSE;
            }
        } else if (error->domain == G_DBUS_ERROR) {
            switch (error->code) {
            case G_DBUS_ERROR_TIMEOUT:
            case G_DBUS_ERROR_TIMED_OUT:
                return TRUE;
            default:
                return FALSE;
            }
        }
    }
    return FALSE;
}

gboolean
ofono_error_is_busy(
    const GError* error)
{
    if (error &&
        error->domain == OFONO_ERROR &&
        error->code == OFONO_ERROR_BUSY) {
        return TRUE;
    }
    return FALSE;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
