#
# Realtek Semiconductor Corp.
#

CROSS_COMPILE ?= arm-linux-

CC := $(CROSS_COMPILE)gcc
CXX := $(CROSS_COMPILE)g++
LD := $(CROSS_COMPILE)gcc
AR := $(CROSS_COMPILE)ar
RANLIB := $(CROSS_COMPILE)ranlib
O ?= $(shell pwd)

exe-y = otp-ipc

all: $(O)/$(exe-y)

$(O)/$(exe-y):
	$(CC) $(CFLAGS) $(LDFLAGS) otp-ipc.c -o $@

clean:
	-rm -f $(O)/*
