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

#include <glib.h>

#include "charger.h"
#include "battery.h"

#ifndef _CHARGING_LOGIC_H_
#define _CHARGING_LOGIC_H_


typedef int ChargeState;
typedef ChargeState (*ChargeStateProc)(nyx_charger_event_t event);

struct ChargeStateNode {
    ChargeState state;
    ChargeStateProc function;
};


/** Charging state machine */

enum {
    kChargeStateIdle,
//    kChargeStateCritical,
//   kChargeStateCriticalWait,
    kChargeStateCharging,
    kChargeStateFault,
    kChargeStateChargeComplete,
    kChargeStateShutdown,
    kChargeStateShutdownWait,
    kChargeStateLast,
};

void ChargingLogicUpdate(nyx_charger_event_t event);

void ChargingLogicResetError(void);

bool BatteryDischarging(void);
void _current_sample_reset(void);

void TurnChargingOff(const char *reason);
bool BatteryLevelCritical(void);
gboolean _shutdown_watchdog(gpointer ctx);

bool BatteryOverchargeFault(nyx_battery_status_t *state);
bool TurnChargingON(void);

bool IsCharging(int current_mA);
void handle_charger_event(nyx_charger_event_t event);

bool trigger_charging_rdx_report(const char *cause, const char *detail, const char *logMessage);

#endif // _CHARGING_LOGIC_H_
