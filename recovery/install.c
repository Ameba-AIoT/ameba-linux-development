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
#define LOG_TAG "Install"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

//#include "log/log.h"

#include "config_parser.h"
#include "install.h"

#if ENABLE_DIFFERENTIAL_UPDATE
#include "applyimages.h"
#endif

//#define printf RTLOGD

#define SLA "slotA"
#define SLB "slotB"

#define CONFIG_FILE_NAME "flash_image.cfg"

// define in DTB
#define KM4_BOOT_LABEL_IN_DTB "KM4 boot"
#define KM4_IMAGE2_LABEL_IN_DTB "KM4 image2"
#define AP_VBMETA_IMAGE_LABEL_IN_DTB "AP vbmeta"
#define AP_IMAGE_LABEL_IN_DTB "AP boot image"
#define DTB_IMAGE_LABEL_IN_DTB "Device tree blob"
#define KERNEL_IMAGE_LABEL_IN_DTB "Kernel image"
#define ROOTFS_IMAGE_LABEL_IN_DTB "Squashfs filesystem"
#define USERDATA_IMAGE_LABEL_IN_DTB "ubi/jffs2 filesystem"
#define MISC_IMAGE_LABEL_IN_DTB "AP Misc"


const char* gUpdateDir = "/tmp";
const char* gSlotInfo = "/sys/boot_slotinfo/boot_slotinfo";
static const char* gImagePath = "ota/updater.tar.gz";


// image_path: upgrade image name(such as: rootfs.img kernel.img)
// partitionInfo: upgrade to partition(such as: /dev/mtdblockX)
typedef struct upgrade_part_info {
    char    image_path[256];
    char    partition_info[256];
    char    partition_name[128];
} upgrade_part_info_t;

static upgrade_part_info_t gPartitionInfoSlot[SLOTCNT][IMAGECNT];


image_info_t gImageInfo[IMAGECNT] = {
    { KM4BOOT, KM4_BOOT_LABEL_IN_DTB, "",  0},
    { KM4IMAGE2, KM4_IMAGE2_LABEL_IN_DTB, "",  0 },
    { APIMAGE, AP_IMAGE_LABEL_IN_DTB, "",  0 },
    { VBMETA, AP_VBMETA_IMAGE_LABEL_IN_DTB, "",  0 },
    { DTB, DTB_IMAGE_LABEL_IN_DTB, "",  0 },
    { KERNEL, KERNEL_IMAGE_LABEL_IN_DTB, "",  0 },
    { ROOTFS, ROOTFS_IMAGE_LABEL_IN_DTB, "",  0 },
    { USERDATA, USERDATA_IMAGE_LABEL_IN_DTB, "",  0},
    { MISC, MISC_IMAGE_LABEL_IN_DTB, "",  0}
};

typedef struct manifest_type {
    uint32_t pattern[2];
    uint8_t  rsvd1[8];
    uint8_t  ver;
    uint8_t  image_id;
    uint8_t  auth_alg;
    uint8_t  hash_alg;
    uint16_t major_ver;
    uint16_t minor_ver;
    uint32_t image_size;
    uint32_t sec_epoch;
    uint8_t  rsip_iv[16];
} manifest_type_t;

static int get_current_slot() {
    uint32_t ver[SLOTCNT];
    int i;

    char* buf = (char*)malloc(sizeof(manifest_type_t));
    if (!buf) {
        printf("get_current_slot malloc failed\n");
        return INSTALL_ERROR;
    }

    for (i = 0; i < SLOTCNT; i++) {
        char path[64] = {0};
        char* p = NULL;

        const char* t = "block";
        char partInfo[128] = {0};

        int fd;
        uint16_t major, minor;

        strcpy(partInfo, gPartitionInfoSlot[i][APIMAGE].partition_info);
        p = strstr(partInfo, t);

        if (p) {
            strncpy(path, partInfo, p-partInfo);
            path[p - partInfo] = '\0';
            strcat(path, p + strlen(t));
            //printf("boot partition block: %s\n", path);
        } else {
            printf("error: boot partition block get failed\n");
            free(buf);
            buf = NULL;
            return INSTALL_ERROR;
        }

        fd = open(path, O_RDONLY);
        if (fd < 0) {
            printf("get_current_slot failed, %s is not existed\n", path);
            free(buf);
            buf = NULL;
            return INSTALL_ERROR;
        }

        read(fd, buf, sizeof(manifest_type_t));

        major = buf[20] << 8 | buf[21];  // major_ver, see manifest_type_t for more detail
        minor = buf[22] << 8 | buf[23];  // minor_ver, see manifest_type_t for more detail

        ver[i] = major << 16 | minor;
        printf("version[%d] = %d\n", i, ver[i]);
    }

    if (buf) {
        free(buf);
        buf = NULL;
    }

    return (ver[SLOTA] >= ver[SLOTB]) ? SLOTA : SLOTB;
}

