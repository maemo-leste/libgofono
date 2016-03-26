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

#ifndef GOFONO_MANAGER_H
#define GOFONO_MANAGER_H

#include "gofono_types.h"

G_BEGIN_DECLS

typedef struct ofono_manager_priv OfonoManagerPriv;

struct ofono_manager {
    GObject object;
    OfonoManagerPriv* priv;
    gboolean valid;
};
  
typedef
void
(*OfonoManagerHandler)(
    OfonoManager* manager,
    void* arg);

typedef
void
(*OfonoManagerModemAddedHandler)(
    OfonoManager* manager,
    OfonoModem* modem,
    void* arg);

typedef
void
(*OfonoManagerModemRemovedHandler)(
    OfonoManager* manager,
    const char* path,
    void* arg);

OfonoManager*
ofono_manager_new(void);

OfonoManager*
ofono_manager_ref(
    OfonoManager* manager);

void
ofono_manager_unref(
    OfonoManager* manager);

GPtrArray*
ofono_manager_get_modems(
    OfonoManager* manager);

gboolean
ofono_manager_has_modem(
    OfonoManager* manager,
    const char* path);

gboolean
ofono_manager_wait_valid(
    OfonoManager* manager,
    int timeout_msec,
    GError** error);

gulong
ofono_manager_add_valid_changed_handler(
    OfonoManager* manager,
    OfonoManagerHandler fn,
    void* arg);

gulong
ofono_manager_add_modem_added_handler(
    OfonoManager* manager,
    OfonoManagerModemAddedHandler fn,
    void* arg);

gulong
ofono_manager_add_modem_removed_handler(
    OfonoManager* manager,
    OfonoManagerModemRemovedHandler fn,
    void* arg);

void
ofono_manager_remove_handler(
    OfonoManager* manager,
    gulong id);

void
ofono_manager_remove_handlers(
    OfonoManager* self,
    gulong* ids,
    unsigned int count);

G_END_DECLS

#endif /* GOFONO_MANAGER_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
