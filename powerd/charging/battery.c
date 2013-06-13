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
 * @file battery.c
 *
 * @brief Battery interface calls to read the battery values.
 */

#include <string.h>
#include <syslog.h>
#include <stdbool.h>
#include <unistd.h>
#include <glib.h>
#include <cjson/json.h>
#include <luna-service2/lunaservice.h>

#include "init.h"
#include "debug.h"
#include "logging.h"
#include "main.h"
#include "battery.h"
#include "config.h"
#include "sysfs.h"

#define LOG_DOMAIN "BATTERY_IPC: "

static nyx_device_handle_t battDev = NULL;

nyx_battery_ctia_t battery_ctia_params;


void battery_read(nyx_battery_status_t *status)
{
	if(battDev == NULL)
		return;

	nyx_error_t err = nyx_battery_query_battery_status(battDev,status);

	if(err != NYX_ERROR_NONE)
	{
		POWERDLOG(LOG_ERR,"%s: nyx_battery_query_battery_status returned with error : %d",__func__,err);
		return;
	}
}


int battery_get_ctia_params(void)
{
	if(!battDev)
		return -1;
	nyx_error_t err = nyx_battery_get_ctia_parameters(battDev,&battery_ctia_params);

	if(err != NYX_ERROR_NONE)
	{
		POWERDLOG(LOG_ERR,"%s: nyx_battery_get_charge_parameters returned with error : %d",__func__,err);
		return -1;
	}

	return 0;
}



bool battery_authenticate()
{
	bool result;

    if (battery_ctia_params.skip_battery_authentication)
    	return true;
	nyx_battery_authenticate_battery(battDev, &result);
	return result;
}

void battery_set_wakeup_percentage(bool charging, bool suspend)
{
	int battlowpercent[] = {20,13,11,9,6,5,4,3,2,1,0};
	nyx_battery_status_t batt;
	int nextchk = 0,i = 0;

	if(!battDev)
		return;

	POWERDLOG(LOG_DEBUG, "In %s\n",__FUNCTION__);
	battery_read(&batt);
	sendBatteryStatus();

	if(charging) {
		nextchk = 0;
	}
	else if(suspend) {
		for(i=0; battlowpercent[i]!=0; i++) {
			if(batt.percentage > battlowpercent[i])
			{
				nextchk=battlowpercent[i];
				break;
			}
		}
	}
	else
		nextchk=batt.percentage;

	POWERDLOG(LOG_DEBUG, "Setting percent limit to %d\n",nextchk);

	nyx_battery_set_wakeup_percentage(battDev, nextchk);
}


#define SYSFS_DEVICE "/sys/devices/w1 bus master/"

#define SYSFS_BATTERY_SEARCH          "w1_master_search"

const gchar *battery_search_file = SYSFS_DEVICE SYSFS_BATTERY_SEARCH;

void battery_search(bool on)
{
    g_debug("%s %s", __FUNCTION__, (on ? "On" : "Off"));

    if (on)
    {
        SysfsWriteString(battery_search_file, "-1");
    }
    else
    {
        SysfsWriteString(battery_search_file, "0");
    }
}


/**
* @brief Generate a fake battery percentage for the UI to consume.
*        This allows us to make 95-100% appear like 100%.
*
* @param  percent
*
* @retval
*/
static int getUiPercent(int percent)
{
    int min   =   0;
    int max   =  95;

    int x = percent;
    int range = max - min;

    if (x < min) x = min;
    else if (x > max) x = max;

    return (x - min) * 100 / (range);
}


