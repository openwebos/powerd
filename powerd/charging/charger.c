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
 * @file charging.c
 *
 * @brief A very minimal charging implementation just for letting the world know about the charging status.
 *
 */


#include <lunaservice.h>
#include <string.h>
#include <syslog.h>
#include <stdbool.h>
#include <unistd.h>
#include <cjson/json.h>

#include "init.h"
#include "debug.h"
#include "logging.h"
#include "main.h"
#include "batterypoll.h"
#include "charging_logic.h"
#include "batterypoll.h"
#include "config.h"

#define LOG_DOMAIN "CHG: "

#include <nyx/nyx_client.h>


static nyx_device_handle_t nyxDev = NULL;
nyx_charger_status_t currStatus;

const char *
ChargerNameToString(int type)
{
    if(type & NYX_CHARGER_PC_CONNECTED)
    	return "pc";
    else if(type & NYX_CHARGER_WALL_CONNECTED)
    	return "wall";
    else if(type & NYX_CHARGER_DIRECT_CONNECTED)
        return "direct";
    else
    	return "none";
}

/**
 * @brief Convert charger type enum to string
 */

const char *
ChargerTypeToString(int type)
{
	 if(type & NYX_CHARGER_USB_POWERED)
    	return "usb";
	 else if(type & NYX_CHARGER_INDUCTIVE_POWERED)
        return "inductive";
	 else
    	return "none";
}

bool ChargerIsConnected(void)
{
	return (currStatus.connected != 0);
}

bool ChargerIsCharging(void)
{
	return currStatus.is_charging;
}

bool
chargerStatusQuery(LSHandle *sh,
                   LSMessage *message, void *user_data)
{
	nyx_charger_status_t status;
	if(!nyxDev)
		return false;
	nyx_error_t err = nyx_charger_query_charger_status(nyxDev,&status);

	if(err != NYX_ERROR_NONE)
	{
		POWERDLOG(LOG_ERR,"%s: nyx_charger_query_battery_status returned with error : %d",__func__,err);
	}

	LSError lserror;
	LSErrorInit(&lserror);

	char *payload = g_strdup_printf("{\"DockConnected\":%s,\"DockPower\":%s,\"DockSerialNo\":\"%s\","
				"\"USBConnected\":%s,\"USBName\":\"%s\",\"Charging\":%s}",(status.connected & NYX_CHARGER_INDUCTIVE_CONNECTED) ? "true" : "false",
				(status.powered & NYX_CHARGER_INDUCTIVE_POWERED) ? "true" :"false",(strlen(status.dock_serial_number)) ? status.dock_serial_number : "NULL",
				(status.powered & NYX_CHARGER_USB_POWERED) ? "true" : "false",ChargerNameToString(status.connected),
				(status.is_charging) ? "true":"false");

	POWERDLOG(LOG_DEBUG,"%s: Sending payload : %s",__func__,payload);
	bool retVal = LSMessageReply(sh, message, payload,NULL);
	if (!retVal)
	{

		LSErrorPrint(&lserror, stderr);
		LSErrorFree(&lserror);
	}
	g_free(payload);
	return TRUE;
}

bool charger_changed = false;

void sendChargerStatus(void)
{
	nyx_charger_status_t status;
	if(!nyxDev)
		return;
	nyx_error_t err = nyx_charger_query_charger_status(nyxDev,&status);
	if(err != NYX_ERROR_NONE)
	{
		POWERDLOG(LOG_ERR,"%s: nyx_charger_query_charger_status returned with error : %d",__func__,err);
	}
	POWERDLOG(LOG_DEBUG,"In %s connected : %d:%d, powered : %d:%d",__func__,currStatus.connected,status.connected,currStatus.powered,status.powered);

	if(currStatus.connected != status.connected || currStatus.powered != status.powered)
	{
		LSError lserror;
		LSErrorInit(&lserror);
		char *payload = g_strdup_printf("{\"DockConnected\":%s,\"DockPower\":%s,\"DockSerialNo\":\"%s\","
			"\"USBConnected\":%s,\"USBName\":\"%s\",\"Charging\":%s}",(status.connected & NYX_CHARGER_INDUCTIVE_CONNECTED) ? "true" : "false",
					(status.powered & NYX_CHARGER_INDUCTIVE_POWERED) ? "true" :"false",(strlen(status.dock_serial_number)) ? status.dock_serial_number : "NULL",
					(status.powered & NYX_CHARGER_USB_POWERED) ? "true" : "false",ChargerNameToString(status.connected),
					(status.is_charging) ? "true":"false");

		POWERDLOG(LOG_DEBUG,"%s: Sending payload : %s",__func__,payload);

		bool retVal = LSSignalSend(GetLunaServiceHandle(),
			"luna://com.palm.powerd/com/palm/power/USBDockStatus",
			payload, &lserror);
		if (!retVal)
		{
			LSErrorPrint(&lserror, stderr);
			LSErrorFree(&lserror);
			g_free(payload);
			return;
		}
	    g_free(payload);

		payload = g_strdup_printf("{\"type\":\"%s\",\"name\":\"%s\",\"connected\":%s,\"current_mA\":%d,\"message_source\":\"powerd\"}",
				ChargerTypeToString(status.powered),
				ChargerNameToString(status.connected),
				status.connected ? "true" : "false",
				status.charger_max_current);
		POWERDLOG(LOG_DEBUG,"%s: Sending payload : %s",__func__,payload);

		retVal = LSSignalSend(GetLunaServiceHandle(),
				"luna://com.palm.powerd/com/palm/power/chargerStatus",
				payload, &lserror);
		if (!retVal)
		{

			LSErrorPrint(&lserror, stderr);
			LSErrorFree(&lserror);
		}
		g_free(payload);
	}
	if(currStatus.connected != status.connected)
	{
	    char *payload = g_strdup_printf("{\"connected\":%s}",
	    		status.connected ? "true" : "false");

	    LSError lserror;
	    LSErrorInit(&lserror);
	    POWERDLOG(LOG_DEBUG,"%s: Sending payload : %s",__func__,payload);

	    bool retVal = LSSignalSend(GetLunaServiceHandle(),
	            "luna://com.palm.power/com/palm/power/chargerConnected",
	            payload, &lserror);
	    g_free(payload);

	    if (!retVal)
	    {
	        LSErrorPrint(&lserror, stderr);
	        LSErrorFree(&lserror);
	    }
	}

	memcpy(&currStatus,&status,sizeof(nyx_charger_status_t));

	// Iterate through both charging as well as battery state machines. Is this required ??
//	ChargingLogicUpdate(NYX_NO_NEW_EVENT);

	return;
}