static int exec_cmd(const char* cmd) {
    //printf("exec_cmd %s\n", cmd);
    pid_t sta = system(cmd);
    if (sta == -1) {
        printf("system %s error\n", cmd);
        return -1;
    } else {
        //printf("exit status value = [0x%x]\n", sta);
        if (WIFEXITED(sta)) {
            if (0 == WEXITSTATUS(sta)) {
                //printf("run %s successfully.\n", cmd);
                return 0;
            } else {
                printf("run %s fail, script exit code: %d\n", cmd, WEXITSTATUS(sta));
                return -1;
            }
        } else {
            printf("exit status = [%d]\n", WEXITSTATUS(sta));
            return -1;
        }
    }
    return 0;
}

static int get_file_md5(const char* file, const char* buff) {
    if(!file || !buff) return -1;

    int len = 0;
    char tmp[256] = {0};
    FILE *fp = fopen(file, "r");
    if(fp == NULL) {
        printf("open %s failed\n", file);
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    len = ftell(fp);
    if (len <= 0) return -1;

    rewind(fp);

    fread(tmp, 1, len, fp);
    strcpy((char*)buff, tmp);

    fclose(fp);

    return 0;

}

static int verify_md5(const char* src, const char* target) {
    if(src == NULL || target == NULL) return -1;

    char sbuff[256] = {0};
    char tbuff[256] = {0};
    get_file_md5(src, sbuff);
    get_file_md5(target, tbuff);

    if(!strcmp(sbuff, tbuff)) {
        printf("%s check pass\n", target);
        return 0;
    }

    printf("%s check failed, src: %s, target: %s\n", target, sbuff, tbuff);
    return -1;
}

int get_misc_partition(char* part) {
    if (!part) return -1;

    for(int j = 0; j < IMAGECNT; j++) {
        if(strstr(MISC_IMAGE_LABEL_IN_DTB, gImageInfo[j].dtb_info)) {
            int id = gImageInfo[j].image_id;
            strcpy(part, gPartitionInfoSlot[0][id].partition_info);
            printf("get_misc_partition, misc block: %s\n", part);
            return 0;
        }
    }

    return -1;
}

static int verify_package(const char* partinfo, const char* mdinfo) {
    if(partinfo == NULL || mdinfo == NULL) return -1;

    char cmd[256] = {0};
    const char* tmp = "/tmp/md5";
    sprintf(cmd, "md5sum %s |  cut -d ' ' -f1 > %s", partinfo, tmp);
    if(exec_cmd(cmd) != 0) {
        printf("verify package exec cmd faied;\n");
        return -1;
    }

    if(verify_md5(tmp, mdinfo) != 0) return -1;

    remove(tmp);  // delete tmp md5 file
    return 0;
}

static int parse_mtd_partition_info(const char* str) {
    if(!str) {
        printf("parse_mtdpartition_info string is null\n");
        return -1;
    }

    //printf("parse_mtdpartition_info str: %s\n", str);

    char* pc = NULL;
    char* pn = NULL;
    char dev[64] = {0};
    char dblock[64] = "/dev/mtdblock";
    char name[128] = {0};

    pc = (char *)strchr(str, ':');

    if(pc) {
        strncpy(dev, str, pc - str);
        dev[pc - str] = '\0';
        //printf("dev: %s\n", dev);
    }

    if(!pc || strstr(dev, "mtd") == NULL) {
        return 0;
    } else {
        strcat(dblock, dev+3);
        //printf("dblock: %s\n", dblock);
    }

    pn = (char *)strchr(str, '"');
    if(pn) {
        char * pl = (char *)strchr(pn+1, '"');
        if(!pl) return -1;
        strncpy(name, pn+1, pl - pn -1);
        name[pl - pn -1] = '\0';
        //printf("dev block name: %s\n", name);
    }

    int slot = -1;
    if (strstr(name, "slotA")) {
        slot = SLOTA;
    } else if (strstr(name, "slotB"))
        slot = SLOTB;

    //printf("dev block name: %s, slot: %d\n", name, slot);

    for(int i = 0; i < SLOTCNT; i++) {
        if(slot == -1 || i == slot) {
            for(int j = 0; j < IMAGECNT; j++) {
                if(strstr(name, gImageInfo[j].dtb_info)) {
                    int id = gImageInfo[j].image_id;
                    strcpy(gPartitionInfoSlot[i][id].image_path, gImageInfo[j].image_path); // null at this moment
                    strcpy(gPartitionInfoSlot[i][id].partition_info, dblock);
                    strcpy(gPartitionInfoSlot[i][id].partition_name, name);
                    //printf("slot: %d, image_id: %d, dtb_info: %s, imageInfo:%s\n", i, gImageInfo[j].image_id, gImageInfo[j].dtb_info, gImageInfo[j].image_path);
                    break;
                }
            }
        }
    }

    return 0;
}

static int umount_data_partition(const char* fs_type) {
    const char* rcs = "/etc/init.d/rcS";
    //const char* ubifs = "ubifs";

    FILE* fp = fopen(rcs, "r");

    if (fp == NULL) {
        printf("open rcs failed\n");
        return -1;
    }

    char buffer[64];
    while(fgets(buffer, sizeof(buffer), fp) != NULL) {
        if (strstr(buffer, fs_type) != NULL) {
            char* mount_point = strrchr(buffer, ' ');
            if (mount_point) {
                char cmd[32];
                printf("userdata mount point: %s\n", mount_point);
                sprintf(cmd, "umount %s", mount_point);
                if (exec_cmd(cmd) < 0) {
                    printf("umount userdata failed\n");
                    return -1;
                }
            }
            break;
        }
    }

    return 0;
}

int wipe_data(int partition, int force_erase) {
    const char* ubi_node = "/dev/ubi0_0";
    if (force_erase == 0 && access(ubi_node, F_OK) == F_OK) { // nand flash
        char upcmd[32];
        printf("Nand flash wipe data\n");
        if (umount_data_partition("ubifs") < 0) {
            printf("umount ubi data partition failed\n");
            return -1;
        }

        sprintf(upcmd, "ubiupdatevol %s -t", ubi_node);
        if (exec_cmd(upcmd) < 0 ) {
            printf("ubiupdatevol error");
            return -1;
        }
    } else {
        char path[64] = {0};
        char wcmd[128] = {0};
        char* p = NULL;

        const char* t = "block";
        char partInfo[128] = {0};
        int slot;
        if (partition < 0 || partition >= IMAGECNT) {
            printf("Error, invalid parition\n");
            return -1;
        }

        if(get_current_slot() == SLOTA)
            slot = SLOTB;
        else
            slot = SLOTA;

        strcpy(partInfo, gPartitionInfoSlot[slot][partition].partition_info);
        p = strstr(partInfo, t);
        //printf("p: %s\n", p);

        if(p) {
            strncpy(path, partInfo, p-partInfo);
            path[p - partInfo] = '\0';
            strcat(path, p + strlen(t));
        }

        printf("partition block: %s\n", path);
        if (force_erase == 0 && umount_data_partition("jffs2") < 0) {
            printf("umount jffs2 data partition failed\n");
            return -1;
        }

        sprintf(wcmd, "flash_eraseall %s ", path);
        if( exec_cmd(wcmd) < 0 ) {
            printf("wipe_data error");
            return -1;
        }

    }

    return 0;
}

static int upgrade_package(const char* path, const int update_from_tar) {
    printf("upgrade_package enter\n");
    if (!path){
        printf("upgrade_package path is null\n");
        return INSTALL_ERROR;
    }

    // update directory
    char zip_path[64];  // for zip update, indicate the zip path
    char target_path[32];
    if (update_from_tar == 1) {
        strcpy(target_path, gUpdateDir); // /tmp
        strcpy(zip_path, path);
        //sprintf(zip_path, "%s/%s", path, gImagePath);
    } else
        strcpy(target_path, path);  ///mnt/usb/storage/ota/

    int md5check = 0;
    int status;
    int sta;

    char config[128] = {0};
    sprintf(config, "%s/%s", target_path, CONFIG_FILE_NAME);
    FILE * fp = fopen(config, "r");

    if(fp == NULL) {
        printf("open image config file failed\n");
        return -1;
    }

    if (parse_config(config, &md5check)) {
        printf("Failed to parse image config\n");
        return INSTALL_ERROR;
    } else {
        for(int i = 0; i < IMAGECNT; i++) {
            for(int j = 0; j < SLOTCNT; j++) {
                strcpy(gPartitionInfoSlot[j][i].image_path, gImageInfo[i].image_path); // update image path
            }
        }
    }

    pid_t pid = fork();

    if (pid == -1) {
        printf("Failed to fork update process\n");
        return INSTALL_ERROR;
    }

    if (pid == 0) {
        printf("Update Process Enter\n");
        int slot;
        if(get_current_slot() == SLOTA) {
            slot = SLOTB;
        } else {
            slot = SLOTA;
        }

        printf("update to: %s\n", (slot==SLOTA?SLA:SLB));

        for(int i = 0; i < IMAGECNT; i++) {
            char partinfo[128] = {0};
            int cfd;

            if(gImageInfo[i].flash_type == 0) continue;

            if (update_from_tar) { // need to unzip seperately to /tmp
                char cmd[256] = {0};
                sprintf(cmd, "tar -xzvf %s %s -C %s", zip_path, gPartitionInfoSlot[slot][i].image_path, target_path);
                //printf("update_package cmd: %s\n", cmd);
                sta = exec_cmd(cmd);
            }

            sprintf(partinfo, "%s/%s", target_path, gPartitionInfoSlot[slot][i].image_path);

            printf("partinfo: %s\n", partinfo);

            cfd = open(partinfo, O_RDONLY);
            //printf("Flash Image: %s\n", partinfo);
            if (cfd != -1) {
                if (md5check == 0) {
                    char mdfile[256] = {0};

                    if (update_from_tar) { // need to unzip .md5 seperately to /tmp
                        char cmd[256] = {0};
                        sprintf(cmd, "tar -xzvf %s ./%s.md5 -C %s", zip_path, gPartitionInfoSlot[slot][i].image_path, target_path);
                        //printf("update_package cmd: %s\n", cmd);
                        sta = exec_cmd(cmd);
                    }

                    sprintf(mdfile, "%s.md5", partinfo);

                    int mdfd = open(mdfile, O_RDONLY);
                    if(mdfd == -1) {
                        printf("Error: %s.md5 file not exist\n", partinfo);
                        _exit(EXIT_FAILURE);
                    }

                    if(verify_package(partinfo, mdfile)) {
                        printf("Error: %s verify failed\n", partinfo);
                        _exit(EXIT_FAILURE);
                    }
                }

                if (i == ROOTFS || i == USERDATA) {
                    printf("erase %s\n", (i == ROOTFS)?"rootfs":"userdata");
                    if (wipe_data(i, 1) < 0) {
                        printf("erase %s failed\n", (i == ROOTFS)?"rootfs":"userdata");
                        _exit(EXIT_FAILURE);
                    }
                }

                char dcmd[256] = {0};
                printf("%s check ok, now update to %s\n", partinfo, gPartitionInfoSlot[slot][i].partition_info);
                sprintf(dcmd, "dd if=%s of=%s", partinfo, gPartitionInfoSlot[slot][i].partition_info);

                sta = exec_cmd(dcmd);
                if (sta == 0) {
                    if (update_from_tar == 1) {
                        char rmcmd[256] = {0};
                        sprintf(rmcmd, "rm -fr %s*", partinfo);
                        exec_cmd(rmcmd);
                    }
                } else
                    _exit(EXIT_FAILURE);
                exec_cmd("sync");
            } else {
                printf("image: %s not existed\n", partinfo);
                _exit(EXIT_FAILURE);
            }
        }
        _exit(EXIT_SUCCESS);
    }

    waitpid(pid, &status, 0);
    printf("Recovery Process: Update Process Exited\n");

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        printf("Install Error, status: %d\n", WEXITSTATUS(status));
        return INSTALL_ERROR;
    }

    return INSTALL_SUCCESS;
}

