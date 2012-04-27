# @@@LICENSE
#
#      Copyright (c) 2007-2012 Hewlett-Packard Development Company, L.P.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# LICENSE@@@

#
# powerd/powerd/CMakeLists.txt
#

# Build the powerd executable

include_directories(../include/private ../include . ./utils ./alarms ./suspend)

# Check that we have nyx-lib
pkg_check_modules(NYXLIB REQUIRED nyx)
add_definitions(${NYXLIB_CFLAGS})

aux_source_directory(alarms alarms_files)
aux_source_directory(charging charging_files)
aux_source_directory(suspend suspend_files)
aux_source_directory(utils utils_files)

add_executable(powerd ${alarms_files} ${charging_files} ${suspend_files} ${utils_files} main.c config.c com_palm_power_lunabus.c timesaver.c)
target_link_libraries(powerd ${GLIB2_LDFLAGS} ${LUNASERVICE2_LDFLAGS} ${CJSON_LDFLAGS} ${NYXLIB_LDFLAGS})

set_property(TARGET powerd PROPERTY INSTALL_RPATH_USE_LINK_PATH TRUE)


install(TARGETS powerd DESTINATION "sbin")
install(FILES ./preferences/powerd.conf DESTINATION "${SYSCONFDIR_FULL}/default")
install(PROGRAMS ./etc/event.d/powerd DESTINATION "${SYSCONFDIR_FULL}/event.d")
