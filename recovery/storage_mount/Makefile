CC := $(CROSS_COMPILE)gcc

INCLUDES +=-I./interfaces

TARGET_LIB = libstorage_mount.a

all: $(TARGET_LIB)

OBJS = storage_mount.o

$(TARGET_LIB): $(OBJS)
	$(CROSS_COMPILE)ar rcs $@ $^

%.o: %.c
	$(CC) -c $< -o $@ $(INCLUDES) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS)

clean:
	rm -f $(OBJS) $(TARGET_LIB)
