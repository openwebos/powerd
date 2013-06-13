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


#ifndef _POWERDLTP_H_
#define _POWERDLTP_H_

#ifdef USE_LTP
#include <libtestutils.h>

#define LTP_COND(testid, result, fmt...)    \
do {                        \
    testPassIfTrue(testid, result, fmt);   \
} while (0)

#define LTP_INFO(fmt...) \
do {                        \
    testInfoMessage(fmt);   \
} while (0)

#else

#define LTP_COND(testid, result, fmt...)    \
do {                        \
    printf(fmt);        \
    printf("\n");        \
} while (0)

#define LTP_INFO(fmt...)    \
do {                    \
    printf(fmt);        \
    printf("\n");        \
} while(0)

#endif // #ifdef USE_LTP

#endif // _POWERDLTP_H_
