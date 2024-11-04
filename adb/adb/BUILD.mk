# Copyright 2005 The Android Open Source Project
#
# Android.mk for adb
#

LOCAL_PATH:= $(call my-dir)

# =========================================================
# adbd device daemon
# =========================================================
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    adb.c \
    backup_service.c \
    fdevent.c \
    transport.c \
    transport_local.c \
    transport_usb.c \
    adb_auth_client.c \
    sockets.c \
    services.c \
    file_sync_service.c \
    jdwp_service.c \
    framebuffer_service.c \
    remount_service.c \
    usb_linux_client.c \
    utils.c

LOCAL_CFLAGS := -O2 -g -DADB_HOST=0 -Wall -Wno-unused-parameter
LOCAL_CFLAGS += -D_XOPEN_SOURCE -D_GNU_SOURCE

ifneq (,$(filter userdebug eng,$(TARGET_BUILD_VARIANT)))
LOCAL_CFLAGS += -DALLOW_ADBD_ROOT=1
endif

LOCAL_MODULE := adbd

LOCAL_C_INCLUDES := \
    $(FWKTOP)third_party/adb/include

LOCAL_SHARED_LIBRARIES := libadbcutils libmincrypt

LOCAL_LDLIBS := -lpthread -lc
LOCAL_CFLAGS += -DNO_AUTH

include $(BUILD_EXECUTABLE)

# =========================================================
# adb host
# =========================================================
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    adb.c \
    usb_linux.c \
    get_my_path_linux.c \
    console.c \
    transport.c \
    transport_local.c \
    transport_usb.c \
    commandline.c \
    adb_client.c \
    sockets.c \
    services.c \
    file_sync_client.c \
    utils.c \
    usb_vendors.c \
    fdevent.c

#adb_auth_host.c \

LOCAL_CFLAGS := -O2 -g -DADB_HOST=1 -Wall -Wno-unused-parameter -DWORKAROUND_BUG6558362
LOCAL_CFLAGS += -D_XOPEN_SOURCE -D_GNU_SOURCE -DNO_AUTH

LOCAL_MODULE := adb

LOCAL_C_INCLUDES := \
    $(FWKTOP)third_party/adb/include

LOCAL_C_INCLUDES += \
    $(FWKTOP)third_party/openssl/openssl-1.1.1l/include

LOCAL_SHARED_LIBRARIES := libmincrypt libadbcutils
LOCAL_LDLIBS := -lpthread -lc

include $(BUILD_EXECUTABLE)