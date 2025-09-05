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
#define LOG_TAG "Recovery"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/reboot.h>

#include "config_parser.h"
#include "install.h"
#include "storage_mount/storage_mount.h"
//#include "log/log.h"

//#define printf RTLOGD
#define RETRYCOUNT 5

//recovery partiontion block path
static char gMiscBlock[32] = "";
static int gStorageState = 0;
static char gStoragePath[64] = "";

static const struct option gOptions[] = {
    { "update_package", required_argument, NULL, 'u' },
    { "retry_count", required_argument, NULL, 'n' },
    { "wipe_userdata", no_argument, NULL, 'w' },
    { NULL, 0, NULL, 0 },
};

static int finish_recovery() {
    int res = system("/usr/bin/fw_setenv entry normal");
    if (res == -1) {
        printf("exec fw_setenv failed\n");
        return res;
    }

    system("/usr/bin/fw_setenv cmd");

    return 0;
}

static int get_uboot_env(const char *key, char *value, int value_size) {
    char cmd[128], val[128];
    snprintf(cmd, sizeof(cmd), "/usr/bin/fw_printenv %s", key);
    FILE *fp = popen(cmd, "r");
    if (!fp) return -1;

    if (!fgets(val, sizeof(val), fp)) {
        printf("variable %s is not set in u-boot-env\n", key);
        pclose(fp);
        return -1;
    }
    pclose(fp);

    // expect format: cmd= --update_package=XXX
    char *eq = strchr(val, '=');
    if (!eq) return -1;

    eq++;
    while (*eq == ' ') eq++;
    char *end = strchr(eq, '\n');
    if (end) *end = '\0';

    int len = strlen(eq) + 1;
    strncpy(value, eq, len);
    value[len] = '\0';

    return 0; // success
}


static int get_args(char** arg, int* num, const char* delim) {
    if(!delim) return -1;

    char value[128];
    if (get_uboot_env("cmd", value, sizeof(value))) {
        printf("get_uboot_env failed\n");
        return -1;
    }

    char* pNext = NULL;
    int count = *num;  //same value with num;
    arg++; // skip 0 for args count

    pNext = (char *)strtok(value, delim);
    //pNext = (char *)strtok(boot.recovery, delim);
    while (pNext != NULL) {
        printf("args: %s\n", pNext);
        *arg++ = pNext;
        ++count;
        pNext = (char *)strtok(NULL, delim);
    }
    *num = count;

    return 0;
}

int storage_state_change(int state, const char* path) {
    printf("storage mount change:%d\n", state);
    if (STORAGE_MOUNT_EVENT == state) {
        printf("storage mount success path:%s\n", path);
        strcpy(gStoragePath, path);
        gStorageState = STORAGE_MOUNT_EVENT;
    } else if(STORAGE_UNMOUNT_EVENT == state) {
        printf("storage umounted!\n");
        strcpy(gStoragePath, "");
        gStorageState = STORAGE_UNMOUNT_EVENT;
    }
    return 1;
}

int main(int argc, char **argv) {

    printf("Recovery Process Enter\n");

    int status = INSTALL_SUCCESS;
    int retrycount = 0;
    const char* dir_name = "ota";
    char update_dir[128];
    int install_from_usb = 0;

    mount_storage(storage_state_change);

    //wait some time for storage mount callback
    while(retrycount++ < RETRYCOUNT) {
        if (gStorageState == STORAGE_MOUNT_EVENT && strcmp(gStoragePath, "")) {
            printf("storage mount success path:%s\n", gStoragePath);
            sprintf(update_dir, "%s/%s", gStoragePath, dir_name);
            install_from_usb = 1;
            break;
        } else {
            sleep(1);
        }
    }

    //const char* package_path = "/data/updater.tar.gz";
    int res = load_partition_info();

    if (res) {
        printf("load_partition_info failed\n");
        goto INSTALL_FAILED;
    }

    //args[0] for cnt of argvs
    int cnt = 1;
    char* args[8] = {0};

    res = get_args(args, &cnt, " ");

    if (res < 0 || cnt == 1) {
        if (install_from_usb)
            goto install_directly;
        else {
            printf("get args failed\n");
            goto INSTALL_FAILED;
        }
    }

    //assign args cnt args[0]
    char t[32];
    sprintf(t, "%d", cnt);
    *args = t;

    int need_WipeData = 0;
    int retry_count = 0;
    char* package_path = NULL;
    int should_delete_package = 0;

    int arg;
    int option_index;
    while ((arg = getopt_long(cnt, args, "", gOptions, &option_index)) != -1) {
        printf("arg: %c, optarg: %s\n", arg, optarg);
        switch (arg) {
            case 'n':
                retry_count = atoi(optarg);
                break;
            case 'u':
                package_path = strdup(optarg);
                break;
            case 'w':
                need_WipeData = 1;
                break;
            case '?':
                printf("Invalid command argument\n");
                continue;
        }
    }

    printf("package_path: %s, retry_count: %d, need_WipeData: %d\n", package_path, retry_count, need_WipeData);

install_directly:
#if ENABLE_DIFFERENTIAL_UPDATE
    if (package_path && strstr(package_path, ".zip") != NULL) {
        printf("%s existed, install differential package\n", package_path);
        status = install_differential_package(package_path);
    } else if (strcmp(gStoragePath, "") && access(update_dir, F_OK) == F_OK) {
#else
    if (strcmp(gStoragePath, "") && access(update_dir, F_OK) == F_OK) {
#endif
        printf("%s existed, install package from storage\n", update_dir);
        status = install_package(gStoragePath);
    } else if (package_path != NULL) {
        status = install_package(package_path);
        if (status == INSTALL_SUCCESS)
            should_delete_package = 1;
    } else if (need_WipeData) {
        if (wipe_data(USERDATA, 0)) {
            printf("Error: WipeData error\n");
            goto INSTALL_FAILED;
        }
    } else {
        printf("Error: package_path is NULL\n");
        goto INSTALL_FAILED;
    }

    if(status != INSTALL_SUCCESS) {
        printf("Installation aborted.\n");
        goto INSTALL_FAILED;
    }

    finish_recovery();

    printf("status: %d, package_path: %s\n", status, package_path);
    if (status == INSTALL_SUCCESS) {
        if (package_path && should_delete_package == 1) {
            char cmd[32];
            //sprintf(cmd, "rm -fr %s/ota", package_path);
            sprintf(cmd, "rm -fr %s", package_path);
            system(cmd);

            system("sync");
            printf("delete ota package\n");
        }
    }

    printf("Success: Recovery Successful!  Return to normal system now\n");

    reboot(RB_AUTOBOOT);
    return INSTALL_SUCCESS;

INSTALL_FAILED:
    printf("Error: Recovery Failed!  Reboot to normal system\n");
    finish_recovery();
    reboot(RB_AUTOBOOT);
    return INSTALL_ERROR;

}
