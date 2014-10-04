# Copyright (C) 2009 The Android Open Source Project
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
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := luajit
LOCAL_SRC_FILES := libluajit.a
include $(PREBUILT_STATIC_LIBRARY)


include $(CLEAR_VARS)

LOCAL_LDLIBS += -llog
LOCAL_MODULE    := TMS9900-JNI
LOCAL_SRC_FILES := TMS9900JNI.c tms9900-core.c tms9900-memory.c tms9900-lua.c ti99-fdc.c
LOCAL_C_INCLUDES := /home/eric/LuaJIT-2.0.2/src
LOCAL_STATIC_LIBRARIES := luajit

include $(BUILD_SHARED_LIBRARY)
