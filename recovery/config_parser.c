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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "cJSON.h"
#include "config_parser.h"

//#define RECOVERY_CONFIGURATION_FILE "/tmp/config"
#define CONFIG_PARSE_SUCCESS  0
#define CONFIG_PARSE_FAILED -1

#define IMAGE_ARR_NAME_IN_JSON "images"
#define IMAGE_NAME "name"
#define IMAGE_PATH "path"
#define IMAGE_FLASH_TYPE "flash_type"

static const char* gConfigInfo[] = {
    IMAGE_NAME,
    IMAGE_PATH,
    IMAGE_FLASH_TYPE
};

extern image_info_t gImageInfo[IMAGECNT];

static char* read_file_to_buffer(const char* config) {
    char* buffer = NULL;
    FILE* fd = NULL;
    struct stat fileStat = {0};
    do {
        if (stat(config, &fileStat) != 0 || fileStat.st_size <= 0) {
            break;
        }

        fd = fopen(config, "r");
        if (fd == NULL) {
            break;
        }

        buffer = (char*)malloc(fileStat.st_size + 1);
        if (buffer == NULL) {
            break;
        }

        if (fread(buffer, fileStat.st_size, 1, fd) != 1) {
            free(buffer);
            buffer = NULL;
            break;
        }
        buffer[fileStat.st_size] = '\0';
    } while (0);

    if (fd != NULL) {
        fclose(fd);
        fd = NULL;
    }
    return buffer;
}

static int get_image_info(const cJSON* curArrItem, image_info_t* curImage, const char* partName) {
    cJSON* filedJ = cJSON_GetObjectItem(curArrItem, partName);

    if(!strcmp(partName, IMAGE_NAME) || !strcmp(partName, IMAGE_PATH)) {
        char* fieldStr = cJSON_GetStringValue(filedJ);
        if (fieldStr == NULL) {
            return CONFIG_PARSE_FAILED;
        }

        size_t strLen = strlen(fieldStr);
        if (strLen == 0) {
            return CONFIG_PARSE_FAILED;
        }

        if(!strcmp(partName, IMAGE_NAME)) {
            memcpy(curImage->dtb_info, fieldStr, strLen);
            curImage->dtb_info[strLen] = '\0';
            //printf("image_info_t name: %s\n", curImage->dtb_info);
        } else {
            memcpy(curImage->image_path, fieldStr, strLen);
            curImage->image_path[strLen] = '\0';
            //printf("image_info_t image path: %s\n", curImage->image_path);
        }
    } else if (!strcmp(partName, IMAGE_FLASH_TYPE)) {
        int value = (int)cJSON_GetNumberValue(filedJ);
        if (value < 0) {
            return CONFIG_PARSE_FAILED;
        }
        curImage->flash_type = value;
        //printf("image_info_t flash_type: %d\n", curImage->flash_type);
    }

    return CONFIG_PARSE_SUCCESS;
}

static cJSON* get_array_item(const cJSON* fileRoot, int* arrSize, const char* arrName) {
    cJSON* arrItem = cJSON_GetObjectItemCaseSensitive(fileRoot, arrName);
    if (!cJSON_IsArray(arrItem)) {
        printf("[Init] get_array_item, item %s is not an array!\n", arrName);
        return NULL;
    }

    *arrSize = cJSON_GetArraySize(arrItem);
    if (*arrSize <= 0) {
        return NULL;
    }
    return arrItem;
}

static int parse_all_partitions(const cJSON* fileRoot) {
    int imageArrSize = 0;
    cJSON* imageArr = get_array_item(fileRoot, &imageArrSize, IMAGE_ARR_NAME_IN_JSON);
    if (imageArr == NULL) {
        printf("get array %s failed.\n", IMAGE_ARR_NAME_IN_JSON);
        return CONFIG_PARSE_FAILED;
    }

    image_info_t* retImages = (image_info_t*)malloc(sizeof(image_info_t) * imageArrSize);
    if (retImages == NULL) {
        printf("malloc for %s arr failed! %d.\n", IMAGE_ARR_NAME_IN_JSON, imageArrSize);
        return CONFIG_PARSE_FAILED;
    }

    memset(retImages, 0, sizeof(image_info_t) * imageArrSize);

    for (int i = 0; i < imageArrSize; ++i) {
        cJSON* curItem = cJSON_GetArrayItem(imageArr, i);
        int cnt = sizeof(gConfigInfo)/sizeof(gConfigInfo[0]);
        for(int j = 0; j < cnt; j++) {
            if (get_image_info(curItem, &retImages[i], gConfigInfo[j]) ) {
                printf("parse information for service %d failed.\n", i);
                return CONFIG_PARSE_FAILED;
            }
        }

        for(int k = 0; k < IMAGECNT; k++) {
            if(!strcmp(retImages[i].dtb_info, gImageInfo[k].dtb_info)) {
                strcpy(gImageInfo[k].image_path, retImages[i].image_path);
                gImageInfo[k].flash_type =  retImages[i].flash_type;
                printf("update image_path: %s, flash_type: %d\n", gImageInfo[k].image_path, gImageInfo[k].flash_type);
                break;
            }
        }
    }

    free(retImages);
    retImages = NULL;
    return CONFIG_PARSE_SUCCESS;
}

int parse_config(const char* config, int* skipmd5 ) {
    if(!config) return CONFIG_PARSE_FAILED;

    char* fileBuf = read_file_to_buffer(config);
    if (fileBuf == NULL) {
        printf("read file %s failed! err %d.\n", config, errno);
        return CONFIG_PARSE_FAILED;
    }

    cJSON* fileRoot = cJSON_Parse(fileBuf);
    free(fileBuf);
    fileBuf = NULL;

    if (fileRoot == NULL) {
        printf("parse failed! please check file %s format.\n", config);
        return CONFIG_PARSE_FAILED;
    }

    cJSON* skipped = cJSON_GetObjectItem(fileRoot, "skip_md5_check");
    if (skipped) {
        *skipmd5 = atoi(skipped->valuestring);
        printf("skip_md5_check: %d\n", *skipmd5);
    }

    if(parse_all_partitions(fileRoot)) {
        printf("parse all partitions failed!\n");
        return CONFIG_PARSE_FAILED;
    }

    // release memory
    cJSON_Delete(fileRoot);

    return CONFIG_PARSE_SUCCESS;
}