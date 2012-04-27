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
 * @file config.c
 *
 * @brief Read configuration from charger.conf file and intialize the global config structure "gChargeConfig"
 *
 */

/**
 * Powerd configuration.
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <glib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <lunaservice.h>

#include "config.h"
#include "init.h"


/**
 * default power config
 */
chargeConfig_t gChargeConfig =
{
	.debug = false,

    .skip_battery_check = false,
    .fake_battery = false,
    .disable_charging = false,
    .disable_overcharge_check = false,
    .skip_battery_authentication = false,
    
    .preference_dir = "/var/preferences/com.palm.power",

    .fasthalt = 0, 
    .maxtemp = 0, // defaults in batterypoll.c
    .temprate = 0,
            
};

#define CONFIG_GET_INT(keyfile,cat,name,var)                    \
do {                                                            \
    int intVal;                                                 \
    GError *gerror = NULL;                                      \
    intVal = g_key_file_get_integer(keyfile,cat,name,&gerror);  \
    if (!gerror) {                                              \
        var = intVal;                                           \
        g_debug(#var " = %d", intVal);                          \
    }                                                           \
    else { g_error_free(gerror); }                              \
} while (0)

#define CONFIG_GET_BOOL(keyfile,cat,name,var)                   \
do {                                                            \
    bool boolVal;                                               \
    GError *gerror = NULL;                                      \
    boolVal = g_key_file_get_boolean(keyfile,cat,name,&gerror); \
    if (!gerror) {                                              \
        var = boolVal;                                          \
        g_debug(#var " = %s",                                   \
                  boolVal ? "true" : "false");                  \
    }                                                           \
    else { g_error_free(gerror); }                              \
} while (0)

static int
parse_kern_cmdline(void)
{
    int fd = open("/proc/cmdline", O_RDONLY); 
    if (fd < 0) return -1;
    char buf[1024+1];
    int ret;

    do {
        ret = read(fd, buf, 1024);
    } while (ret < 0 && EINTR == errno);

    if (ret < 0)
    {
        goto cleanup;
    }

    buf[ret] = '\0';

    char *tok = strstr(buf, "skip_battery_check=1");
    if (tok != NULL)
    {
        g_debug("skip_battery_check env => fake_battery = true");
        gChargeConfig.skip_battery_check = true;
    }

    ret = 0;  /// [suspend]

cleanup:
    close(fd);
    return ret;
}

int
config_init(void)
{
    int ret;

    ret = mkdir(gChargeConfig.preference_dir, 0755);
    if (ret < 0 && errno != EEXIST)
    {
        perror("Powerd: Could not mkdir the preferences dir.");
    }

    GKeyFile *config_file = NULL;
    bool retVal;

    config_file = g_key_file_new();
    if (!config_file)
    {
        return -1;
    }

    char *config_path =
        g_build_filename(gChargeConfig.preference_dir, "powerd.conf", NULL);
    retVal = g_key_file_load_from_file(config_file, config_path,
        G_KEY_FILE_NONE, NULL);
    if (!retVal)
    {
        g_warning("%s cannot load config file from %s",
                __FUNCTION__, config_path);
        goto end;
    }

    /// [general]
    CONFIG_GET_INT(config_file, "general", "debug", gChargeConfig.debug);

    /// [battery]
    CONFIG_GET_BOOL(config_file, "battery", "fake_battery",
                    gChargeConfig.fake_battery);
    CONFIG_GET_BOOL(config_file, "battery", "disable_charging",
                    gChargeConfig.disable_charging);
    CONFIG_GET_BOOL(config_file, "battery", "disable_overcharge_check",
                    gChargeConfig.disable_overcharge_check);


    parse_kern_cmdline();

end:
    g_free(config_path);
    if (config_file) g_key_file_free(config_file);
    return 0;
}

INIT_FUNC(INIT_FUNC_FIRST, config_init);


