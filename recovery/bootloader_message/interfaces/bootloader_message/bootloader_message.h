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

#ifndef RECOVERY_BOOTLOADER_MESSAGE_H
#define RECOVERY_BOOTLOADER_MESSAGE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//Bootloader Message (2KB)
struct bootloader_message {
    char cmd[32];
    char status[32];
    char recovery[768];

    char stage[32];
    char reserved[1184];
};

int get_bootloader_message(struct bootloader_message *out, const char* misc);

int set_bootloader_message(struct bootloader_message *in, const char* misc);

int clear_bootloader_message(const char* misc);

#ifdef __cplusplus
}
#endif

#endif

