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
# powerd/libpowerd/CMakeLists.txt
#

# Build the libpowerd shared library

include_directories(../include/private ../include .)

add_definitions("-fPIC -DSTACK_GROWS_DOWN")

# Can't use ${POWERD_LIBRARY_NAME} for target name since that's the name of the daemon.
add_library(libpowerd SHARED clock.c commands.c init.c wait.c)
target_link_libraries(libpowerd ${GLIB2_LDFLAGS} ${LUNASERVICE2_LDFLAGS} ${CJSON_LDFLAGS} pthread rt)


set_target_properties(libpowerd PROPERTIES VERSION ${POWERD_LIBRARY_VERSION} SOVERSION ${POWERD_API_VERSION_MAJOR})
set_target_properties(libpowerd PROPERTIES OUTPUT_NAME ${POWERD_LIBRARY_NAME})


install(FILES ../include/powerd-glib.h ../include/powerd.h ../include/powerd-lunaservice.h DESTINATION "include/${POWERD_LIBRARY_NAME}")
install(TARGETS libpowerd LIBRARY DESTINATION "lib${LIB_SUFFIX}/")

configure_file(${POWERD_LIBRARY_NAME}.pc.in
                ${PROJECT_BINARY_DIR}/libpowerd/${POWERD_LIBRARY_NAME}.pc @ONLY)
install (FILES "${PROJECT_BINARY_DIR}/libpowerd/${POWERD_LIBRARY_NAME}.pc" DESTINATION "lib/pkgconfig")