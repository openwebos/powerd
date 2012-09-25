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

#include <assert.h>

#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>

#include <luna-service2/lunaservice.h>

#include "init.h"
#include "debug.h"

void _PowerdClientIPCRun(void);
void _PowerdClientIPCStop(void);

void _LSHandleAttach(LSHandle *sh);

extern GMainLoop *gMainLoop;
extern bool gOwnMainLoop;

static PowerdHandle sHandle =
{
    .clientName = "",
    .clientId = NULL, 
    .suspendRequestRegistered = false,
    .prepareSuspendRegistered = false,
    .lock     = PTHREAD_MUTEX_INITIALIZER,
};

static void
PowerdHandleInit(PowerdHandle *handle)
{
    if (handle->clientId)
        g_free(handle->clientId);

    handle->clientName = "";
    handle->clientId = NULL; 
    handle->suspendRequestRegistered = false;
    handle->prepareSuspendRegistered = false;
}

PowerdHandle*
PowerdGetHandle(void)
{
    return &sHandle;
}

/** 
* @brief Register as a power-aware client with name 'clientName'.
* 
* @param  clientName 
*/
void
PowerdClientInit(const char *clientName)
{
    PowerdHandleInit(&sHandle);

    sHandle.clientName = clientName;

    _PowerdClientIPCRun();
}

/** 
* @brief Register as a power-aware client using the existing
*        LunaService handle.
* 
* @param  clientName 
* @param  sh 
*/
void
PowerdClientInitLunaService(const char *clientName, LSHandle *sh)
{
    _LSHandleAttach(sh);
    PowerdClientInit(clientName);
}

/** 
* @brief Use this mainloop instead of creating a powerd IPC
*        thread automatically.  This MUST be called before PowerdClientInit()
*        if you wish to use your own mainloop.
* 
* @param  mainLoop 
*/
void
PowerdGmainAttach(GMainLoop *mainLoop)
{
    if (mainLoop)
    {
        gMainLoop = g_main_loop_ref(mainLoop);
        gOwnMainLoop = true;
    }
    else
    {
        gMainLoop = NULL;
        gOwnMainLoop = false;
    }
}

void
PowerdClientLock(PowerdHandle *handle)
{
    int ret = pthread_mutex_lock(&sHandle.lock);
    assert(ret == 0);
}

void
PowerdClientUnlock(PowerdHandle *handle)
{
    int ret = pthread_mutex_unlock(&sHandle.lock);
    assert(ret == 0);
}

void
PowerdSetClientId(PowerdHandle *handle, const char *clientId)
{
    PowerdClientLock(handle);

    if (handle->clientId) g_free(handle->clientId);
    handle->clientId = g_strdup(clientId);

    PowerdClientUnlock(handle);
}

/** 
* @brief Stop being a powerd client.
*        Implicit in this is we disconnect from any communications
*        from powerd.
*/
void
PowerdClientDeinit(void)
{
    PowerdHandleInit(&sHandle);
    _PowerdClientIPCStop();

}
