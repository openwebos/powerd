/* @@@LICENSE
*
*      Copyright (c) 2007-2013 LG Electronics, Inc.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* LICENSE@@@ */


#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <glib.h>
#include <pthread.h>
#include <stdbool.h>
#include <getopt.h>
#include <stdlib.h>

#include <luna-service2/lunaservice.h>

#include "init.h"
#include "debug.h"
#include "timesaver.h"
#include "logging.h"

static GMainLoop *mainloop = NULL;
static LSHandle* private_sh = NULL;
static LSPalmService *psh = NULL;

bool powerd_debug = false;
bool powerd_is_running = false;

void
term_handler(int signal)
{
    powerd_is_running = false;
    g_main_loop_quit(mainloop);
}


GMainContext *
GetMainLoopContext(void)
{
    return g_main_loop_get_context(mainloop);
}

LSHandle *
GetLunaServiceHandle(void)
{
    return private_sh;
}

LSPalmService *
GetPalmService(void)
{
    return psh;
}
int
main(int argc, char **argv)
{
    bool retVal;

    gboolean debug = FALSE;
    gboolean fake_battery = FALSE;
    gboolean visual_leds_suspend = FALSE;
    gboolean verbose = FALSE;
    gboolean err_on_crit = FALSE;
    gboolean fasthalt = FALSE;
    gint maxtemp = 0;
    gint temprate = 0;


    GOptionEntry entries[] = {
        {"debug", 'd', 0, G_OPTION_ARG_NONE, &debug, "turn debug logging on", NULL},
        {"use-fake-battery", 'b', 0, G_OPTION_ARG_NONE, &fake_battery, "Use fake battery", NULL},
        {"visual-leds-suspend", 'l', 0, G_OPTION_ARG_NONE, &visual_leds_suspend, "Use LEDs to show wake/suspend state", NULL},
        {"verbose-syslog", 'v', 0, G_OPTION_ARG_NONE, &verbose, "Use Verbose syslog output", NULL},
        {"error-on-critical", 'e', 0, G_OPTION_ARG_NONE, &err_on_crit, "Crash on critical error", NULL},
        {"maxtemp", 'M', 0, G_OPTION_ARG_INT, &maxtemp, "Set maximum temperature before shutdown (default 60)", NULL},
        {"temprate", 'T', 0, G_OPTION_ARG_INT, &temprate, "Expected maxiumum temperature slew rate (default 12)", NULL},
        {"fasthalt", 'F', 0, G_OPTION_ARG_NONE, &fasthalt, "On overtemp, shut down quickly not cleanly", NULL},
        { NULL }
    };

    GError *error = NULL;
    GOptionContext *ctx;
    ctx = g_option_context_new(" - power daemon");
    g_option_context_add_main_entries(ctx, entries, NULL);
    if (!g_option_context_parse(ctx, &argc, &argv, &error)) {
        g_critical("option parsing failed: %s", error->message);
        exit(1);
    }

    g_option_context_free (ctx);

    // FIXME integrate this into TheOneInit()
    LOGInit();
    LOGSetHandler(LOGSyslog);

    if (debug) {
        powerd_debug = true;

        LOGSetLevel(G_LOG_LEVEL_DEBUG);
        LOGSetHandler(LOGGlibLog);
    }
   
    signal(SIGTERM, term_handler);
    signal(SIGINT, term_handler);

    if (!g_thread_supported ()) g_thread_init (NULL);

    mainloop = g_main_loop_new(NULL, FALSE);

    /**
     *  initialize the lunaservice and we want it before all the init
     *  stuff happening.
     */
    LSError lserror;
    LSErrorInit(&lserror);

    retVal = LSRegisterPalmService("com.palm.power", &psh, &lserror);
    if (!retVal)
    {
        goto ls_error;
    }

    retVal = LSGmainAttachPalmService(psh, mainloop, &lserror);
    if (!retVal)
    {
        goto ls_error;
    }

    private_sh = LSPalmServiceGetPrivateConnection(psh);

    /**
     * Calls the init functions of all the modules in priority order.
     */
    TheOneInit();

    g_main_loop_run(mainloop);

end:
    g_main_loop_unref(mainloop);

    // save time before quitting...
    timesaver_save();

    return 0;
ls_error:
    g_critical("Fatal - Could not initialize powerd.  Is LunaService Down?. %s",
        lserror.message);
    LSErrorFree(&lserror);
    goto end;
}
