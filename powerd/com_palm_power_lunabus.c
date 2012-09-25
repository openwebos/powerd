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
 * @file com_palm_power_lunabus.c
 *
 * @brief Register lunabus methods and signals for powerd.
 */

#include <luna-service2/lunaservice.h>
#include "main.h"
#include "init.h"

#define DECLARE_LSMETHOD(methodName) \
    bool methodName(LSHandle *handle, LSMessage *message, void *user_data)

DECLARE_LSMETHOD(identifyCallback);

DECLARE_LSMETHOD(batteryStatusQuery);
DECLARE_LSMETHOD(chargerStatusQuery);

DECLARE_LSMETHOD(suspendRequestRegister);
DECLARE_LSMETHOD(suspendRequestAck);

DECLARE_LSMETHOD(prepareSuspendRegister);
DECLARE_LSMETHOD(prepareSuspendAck);

DECLARE_LSMETHOD(forceSuspendCallback);

DECLARE_LSMETHOD(TESTSuspendCallback);

DECLARE_LSMETHOD(activityStartCallback);
DECLARE_LSMETHOD(activityEndCallback);

DECLARE_LSMETHOD(visualLedSuspendCallback);

DECLARE_LSMETHOD(TESTChargeStateFault);
DECLARE_LSMETHOD(TESTChargeStateShutdown);

LSMethod com_palm_power_methods[] = {

    { "batteryStatusQuery", batteryStatusQuery },
    { "chargerStatusQuery", chargerStatusQuery },

    /* suspend methods*/

    { "suspendRequestRegister", suspendRequestRegister },
    { "prepareSuspendRegister", prepareSuspendRegister },
    { "suspendRequestAck", suspendRequestAck },
    { "prepareSuspendAck", prepareSuspendAck },
    { "forceSuspend", forceSuspendCallback },
    { "identify", identifyCallback },

    { "visualLedSuspend", visualLedSuspendCallback },
    { "TESTSuspend", TESTSuspendCallback },

    { },
};

LSMethod com_palm_power_public_methods[] = {
    { "activityStart", activityStartCallback },
    { "activityEnd", activityEndCallback },
};

LSSignal com_palm_power_signals[] = {
    { "batteryStatus" },
    { "batteryStatusQuery" },
    { "chargerStatus" },
    { "chargerStatusQuery" },

    { "chargerConnected" },
    { "USBDockStatus" },
    /* Suspend signals */

    { "suspendRequest" },
    { "prepareSuspend" },
    { "suspended" },
    { "resume" },

    { },
};

static int
com_palm_power_lunabus_init(void)
{
    LSError lserror;
    LSErrorInit(&lserror);

    if (!LSPalmServiceRegisterCategory(GetPalmService(), "/com/palm/power",
        com_palm_power_public_methods, com_palm_power_methods, com_palm_power_signals,
        NULL, &lserror))
    {
        goto error;
    }
    return 0;

error:
    LSErrorPrint(&lserror, stderr);
    LSErrorFree(&lserror);
    return -1;
}

INIT_FUNC(INIT_FUNC_END, com_palm_power_lunabus_init);
