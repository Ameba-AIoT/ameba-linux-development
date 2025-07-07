CC := $(CROSS_COMPILE)gcc
O ?= $(shell pwd)

OBJS = recoveryd.o

all: recoveryd

recoveryd: ${OBJS}
	$(CC) -o $@ ${OBJS} $(LIBS) $(LDFLAGS) $(CFLAGS) $(LIBJS_LIB) 

%.o: %.c
	$(CC) -c $< -o $@ $(LDFLAGS) $(INCLUDES) $(CFLAGS)

clean:
	rm -fr recoveryd
	rm -fr *.o *.a
