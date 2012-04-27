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
 * @file batterypoll.c
 *
 * @brief This file implements the battery poll state machine with the following states:
 *
 * 1. Removed: This state implies the battery is disconnected from the system.
 *
 * 2. Inserted : In the "removed" state if battery is detected ( if battery voltage is positive), then the
 * battery state machine enters the "inserted" state.
 *
 * 3. Authentic : In "inserted" state if the battery authentication is successful, the state machine enters
 * the "authentic" state.
 *
 * 4. Notauthentic : This is the state reached if the battery authentication is unsuccessful.
 *
 * (Note : In all the A6 devices, battery authentication has been skipped from powerd side, so by default state
 * "inserted" would iterate to state "authentic", and will never go to state "notauthentic")
 *
 * 5. Debounce : If in "authentic" or "notauthentic" states, battery is not detected, it enters the "debounce"
 *  state, where the battery is checked again thrice , once every second, and if battery is still not detected,
 *  it enters the "removed" state. However if battery is detected, it goes to the "inserted" state.
 *  However in case of A6 devices, battery presence or absence is notified by interrupt, and in that case, the
 *  "debounce" state is not used as this interrupt is reliable. So the state machine directly goes to the
 *  "removed" state.
 *
 */

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>

#include "common.h"
#include "config.h"
#include "battery.h"
#include "batterypoll.h"

#include "timersource.h"
#include "charging_logic.h"
#include "logging.h"
#include "utils/sysfs.h"
#include "init.h"

#define LOG_DOMAIN "BATTERYPOLL: "


enum {
    kBatteryRemoved = 0,
    kBatteryDebounce,
    kBatteryInserted,
    kBatteryAuthentic,
    kBatteryNotAuthentic,
    kBatteryLast,
};
typedef int BatteryState;

static char *debug_battery_state[] =
{
    "removed",
    "debounce",
    "inserted",
    "authentic",
    "noauthentic",
    "last",
};

typedef BatteryState (*BatteryStateProc)(void);

typedef struct {
    BatteryState     state;
    BatteryStateProc function;
} BatteryStateNode;

static BatteryState StateRemoved(void);
static BatteryState StateDebounce(void);
static BatteryState StateInserted(void);
static BatteryState StateAuthentic(void);
static BatteryState StateNotAuthentic(void);

static const BatteryStateNode kStateMachine[] = {
    { kBatteryRemoved, StateRemoved },
    { kBatteryDebounce, StateDebounce },
    { kBatteryInserted, StateInserted },
    { kBatteryAuthentic, StateAuthentic },
    { kBatteryNotAuthentic, StateNotAuthentic },
};

/* @brief when bad samples > threshold, mark battery as removed. */
#define BAD_SAMPLES_THRESHOLD	3

static BatteryStateNode state_node;

extern struct battery_charge battery_params;

#define MAX_DISCHARGE_COUNT	25

static int discharge_count = 0;

/**
 * @defgroup Battery Battery
 * @ingroup Charging
 * @brief Battery State Machine :
 *
 * 1. Removed: This state implies the battery is disconnected from the system.
 *
 * 2. Inserted : In the "removed" state if battery is detected ( if battery voltage is positive), then the
 * battery state machine enters the "inserted" state.
 *
 * 3. Authentic : In "inserted" state if the battery authentication is successful, the state machine enters
 * the "authentic" state.
 *
 * 4. Notauthentic : This is the state reached if the battery authentication is unsuccessful.
 *
 * (Note : In all the A6 devices, battery authentication has been skipped from powerd side, so by default state
 * "inserted" would iterate to state "authentic", and will never go to state "notauthentic").
 *
 * 5. Debounce : If in "authentic" or "notauthentic" states, battery is not detected, it enters the "debounce"
 *  state, where the battery is checked again thrice , once every second, and if battery is still not detected,
 *  it enters the "removed" state. However if battery is detected, it goes to the "inserted" state.
 *  However in case of A6 devices, battery presence or absence is notified by interrupt, and in that case, the
 *  "debounce" state is not used as this interrupt is reliable. So the state machine directly goes to the
 *  "removed" state.
 */

/**
 * @addtogroup Battery
 * @{
 */


/**
 * @brief Check if the current battery state is "Authentic".
 */
bool BatteryIsAuthentic(void)
{
    return state_node.state == kBatteryAuthentic;
}

