CC := $(CROSS_COMPILE)gcc
O ?= $(shell pwd)

OBJS = test.o

all: recovery_test

recovery_test: ${OBJS}
	$(CC) -o $@ ${OBJS} $(LIBS) $(LDFLAGS) $(CFLAGS) $(LIBJS_LIB) 

%.o: %.c
	$(CC) -c $< -o $@ $(LDFLAGS) $(INCLUDES) $(CFLAGS)

clean:
	rm -fr recovery_test
	rm -fr *.o *.a
