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
#define LOG_TAG "test"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bootloader_message/bootloader_message.h"
//#include "log/log.h"

//#define printf RTLOGD
const char* gDataBlock = "/dev/mtdblock0";

int main(int argc, char **argv) {

    if (argc < 1) {
        fprintf(stderr, "Usage: %s  [--update_package=packagepath]" "[--wipe_userdata] \n", argv[0]);
        return 1;
    }

    argv += 1;

    while(*argv) {
        if(strstr(*argv, "--update_package") || !strcmp(*argv, "--wipe_userdata")){
            struct bootloader_message boot = {};
            strcpy(boot.recovery, *argv);
            strcpy(boot.cmd, "boot-recovery");
            set_bootloader_message(&boot, gDataBlock);
        }else if(!strcmp(*argv, "-c")){
            clear_bootloader_message(gDataBlock);
        }

        if(*argv)
            argv++;
    }

    return 0;
}
