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

target = km4_console

all: $(O)/$(target)

$(O)/$(target): $(O)/km4_console.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

$(O)/%.o: %.c
	$(CC) -c $(CFLAGS) $(LDFLAGS) -o $@ $<

clean:
	-rm -f $(O)/*.o
