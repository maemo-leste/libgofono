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
#include "gofono_simmgr.h"
#include "gofono_names.h"
#include "gofono_util.h"

#include <gutil_log.h>

#define RET_OK          (0)
#define RET_NOTFOUND    (1)
#define RET_NOTPRESENT  (2)
#define RET_ERR         (3)
#define RET_TIMEOUT     (4)

typedef struct app {
    const char* path;
    const char* key;
    gboolean all;
    guint timeout_id;
    GMainLoop* loop;
    OfonoSimMgr* sim;
    gulong sim_valid_id;
    int in_progress;
    gint timeout;
    int ret;
} App;

static
void
app_async_done(
    App* app)
{
    GASSERT(app->in_progress > 0);
    if (!(--app->in_progress)) g_main_loop_quit(app->loop);
}

static
void
handle_sim(
    App* app)
{
    guint i;
    OfonoObject* obj = ofono_simmgr_object(app->sim);
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
void
sim_valid(
    OfonoSimMgr* sim,
    void* arg)
{
    if (sim->intf.object.valid) {
        App* app = arg;
        GASSERT(app->sim == sim);
        handle_sim(app);
        app_async_done(app);
    }
}

static
void
handle_modem(
    App* app,
    OfonoModem* modem)
{
    if (ofono_modem_has_interface(modem, OFONO_SIMMGR_INTERFACE_NAME)) {
        if (app->path) {
            GASSERT(!strcmp(app->path, modem->object.path));
            GASSERT(!app->sim);
            app->sim = ofono_simmgr_new(modem->object.path);
            if (app->sim->intf.object.valid) {
                handle_sim(app);
            } else {
                app->in_progress++;
                app->sim_valid_id = ofono_simmgr_add_valid_changed_handler(
                    app->sim, sim_valid, app);
            }
        } else {
            printf("%s\n", modem->object.path);
        }
    } else if (app->path) {
        app->ret = RET_NOTPRESENT;
    }
}

static
void
modem_valid(
    OfonoModem* modem,
    void* arg)
{
    if (modem->object.valid) {
        App* app = arg;
        handle_modem(app, modem);
        ofono_modem_unref(modem);
        app_async_done(app);
    }
}

static
void
handle_manager(
    App* app,
    OfonoManager* manager)
{
    guint i;
    GPtrArray* modems = ofono_manager_get_modems(manager);
    for (i=0; i<modems->len; i++) {
        OfonoModem* modem = modems->pdata[i];
        if (!app->path || !strcmp(app->path, modem->object.path)) {
            app->ret = RET_OK;
            if (modem->object.valid) {
                handle_modem(app, modem);
            } else {
                app->in_progress++;
                ofono_modem_add_valid_changed_handler(ofono_modem_ref(modem),
                    modem_valid, app);
            }
            if (app->path) break;
        }
    }
}

static
void
manager_valid(
    OfonoManager* manager,
    void* arg)
{
    if (manager->valid) {
        App* app = arg;
        handle_manager(app, manager);
        app_async_done(app);
    }
}

static
gboolean
app_timeout(
    gpointer data)
{
    App* app = data;
    GDEBUG("Timed out");
    app->timeout_id = 0;
    app->ret = RET_TIMEOUT;
    g_main_loop_quit(app->loop);
    return FALSE;
}

static
int
app_run(
    App* app)
{
    gulong valid_id = 0;
    OfonoManager* manager = ofono_manager_new();
    if (manager->valid) {
        handle_manager(app, manager);
    } else {
        valid_id = ofono_manager_add_valid_changed_handler(manager,
            manager_valid, app);
        app->in_progress++;
    }
    if (app->in_progress) {
        if (app->timeout > 0) {
            GDEBUG("Timeout %d sec", app->timeout);
            app->timeout_id = g_timeout_add_seconds(app->timeout,
                app_timeout, app);
            GASSERT(app->timeout_id);
        }
        app->loop = g_main_loop_new(NULL, FALSE);
        g_main_loop_run(app->loop);
        g_main_loop_unref(app->loop);
        app->loop = NULL;
        if (app->timeout_id) {
            g_source_remove(app->timeout_id);
            app->timeout_id = 0;
        }
    }
    if (app->sim) {
        ofono_simmgr_remove_handler(app->sim, app->sim_valid_id);
        ofono_simmgr_unref(app->sim);
    }
    ofono_manager_remove_handler(manager, valid_id);
    ofono_manager_unref(manager);
    ofono_idle_pool_drain();
    return app->ret;
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
          "Print all SIM properties and their values", NULL },
        { NULL }
    };
    GError* error = NULL;
    GOptionContext* options = g_option_context_new("[PATH [KEY]]");
    g_option_context_add_main_entries(options, entries, NULL);
    if (g_option_context_parse(options, &argc, &argv, &error)) {
        if (argc < 4) {
            if (argc > 1) {
                app->ret = RET_NOTFOUND;
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
    gutil_log_timestamp = FALSE;
    gutil_log_set_type(GLOG_TYPE_STDERR, "ofono-sim");
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