bool batteryStatusQuery(LSHandle *sh,
                   LSMessage *message, void *user_data)
{
	nyx_battery_status_t status;
	if(!battDev)
		return false;

	nyx_error_t err = nyx_battery_query_battery_status(battDev,&status);

	if(err != NYX_ERROR_NONE)
	{
		POWERDLOG(LOG_ERR,"%s: nyx_charger_query_battery_status returned with error : %d",__func__,err);
	}
	int percent_ui = getUiPercent(status.percentage);


	POWERDLOG(LOG_INFO,
			"(%fmAh, %d%%, %d%%_ui, %dC, %dmA, %dmV)\n",
			status.capacity, status.percentage,
			percent_ui,
			status.temperature,
			status.current, status.voltage);

	GString *buffer = g_string_sized_new(500);
	g_string_append_printf(buffer,"{\"percent\":%d,\"percent_ui\":%d,"
				"\"temperature_C\":%d,\"current_mA\":%d,\"voltage_mV\":%d,"
				"\"capacity_mAh\":%f}",
		status.percentage,
		percent_ui,
		status.temperature,
		status.current,
		status.voltage,
		status.capacity);

	char *payload = g_string_free(buffer, FALSE);

	POWERDLOG(LOG_DEBUG,"%s: Sending payload : %s",__func__,payload);
	LSError lserror;
	LSErrorInit(&lserror);
    bool retVal = LSMessageReply(sh, message, payload,NULL);
	if (!retVal)
	{
		LSErrorPrint(&lserror, stderr);
		LSErrorFree(&lserror);
	}
	g_free(payload);
	return TRUE;
}

void machineShutdown(void)
{
	char *payload = g_strdup_printf("{\"reason\":\"Battery level is critical\"}");

	LSError lserror;
	LSErrorInit(&lserror);
	POWERDLOG(LOG_DEBUG,"%s: Sending payload : %s",__func__,payload);

	bool retVal = LSSignalSend(GetLunaServiceHandle(),
			"luna://com.palm.power/shutdown/machineOff",
			payload, &lserror);
	g_free(payload);

	if (!retVal)
	{
		LSErrorPrint(&lserror, stderr);
		LSErrorFree(&lserror);
	}
}

void sendBatteryStatus(void)
{
	nyx_battery_status_t status;
	if(!battDev)
		return;

	nyx_error_t err = nyx_battery_query_battery_status(battDev,&status);

	if(err != NYX_ERROR_NONE)
	{
		POWERDLOG(LOG_ERR,"%s: nyx_charger_query_battery_status returned with error : %d",__func__,err);
	}

	int percent_ui = getUiPercent(status.percentage);


	POWERDLOG(LOG_INFO,
			"(%fmAh, %d%%, %d%%_ui, %dC, %dmA, %dmV)\n",
			status.capacity, status.percentage,
			percent_ui,
			status.temperature,
			status.current, status.voltage);

	GString *buffer = g_string_sized_new(500);
	g_string_append_printf(buffer,"{\"percent\":%d,\"percent_ui\":%d,"
				"\"temperature_C\":%d,\"current_mA\":%d,\"voltage_mV\":%d,"
				"\"capacity_mAh\":%f}",
		status.percentage,
		percent_ui,
		status.temperature,
		status.current,
		status.voltage,
		status.capacity);

	char *payload = g_string_free(buffer, FALSE);

	POWERDLOG(LOG_DEBUG,"%s: Sending payload : %s",__func__,payload);
	LSError lserror;
	LSErrorInit(&lserror);
	bool retVal = LSSignalSend(GetLunaServiceHandle(),
		"luna://com.palm.powerd/com/palm/power/batteryStatus",
		payload, &lserror);
	if (!retVal)
	{
		LSErrorPrint(&lserror, stderr);
		LSErrorFree(&lserror);
	}

	g_free(payload);
	return;
}


void notifyBatteryStatus(nyx_device_handle_t handle, nyx_callback_status_t status, void* data)
{
	sendBatteryStatus();
}

bool batteryStatusQuerySignal(LSHandle *sh,
                   LSMessage *message, void *user_data)
{
	sendBatteryStatus();
	return true;
}

static
bool BatteryDummyValues(int percent, int temp_C, int current_mA, int voltage_mV, float capacity_mAh)
{
	#define FAKEBATT	"/tmp/fakebattery/"

	char value[256];
	int ret = 0;

	snprintf(value,sizeof(value),"mkdir %s; touch %s/percentage; touch %s/temperature; "
		"touch %s/voltage; touch %s/current ; touch %s/capacity",FAKEBATT,FAKEBATT,
		FAKEBATT,FAKEBATT,FAKEBATT,FAKEBATT);

	system(value);

	snprintf(value,sizeof(value),"%d",percent);
	ret = SysfsWriteString(FAKEBATT "percentage",value);
	if(ret)
		return false;

	snprintf(value,sizeof(value),"%d",temp_C);
	ret = SysfsWriteString(FAKEBATT "temperature",value);
	if(ret)
		return false;

	snprintf(value,sizeof(value),"%d",current_mA);
	ret = SysfsWriteString(FAKEBATT "current",value);
	if(ret)
		return false;

	snprintf(value,sizeof(value),"%d",voltage_mV);
	ret = SysfsWriteString(FAKEBATT "voltage",value);
	if(ret)
		return false;

	snprintf(value,sizeof(value),"%8.3f",capacity_mAh);
	ret = SysfsWriteString(FAKEBATT "capacity",value);
	if(ret)
		return false;

	return true;
}


