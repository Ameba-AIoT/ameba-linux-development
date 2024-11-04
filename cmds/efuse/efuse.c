#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>

#define BUF_SIZE    0x400

#define PHYSICAL_NVMEM "/sys/bus/nvmem/devices/otp_raw0/nvmem"
#define LOGICAL_NVMEM "/sys/bus/nvmem/devices/otp_map0/nvmem"

static int isAlpha(char c) {
    if ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) return 1;
    return 0;
}

static int mypow(int num, int cnt) {
    if (cnt == 0) return 1;
    if (cnt == 1) return num;

    int ret = mypow(num, cnt/2);

    if (cnt % 2)
        return ret * ret * num;

    return ret * ret;
}

static int toInt(char c) {
    int num = 0;
    if(isAlpha(c)) {
        num = ((c + 32 - 'a') % 32) + 10;
    } else num = c - '0';

    return num;
}

static int convert(char* t) {
    int len = strlen(t);
    int ret = 0;

    for (int i = len-1; i >= 0; i--) {
        int num = toInt(t[i]);
        ret += num * mypow(16, len-1-i);
    }

    return ret;
}

static void fillbuf(unsigned char* buf, const char* str) {
    int i = 0;
    int idx = 0;
    int len = strlen(str);
    if (len % 2) {
        printf("val buffer is invalid\n");
        return;
    }

    for (; i < len-1; i += 2) {
        int num = toInt(str[i]) * 16 + toInt(str[i+1]);
        buf[idx++] = num;
    }
}

int main(int argc, char **argv[]) {
    int fd = 0;
    int i;
    int ret = 0;
    int flag = 0;
    int addr = 0, size = 0;
    unsigned char dev[64];
    unsigned char buf[BUF_SIZE];

    if (argc < 4) {
        fprintf(stderr, "[rraw/rmap/wraw/wmap] [addr] [len] [write val]\n");
        return 1;
    }

    if (strcmp((char *)argv[1], "rraw") == 0) {
        strcpy(dev, PHYSICAL_NVMEM);
        flag = O_RDONLY;
    } else if (strcmp((char *)argv[1], "wraw") == 0) {
        strcpy(dev, PHYSICAL_NVMEM);
        flag = O_RDWR;
    } else if (strcmp((char *)argv[1], "rmap") == 0) {
        strcpy(dev, LOGICAL_NVMEM);
        flag = O_RDONLY;
    } else if (strcmp((char *)argv[1], "wmap") == 0) {
        strcpy(dev, LOGICAL_NVMEM);
        flag = O_RDWR;
    }

    if (flag == O_RDWR && !argv[4]) {
        fprintf(stderr, "Usage:[wraw/wmap] [addr] [len] [write val]\n");
        return 1;
    }

    fd = open(dev, flag);

    if (fd < 0) {
        printf("Can't open nvmem device\n");
        return -1;
    }

    if (argv[2]) {
        char* t = (char*)argv[2];
        addr = convert(t);
    }

    if (argv[3]) {
        size = atoi(argv[3]);
    }

    printf("addr : 0x%x, size: %d\n", addr, size);
    if (flag == O_RDONLY) {
        ret = pread(fd, buf, size, addr);
        for (i = 0; i < size; i++) {
            printf("%02x ", buf[i]);
            if (i % 16 == 15) printf("\n");
        }
    } else if (flag == O_RDWR) {
        fillbuf(buf, (char*)argv[4]);
        for (i = 0; i < size; i++)
            printf("buf[%d] = %x\n", i, buf[i]);
        pwrite(fd, buf, size, addr);
    }

    close(fd);
    return ret;
}