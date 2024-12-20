APP = rtkcfu

CFLAGS += -I./
CFLAGS += -I./hdlc

LDFLAGS += 

CSRCS += ./main.c
CSRCS += ./ttydevice.c
CSRCS += ./trace.c
CSRCS += ./FwUpdateSubProcess.c
CSRCS += ./FwUpdateProcess.c
CSRCS += ./hdlc/CFUonHDLC.c
CSRCS += ./RcpConfig.c
COBJS = $(foreach SRC,$(CSRCS),$(SRC:.c=.o))


.PHONY: all clean

all:$(APP)

$(APP): $(COBJS)
	$(CROSS_COMPILE)gcc -g $^ $(CFLAGS) $(LDFLAGS) -o $@

%.o: %.c
	$(CROSS_COMPILE)gcc -g $(CFLAGS) -c $^ -o $@

clean:
	rm *.o
	rm *.a
	rm $(APP)
