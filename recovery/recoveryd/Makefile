CC := $(CROSS_COMPILE)gcc
O ?= $(shell pwd)

OBJS = recoveryd.o

INCLUDES +=-I./../bootloader_message/interfaces

LIBS += -L./../bootloader_message -static -lbootloader_message

all: recoveryd

recoveryd: ${OBJS} libbootloader_message.a
	$(CC) -o $@ ${OBJS} $(LIBS) $(LDFLAGS) $(CFLAGS) $(LIBJS_LIB) 

libbootloader_message.a:
	$(MAKE) -C ../bootloader_message CC=$(CROSS_COMPILE)gcc

%.o: %.c
	$(CC) -c $< -o $@ $(LDFLAGS) $(INCLUDES) $(CFLAGS)

clean:
	rm -fr recoveryd
	rm -fr *.o *.a
	$(MAKE) -C ../bootloader_message clean

