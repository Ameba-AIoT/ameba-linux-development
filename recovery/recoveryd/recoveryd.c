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

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/reboot.h>

#include "bootloader_message/bootloader_message.h"

const char* gDataBlock = "/dev/mtdblock0";
const char* gMountPath[] = {"/mnt/storage", "/rom/mnt/storage/"};

static int checkOtaPackage(void) {
    const char* devblock[] = {"sda1", "sdb1", "sdc1", "sdd1", "mmcblk0p1"};
    int i = 0;
    int mount_size = sizeof(gMountPath)/sizeof(char*);
    int size = sizeof(devblock)/sizeof(char*);

    for (; i < size; i++) {
        int j = 0;
        for (; j < mount_size; j++) {
            DIR *dir_ptr = NULL;
            char path[64] = {0};
            sprintf(path, "%s/%s/ota", gMountPath[j], devblock[i]);

            //printf("dir path: %s\n", path);

            dir_ptr = opendir(path);
            if (dir_ptr)
                return 1;
        }
    }

    return 0;
}

int main(int argc, char **argv) {

    while(1) {
        sleep(2);

        if (checkOtaPackage()) {
            struct bootloader_message boot = {};
            strcpy(boot.recovery, *argv);
            strcpy(boot.cmd, "boot-recovery");
            set_bootloader_message(&boot, gDataBlock);
            reboot(RB_AUTOBOOT);
        }
    }

    return 0;
}