int install_package(const char* package) {
    printf("install_package enter\n");

    int status;
    int sta;

    if(!package) return -1;

    char path[64];
    sprintf(path, "%s/ota/%s", package, CONFIG_FILE_NAME);

    if (access(path, F_OK) == F_OK) { // update from ota direcory
        printf("upgrae_package from ota directory\n");
        char imagepath[64];
        sprintf(imagepath, "%s/ota", package);
        upgrade_package(imagepath, 0);
    } else { //update from gz file
        char gzpath[64];
        char gzpackage[64];
        sprintf(gzpath, "%s/%s", package, gImagePath);
        if (access(gzpath, F_OK) == F_OK) {
            printf("upgrae_package from gz file\n");
            strcpy(gzpackage, gzpath);
        } else {
            int fd = open(package, O_RDONLY);

            if (fd == -1) {
                printf("%s is not existed\n", package);
                return INSTALL_ERROR;
            }
            strcpy(gzpackage, package);
        }

        char cmd[256] = {0};
        sprintf(cmd, "tar -xzvf %s %s -C %s", gzpackage, CONFIG_FILE_NAME ,gUpdateDir);
        printf("install_package cmd: %s\n", cmd);

        sta = exec_cmd(cmd);
        if(sta == 0) {
            sta = upgrade_package(gzpackage, 1);
            //sta = upgrade_package(package, 1);
        } else {
            printf("exec tar failed\n");
            return INSTALL_ERROR;
        }

        sprintf(cmd, "rm -fr %s/*", gUpdateDir);
        printf("install_package cmd: %s\n", cmd);

        exec_cmd(cmd);
        return sta;
    }

    return INSTALL_SUCCESS;
}

