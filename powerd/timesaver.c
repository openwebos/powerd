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


#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>


#include "init.h"
#include "logging.h"
#include "main.h"

#define LOG_DOMAIN "timesaver: "

#define POWERD_RESTORES_TIME

static char *time_db = NULL;
static char *time_db_tmp = NULL;

#define PREFDIR "/var/preferences/com.palm.sleep"


/**
 * @brief Read the last saved time (in sec) from the file "time_saver".
 *
 * @retval
 */

time_t
timesaver_get_saved_secs()
{
    time_t secs_since_epoch = 0;
    char buf[512];
    FILE *file = NULL;

    file = fopen(time_db, "r");
    if (!file)
    {
        g_warning("%s: Could not read %s", __FUNCTION__, time_db);
       goto cleanup;
    }

    int toks = fscanf(file, "%511s", buf);
    if (1 != toks)
    {
        g_warning("%s: Could not read timestamp from %s",
                __FUNCTION__, time_db);
        goto cleanup;
    }

    secs_since_epoch = strtoul(buf, NULL, 10);

cleanup:
    if (file) fclose(file);
    return secs_since_epoch;
}

/**
 * @brief Restore the time from the given time.
 *
 * @retval
 */
#ifdef POWERD_RESTORES_TIME
static void
timesaver_restore(time_t secs_since_epoch)
{
    if (secs_since_epoch)
    {
        struct timespec tp;
        tp.tv_sec = secs_since_epoch;
        tp.tv_nsec = 0;

        clock_settime(CLOCK_REALTIME, &tp);

        struct tm time;
        gmtime_r(&tp.tv_sec, &time);

        POWERDLOG(LOG_INFO,
            "%s Setting the time to be "
            "%02d-%02d-%04d %02d:%02d:%02d",
            __FUNCTION__,
            time.tm_mon+1, time.tm_mday, time.tm_year+1900,
            time.tm_hour, time.tm_min, time.tm_sec);
    }
}
#endif

/**
 * @brief Save the current time in the file "time_saver" so that it can be used in future.
 *
 * @retval
 */

void
timesaver_save()
{
    if (NULL == time_db)
    {
        // This can happen if we goto ls_error in main()
        g_warning("%s called with time database name (time_db) uninitialized", __FUNCTION__);
        goto cleanup;
    }

    //  First write the contents to tmp file and then rename to "time_saver" file
    //  to ensure file integrity with power cut or battery pull.

    int file = open(time_db_tmp, O_CREAT | O_WRONLY, S_IRWXU | S_IRGRP | S_IROTH);
    if (!file)
    {
        g_warning("%s: Could not save time to \"%s\"", __FUNCTION__, time_db_tmp);
        goto cleanup;
    }

    struct timespec tp;
    clock_gettime(CLOCK_REALTIME, &tp);

    POWERDLOG(LOG_DEBUG, "%s Saving to file %ld", __FUNCTION__, tp.tv_sec);

    char timestamp[16];

    sprintf(timestamp,"%ld", tp.tv_sec);

    write(file,timestamp,strlen(timestamp));
    fsync(file);
    close(file);

    int ret = rename(time_db_tmp,time_db);
    if (ret)
    {
    	g_warning("%s : Unable to rename %s to %s",__FUNCTION__,time_db_tmp,time_db);
    }
	unlink(time_db_tmp);

cleanup:
    return;
}

#ifdef POWERD_RESTORES_TIME

/** 
* @brief Heuristic to detect that the time has been reset.
*        We do this till we have a way to detect cold resets.
*
* The current heuristic is if the current time is earlier than
* the saved time, then we'll assume that the time has been reset.
* 
* @retval
*/

static bool
time_out_of_date(time_t saved_time)
{
    struct timespec tp;
    clock_gettime(CLOCK_REALTIME, &tp);

    /* diff = saved_time - tp.tv_sec */
    double diff = difftime(saved_time, tp.tv_sec);

    if (diff > 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}
#endif

/** 
* @brief Restores the time from disk if it has been reset.
* 
* @retval
*/
static int
timesaver_init()
{
    if (!time_db)
    {
        time_db = g_build_filename(
                PREFDIR, "time_saver", NULL);
        time_db_tmp = g_build_filename(
                PREFDIR, "time_saver.tmp", NULL);
    }

#ifdef POWERD_RESTORES_TIME
    time_t saved_time = timesaver_get_saved_secs();

    if (saved_time && time_out_of_date(saved_time))
    {
        timesaver_restore(saved_time);
    }
#endif

    return 0;
}

INIT_FUNC(INIT_FUNC_EARLY, timesaver_init);
