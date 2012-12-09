/* @@@LICENSE
*
*      Copyright (c) 2007-2012 Hewlett-Packard Development Company, L.P.
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
* @brief  This file is a wrapper for the suspend/resume methods in "sleepd" component which will maintain the
* suspend/resume state-machine management. All the other components calling powerd suspend methods
* don't have to change their interface. However each of the methods in this file just does the
* task of calling the respective methods in the "sleepd" component.
*
* For eg: If a caller calls luna://com.palm.power/com/palm/power/identify {...}, the call will reach the powerd
* suspend interface , which will take the whole message ,and send it over to
* luna://com.palm.sleep/com/palm/power/identify {...}. Similarly com.palm.power/com/palm/power/identify sends the
* response from com.palm.sleep/com/palm/power/identify back to the original caller.
*
*/

#ifdef USE_DBUS
#include <lunaservice-dbus.h>
#endif

#include <syslog.h>
#include <glib.h>
#include <cjson/json.h>

#include "wait.h"
#include "main.h"
#include "debug.h"
#include "suspend.h"
#include "logging.h"
#include "lunaservice_utils.h"
#include "init.h"

#define LOG_DOMAIN "POWERD-SUSPEND: "

#define SLEEPD_SUSPEND_SERVICE "luna://com.palm.sleep/com/palm/power/"

/**
 * @defgroup Suspend Suspend & Activities
 * @ingroup SleepdCalls
 * @brief Suspend method calls
 */

/**
 * @addtogroup Suspend
 * @{
 */


/**
 * @brief Callback function called whenever there is a response from sleepd method call for any of the suspend
 * interface calls like identify , activityStart.
 *
 * @retval
 */

static bool
suspend_ipc_method_cb(LSHandle *sh, LSMessage *message, void *ctx)
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

/**
 * @brief Unregister a client from suspend ipc calls.
 * This call is different from other redirected calls, since it forwards the request to "clientCancelByName"
 * instead of "clientCancel", the reason being clientCancel removes a client by its id which is derived from
 * the original message. Since the message which sleepd gets is different from the original message it cannot
 * identify that client. So we use a new call clientCancelByName which will delete a client by its name which
 * remains the same.
 */

static bool
clientCancel(LSHandle *sh, LSMessage *message, void *ctx)
{
    LSCall(GetLunaServiceHandle(), SLEEPD_SUSPEND_SERVICE"clientCancelByName",
	       		LSMessageGetPayload(message), NULL,(void *)message, NULL, NULL);
    return true;
}

/**
 * @brief Start an activity.
 */
bool
activityStartCallback(LSHandle *sh, LSMessage *message, void *user_data)
{
	LSMessageRef(message);
	LSCall(GetLunaServiceHandle(), SLEEPD_SUSPEND_SERVICE"activityStart",
					       		LSMessageGetPayload(message), suspend_ipc_method_cb,(void *)message, NULL, NULL);
	return true;
}

/**
 * @brief End an activity.
 */

bool
activityEndCallback(LSHandle *sh, LSMessage *message, void *user_data)
{
	LSMessageRef(message);
	LSCall(GetLunaServiceHandle(), SLEEPD_SUSPEND_SERVICE"activityEnd",
					       		LSMessageGetPayload(message), suspend_ipc_method_cb,(void *)message, NULL, NULL);
	return true;
}


/**
 * @brief Register a new client.
 */
bool
identifyCallback(LSHandle *sh, LSMessage *message, void *data)
{
	LSMessageRef(message);
	LSCall(GetLunaServiceHandle(), SLEEPD_SUSPEND_SERVICE"identify",
					       		LSMessageGetPayload(message), suspend_ipc_method_cb,(void *)message, NULL, NULL);

	struct json_object *object = json_tokener_parse(LSMessageGetPayload(message));
	if ( is_error(object) ) {
		goto out;
	}

	bool subscribe = json_object_get_boolean(json_object_object_get(object, "subscribe"));

	if (subscribe)
	{
		LSSubscriptionAdd(sh, "PowerdClients", message, NULL);
	}
out:
	return true;
}

