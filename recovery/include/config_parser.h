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

#ifndef RECOVERY_CONFIG_PARSER_H_
#define RECOVERY_CONFIG_PARSER_H_

enum {
    KM4BOOT,
    KM4IMAGE2,
    APIMAGE,
    VBMETA,
    DTB,
    KERNEL,
    ROOTFS,
    USERDATA,
    MISC,
    IMAGECNT
};

typedef struct image_info {
    int image_id;
    char dtb_info[128];
    char image_path[128];
    int flash_type;
} image_info_t;

#ifdef __cplusplus
extern "C" {
#endif

int parse_config(const char* config, int* skppedmd5);

#ifdef __cplusplus
}
#endif

#endif