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
 * @file timeout_alarm.c
 *
 * For eg: If a caller calls luna://com.palm.power/timeout/set {...}, the call will reach the powerd
 * alarms interface , which will take the whole message ,and send it over to
 * luna://com.palm.sleep/timeout/set {...}. Similarly com.palm.power/timeout/set sends the response
 * from com.palm.sleep/timeout/set back to the original caller.
 *
 */

#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <cjson/json.h>
#include <sys/stat.h>
#include <unistd.h>
#include <luna-service2/lunaservice.h>

#include "main.h"

#include "logging.h"
#include "timesaver.h"

#include "lunaservice_utils.h"

#include "utils/init.h"
#include "utils/uevent.h"
#include "utils/timersource.h"

#include "powerd.h"

#include "timeout_alarm.h"

#define LOG_DOMAIN "POWERD-TIMEOUT: "

static LSPalmService *psh = NULL;

/**
 * @defgroup SleepdCalls Calls redirected to Sleepd component
 * @ingroup Powerd
 */


/**
 * @defgroup Alarms Alarms
 * @ingroup SleepdCalls
 * @brief Alarms for RTC wakeup.
 */

/**
 * @addtogroup Alarms
 * @{
 */


/**
 * @brief Callback function called whenever there is a response from sleepd method call for any of the alarms
 * interface calls like alarmAdd, timeout/set.
 *
 * @retval
 */
static bool
alarms_timeout_cb(LSHandle *sh, LSMessage *message, void *ctx)
{
	bool retVal;
	LSMessage *replyMessage = (LSMessage *)ctx;

	POWERDLOG(LOG_INFO,"%s: response with payload %s", __FUNCTION__, LSMessageGetPayload(message));
	if(replyMessage && LSMessageGetConnection(replyMessage))
	{
		retVal = LSMessageReply(LSMessageGetConnection(replyMessage), replyMessage, LSMessageGetPayload(message), NULL);
		if (!retVal)
		{
			POWERDLOG(LOG_WARNING, "%s could not send reply.", __FUNCTION__);
		}
		LSMessageUnref(replyMessage);
	}
	else
		POWERDLOG(LOG_CRIT,"%s: replyMessage is NULL",__func__);
    return true;
}

struct context
{
	LSMessage *replyMessage;
	int count;
};

/**
 * @brief This is a special callback function for the method alarmAdd. If the caller of this method sets the
 * "subscribe" option to true, this callback function is used, which is called twice , first time as a response
 * from alarmAdd call in sleepd and second time when the alarm actually fires. In both the cases the corresponding
 * response is sent to the original caller.
 *
 * @retval
 */

static bool
alarms_timeout_subscribe_cb(LSHandle *sh, LSMessage *message, void *ctx)
{
	bool retVal;
	struct context *alrm_ctx= (struct context *)ctx;
	const char *payload = LSMessageGetPayload(message);
    struct json_object *object = json_tokener_parse(payload);
    bool fired = json_object_get_boolean(
    		json_object_object_get(object, "fired"));

    POWERDLOG(LOG_INFO,"%s: response with payload %s, count : %d", __FUNCTION__, payload,alrm_ctx->count);

	if(alrm_ctx->replyMessage)
	{
		if(fired)
		{
			retVal = LSMessageReply(GetLunaServiceHandle(),alrm_ctx->replyMessage, payload, NULL);
			if (!retVal)
			{
				POWERDLOG(LOG_WARNING, "%s could not send reply.", __FUNCTION__);
			}
		}
		else if(LSMessageGetConnection(alrm_ctx->replyMessage))
		{
			retVal = LSMessageReply(LSMessageGetConnection(alrm_ctx->replyMessage), alrm_ctx->replyMessage, payload, NULL);
			if (!retVal)
			{
				POWERDLOG(LOG_WARNING, "%s could not send reply.", __FUNCTION__);
			}
		}
		alrm_ctx->count++;
	}
	else
		POWERDLOG(LOG_CRIT,"%s: replyMessage is NULL",__func__);

	if(alrm_ctx->count == 2) {
		LSMessageUnref(alrm_ctx->replyMessage);
		free(alrm_ctx);
	}

    return true;
}

/** 
* @brief Handle a timeout/set message and add a new power timeout.
*/
static bool
_power_timeout_set(LSHandle *sh, LSMessage *message, void *ctx)
{
    LSMessageRef(message);
    LSCallOneReply(GetLunaServiceHandle(), "palm://com.palm.sleep/timeout/set",
       		LSMessageGetPayload(message), alarms_timeout_cb,(void *)message, NULL, NULL);

    return true;

}

/**
 * @brief Handle a timeout/clear message and clear the given alarm.
 */

static bool
_power_timeout_clear(LSHandle *sh, LSMessage *message, void *ctx)
{
	LSMessageRef(message);
    LSCallOneReply(GetLunaServiceHandle(), "palm://com.palm.sleep/timeout/clear",
    		LSMessageGetPayload(message), alarms_timeout_cb, (void *)message, NULL, NULL);

    return true;
}


/**
 * @brief Add a new alarm based on calender time. Sets the callback function based on the "subscribe" option value.
 */
