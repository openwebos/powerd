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


/**
* @file suspend_ipc.c
*
* @brief This file contains helper calls for sleepd suspend/resume logic to perform all battery
* related stuff whenever the device suspends or resume. This is done by registering for "suspended" and
* "resume" signal.
*/

#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>

#include <syslog.h>
#include <cjson/json.h>

#include "utils/sysfs.h"

#include "suspend.h"

#include "init.h"
#include "clock.h"
#include "config.h"
#include "wait.h"
#include "debug.h"
#include "main.h"
#include "timesaver.h"
#include "timersource.h"
#include "logging.h"

#include "battery.h"
#include "timeout_alarm.h"

#define LOG_DOMAIN "POWERD-SUSPEND: "


#define kPowerBatteryCheckReasonSysfs "/sys/power/batterycheck_wakeup"
#define kPowerWakeupSourcesSysfs      "/sys/power/wakeup_event_list"

enum {
    kResumeTypeKernel,
    kResumeTypeActivity,
    kResumeTypeNonIdle
};

static char *resume_type_descriptions[] =
{
    "kernel",
    "powerd_activity",
    "powerd_non_idle",
};

struct timespec sTimeOnSuspended;
struct timespec sTimeOnWake;

struct timespec sSuspendRTC;
struct timespec sWakeRTC;

static const char *batterycheck_wakeup_string[BATTERYCHECK_END] = {
    "none",
    "threshold",
    "criticalbatt",
    "criticaltemp",
};

static int
_ParseBatteryCheck(const char *batterycheck_reason)
{
    int i;
    for (i = 0; i < BATTERYCHECK_END; i++)
    {
        if (strcmp(batterycheck_reason, batterycheck_wakeup_string[i]) == 0)
            return i;
    }
    return BATTERYCHECK_NONE;
}

static void
_ParseWakeupSources(int resumeType)
{
    int retval;
    char batterycheck_reason[1024] = {0};
    char wakeup_sources[1024] = {0};
    gchar ** wakeup_source_list = NULL;
    int i;

    if (kResumeTypeKernel != resumeType)
    {
        POWERDLOG(LOG_INFO, "Wakeup Source: [ 0.0 ] %s () %s (0)", resume_type_descriptions[resumeType], resume_type_descriptions[resumeType]);
        goto done;
    }

    /* kPowerBatteryCheckReasonSysfs doesn't exist on all systems, but
     * if it does, read it and parse it.  See NOV-104656
     */
    if (access(kPowerBatteryCheckReasonSysfs, R_OK) == 0)
    {
	retval = SysfsGetString(kPowerBatteryCheckReasonSysfs,
			batterycheck_reason, sizeof(batterycheck_reason));
	if (retval)
	{
	    goto cleanup;
	}

	BatteryCheckReason(
	    _ParseBatteryCheck(batterycheck_reason));
    }

    retval = SysfsGetString(kPowerWakeupSourcesSysfs, wakeup_sources, sizeof(wakeup_sources));
    if (retval || wakeup_sources[0] == '\0' )
        snprintf(wakeup_sources, sizeof(wakeup_sources), "[ 0.0 ] MISSING () MISSING (0)");
    else
        wakeup_source_list = g_strsplit(wakeup_sources, "\n", 0);


cleanup:
    POWERDLOG(LOG_DEBUG, "Powerd awoke with batterycheck %s.", batterycheck_reason);

    if (wakeup_source_list != NULL) {
        for (i=0; wakeup_source_list[i] != NULL ;i++) {
            POWERDLOG(LOG_INFO, "Wakeup Source: %s", wakeup_source_list[i]);
        }
        g_strfreev(wakeup_source_list);
    }

done:
    return;
}



bool resumeSignal(LSHandle *sh,
                   LSMessage *message, void *user_data)
{
	int resumetype;

	struct json_object *object = json_tokener_parse(LSMessageGetPayload(message));
	if (is_error(object)) goto out;

	bool registration = json_object_get_boolean(
						 json_object_object_get(object, "returnValue"));
	if(registration)
		goto out;

	resumetype = json_object_get_boolean(json_object_object_get(object, "resumetype"));

	if(resumetype <= kResumeTypeNonIdle)
	{
		battery_set_wakeup_percentage(false,false);
		_ParseWakeupSources(resumetype);
	}
out:
	if (!is_error(object)) json_object_put(object);

	return true;
}

bool suspendedSignal(LSHandle *sh,
                   LSMessage *message, void *user_data)
{
	struct json_object *object = json_tokener_parse(LSMessageGetPayload(message));
	if (is_error(object)) goto out;

	bool registration = json_object_get_boolean(
						 json_object_object_get(object, "returnValue"));
	if(registration)
		goto out;

	POWERDLOG(LOG_INFO,"Received Suspended signal");
	battery_set_wakeup_percentage(false,true);

out:
	if (!is_error(object)) json_object_put(object);

	return true;
}


static int
SuspendInit(void)
{
	bool retVal;
    LSError lserror;
    LSErrorInit(&lserror);

    retVal = LSCall(GetLunaServiceHandle(),
         "luna://com.palm.lunabus/signal/addmatch",
             "{\"category\":\"/com/palm/power\","
              "\"method\":\"resume\"}",
              resumeSignal,
              NULL, NULL, &lserror);

    if (!retVal) goto ls_error;

    retVal = LSCall(GetLunaServiceHandle(),
		  "luna://com.palm.lunabus/signal/addmatch",
			  "{\"category\":\"/com/palm/power\","
			   "\"method\":\"suspended\"}",
			   suspendedSignal,
			   NULL, NULL, &lserror);

    if (!retVal) goto ls_error;

ls_error:
	LSErrorFree(&lserror);
    return 0;
}

INIT_FUNC(INIT_FUNC_END,SuspendInit);
