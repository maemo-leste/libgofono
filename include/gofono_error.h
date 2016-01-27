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

#ifndef GOFONO_ERROR_H
#define GOFONO_ERROR_H

#include "gofono_types.h"

G_BEGIN_DECLS

#define OFONO_ERROR (ofono_error_quark())
GQuark ofono_error_quark(void);

typedef enum ofono_error {
    OFONO_ERROR_INVALID_ARGS,          /* org.ofono.Error.InvalidArguments */
    OFONO_ERROR_INVALID_FORMAT,        /* org.ofono.Error.InvalidFormat */
    OFONO_ERROR_NOT_IMPLEMENTED,       /* org.ofono.Error.NotImplemented */
    OFONO_ERROR_FAILED,                /* org.ofono.Error.Failed */
    OFONO_ERROR_BUSY,                  /* org.ofono.Error.InProgress */
    OFONO_ERROR_NOT_FOUND,             /* org.ofono.Error.NotFound */
    OFONO_ERROR_NOT_ACTIVE,            /* org.ofono.Error.NotActive */
    OFONO_ERROR_NOT_SUPPORTED,         /* org.ofono.Error.NotSupported */
    OFONO_ERROR_NOT_AVAILABLE,         /* org.ofono.Error.NotAvailable */
    OFONO_ERROR_TIMED_OUT,             /* org.ofono.Error.Timedout */
    OFONO_ERROR_SIM_NOT_READY,         /* org.ofono.Error.SimNotReady */
    OFONO_ERROR_IN_USE,                /* org.ofono.Error.InUse */
    OFONO_ERROR_NOT_ATTACHED,          /* org.ofono.Error.NotAttached */
    OFONO_ERROR_ATTACH_IN_PROGRESS,    /* org.ofono.Error.AttachInProgress */
    OFONO_ERROR_NOT_REGISTERED,        /* org.ofono.Error.NotRegistered */
    OFONO_ERROR_CANCELED,              /* org.ofono.Error.Canceled */
    OFONO_ERROR_ACCESS_DENIED,         /* org.ofono.Error.AccessDenied */
    OFONO_ERROR_EMERGENCY_ACTIVE,      /* org.ofono.Error.EmergencyActive */
    OFONO_ERROR_INCORRECT_PASSWORD,    /* org.ofono.Error.IncorrectPassword */
    OFONO_ERROR_NOT_ALLOWED,           /* org.ofono.Error.NotAllowed */
    OFONO_ERROR_NOT_RECOGNIZED,        /* org.ofono.Error.NotRecognized */
    OFONO_ERROR_NETWORK_TERMINATED,    /* org.ofono.Error.Terminated */
    OFONO_NUM_ERRORS
} OfonoError;

G_END_DECLS

#endif /* GOFONO_ERROR_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
