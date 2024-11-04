LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := libbootloader_message
LOCAL_CFLAGS := -Werror

LOCAL_SRC_FILES := \
    bootloader_message.c

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/interfaces

LOCAL_FORCE_STATIC_EXECUTABLE:=true

LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/interfaces

include $(BUILD_STATIC_LIBRARY)

