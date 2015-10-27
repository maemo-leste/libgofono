/*
 * Copyright (C) 2014-2015 Jolla Ltd.
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
#include "gofono_connmgr.h"
#include "gofono_connctx.h"
#include "gofono_names.h"

#include <gutil_log.h>
#include <gutil_macros.h>
#include <stdlib.h>

#define RET_OK          (0)
#define RET_NOTFOUND    (1)
#define RET_NOTPRESENT  (2)
#define RET_ERR         (3)
#define RET_TIMEOUT     (4)

typedef struct app {
    const char* path;
    GSList* contexts;
    GSList* actions;
    gboolean all;
    guint timeout_id;
    GMainLoop* loop;
    int in_progress;
    gint timeout;
    int ret;
} App;

typedef struct context {
    App* app;
    OfonoConnCtx* obj;
    gulong action_id;
} Context;

typedef struct action Action;
typedef int (*ActionProc)(Action* action, Context* ctx);
typedef int (*ActionSimpleProc)(Context* ctx);
typedef int (*ActionEnumProc)(Context* ctx, int val);
typedef int (*ActionStrProc)(Context* ctx, const char* name, const char* val);

struct action {
    int (*run)(Action* action, Context* ctx);
    void (*free)(Action* action);
};

typedef struct action_simple {
    Action action;
    ActionSimpleProc proc;
} ActionSimple;

typedef struct action_enum {
    Action action;
    ActionEnumProc proc;
    int value;
} ActionEnum;

typedef struct action_str {
    Action action;
    ActionStrProc proc;
    char* name;
    char* value;
} ActionStr;

G_INLINE_FUNC ActionSimple* action_simple_cast(Action* action)
    { return G_CAST(action, ActionSimple, action); }

G_INLINE_FUNC ActionEnum* action_enum_cast(Action* action)
    { return G_CAST(action, ActionEnum, action); }

G_INLINE_FUNC ActionStr* action_str_cast(Action* action)
    { return G_CAST(action, ActionStr, action); }

G_INLINE_FUNC gboolean app_add_action(App* app, Action* action)
    { app->actions = g_slist_append(app->actions, action); return TRUE; }

static
void
action_simple_free(
    Action* action)
{
    g_free(action_simple_cast(action));
}

static
int
action_simple_run(
    Action* action,
    Context* ctx)
{
    return action_simple_cast(action)->proc(ctx);
}

static
Action*
action_simple_new(
    ActionSimpleProc proc)
{
    ActionSimple* a = g_new0(ActionSimple, 1);
    a->action.run = action_simple_run;
    a->action.free= action_simple_free;
    a->proc = proc;
    return &a->action;
}

static
void
action_enum_free(
    Action* action)
{
    g_free(action_enum_cast(action));
}

static
int
action_enum_run(
    Action* action,
    Context* ctx)
{
    ActionEnum* a = action_enum_cast(action);
    return a->proc(ctx, a->value);
}

static
Action*
action_enum_new(
    ActionEnumProc proc,
    int value)
{
    ActionEnum* a = g_new0(ActionEnum, 1);
    a->action.run = action_enum_run;
    a->action.free= action_enum_free;
    a->proc = proc;
    a->value = value;
    return &a->action;
}

static
void
action_str_free(
    Action* action)
{
    ActionStr* a = action_str_cast(action);
    g_free(a->name);
    g_free(a->value);
    g_free(a);
}

static
int
action_str_run(
    Action* action,
    Context* ctx)
{
    ActionStr* a = action_str_cast(action);
    return a->proc(ctx, a->name, a->value);
}

static
Action*
action_str_new(
    ActionStrProc proc,
    const char* name,
    const char* value)
{
    ActionStr* a = g_new0(ActionStr, 1);
    a->action.run = action_str_run;
    a->action.free= action_str_free;
    a->proc = proc;
    a->name = g_strdup(name);
    a->value = g_strdup(value);
    return &a->action;
}

static
int
handle_error(
    GError* error)
{
    int ret;
    GERR("%s", GERRMSG(error));
    ret = (error->code == G_IO_ERROR_TIMED_OUT) ? RET_TIMEOUT : RET_ERR;
    g_error_free(error);
    return ret;
}

static
void
app_next_action(
    App* app)
{
    while (app->ret == RET_OK && app->actions && !app->in_progress) {
        GSList* elem;
        Action* action = app->actions->data;
        app->actions = g_slist_delete_link(app->actions, app->actions);
        for (elem = app->contexts; elem; elem = elem->next) {
            int res = action->run(action, elem->data);
            if (res != RET_OK) {
                if (app->ret == RET_OK) app->ret = res;
                break;
            }
        }
        action->free(action);
    }
    if (!app->in_progress && app->loop) {
        g_main_loop_quit(app->loop);
    }
}

static
void
app_async_done(
    App* app)
{
    GASSERT(app->in_progress > 0);
    if (!(--app->in_progress)) {
        app_next_action(app);
    }
}

static
void
action_finish(
    Context* ctx)
{
    ofono_connctx_remove_handler(ctx->obj, ctx->action_id);
    ctx->action_id = 0;
    app_async_done(ctx->app);
}

static
void
action_async_completion(
    OfonoConnCtx* obj,
    const GError* error,
    void* arg)
{
    Context* ctx = arg;
    if (error) {
        GERR("%s", GERRMSG(error));
        ctx->app->ret = RET_ERR;
    }
    app_async_done(ctx->app);
}

static
int
action_set_string(
    Context* ctx,
    const char* name,
    const char* value)
{
    if (ofono_connctx_set_string_full(ctx->obj, name, value,
        action_async_completion, ctx)) {
        ctx->app->in_progress++;
        return RET_OK;
    } else {
        return RET_ERR;
    }
}

static
int
action_set_protocol(
    Context* ctx,
    int value)
{
    if (ofono_connctx_set_protocol_full(ctx->obj, value,
        action_async_completion, ctx)) {
        ctx->app->in_progress++;
        return RET_OK;
    } else {
        return RET_ERR;
    }
}

static
int
action_provision(
    Context* ctx)
{
    if (ofono_connctx_provision_full(ctx->obj, action_async_completion, ctx)) {
        ctx->app->in_progress++;
        return RET_OK;
    } else {
        return RET_ERR;
    }
}

static
void
action_deactivate_done(
    OfonoConnCtx* ctx,
    void* arg)
{
    if (!ctx->active) {
        action_finish(arg);
    }
}

static
int
action_deactivate(
    Context* ctx)
{
    GASSERT(!ctx->action_id);
    if (ctx->obj->active) {
        ctx->app->in_progress++;
        ctx->action_id = ofono_connctx_add_active_changed_handler(ctx->obj,
            action_deactivate_done, ctx);
        ofono_connctx_deactivate(ctx->obj);
    }
    return RET_OK;
}

static
void
action_activate_done(
    OfonoConnCtx* ctx,
    void* arg)
{
    if (ctx->active) {
        action_finish(arg);
    }
}

static
int
action_activate(
    Context* ctx)
{
    GASSERT(!ctx->action_id);
    if (!ctx->obj->active) {
        ctx->app->in_progress++;
        ctx->action_id = ofono_connctx_add_active_changed_handler(ctx->obj,
            action_activate_done, ctx);
        ofono_connctx_activate(ctx->obj);
    }
    return RET_OK;
}

static
int
action_list(
    Context* ctx)
{
    printf("%s\n", ofono_connctx_path(ctx->obj));
    return RET_OK;
}

static
int
add_context(
    App* app,
    OfonoConnCtx* obj)
{
    GError* error = NULL;
    if (ofono_connctx_wait_valid(obj, app->timeout, &error)) {
        Context* ctx = g_new0(Context, 1);
        ctx->app = app;
        ctx->obj = ofono_connctx_ref(obj);
        app->contexts = g_slist_append(app->contexts, ctx);
        return RET_OK;
    } else {
        return handle_error(error);
    }
}

static
int
handle_connmgr(
    App* app,
    OfonoConnMgr* cm)
{
    guint i;
    int ret = RET_OK;
    GPtrArray* contexts = ofono_connmgr_get_contexts(cm);
    for (i=0; i<contexts->len; i++) {
        const int status = add_context(app, contexts->pdata[i]);
        if (ret == RET_OK && status != RET_OK) ret = status;
    }
    g_ptr_array_unref(contexts);
    return ret;
}

static
int
handle_manager(
    App* app,
    OfonoManager* manager)
{
    guint i;
    int ret = RET_OK;
    const int timeout = app->timeout;
    GPtrArray* modems = ofono_manager_get_modems(manager);
    for (i=0; i<modems->len; i++) {
        OfonoModem* modem = modems->pdata[i];
        GError* error = NULL;
        if (ofono_modem_wait_valid(modem, timeout, &error)) {
            if (ofono_modem_has_interface(modem,OFONO_CONNMGR_INTERFACE_NAME)) {
                int status;
                const char* path = ofono_modem_path(modem);
                OfonoConnMgr* cm = ofono_connmgr_new(path);
                if (ofono_connmgr_wait_valid(cm, timeout, &error)) {
                    status = handle_connmgr(app, cm);
                } else {
                    status = handle_error(error);
                }
                if (ret == RET_OK && status != RET_OK) ret = status;
                ofono_connmgr_unref(cm);
            }
        } else {
            ret = handle_error(error);
        }
    }
    g_ptr_array_unref(modems);
    return ret;
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
void
context_free(
    gpointer data)
{
    Context* ctx = data;
    ofono_connctx_remove_handler(ctx->obj, ctx->action_id);
    ofono_connctx_unref(ctx->obj);
    g_free(ctx);
}

static
void
action_free(
    gpointer data)
{
    Action* action = data;
    action->free(action);
}

static
int
app_run(
    App* app)
{
    GError* error = NULL;
    OfonoManager* manager = NULL;
    if (app->timeout > 0) GDEBUG("Timeout %d sec", app->timeout);
    if (app->all) {
        manager = ofono_manager_new();
        app->ret = (ofono_manager_wait_valid(manager, app->timeout, &error) ?
            handle_manager(app, manager) : handle_error(error));
    } else if (app->path) {
        OfonoConnCtx* ctx = ofono_connctx_new(app->path);
        app->ret = add_context(app, ctx);
        ofono_connctx_unref(ctx);
    }
    if (app->ret == RET_OK) app_next_action(app);
    if (app->in_progress) {
        if (app->timeout > 0) {
            app->timeout_id = g_timeout_add_seconds(app->timeout,
                app_timeout, app);
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
    ofono_manager_unref(manager);
    g_slist_free_full(app->contexts, context_free);
    g_slist_free_full(app->actions, action_free);
    return app->ret;
}

static
gboolean
app_opt_activate(
    const char* option,
    const char* value,
    gpointer app,
    GError** error)
{
    return app_add_action(app, action_simple_new(action_activate));
}

static
gboolean
app_opt_deactivate(
    const char* option,
    const char* value,
    gpointer app,
    GError** error)
{
    return app_add_action(app, action_simple_new(action_deactivate));
}

static
gboolean
app_opt_provision(
    const char* option,
    const char* value,
    gpointer app,
    GError** error)
{
    return app_add_action(app, action_simple_new(action_provision));
}

static
gboolean
app_opt_list(
    const char* option,
    const char* value,
    gpointer app,
    GError** error)
{
    return app_add_action(app, action_simple_new(action_list));
}

static
gboolean
app_opt_set_str(
    const char* option,
    const char* value,
    gpointer app,
    GError** error)
{
    gboolean ok;
    gchar** s = g_strsplit_set(value, ":=", -1);
    if (g_strv_length(s) == 2) {
        ok = app_add_action(app, action_str_new(action_set_string, s[0], s[1]));
    } else {
         if (error) {
             *error = g_error_new(G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                 "Invalid property specification \'%s\'", value);
         }
         ok = FALSE;
    }
    g_strfreev(s);
    return ok;
}

static
gboolean
app_opt_set_enum(
    ActionEnumProc proc,
    const char* value,
    App* app,
    GError** error)
{
    gboolean ok;
    int ivalue = atoi(value);
    if (ivalue > 0) {
        ok = app_add_action(app, action_enum_new(proc, ivalue));
    } else {
         if (error) {
             *error = g_error_new(G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                 "Invalid value \'%s\'", value);
         }
         ok = FALSE;
    }
    return ok;
}

static
gboolean
app_opt_set_protocol(
    const char* option,
    const char* value,
    gpointer app,
    GError** error)
{
    return app_opt_set_enum(action_set_protocol, value, app, error);
}

static
gboolean
app_opt_verbose(
    const char* option,
    const char* value,
    gpointer data,
    GError** error)
{
    gutil_log_default.level = GLOG_LEVEL_VERBOSE;
    return TRUE;
}

static
gboolean
app_opt_quiet(
    const char* option,
    const char* value,
    gpointer data,
    GError** error)
{
    gutil_log_default.level = GLOG_LEVEL_ERR;
    return TRUE;
}

static
gboolean
app_init(
    App* app,
    int argc,
    char* argv[])
{
    gboolean ok = FALSE;
    GOptionEntry main_entries[] = {
        { "verbose", 'v', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
          app_opt_verbose, "Enable verbose output", NULL },
        { "quiet", 'q', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
          app_opt_quiet, "Be quiet", NULL },
        { "timeout", 't', 0, G_OPTION_ARG_INT,
          &app->timeout, "Timeout in seconds", "SECONDS" },
        { "all", 'a', 0, G_OPTION_ARG_NONE, &app->all,
          "Perform actions on all contexts", NULL },
        { NULL }
    };
    GOptionEntry action_entries[] = {
        { "list", 'l', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
          app_opt_list, "Print the context path", NULL },
        { "provision", 'p', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
          app_opt_provision, "Provision the context", NULL },
        { "activate", 'A', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
          app_opt_activate, "Activate the context", NULL },
        { "deactivate", 'd', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
          app_opt_deactivate, "Deactivate the context", NULL },
        { "protocol", 'P', 0, G_OPTION_ARG_CALLBACK,
          app_opt_set_protocol, "Set protocol", "PROTOCOL"},
        { "set", 's', 0, G_OPTION_ARG_CALLBACK,
          app_opt_set_str, "Set property", "NAME:VALUE" },
        { NULL }
    };
    GError* error = NULL;
    GOptionContext* options = g_option_context_new("[PATH]");
    GOptionGroup* actions = g_option_group_new("actions",
        "Action Options:", "Show all actions", app, NULL);
    g_option_context_add_main_entries(options, main_entries, NULL);
    g_option_group_add_entries(actions, action_entries);
    g_option_context_add_group(options, actions);
    if (g_option_context_parse(options, &argc, &argv, &error)) {
        if (argc == 2 || (argc == 1 && app->all)) {
            if (argc > 1) app->path = argv[1];
            ok = TRUE;
        } else {
            char* help = g_option_context_get_help(options, TRUE, NULL);
            fprintf(stderr, "You need to either specify the path or "
                "use -a option\n\n");
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
    gutil_log_set_type(GLOG_TYPE_STDERR, "ofono-provision");
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
