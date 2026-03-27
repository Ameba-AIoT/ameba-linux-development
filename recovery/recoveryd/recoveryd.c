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
#include <unistd.h>
#include <dirent.h>
#include <sys/reboot.h>

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
            DIR *ota_ptr = NULL;
            struct dirent *entry = NULL;
            char path[64] = {0};
            char ota_path[64] = {0};
            sprintf(path, "%s/%s", gMountPath[j], devblock[i]);

            dir_ptr = opendir(path);
            if (!dir_ptr)
                continue;

            sprintf(ota_path, "%s/%s/ota", gMountPath[j], devblock[i]);
            ota_ptr = opendir(ota_path);
            if (ota_ptr) {
                printf("update from ota folder\n");

                int res = system("/usr/bin/fw_setenv update_mode normal");
                if (res == -1) {
                    printf("exec fw_setenv failed\n");
                    continue;
                }

                closedir(ota_ptr);
                closedir(dir_ptr);
                return 1;
            }

            while ((entry = readdir(dir_ptr)) != NULL) {
                size_t name_len = strlen(entry->d_name);
                if (name_len > 4 && !strcmp(entry->d_name + name_len - 4, ".swu")) {
                    printf("Found .swu file: %s\n", entry->d_name);

                    int res = system("/usr/bin/fw_setenv update_mode swupdate");
                    if (res == -1) {
                        printf("exec fw_setenv failed\n");
                        continue;
                    }

                    closedir(dir_ptr);
                    return 1; // Found a .swu file
                }
            }

            closedir(dir_ptr);
        }
    }

    return 0;
}

int main(int argc, char **argv) {
    printf("recoveryd enter\n");
    while (1) {
        sleep(2);

        if (checkOtaPackage()) {
            int res = system("/usr/bin/fw_setenv entry recovery");
            if (res == -1) {
                printf("exec fw_setenv failed\n");
                continue;
            }

            reboot(RB_AUTOBOOT);
        }
    }

    return 0;
}
