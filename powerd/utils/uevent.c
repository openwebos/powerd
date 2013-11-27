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
 * @file uevent.c
 *
 *@brief Helper functions to catch udev events using sockets.
 */

#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <errno.h>
#include <glib.h>

#include "uevent.h"
#include "debug.h"

static gboolean
ChangeGIOHelper(GIOChannel *source, GIOCondition condition, gpointer ctx)
{
    int socket = g_io_channel_unix_get_fd(source);
    struct msghdr msg;
    struct iovec iov;
    char	*data;
    int nbytes;

    UEventChangeFunc func = (UEventChangeFunc)ctx;

    char buf[4096];
    g_info("ChangeGIOHelper\n");

    memset(buf, 0x00, sizeof(buf));
    memset(&msg, 0x00, sizeof (struct msghdr));

    iov.iov_base = &buf;
    iov.iov_len = sizeof(buf);

    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    nbytes = recvmsg(socket, &msg, 0);
    g_info("ChangeGIOHelper: %d\n", nbytes);
    if (nbytes < 0)
    {
        if (EINTR != errno)
        {
            g_critical("Unable to receive udev event %s\n", strerror(errno));
        }

        return TRUE;
    }

    if (!strstr(buf, "@/"))
    {
        g_warning("Invalid message format for udev event: %s.\n", buf);
    }

    //g_debug("Received uevent %s.\n", buf);
    g_info("Received uevent %d:%s.\n", nbytes, buf);

    if (strncmp(buf, "change", strlen("change")) == 0)
    {
    	data = malloc(nbytes);
    	if(data)
    		memcpy(data, buf, nbytes);
    	else
    		g_critical("%s: Malloc failed\n", __func__);

        func(nbytes, data);

        free(data);
    }

    return TRUE;
}

#define SUN_PATH_LEN 108
int
UEventListen(const char *ueventPath, UEventChangeFunc func)
{
    //const int on = 1;
    int s;
    struct sockaddr_un addr;
    socklen_t len;
    GIOChannel *udev_channel;

    g_info("UEventListen\n");
    memset(&addr, 0x00, sizeof(addr));
    addr.sun_family = AF_LOCAL;

    // use an abstract namespace by starting on 1
    strncpy(&addr.sun_path[1], ueventPath, SUN_PATH_LEN-2);
    len = offsetof(struct sockaddr_un, sun_path) +
            strlen(addr.sun_path+1) + 1;

    s = socket(AF_LOCAL, SOCK_DGRAM, 0);
    if (-1 == s)
    {
        // Ubuntu 9.04 toochain didn't like g_perror
        g_error("%s: error %d from socket()", __func__, s);

        return -1;
    }

    if (bind(s, (struct sockaddr *)&addr, len) < 0)
    {
        g_critical("Could not bind to socket: %s\n", strerror(errno));
        close(s);
        return -1;
    }

    // do we need to check cred?
    //setsockopt(s, SOL_SOCKET, SO_PASSCRED, &on, sizeof(on));

    udev_channel = g_io_channel_unix_new(s);
    g_io_add_watch(udev_channel, G_IO_IN, ChangeGIOHelper, func);
    g_io_channel_unref(udev_channel);

    return 0;
}
