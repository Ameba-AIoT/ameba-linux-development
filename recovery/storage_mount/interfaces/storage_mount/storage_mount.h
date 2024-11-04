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
#ifndef RECOVERY_STORAGE_MOUNT_H
#define RECOVERY_STORAGE_MOUNT_H

#define STORAGE_MOUNT_EVENT     1
#define STORAGE_UNMOUNT_EVENT   2

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*storage_fun_ptr) (int state, const char* path);

int mount_storage(storage_fun_ptr pfunc);

#ifdef __cplusplus
}
#endif

#endif  //RECOVERY_STORAGE_MOUNT_H
