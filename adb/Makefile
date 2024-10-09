CC := $(CROSS_COMPILE)gcc
INCS += -I./include -I./libcutils -I./adb

SRCS := $(wildcard ./libcutils/*.c)
SRCS += $(wildcard ./libmincrypt/*.c)
SRCS += ./adb/adb.c
SRCS += ./adb/backup_service.c
SRCS += ./adb/fdevent.c
SRCS += ./adb/transport.c
SRCS += ./adb/transport_local.c
SRCS += ./adb/transport_usb.c
SRCS += ./adb/adb_auth_client.c
SRCS += ./adb/sockets.c
SRCS += ./adb/services.c
SRCS += ./adb/file_sync_service.c
SRCS += ./adb/jdwp_service.c
SRCS += ./adb/framebuffer_service.c
SRCS += ./adb/remount_service.c
SRCS += ./adb/usb_linux_client.c
SRCS += ./adb/utils.c

OBJS := $(patsubst %.c, %.o, $(SRCS))

CFLAGS += -DADB_HOST=0
CFLAGS += -D_XOPEN_SOURCE
CFLAGS += -D_GNU_SOURCE
CFLAGS += -DALLOW_ADBD_ROOT=1
#CFLAGS += -DNO_AUTH

LDFLAGS += -lpthread -lresolv

all : adbd

adbd: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS) $(INCS)

%.o: %.c
	$(CC) $(CFLAGS) -c  $< -o $@ $(LDFLAGS) $(INCS)

clean:
	rm -f adbd $(OBJS)
