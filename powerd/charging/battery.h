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


#include <stdbool.h>
#include <luna-service2/lunaservice.h>

#include <nyx/nyx_client.h>

#ifndef __BATTERY_H__
#define __BATTERY_H__

/**
 * Structures
 */

extern struct battery_charge battery_params;

void BatteryCheckReason(int batterycheck);

bool BatteryIsPresent();
bool BatteryIsAuthentic();

int battery_init(void);

bool battery_present_sample(nyx_battery_status_t  *state);
bool battery_present(void);

int battery_get_percent(void);
int battery_get_temperature(void);
int battery_get_voltage(void);
int battery_get_current(void);

int battery_get_avg_current(void);
bool battery_authenticate(void);

double battery_get_full40(void);
double battery_get_rawcoulomb(void);
double battery_get_coulomb(void);
double battery_get_age(void);

const char * battery_status(void);

void battery_read(nyx_battery_status_t *state);
void battery_set_empty(nyx_battery_status_t *state);

void battery_search(bool on);

int battery_get_ctia_params(void);
void battery_set_wakeup_percentage(bool charging, bool suspend);
void battery_init_wakeup_params(void);



/**
 * Lunabus callbacks.
 */

bool batteryStatusQuery(LSHandle *sh, LSMessage *message, void *user_data);

/**
 * Lunabus signals
 */

int SendBatteryNotification(bool significant);
void sendBatteryStatus(void);

#endif // __BATTERY_H__
