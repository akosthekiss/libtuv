# Copyright 2015 Samsung Electronics Co., Ltd.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# application name
set(MBEDMODULE "tuvtester")

# add include jerry-core
set(TUVROOT ${CMAKE_CURRENT_LIST_DIR}/../../..)
include_directories(${TUVROOT}/include)
include_directories(${TUVROOT}/source)
include_directories(${TUVROOT}/source/mbed)
include_directories(${TUVROOT}/test)

# compile flags
set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS}
                  -mlittle-endian
                  -mthumb
                  -mcpu=cortex-m4
                  )
add_definitions("-D__TUV_MBED__")
add_definitions("-D__TUV_RAW__")

# link tuv and tuvtester
set(TUVLIBSPATH ${CMAKE_CURRENT_LIST_DIR}/../libtuv)

set(TUVLIBSFILES "")
set(TUVLIBSFILES ${TUVLIBSFILES} ${TUVLIBSPATH}/libtuv.a)
set(TUVLIBSFILES ${TUVLIBSFILES} ${TUVLIBSPATH}/libtuvtester.a)

target_link_libraries(${MBEDMODULE} ${TUVLIBSFILES})