/**
 * @brief Check if the battery is present by comparing the current battery state to "removed" state.
 */
bool BatteryIsPresent(void)
{
    return state_node.state != kBatteryRemoved;
}

/**
 * @brief Check the current battery voltage and return true if its positive.
 */
bool battery_present_sample(nyx_battery_status_t *state)
{
    return state && state->voltage > 0;
}

/**
 * @brief The current battery percentage and temperature are compared to the ones from the last reading
 * and if any one of them have changed, implies the current sample is significant and should be reported
 * to the system.
 */
static bool sample_is_new(nyx_battery_status_t *state)
{
    static nyx_battery_status_t last = {0};
    bool percentChange = abs(state->percentage - last.percentage) > 0;
    bool tempChange = abs(state->temperature - last.temperature) > 1;

    if (percentChange || tempChange) {
        last = *state;
        return true;
    }
    return false;
}

/**
 * @brief The battery poll state machine is initialized to start from the "debounce" state.
 */
static void battery_state_init()
{
    state_node = kStateMachine[kBatteryDebounce];
}


/**
 * @brief Log the current state.
 */
static void battery_state_log()
{
    static BatteryState last_state = kBatteryLast;
    if (last_state != state_node.state) {
        POWERDLOG(LOG_INFO, "BatteryState %s",
            debug_battery_state[state_node.state]);
        last_state = state_node.state;
    }
}


/**
 * @brief Iterate through the battery poll state machine.
 *
 */
void battery_state_iterate()
{
    BatteryState next_state;
    do {
        battery_state_log();
        next_state = state_node.function();
        if (kBatteryLast != next_state) {
            state_node = kStateMachine[next_state];
        }
    } while (kBatteryLast != next_state);
}

/**
 * @brief State "debounce".
 */

static BatteryState StateDebounce(void)
{
    static int debounce_bad = 0;
    nyx_battery_status_t battery;

    battery_read(&battery);
    if (battery.present) {
        debounce_bad = 0;
        return kBatteryInserted;
    }
    else if (++debounce_bad > BAD_SAMPLES_THRESHOLD) {

        POWERDLOG(LOG_INFO, "Battery has been removed.\n");
        battery_search(true);
        return kBatteryRemoved;
    }
    return kBatteryLast;
}

/**
 * @brief State "removed".
 */
static BatteryState StateRemoved(void)
{
    nyx_battery_status_t battery;

    battery_read(&battery);

    if (battery.present)
        return kBatteryInserted;
    else
        return kBatteryLast;
}

/**
 * @brief State "inserted".
 */
static BatteryState StateInserted(void)
{
    battery_search(false);

    if (battery_authenticate()) {
        return kBatteryAuthentic;
    }
    else {
    	POWERDLOG(LOG_CRIT,"Battery authentication failure");
        return kBatteryNotAuthentic;
    }
}

/**
 * @brief Function called from both the states "authentic" and "notauthentic".
 */

static BatteryState StateAuthenticOrNot(void)
{
    nyx_battery_status_t battery;

    battery_read(&battery);

    if(ChargerIsCharging() && battery.current <= 0 )
    {
    	POWERDLOG(LOG_INFO,"%d: BATTERY DISCHARGING ....",discharge_count);
    	discharge_count++;
    }
    else
    	discharge_count = 0;

    if(discharge_count == MAX_DISCHARGE_COUNT)
    {
    	POWERDLOG(LOG_CRIT,"Battery discharging while on charger");
        discharge_count = 0;
    }

    if(!battery.present)
    {
        return kBatteryDebounce;
    }

    if(sample_is_new(&battery))
    	sendBatteryStatus();

    return kBatteryLast;
}

/**
 * @brief State "notauthentic".
 */
static BatteryState StateNotAuthentic(void)
{
    if (battery_authenticate()) {
        return kBatteryAuthentic;
    }
    else
    	POWERDLOG(LOG_CRIT,"Battery authentication failure");

    return StateAuthenticOrNot();
}

/**
 * @brief State "authentic".
 */
static BatteryState StateAuthentic(void)
{
	return StateAuthenticOrNot();
}

int batterypoll_init(void)
{
    battery_state_init();

    battery_state_iterate();
    discharge_count = 0;
    return 0;
}

INIT_FUNC(INIT_FUNC_END, batterypoll_init);

/* @} END OF Battery */