void notifyChargerStatus(nyx_device_handle_t handle, nyx_callback_status_t status, void* data)
{
	sendChargerStatus();
}

void notifyStateChange(nyx_device_handle_t handle, nyx_callback_status_t status, void* data)
{
	nyx_charger_event_t new_event;

	nyx_error_t err = nyx_charger_query_charger_event(nyxDev,&new_event);
	if(err != NYX_ERROR_NONE)
	{
		POWERDLOG(LOG_ERR,"%s: nyx_charger_query_event returned with error : %d",__func__,err);
	}

    handle_charger_event(new_event);
}

bool
chargerStatusQuerySignal(LSHandle *sh,
                   LSMessage *message, void *user_data)
{
	sendChargerStatus();
	return true;
}

bool
chargerEnableCharging(int *max_charging_current)
{
	nyx_charger_status_t status;
	nyx_error_t err = nyx_charger_enable_charging(nyxDev,&status);
	if(err != NYX_ERROR_NONE)
	{
		POWERDLOG(LOG_ERR,"%s: nyx_charger_enable_charging returned with error : %d",__func__,err);
		return false;
	}

	*max_charging_current = currStatus.charger_max_current;
	battery_set_wakeup_percentage(true,false);
	return true;
}

bool
chargerDisableCharging(void)
{
	nyx_charger_status_t status;
	nyx_error_t err = nyx_charger_disable_charging(nyxDev,&status);
	if(err != NYX_ERROR_NONE)
	{
		POWERDLOG(LOG_ERR,"%s: nyx_charger_disable_charging returned with error : %d",__func__,err);
	}
	battery_set_wakeup_percentage(false,false);

	return true;
}

void getNewEvent(void)
{
	nyx_charger_event_t new_event;
	nyx_error_t err = nyx_charger_query_charger_event(nyxDev,&new_event);
	if(err != NYX_ERROR_NONE)
	{
		POWERDLOG(LOG_ERR,"%s: nyx_charger_query_event returned with error : %d",__func__,err);
	}

	handle_charger_event(new_event);
}

/**
 * @brief Initialize tha NYX api for charging and send the powerd config parameters.
 */

int ChargerInit(void)
{
	int ret = 0;
	nyx_init();

	nyx_error_t error = NYX_ERROR_NONE;
	nyx_device_iterator_handle_t iteraror = NULL;

	error = nyx_device_get_iterator(NYX_DEVICE_CHARGER, NYX_FILTER_DEFAULT, &iteraror);
	if(error != NYX_ERROR_NONE || iteraror == NULL) {
	   goto error;
	}
	else if (error == NYX_ERROR_NONE)
	{
		nyx_device_id_t id = NULL;
		while ((error = nyx_device_iterator_get_next_id(iteraror,
			&id)) == NYX_ERROR_NONE && NULL != id)
		{
			g_debug("Powerd: Charger device id \"%s\" found",id);
			error = nyx_device_open(NYX_DEVICE_CHARGER, id, &nyxDev);
			if(error != NYX_ERROR_NONE)
			{
				goto error;
			}
			break;
		}
	}

	memset(&currStatus,0,sizeof(nyx_charger_status_t));

	LSError lserror;
	LSErrorInit(&lserror);
	bool retVal;

	retVal = LSCall(GetLunaServiceHandle(),
		"luna://com.palm.lunabus/signal/addmatch",
			"{\"category\":\"/com/palm/power\","
			 "\"method\":\"chargerStatusQuery\"}",
			 chargerStatusQuerySignal, NULL, NULL, &lserror);
	if (!retVal)
		goto lserror;

	nyx_charger_register_charger_status_callback(nyxDev,notifyChargerStatus,NULL);

    if (!gChargeConfig.skip_battery_check && !gChargeConfig.disable_charging)
    	nyx_charger_register_state_change_callback(nyxDev,notifyStateChange,NULL);

out:
	if(iteraror)
		free(iteraror);
	return ret;

lserror:
	LSErrorPrint (&lserror, stderr);
	LSErrorFree (&lserror);
	ret = -1;
	goto out;

error:
	g_critical("Powerd: No charger device found\n");
	gChargeConfig.skip_battery_check = 1;
//	abort();
	return 0;
}

INIT_FUNC(INIT_FUNC_END, ChargerInit);
