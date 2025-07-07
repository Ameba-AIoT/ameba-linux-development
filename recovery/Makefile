CC := $(CROSS_COMPILE)gcc
O ?= $(shell pwd)

OBJS = recovery.o install.o config_parser.o

INCLUDES +=-I./include -I./storage_mount/interfaces
INCLUDES += $(LIBJS_INC)

LIBS += -L./storage_mount -static -lstorage_mount

all: recovery

recovery: ${OBJS}
	$(CC) -o $@ ${OBJS} $(LIBS) $(LDFLAGS) $(CFLAGS) $(LIBJS_LIB)

%.o: %.c
	$(MAKE) -C storage_mount CC=$(CROSS_COMPILE)gcc
	$(CC) -c $< -o $@ $(LDFLAGS)  $(INCLUDES)  $(CFLAGS) $(LIBJS_LIB)

clean:
	rm -fr recovery
	rm -fr *.o
	$(MAKE) -C storage_mount clean

