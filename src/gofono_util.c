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
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
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

#include "gofono_util_p.h"
#include "gofono_log.h"

typedef struct ofono_condition_wait_data {
    GMainLoop* loop;
    gboolean timed_out;
    guint timeout_id;
    OfonoConditionCheck check;
} OfonoConditionWaitData;

static GUtilIdlePool* ofono_shared_pool = NULL;

GUtilIdlePool*
ofono_idle_pool()
{
    return gutil_idle_pool_get(&ofono_shared_pool);
}

void
ofono_idle_pool_drain()
{
    if (ofono_shared_pool) {
        gutil_idle_pool_ref(ofono_shared_pool);
        gutil_idle_pool_drain(ofono_shared_pool);
        gutil_idle_pool_unref(ofono_shared_pool);
    }
}

static
gboolean
ofono_condition_wait_timeout(
    gpointer data)
{
    OfonoConditionWaitData* wait = data;
    wait->timed_out = TRUE;
    wait->timeout_id = 0;
    g_main_loop_quit(wait->loop);
    return G_SOURCE_REMOVE;
}

static
void
ofono_condition_wait_handler(
    GObject* object,
    void* data)
{
    OfonoConditionWaitData* wait = data;
    if (wait->check(object)) {
        g_main_loop_quit(wait->loop);
    }
}

static
gboolean
ofono_condition_wait_internal(
    GObject* object,
    OfonoConditionCheck check,
    OfonoConditionAddHandler add_handler,
    OfonoWaitConditionRemoveHandler remove_handler,
    int timeout_msec,
    GError** error)
{
    gboolean ok;
    OfonoConditionWaitData wait;
    const gulong id = add_handler(object, ofono_condition_wait_handler, &wait);
    g_object_ref(object);
    memset(&wait, 0, sizeof(wait));
    wait.loop = g_main_loop_new(NULL, TRUE);
    wait.check = check;
    if (timeout_msec > 0) {
        wait.timeout_id = g_timeout_add(timeout_msec,
            ofono_condition_wait_timeout, &wait);
    }

    g_main_loop_run(wait.loop);
    g_main_loop_unref(wait.loop);

    remove_handler(object, id);
    if (wait.timeout_id) g_source_remove(wait.timeout_id);
    ok = check(object);
    g_object_unref(object);
    if (ok) {
        return TRUE;
    } else {
        if (error) {
            g_propagate_error(error, wait.timed_out ?
                g_error_new(G_IO_ERROR, G_IO_ERROR_TIMED_OUT, "Wait timeout") :
                g_error_new(G_IO_ERROR, G_IO_ERROR_FAILED, "Wait failed"));
        }
        return FALSE;
    }
}

gboolean
ofono_condition_wait(
    GObject* object,
    OfonoConditionCheck check,
    OfonoConditionAddHandler add_handler,
    OfonoWaitConditionRemoveHandler remove_handler,
    int timeout_msec,
    GError** error)
{
    GASSERT(!error || !*error);
    if (object) {
        if (check(object)) {
            /* Nothing to wait for */
            return TRUE;
        } else if (timeout_msec != 0) {
            /* Wait for the condition */
            return ofono_condition_wait_internal(object, check, add_handler,
                remove_handler, timeout_msec, error);
        } else {
            /* Need to wait for timeout is zero */
            if (error) {
                g_propagate_error(error, g_error_new(G_IO_ERROR,
                    G_IO_ERROR_WOULD_BLOCK, "Need to wait"));
            }
            return FALSE;
        }
    } else {
        if (error) {
            g_propagate_error(error, g_error_new(G_IO_ERROR,
                G_IO_ERROR_INVALID_ARGUMENT, "Invalid argument"));
        }
        return FALSE;
    }
}

static
gint
ofono_string_list_sort_proc(
    gconstpointer s1,
    gconstpointer s2)
{
    return strcmp(s1, s2);
}

GPtrArray*
ofono_string_array_sort(
    GPtrArray* strings)
{
    g_ptr_array_sort(strings, ofono_string_list_sort_proc);
    return strings;
}

GPtrArray*
ofono_string_array_from_variant(
    GVariant* value)
{
    const guint n = (value ? g_variant_n_children(value) : 0);
    GPtrArray* strings = g_ptr_array_new_full(n, g_free);
    if (value) {
        GVariantIter iter;
        GVariant* child;
        for (g_variant_iter_init(&iter, value);
            (child = g_variant_iter_next_value(&iter)) != NULL;
             g_variant_unref(child)) {
            const char* ifname = NULL;
            g_variant_get(child, "&s", &ifname);
            if (ifname) g_ptr_array_add(strings, g_strdup(ifname));
        }
    }
    return strings;
}

gboolean
ofono_string_array_equal(
    GPtrArray* strings1,
    GPtrArray* strings2)
{
    if (strings1 == strings2) {
        return TRUE;
    } else {
        const guint len1 = strings1 ? strings1->len : 0;
        const guint len2 = strings2 ? strings2->len : 0;
        if (len1 != len2) {
            return FALSE;
        } else {
            guint i;
            for (i=0; i<len1; i++) {
                if (g_strcmp0(strings1->pdata[i], strings2->pdata[i])) {
                    return FALSE;
                }
            }
            return TRUE;
        }
    }
}

int
ofono_name_to_int(
    const OfonoNameIntMap* map,
    const char* name)
{
    unsigned int i;
    for (i=0; i<map->count; i++) {
        const OfonoNameIntPair* entry = map->entries + i;
        if (!g_strcmp0(name, entry->name)) {
            return entry->value;
        }
    }
#if GUTIL_LOG_DEBUG
    if (g_strcmp0(name, map->defaults.name)) {
        GDEBUG("Unknown %s %s", map->name, name);
    }
#endif /* GUTIL_LOG_DEBUG */
    return map->defaults.value;
}

const char*
ofono_int_to_name(
    const OfonoNameIntMap* map,
    int value)
{
    unsigned int i;
    for (i=0; i<map->count; i++) {
        const OfonoNameIntPair* entry = map->entries + i;
        if (entry->value == value) {
            return entry->name;
        }
    }
#if GUTIL_LOG_DEBUG
    if (value != map->defaults.value) {
        GDEBUG("Invalid %s %d", map->name, value);
    }
#endif /* GUTIL_LOG_DEBUG */
    return map->defaults.name;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
