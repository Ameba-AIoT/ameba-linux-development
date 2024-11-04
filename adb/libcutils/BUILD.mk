#
# Copyright (C) 2008 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
LOCAL_PATH := $(my-dir)
include $(CLEAR_VARS)

commonSources := \
    sockets.c \
    config_utils.c \
    load_file.c \
    socket_inaddr_any_server.c \
    socket_local_client.c \
    socket_local_server.c \
    socket_loopback_client.c \
    socket_loopback_server.c \
    socket_network_client.c

include $(CLEAR_VARS)
LOCAL_MODULE := libadbcutils
LOCAL_SRC_FILES := $(commonSources)

LOCAL_C_INCLUDES := \
    $(FWKTOP)third_party/adb/include

include $(BUILD_SHARED_LIBRARY)