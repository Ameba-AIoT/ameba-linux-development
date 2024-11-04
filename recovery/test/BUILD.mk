LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := recovery_test
LOCAL_CFLAGS := -Werror

LOCAL_SRC_FILES := \
    test.c \

LOCAL_C_INCLUDES :=

LOCAL_STATIC_LIBRARIES := \
    libbootloader_message

LOCAL_FORCE_STATIC_EXECUTABLE:=true

include $(BUILD_EXECUTABLE)
