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

CFLAGS += "-I../../../kernel/linux-5.4/include/uapi/misc"

target = rtk_hciattach

all: $(O)/$(target)

$(O)/$(target): $(O)/hciattach.o $(O)/hciattach_h4.o $(O)/hciattach_rtk.o $(O)/rtb_fwc.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

$(O)/%.o: %.c
	$(CC) -c $(CFLAGS) $(LDFLAGS) -o $@ $<

clean:
	-rm -f $(O)/*.o
