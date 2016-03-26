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

#include "gofono_manager.h"
#include "gofono_modem.h"
#include "gofono_util.h"

#include <gutil_log.h>

#define RET_OK          (0)
#define RET_NOTFOUND    (1)
#define RET_ERR         (2)
#define RET_TIMEOUT     (3)

typedef struct app {
    const char* path;
    const char* key;
    gboolean all;
    gint timeout;
} App;

static
void
handle_modem(
    App* app,
    OfonoModem* modem)
{
    guint i;
    OfonoObject* obj = &modem->object;
    GPtrArray* keys = ofono_object_get_property_keys(obj);
    for (i=0; i<keys->len; i++) {
        const char* key = keys->pdata[i];
        if (app->all) {
            GVariant* v = ofono_object_get_property(obj, key, NULL);
            gchar* text = g_variant_print(v, FALSE);
            printf("%s: %s\n", key, text);
            g_free(text);
        } else if (app->key) {
            if (!strcmp(app->key, key)) {
                GVariant* v = ofono_object_get_property(obj, key, NULL);
                gchar* text = g_variant_print(v, FALSE);
                printf("%s\n", text);
                g_free(text);
            }
        } else {
            printf("%s\n", key);
        }
    }
}

static
int
handle_manager(
    App* app,
    OfonoManager* manager)
{
    guint i, ret;
    GPtrArray* modems = ofono_manager_get_modems(manager);
    if (!app->path) {
        for (i=0; i<modems->len; i++) {
            OfonoModem* modem = modems->pdata[i];
            printf("%s\n", modem->object.path);
        }
        ret = RET_OK;
    } else {
        ret = RET_NOTFOUND;
        for (i=0; i<modems->len; i++) {
            OfonoModem* modem = modems->pdata[i];
            if (!strcmp(app->path, modem->object.path)) {
                OfonoObject* object = &modem->object;
                GError* error = NULL;
                if (ofono_object_wait_valid(object, app->timeout, &error)) {
                    handle_modem(app, modem);
                    ret = RET_OK;
                } else {
                    ret = (error->code == G_IO_ERROR_TIMED_OUT) ?
                        RET_TIMEOUT : RET_ERR;
                    g_error_free(error);
                }
                break;
            }
        }
    }
    return ret;
}

static
int
app_run(
    App* app)
{
    int ret;
    GError* error = NULL;
    OfonoManager* manager = ofono_manager_new();
    if (app->timeout > 0) GDEBUG("Timeout %d sec", app->timeout);
    if (ofono_manager_wait_valid(manager, app->timeout, &error)) {
        ret = handle_manager(app, manager);
    } else {
        GERR("%s", GERRMSG(error));
        ret = (error->code == G_IO_ERROR_TIMED_OUT) ? RET_TIMEOUT : RET_ERR;
        g_error_free(error);
    }
    ofono_manager_unref(manager);
    ofono_idle_pool_drain();
    return ret;
}

static
gboolean
app_init(
    App* app,
    int argc,
    char* argv[])
{
    gboolean ok = FALSE;
    gboolean verbose = FALSE;
    GOptionEntry entries[] = {
        { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
          "Enable verbose output", NULL },
        { "timeout", 't', 0, G_OPTION_ARG_INT,
          &app->timeout, "Timeout in seconds", "SECONDS" },
        { "all", 'a', 0, G_OPTION_ARG_NONE, &app->all,
          "Print all modem properties and their values", NULL },
        { NULL }
    };
    GError* error = NULL;
    GOptionContext* options = g_option_context_new("[PATH [KEY]]");
    g_option_context_add_main_entries(options, entries, NULL);
    if (g_option_context_parse(options, &argc, &argv, &error)) {
        if (argc < 4) {
            if (argc > 1) {
                app->path = argv[1];
                if (argc > 2) app->key = argv[2];
            }
            if (verbose) gutil_log_default.level = GLOG_LEVEL_VERBOSE;
            ok = TRUE;
        } else {
            char* help = g_option_context_get_help(options, TRUE, NULL);
            fprintf(stderr, "%s", help);
            g_free(help);
        }
    } else {
        GERR("%s", error->message);
        g_error_free(error);
    }
    g_option_context_free(options);
    return ok;
}

int main(int argc, char* argv[])
{
    int ret = RET_ERR;
    App app;
    memset(&app, 0, sizeof(app));
    app.timeout = -1;
    gutil_log_timestamp = FALSE;
    gutil_log_set_type(GLOG_TYPE_STDERR, "ofono-modem");
    gutil_log_default.level = GLOG_LEVEL_DEFAULT;
    if (app_init(&app, argc, argv)) {
        ret = app_run(&app);
    }
    return ret;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
