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

#ifndef GOFONO_OBJECT_H
#define GOFONO_OBJECT_H

#include "gofono_types.h"

G_BEGIN_DECLS

typedef struct ofono_object_priv OfonoObjectPriv;

typedef struct ofono_object {
    GObject object;
    OfonoObjectPriv* priv;
    const char* intf;
    const char* path;
    gboolean valid;
} OfonoObject;

typedef
void
(*OfonoObjectHandler)(
    OfonoObject* sender,
    void* arg);

typedef
void
(*OfonoObjectPropertyHandler)(
    OfonoObject* sender,
    const char* name,
    GVariant* value,
    void* arg);

typedef
void
(*OfonoObjectCallFinishedCallback)(
    OfonoObject* object,
    const GError* error,
    void* arg);

OfonoObject*
ofono_object_new(
    const char* intf,
    const char* path);

OfonoObject*
ofono_object_ref(
    OfonoObject* object);

void
ofono_object_unref(
    OfonoObject* object);

GCancellable*
ofono_object_set_property(
    OfonoObject* object,
    const char* name,
    GVariant* value,
    OfonoObjectCallFinishedCallback callback,
    void* arg);

GCancellable*
ofono_object_set_string(
    OfonoObject* object,
    const char* name,
    const char* value,
    OfonoObjectCallFinishedCallback callback,
    void* arg);

GCancellable*
ofono_object_set_boolean(
    OfonoObject* object,
    const char* name,
    gboolean value,
    OfonoObjectCallFinishedCallback callback,
    void* arg);

GVariant*
ofono_object_get_property(
    OfonoObject* object,
    const char* name,
    const GVariantType* type);

GVariant*
ofono_object_get_properties(
    OfonoObject* self);

const char*
ofono_object_get_string(
    OfonoObject* object,
    const char* name);

gboolean
ofono_object_get_boolean(
    OfonoObject* object,
    const char* name,
    gboolean default_value);

GPtrArray*
ofono_object_get_property_keys(
    OfonoObject* object);

gboolean
ofono_object_wait_valid(
    OfonoObject* object,
    int timeout_msec,
    GError** error);

gulong
ofono_object_add_valid_changed_handler(
    OfonoObject* object,
    OfonoObjectHandler handler,
    void* arg);

gulong
ofono_object_add_property_changed_handler(
    OfonoObject* object,
    OfonoObjectPropertyHandler handler,
    const char* name,
    void* arg);

void
ofono_object_remove_handler(
    OfonoObject* object,
    gulong id);

void
ofono_object_remove_handlers(
    OfonoObject* object,
    gulong* ids,
    unsigned int count);

G_END_DECLS

#endif /* GOFONO_OBJECT_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
