/*
 * Copyright (c) 2021 Realtek, LLC.
 * All rights reserved.
 *
 * Licensed under the Realtek License, Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License from Realtek
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "BootLoaderMessage"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//#include "log/log.h"
#include "bootloader_message/bootloader_message.h"

//#define printf RTLOGD

int get_bootloader_message(struct bootloader_message *out, const char* misc) {
    char device[30];
    FILE* fd;
    printf("get_bootloader_message enter\n");

    strcpy(device, misc);

    fd= fopen(device, "rb");
    if (fd == NULL) {
        printf("Can't open %s\n(%s)\n", device, strerror(errno));
        return -1;
    }
    struct bootloader_message temp;
    int cnt = fread(&temp, sizeof(temp), 1, fd);
    if (cnt != 1) {
        printf("Failed reading %s\n(%s)\n", device, strerror(errno));
        fclose(fd);
        return -1;
    }
    if (fclose(fd) != 0) {
        printf("Failed closing %s\n(%s)\n", device, strerror(errno));
        return -1;
    }

    memcpy(out, &temp, sizeof(temp));
    return 0;
}

int set_bootloader_message(struct bootloader_message *in, const char* misc) {
    char device[30];
    FILE* fd;
    printf("set_bootloader_message enter\n");

    strcpy(device,misc);

    fd = fopen(device,"wb");
    if (fd == NULL) {
        printf("Can't open %s\n(%s)\n", device, strerror(errno));
        return -1;
    }

    int cnt = fwrite(in, sizeof(*in), 1, fd);
    if (cnt != 1) {
        printf("Failed writing %s\n(%s)\n", device, strerror(errno));
        fclose(fd);
        return -1;
    }

    fflush(fd);
    if (fclose(fd) != 0) {
        printf("Failed closing %s\n(%s)\n", device, strerror(errno));
        return -1;
    }
    return 0;
}

int clear_bootloader_message(const char* misc) {
    struct bootloader_message boot = {};
    printf("clear_bootloader_message enter\n");

    return set_bootloader_message(&boot, misc);

}