bool
fakeBatteryStatus(LSHandle *sh,
                   LSMessage *message, void *user_data)
{
    /* Ignore the successful registration. */
    if (strcmp(LSMessageGetMethod(message), LUNABUS_SIGNAL_REGISTERED) == 0)
    {
         return true;
    }

    const char *payload = LSMessageGetPayload(message);
    struct json_object *object = json_tokener_parse(payload);
    if (is_error(object)) {
    	goto end;
    }

    int percent; int temp_C; int current_mA; int voltage_mV;
    float capacity_mAh;

    percent = json_object_get_int(
            json_object_object_get(object, "percent"));

    temp_C = json_object_get_int(
            json_object_object_get(object, "temperature_C"));

    current_mA = json_object_get_int(
            json_object_object_get(object, "current_mA"));

    voltage_mV = json_object_get_int(
            json_object_object_get(object, "voltage_mV"));

    capacity_mAh = json_object_get_double(
            json_object_object_get(object, "capacity_mAh"));

    if(!BatteryDummyValues(percent,temp_C,current_mA,voltage_mV,capacity_mAh))
    {
    	POWERDLOG(LOG_ERR,"Unable to load fake battery values");
    }

    g_debug("%s %f mAh, P: %d%%, T: %d C, C: %d mA, V: %d mV",
        __FUNCTION__,capacity_mAh,percent,temp_C,
        current_mA, voltage_mV);

end:
    if (!is_error(object)) json_object_put(object);
    return true;
}


/**
 * @brief Initialize tha NYX api for charging and send the powerd config parameters.
 */

int BatteryInit(void)
{
	int ret = 0;
	nyx_error_t error = NYX_ERROR_NONE;
	nyx_device_iterator_handle_t iteraror = NULL;

	error = nyx_device_get_iterator(NYX_DEVICE_BATTERY, NYX_FILTER_DEFAULT, &iteraror);
	if(error != NYX_ERROR_NONE || iteraror == NULL) {
		 goto error;
	}
	else if (error == NYX_ERROR_NONE)
	{
		nyx_device_id_t id = NULL;

		while ((error = nyx_device_iterator_get_next_id(iteraror,
			&id)) == NYX_ERROR_NONE && NULL != id)
		{
			g_debug("Powerd: Battery device id \"%s\" found",id);
			error = nyx_device_open(NYX_DEVICE_BATTERY, id, &battDev);
			if(error != NYX_ERROR_NONE)
			{
				goto error;
			}
			break;
		}
	}

	LSError lserror;
	LSErrorInit(&lserror);
	bool retVal;
    retVal = LSCall(GetLunaServiceHandle(),
        "luna://com.palm.lunabus/signal/addmatch",
            "{\"category\":\"/com/palm/power\","
             "\"method\":\"batteryStatusQuery\"}",
             batteryStatusQuerySignal,
             NULL, NULL, &lserror);
    if (!retVal)
    	goto lserror;

    if (gChargeConfig.fake_battery)
    {
        retVal = LSCall(GetLunaServiceHandle(),
            "luna://com.palm.lunabus/signal/addmatch",
                "{\"category\":\"/com/palm/power\","
                 "\"method\":\"fakeBatteryStatus\"}",
                 fakeBatteryStatus,
                 NULL, NULL, &lserror);
        if (!retVal) goto lserror;
    }

	nyx_battery_register_battery_status_callback(battDev,notifyBatteryStatus,NULL);

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
	g_critical("Powerd: No battery device found\n");
	battDev = NULL;
//	abort();
	return 0;
}

INIT_FUNC(INIT_FUNC_FIRST, BatteryInit);