int load_partition_info() {
    printf("load_partition_info\n");

    int sta;
    char cmd[128] = {0};
    const char* tmp = "/tmp/partinfo.txt";
    sprintf(cmd, "cat /proc/mtd > %s", tmp);

    sta = exec_cmd(cmd);
    if(sta == 0) {
        FILE * fp = fopen(tmp, "r");

        if(fp == NULL) {
            printf("open partinfo failed\n");
            return -1;
        }

        char str[60];
        while(fgets(str, 60, fp) != NULL) {
            if( parse_mtd_partition_info(str) < 0 ) {
                printf("parse_mtdpartition_info failed\n");
                return -1;
            }
        }

        fclose(fp);

        sprintf(cmd, "rm -fr %s", tmp);
        exec_cmd(cmd);

        return 0;
    }

    return -1;
}


/*************************************************************************
 **********************Differential Update********************************
 *************************************************************************/
#if ENABLE_DIFFERENTIAL_UPDATE

static int get_image_dir(const char* image, char* path) {
    if (!image) return -1;
    printf("get_image_dir image:%s\n", image);

    char *p = (char *)strchr(image, '.');

    if (p) {
        strncpy(path, image, p - image);
        printf("get_image_dir image name: %s\n", path);
        return 0;
    }

    return -1;
}


