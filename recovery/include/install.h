/*
 * Copyright (c) 2022 Realtek, LLC.
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

#ifndef RECOVERY_INSTALL_H_
#define RECOVERY_INSTALL_H_

#define ENABLE_DIFFERENTIAL_UPDATE 0

enum {
    INSTALL_SUCCESS,
    INSTALL_ERROR,
    INSTALL_CORRUPT,
    INSTALL_NONE,
    INSTALL_SKIPPED,
    INSTALL_RETRY
};

/*
enum {
    KM4BOOT,
    KM4IMAGE2,
    APIMAGE,
    DTB,
    KERNEL,
    ROOTFS,
    USERDATA,
    IMAGECNT
};*/

enum {
    SLOTA,
    SLOTB,
    SLOTCNT
};

#ifdef __cplusplus
extern "C" {
#endif

int load_partition_info();

int install_package(const char* package);

int wipe_data(int partition, int force_erase);

int get_misc_partition(char* part);

#if ENABLE_DIFFERENTIAL_UPDATE
int install_differential_package(const char* package);
#endif

#ifdef __cplusplus
}
#endif

#endif
