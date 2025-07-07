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

int main(int argc, char **argv) {
    int res = 0;

    if (argc < 1) {
        fprintf(stderr, "Usage: %s  [--update_package=packagepath]" "[--wipe_userdata] \n", argv[0]);
        return 1;
    }

    argv += 1;

    while (*argv) {
        if (strstr(*argv, "--update_package") || !strcmp(*argv, "--wipe_userdata")) {
            char cmd[128], param[128];

            strncpy(param, *argv, strlen(*argv));
            param[strlen(*argv)] = '\0';
            snprintf(cmd, sizeof(cmd), "/usr/bin/fw_setenv cmd \" %s\"", param);

            res = system(cmd);
            if (res < 0) {
                break;
            }

            res = system("/usr/bin/fw_setenv entry recovery");
            if (res < 0) {
                break;
            }

            res = system("/usr/bin/fw_setenv update_mode normal");
            if (res < 0) {
                break;
            }
        } else if (!strcmp(*argv, "-c")) {
            res = system("/usr/bin/fw_setenv entry normal");
            if (res < 0) {
                break;
            }
        }

        if (*argv)
            argv++;
    }

    return res;
}