int install_differential_package(const char* package) {
    printf("install_differential_package enter\n");
    if(package == NULL) {
        printf("Error: install_differential_package package is NULL\n");
        return INSTALL_ERROR;
    }

    int status = 0;

    pid_t pid = fork();

    if (pid == -1) {
        printf("Failed to fork update process for differential update\n");
        return INSTALL_ERROR;
    }

    if (pid == 0) {
        char cmd[128] = {0};
        char cur_path[64];
        char zip_name[64];
        /*sprintf(cmd, "applypatch -u %s %s", package, gUpdateDir);
        if (exec_cmd(cmd) < 0) {
            printf("exec_cmd %s failed\n", cmd);
            goto UPDATE_ERR;
        }*/

        int sta = unzip_file((char*)package, "", (char*)gUpdateDir, "");
        if(sta < 0) {
            printf("unzip %s failed\n", package);
        }

        char* p = (char *)strrchr(package, '/');
        char* dot = (char *)strrchr(package, '.');
        if(p && dot && dot - p > 0)
            strncpy(zip_name, p, dot - p);
        printf("zip_name: %s\n", zip_name);

        strcpy(cur_path, gUpdateDir);
        strcat(cur_path, zip_name); // update.zip
        //strcat(cur_path, "/update"); // update.zip

        int md5check = 0;

        char config[128] = {0};
        sprintf(config, "%s/%s", cur_path, CONFIG_FILE_NAME);
        FILE * fp = fopen(config, "r");

        if(fp == NULL) {
            printf("open image config file failed\n");
            goto UPDATE_ERR;
        }

        if (parse_config(config, &md5check)) {
            printf("Failed to parse image config\n");
            goto UPDATE_ERR;
        } else {
            for(int i = 0; i < IMAGECNT; i++) {
                for(int j = 0; j < SLOTCNT; j++) {
                    strcpy(gPartitionInfoSlot[j][i].image_path, gImageInfo[i].image_path); // update image path
                }
            }
        }

        int slot = get_current_slot();

        printf("current slot: %s\n", (slot==SLOTA?SLA:SLB));

        for(int i = 0; i < IMAGECNT; i++) {
            char partitioninfo[256] = { 0 };
            char imageinfo[128] = { 0 };
            char image_dir[64] = { 0 };

            if(gImageInfo[i].flash_type == 0) continue;

            if(get_image_dir(gPartitionInfoSlot[slot][i].image_path, image_dir) < 0) {
                printf("get %s image dir failed\n", gPartitionInfoSlot[slot][i].image_path);
                goto UPDATE_ERR;
            }

            sprintf(imageinfo, "%s/%s", cur_path, image_dir);

            printf("imageinfo: %s\n", imageinfo);

            if (access(imageinfo, F_OK) == F_OK) {
                /*char dcmd[256] = {0};
                printf("applypatch %s %s\n", gPartitionInfoSlot[slot][i].partition_info, imageinfo);
                sprintf(dcmd, "applypatch %s %s", gPartitionInfoSlot[slot][i].partition_info, imageinfo);

                if(exec_cmd(dcmd) < 0) {
                    printf("applypatch %s failed\n", gImageInfo[i].dtb_info);
                    goto UPDATE_ERR;
                }*/
                //printf("applypatch %s %s\n", gPartitionInfoSlot[slot][i].partition_info, imageinfo);
                status = patchImage(gPartitionInfoSlot[slot][i].partition_name, imageinfo);
                if(status != 0) {
                    printf("applypatch %s failed, status: %d\n", gImageInfo[i].dtb_info, status);
                    goto UPDATE_ERR;
                }
                printf("applypatch %s success\n", gImageInfo[i].dtb_info);
            } else {
                printf("image: %s not existed\n", imageinfo);
                goto UPDATE_ERR;
            }
        }

        _exit(EXIT_SUCCESS);


UPDATE_ERR:
        _exit(EXIT_FAILURE);

    }

    waitpid(pid, &status, 0);
    printf("Recovery Process: Update Process Exited\n");

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        printf("Install Error, status: %d\n", WEXITSTATUS(status));
        return INSTALL_ERROR;
    }

    return INSTALL_SUCCESS;
}

#endif
