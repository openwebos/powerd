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


#ifndef _TIMEOUT_ALARM_H_
#define _TIMEOUT_ALARM_H_

typedef struct _PowerTimeout {
    const char *table_id;

    const char *app_id;
    const char *key;
    const char *uri;
    const char *params;
    const char *activity_id;
    int activity_duration_ms;
    bool        public_bus;
    bool        wakeup;
    bool        calendar;
    time_t      expiry;
} _PowerTimeout;

typedef struct _PowerTimeoutNonConst {
    char *table_id;

    char *app_id;
    char *key;
    char *uri;
    char *params;
    char *activity_id;
    int activity_duration_ms;
    bool        public_bus;
    bool        wakeup;
    bool        calendar;
    time_t      expiry;
} _PowerTimeoutNonConst;

time_t rtc_wall_time(void);

void _free_timeout_fields(_PowerTimeoutNonConst *timeout);

void _timeout_create(_PowerTimeout *timeout,
        const char *app_id, const char *key,
        const char *uri, const char *params,
        bool public_bus, bool wakeup,
        const char *activity_id,
        int activity_duration_ms,
        bool calendar, time_t expiry);

bool _timeout_set(_PowerTimeout *timeout);

bool _timeout_read(_PowerTimeoutNonConst *timeout, const char *app_id, const char *key, bool public_bus);

bool _timeout_clear(const char *app_id, const char *key, bool public_bus);

bool _timeout_delete(const char *app_id, const char *key, bool public_bus);

bool timeout_get_next_wakeup(time_t *expiry, gchar **app_id, gchar **key);

#endif // _TIMEOUT_ALARM_H