/**
 * @brief Force the device to suspend
 */
bool
forceSuspendCallback(LSHandle *sh, LSMessage *message, void *user_data)
{
	LSMessageRef(message);
	LSCall(GetLunaServiceHandle(), SLEEPD_SUSPEND_SERVICE"forceSuspend",
					       		LSMessageGetPayload(message), suspend_ipc_method_cb,(void *)message, NULL, NULL);
	return true;
}

/**
 * @brief Test call to schedule idleCheck thread to suspend the device.
 */
bool
TESTSuspendCallback(LSHandle *sh, LSMessage *message, void *user_data)
{
	LSMessageRef(message);
	LSCall(GetLunaServiceHandle(), SLEEPD_SUSPEND_SERVICE"TESTSuspend",
					       		LSMessageGetPayload(message), suspend_ipc_method_cb,(void *)message, NULL, NULL);
	return true;
}


/**
 * @brief Register for "suspendRequest" notifications.
 */
bool
suspendRequestRegister(LSHandle *sh, LSMessage *message, void *data)
{
	LSMessageRef(message);
	LSCall(GetLunaServiceHandle(), SLEEPD_SUSPEND_SERVICE"suspendRequestRegister",
					       		LSMessageGetPayload(message), suspend_ipc_method_cb,(void *)message, NULL, NULL);
	return true;
}

/**
 * @brief Ack the "suspendRequest" signal.
 */
bool
suspendRequestAck(LSHandle *sh, LSMessage *message, void *data)
{
	LSMessageRef(message);
	LSCall(GetLunaServiceHandle(), SLEEPD_SUSPEND_SERVICE"suspendRequestAck",
					       		LSMessageGetPayload(message), suspend_ipc_method_cb,(void *)message, NULL, NULL);
	return true;
}

/**
 * @brief Register for "prepareSuspend" notifications.
 */

bool
prepareSuspendRegister(LSHandle *sh, LSMessage *message, void *data)
{
	LSMessageRef(message);
	LSCall(GetLunaServiceHandle(), SLEEPD_SUSPEND_SERVICE"prepareSuspendRegister",
					       		LSMessageGetPayload(message), suspend_ipc_method_cb,(void *)message, NULL, NULL);
	return true;
}

/**
 * @brief Ack the "prepareSuspend" signal.
 */

bool
prepareSuspendAck(LSHandle *sh, LSMessage *message, void *data)
{
	LSMessageRef(message);
	LSCall(GetLunaServiceHandle(), SLEEPD_SUSPEND_SERVICE"prepareSuspendAck",
					       		LSMessageGetPayload(message), suspend_ipc_method_cb,(void *)message, NULL, NULL);
	return true;
}

/** 
* @brief Turn on/off visual leds suspend via luna-service.
* 
* @retval
*/
bool
visualLedSuspendCallback(LSHandle *sh, LSMessage *message, void *data)
{
	LSMessageRef(message);
	LSCall(GetLunaServiceHandle(), SLEEPD_SUSPEND_SERVICE"visualLedSuspend",
				       		LSMessageGetPayload(message), suspend_ipc_method_cb,(void *)message, NULL, NULL);
	return true;
}

static int
SuspendIPCInit(void)
{
    bool retVal;
    LSError lserror;
    LSErrorInit(&lserror);

    retVal = LSSubscriptionSetCancelFunction(GetLunaServiceHandle(),
        clientCancel, NULL, &lserror);
    if (!retVal)
    {
        LSErrorPrint(&lserror, stderr);
        LSErrorFree(&lserror);
    }
    return 0;
}

INIT_FUNC(INIT_FUNC_END,SuspendIPCInit);

/* @} END OF Suspend */
