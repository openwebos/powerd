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


#ifndef _LIBPOWERD_H_
#define _LIBPOWERD_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Interface to libpowerd.
 * See doxygen pages for more information.
 */

typedef void (*PowerdCallback)(void);
typedef void (*PowerdCallback_Int32)(int);
typedef void (*PowerdCallback_Int32_4)(int, int, int, int);
typedef void (*PowerdCallback_String_Int32)(const char *, int);

void PowerdClientInit(const char *clientName);
void PowerdClientDeinit(void);

void PowerdDeferIdle(void);

void PowerdForceSuspend(void);

void PowerdSuspendRequestRegister(PowerdCallback callback);

void PowerdSuspendRequestAck(bool allowSuspend);

void PowerdPrepareSuspendRegister(PowerdCallback callback);

void PowerdPrepareSuspendAck(bool finishedSuspend);

void PowerdResumeRegister(PowerdCallback callback);

void PowerdSuspendedRegister(PowerdCallback callback);

void PowerdGetChargerStatusNotification(void);

void PowerdChargerStatusRegister(PowerdCallback_String_Int32 callback);

void PowerdGetBatteryStatusNotification(void);

void PowerdBatteryStatusRegister(PowerdCallback_Int32_4 callback);

int PowerdSetDisplayMode(bool on);
int PowerdSetBacklightBrightness(int32_t percentBrightness);
int PowerdSetKeylightBrightness(int32_t percentBrightness);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // _LIBPOWERD_H_
