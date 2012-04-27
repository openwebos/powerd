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
* @file shutdown.c
* 
* 
* @brief  This file is a wrapper for the shutdown methods in "sleepd" component which will do the
* actual shutdown management. All the other components calling palm://com.palm.power/shutdown methods
* don't have to change their interface. However each of the methods in this file just does
* the task of calling the respective methods in the "sleepd" component.
*
* For eg: If a caller calls luna://com.palm.power/shutdown/initiate {...}, the call will reach the powerd
* shutdown interface , which will take the whole message ,and send it over to
* luna://com.palm.sleep/shutdown/initiate {...}. Similarly com.palm.power/shutdown/initiate sends the
* response from com.palm.sleep/shutdown/initiate back to the original caller.
*
*/

#include <glib.h>
#include <lunaservice.h>

#include <cjson/json.h>

#include <syslog.h>

#include "lunaservice_utils.h"
#include "debug.h"
#include "init.h"
#include "main.h"
#include "logging.h"

#define LOG_DOMAIN "SHUTDOWN: "

#define SLEEPD_SHUTDOWN_SERVICE "luna://com.palm.sleep/shutdown/"

/**
 * @defgroup Shutdown Shutdown
 * @ingroup SleepdCalls
 * @brief Shutdown method calls
 */

/**
 * @addtogroup Shutdown
 * @{
 */


/**
 * @brief Callback function called whenever there is a response from sleepd method call for any of the shutdown
 * interface calls like initiateShutdown, machineOff, machineReboot.
 *
 * @retval
 */
static bool
shutdown_method_cb(LSHandle *sh, LSMessage *message, void *ctx)
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
 * @brief Initiate the shutdown sequence.
 */
static bool
initiateShutdown(LSHandle *sh, LSMessage *message, void *user_data)
{
    LSMessageRef(message);
    LSCall(GetLunaServiceHandle(), SLEEPD_SHUTDOWN_SERVICE"initiate",
	       		LSMessageGetPayload(message), shutdown_method_cb,(void *)message, NULL, NULL);
    return true;

}

/** 
* @brief Called by test code to reset state machine to square 1.
* 
* @retval
*/
static bool
TESTresetShutdownState(LSHandle *sh, LSMessage *message, void *user_data)
{
    LSMessageRef(message);
    LSCall(GetLunaServiceHandle(), SLEEPD_SHUTDOWN_SERVICE"TESTresetShutdownState",
	       		LSMessageGetPayload(message), shutdown_method_cb,(void *)message, NULL, NULL);
    return true;
}


/**
 * @brief Ack the "shutdownApplications" signal.
 */
static bool
shutdownApplicationsAck(LSHandle *sh, LSMessage *message,
                             void *user_data)
{
    LSMessageRef(message);
    LSCall(GetLunaServiceHandle(), SLEEPD_SHUTDOWN_SERVICE"shutdownApplicationsAck",
	       		LSMessageGetPayload(message), shutdown_method_cb,(void *)message, NULL, NULL);
    return true;
}

/**
 * @brief Ack the "shutdownServices" signal.
 */
static bool
shutdownServicesAck(LSHandle *sh, LSMessage *message,
                             void *user_data)
{
	LSMessageRef(message);
	LSCall(GetLunaServiceHandle(), SLEEPD_SHUTDOWN_SERVICE"shutdownServicesAck",
		       		LSMessageGetPayload(message), shutdown_method_cb,(void *)message, NULL, NULL);
	return true;
}


/**
 * @brief Register a client for "shutdownApplications" signal.
 */
static bool
shutdownApplicationsRegister(LSHandle *sh, LSMessage *message,
                             void *user_data)
{
	 LSMessageRef(message);
	 LSCall(GetLunaServiceHandle(), SLEEPD_SHUTDOWN_SERVICE"shutdownApplicationsRegister",
		       		LSMessageGetPayload(message), shutdown_method_cb,(void *)message, NULL, NULL);
	 bool retVal;
	 LSError lserror;
	 LSErrorInit(&lserror);

	 retVal = LSSubscriptionAdd(sh, "shutdownClient", message, &lserror);
	 if (!retVal)
	 {
		 g_critical("LSSubscriptionAdd failed.");
		 LSErrorPrint(&lserror, stderr);
		 LSErrorFree(&lserror);
	 }
	 return true;
}

/**
 * @brief Register a client for "shutdownServices" signal.
 */

static bool
shutdownServicesRegister(LSHandle *sh, LSMessage *message,
                             void *user_data)
{
	LSMessageRef(message);
	LSCall(GetLunaServiceHandle(), SLEEPD_SHUTDOWN_SERVICE"shutdownServicesRegister",
			       		LSMessageGetPayload(message), shutdown_method_cb,(void *)message, NULL, NULL);
    bool retVal;
    LSError lserror;
    LSErrorInit(&lserror);

    retVal = LSSubscriptionAdd(sh, "shutdownClient", message, &lserror);
    if (!retVal)
    {
        g_critical("LSSubscriptionAdd failed.");
        LSErrorPrint(&lserror, stderr);
        LSErrorFree(&lserror);
    }
	return true;
}

/**
 * @brief Shutdown the machine.
 */
static bool
machineOff(LSHandle *sh, LSMessage *message,
                             void *user_data)
{
	LSMessageRef(message);
	LSCall(GetLunaServiceHandle(), SLEEPD_SHUTDOWN_SERVICE"machineOff",
			       		LSMessageGetPayload(message), shutdown_method_cb,(void *)message, NULL, NULL);
	return true;
}

/**
 * @brief Reboot the machine.
 */

static bool
machineReboot(LSHandle *sh, LSMessage *message,
                             void *user_data)
{
	LSMessageRef(message);
	LSCall(GetLunaServiceHandle(), SLEEPD_SHUTDOWN_SERVICE"machineReboot",
			       		LSMessageGetPayload(message), shutdown_method_cb,(void *)message, NULL, NULL);
	return true;
}

LSMethod shutdown_methods[] = {
    { "initiate", initiateShutdown, },

    { "shutdownApplicationsRegister", shutdownApplicationsRegister },
    { "shutdownApplicationsAck", shutdownApplicationsAck },

    { "shutdownServicesRegister", shutdownServicesRegister },
    { "shutdownServicesAck", shutdownServicesAck },

    { "TESTresetShutdownState", TESTresetShutdownState },

    { "machineOff", machineOff },
    { "machineReboot", machineReboot },

    { },
};

static int
shutdown_init(void)
{
    LSError lserror;
    LSErrorInit(&lserror);

    if (!LSRegisterCategory(GetLunaServiceHandle(),
            "/shutdown", shutdown_methods, NULL, NULL, &lserror))
    {
        goto error;
    }

    return 0;

error:
    LSErrorPrint(&lserror, stderr);
    LSErrorFree(&lserror);
    return -1;
}

INIT_FUNC(INIT_FUNC_MIDDLE, shutdown_init);

/* @} END OF Shutdown */
