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
 * @file init.c
 *
 * @brief Utility for doing timed waits with a monotonic clock.
 */

#include <assert.h>

#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <stdbool.h>
#include <glib.h>

#include "wait.h"
#include "clock.h"
#include "debug.h"

#if !defined(_GNU_SOURCE)
#endif

#if (_POSIX_C_SOURCE - 0) >= 200112L
    #define HAVE_PTHREAD_CONDATTR_SETCLOCK
#else
    //#error Powerd requires pthread extensions for using monotonic clock.
    //#error warn Please use glibc >= 2.7
#endif

void
WaitObjectInit(WaitObj *obj)
{
    assert(obj != NULL);

    pthread_mutex_init(&obj->mutex, NULL);

    pthread_condattr_init(&obj->condattr);

#ifdef HAVE_PTHREAD_CONDATTR_SETCLOCK
    pthread_condattr_setclock(&obj->condattr, CLOCK_MONOTONIC);
#endif

    pthread_cond_init(&obj->cond, &obj->condattr);
}

void
WaitObjectLock(WaitObj *obj)
{
    pthread_mutex_lock(&obj->mutex);
}

void
WaitObjectUnlock(WaitObj *obj)
{
    pthread_mutex_unlock(&obj->mutex);
}

/**
 * Wait up to ms milliseconds on WaitObj's condition variable.
 *
 * This function requires that wait object be locked.
 *
 * @returns 0 if woken by cond signal
 * @returns 1 if timed-out
 *
 * Warning the resolution level is that of system jiffies.
 */
int
WaitObjectWait(WaitObj *obj, int ms)
{
    struct timespec time;

    if (ms < 0)
    {
        time.tv_sec = -1;
        time.tv_nsec = 0;
    }
    else
    {
        time.tv_sec = ms/1000;
        time.tv_nsec = (ms % 1000) * 1000000000;
    }

    return WaitObjectWaitTimeSpec(obj, &time);
}

int
WaitObjectWaitTimeSpec(WaitObj *obj, struct timespec *delta)
{
    int ret = 1;
    struct timespec time;

    // wait object must be locked before use.
    g_assert(WaitObjectIsLocked(obj));
    g_assert(delta != NULL);

    if (-1 == delta->tv_sec) // wait forever
    {
        ret = pthread_cond_wait(&obj->cond, &obj->mutex);
        return (ret != 0);
    }

#ifdef HAVE_PTHREAD_CONDATTR_SETCLOCK
    ret = clock_gettime(CLOCK_MONOTONIC, &time);
#endif
    if (ret)
    {
        TRACE("%s: Error getting monotonic clock, "
              "using realtime clock instead.\n", __FUNCTION__);

        struct timeval tv;
        gettimeofday(&tv, NULL);

        time.tv_sec = tv.tv_sec;
        time.tv_nsec = tv.tv_usec * 1000;
    }

    // time += delta
    ClockAccum(&time, delta);

    return WaitObjectWaitAbsTime(obj, &time);
}

int
WaitObjectWaitAbsTime(WaitObj *obj, struct timespec *abstime)
{
    // wait object must be locked before use.
    g_assert(WaitObjectIsLocked(obj));
    g_assert(abstime != NULL);

    int ret = pthread_cond_timedwait(&obj->cond, &obj->mutex, abstime);

    if (ETIMEDOUT == ret)
    {
        // we timed-out
        return 1;
    }
    else if (0 == ret)
    {
        // we were awoken
        return 0;
    }
    else
    {
        // Ubuntu 9.04 toolchain didn't like TRACE_PERROR
        g_error("%s pthread_cond_timedwait failed (ret = %d).\n", __FUNCTION__, ret);

        return -1;
    }
}

void
WaitObjectSignal(WaitObj *obj)
{
    WaitObjectLock(obj);

    pthread_cond_signal(&obj->cond);

    WaitObjectUnlock(obj);
}

void
WaitObjectSignalUnlocked(WaitObj *obj)
{
    pthread_cond_signal(&obj->cond);
}

void
WaitObjectBroadcast(WaitObj *obj)
{
    WaitObjectLock(obj);

    pthread_cond_broadcast(&obj->cond);

    WaitObjectUnlock(obj);
}

void
WaitObjectBroadcastUnlocked(WaitObj *obj)
{
    pthread_cond_broadcast(&obj->cond);
}

bool
WaitObjectIsLocked(WaitObj *obj)
{
    int ret;

    ret = pthread_mutex_trylock(&obj->mutex);
    if (ret == EBUSY)
    {
        return true;
    }
    else
    {
        pthread_mutex_unlock(&obj->mutex);
        return false;
    }
}
