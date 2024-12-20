UART_TEST = uart_test
HDLC_TEST = hdlctest

CFLAGS += -I../
CFLAGS += -I../hdlc/
CFLAGS += -DHDLC_ENABLE=1

LDFLAGS += -L./
#LDFLAGS += -lappinternal
#LDFLAGS += -lpthread

UART_CSRCS += ../ttydevice.c
UART_CSRCS += ../trace.c
UART_CSRCS += ./uart_test.c
UART_COBJS = $(foreach SRC,$(UART_CSRCS),$(SRC:.c=.o))

HDLC_CSRCS += ../hdlc/CFUonHDLC.c
HDLC_CSRCS += ./hdlc_test.c
HDLC_COBJS = $(foreach SRC,$(HDLC_CSRCS),$(SRC:.c=.o))

.PHONY: all clean

all:$(UART_TEST) $(HDLC_TEST)

$(UART_TEST): $(UART_COBJS)
	$(CROSS_COMPILE)gcc -g $^ $(CFLAGS) $(LDFLAGS) -o $@

$(HDLC_TEST): $(HDLC_COBJS)
	$(CROSS_COMPILE)gcc -g $^ $(CFLAGS) $(LDFLAGS) -o $@

%.o: %.c
	$(CROSS_COMPILE)gcc -g $(CFLAGS) -c $^ -o $@

clean:
	rm *.o
	rm *.a
	rm ../*.o
	rm ../*.a
	rm $(UART_TEST)
	rm $(HDLC_TEST)
