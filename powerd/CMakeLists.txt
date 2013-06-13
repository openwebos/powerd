# @@@LICENSE
#
#      Copyright (c) 2007-2013 LG Electronics, Inc.
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

include_directories(${CMAKE_CURRENT_SOURCE_DIR}  ${CMAKE_CURRENT_SOURCE_DIR}/utils ${CMAKE_CURRENT_SOURCE_DIR}/alarms ${CMAKE_CURRENT_SOURCE_DIR}/suspend)

file(GLOB SOURCE_FILES alarms/* charging/* suspend/* utils/* *.c)
webos_configure_source_files(SOURCE_FILES config.c timesaver.c)

add_executable(powerd ${SOURCE_FILES})
target_link_libraries(powerd ${GLIB2_LDFLAGS} ${LUNASERVICE2_LDFLAGS} ${CJSON_LDFLAGS} ${NYXLIB_LDFLAGS} pthread rt)

webos_build_daemon()
webos_build_system_bus_files()

install(FILES ../files/conf/powerd.conf DESTINATION ${WEBOS_INSTALL_DEFAULTCONFDIR})
