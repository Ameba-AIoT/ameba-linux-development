LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := libmincrypt
LOCAL_SRC_FILES := rsa.c rsa_e_3.c rsa_e_f4.c sha.c
LOCAL_C_INCLUDES := $(FWKTOP)third_party/adb/include
include $(BUILD_SHARED_LIBRARY)