static bool
alarmAddCalendar(LSHandle *sh, LSMessage *message, void *ctx)
{
	struct json_object *object=NULL;
	struct context *alrm_ctx=NULL;
	alrm_ctx = malloc(sizeof(struct context));
	if(!alrm_ctx) goto error;
	memset(alrm_ctx,0,sizeof(struct context));

	object = json_tokener_parse(LSMessageGetPayload(message));
	if ( is_error(object) )
	{
		goto malformed_json;
	}

	struct json_object *subscribe_json =
	            json_object_object_get(object, "subscribe");

	bool subscribe = json_object_get_boolean(subscribe_json);

	LSMessageRef(message);
	if(subscribe) {
		alrm_ctx->replyMessage = message;
		LSCall(GetLunaServiceHandle(), "palm://com.palm.sleep/time/alarmAddCalender",
					LSMessageGetPayload(message), alarms_timeout_subscribe_cb, (void *)alrm_ctx, NULL, NULL);
	}
	else
		LSCall(GetLunaServiceHandle(), "palm://com.palm.sleep/time/alarmAddCalender",
			LSMessageGetPayload(message), alarms_timeout_cb, (void *)message, NULL, NULL);

	goto cleanup;

malformed_json:
	LSMessageReplyErrorBadJSON(sh, message);
	goto cleanup;
error:
	POWERDLOG(LOG_ERR,"Failed to allocate memory");
	LSMessageReplyErrorUnknown(sh, message);
cleanup:
	if (!is_error(object)) json_object_put(object);
	return true;

}

/**
 * @brief Add a new alarm based on relative time. Sets the callback function based on the "subscribe" option value.
 */

static bool
alarmAdd(LSHandle *sh, LSMessage *message, void *ctx)
{
	struct json_object *object = NULL;
	struct context *alrm_ctx = NULL;
	alrm_ctx = malloc(sizeof(struct context));
	if(!alrm_ctx) goto error;
	memset(alrm_ctx,0,sizeof(struct context));

	object = json_tokener_parse(LSMessageGetPayload(message));
	if ( is_error(object) )
	{
		goto malformed_json;
	}

	struct json_object *subscribe_json =
	            json_object_object_get(object, "subscribe");

	bool subscribe = json_object_get_boolean(subscribe_json);

	LSMessageRef(message);
	if(subscribe) {
		alrm_ctx->replyMessage = message;
		LSCall(GetLunaServiceHandle(), "palm://com.palm.sleep/time/alarmAdd",
					LSMessageGetPayload(message), alarms_timeout_subscribe_cb, (void *)alrm_ctx, NULL, NULL);
	}
	else
		LSCall(GetLunaServiceHandle(), "palm://com.palm.sleep/time/alarmAdd",
			LSMessageGetPayload(message), alarms_timeout_cb, (void *)message, NULL, NULL);

	goto cleanup;

malformed_json:
	LSMessageReplyErrorBadJSON(sh, message);
	goto cleanup;
error:
	POWERDLOG(LOG_ERR,"Failed to allocate memory");
	LSMessageReplyErrorUnknown(sh, message);
cleanup:
	if (!is_error(object)) json_object_put(object);
	return true;
}

/**
 * @brief Get info about the specified alarm.
 */

static bool
alarmQuery(LSHandle *sh, LSMessage *message, void *ctx)
{
	LSMessageRef(message);
	LSCallOneReply(GetLunaServiceHandle(), "palm://com.palm.sleep/time/alarmQuery",
			LSMessageGetPayload(message), alarms_timeout_cb, (void *)message, NULL, NULL);

	return true;
}

/**
 * @brief Delete an alarm.
 */

static bool
alarmRemove(LSHandle *sh, LSMessage *message, void *ctx)
{
	LSMessageRef(message);
	LSCallOneReply(GetLunaServiceHandle(), "palm://com.palm.sleep/time/alarmRemove",
			LSMessageGetPayload(message), alarms_timeout_cb, (void *)message, NULL, NULL);

	return true;
}


/**
 * @brief New Alarm interface methods.
 */

static LSMethod timeout_methods[] = {
    { "set", _power_timeout_set },
    { "clear", _power_timeout_clear },
    { },
};

/**
 * @brief Old Alarm interface methods.
 */

LSMethod time_methods[] = {

    { "alarmAddCalendar", alarmAddCalendar },
    { "alarmAdd", alarmAdd },
    { "alarmQuery", alarmQuery },
    { "alarmRemove", alarmRemove },
    { },
};

/** 
* @brief Called when a new time change event occurs when
*        the time is set.
*/
static void
_timechange_callback(void)
{
    g_debug(LOG_DOMAIN "%s ", __FUNCTION__);

    // Make a luna-call to update timeouts

    timesaver_save();
}

static int
_power_timeout_init(void)
{
    /* Set up luna service */


    psh = GetPalmService();

    LSError lserror;
    LSErrorInit(&lserror);
    if (!LSPalmServiceRegisterCategory(psh,
                "/timeout", timeout_methods /*public*/, NULL /*private*/, NULL, NULL, &lserror)) {
        POWERDLOG(LOG_ERR, "%s could not register category: %s",
                __FUNCTION__, lserror.message);
        LSErrorFree(&lserror);
        goto error;
    }

    if (!LSRegisterCategory(GetLunaServiceHandle(),
          "/time", time_methods, NULL, NULL, &lserror))
      {
          goto error;
      }

    UEventListen("/com/palm/powerd/timechange/uevent", _timechange_callback);

    return 0;

error:
    return -1;
}

INIT_FUNC(INIT_FUNC_END, _power_timeout_init);

/* @} END OF Alarms */
