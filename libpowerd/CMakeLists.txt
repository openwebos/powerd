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
# powerd/libpowerd/CMakeLists.txt
#

# Build the libpowerd shared library

webos_build_pkgconfig()

webos_add_compiler_flags(ALL -fPIC -DSTACK_GROWS_DOWN)

add_library(libpowerd SHARED clock.c commands.c init.c wait.c)
target_link_libraries(libpowerd ${GLIB2_LDFLAGS} ${LUNASERVICE2_LDFLAGS} ${CJSON_LDFLAGS} pthread rt)
webos_build_library(NAME libpowerd)
