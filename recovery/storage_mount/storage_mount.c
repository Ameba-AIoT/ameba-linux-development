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
#include <dirent.h>
#include <linux/netlink.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <sys/mount.h>
#include <stddef.h>
#include <pthread.h>
#include <linux/kdev_t.h>
#include "storage_mount/storage_mount.h"

#define OTA_USB_DEBUG //printf

#define         BASE_BUFFER_SIZE        1024
#define         NL_PARAMS_MAX           32
#define         STORAGE_MAX_PARTITIONS  32
#define         STORAGE_ADD             1
#define         STORAGE_REMOVE          2
#define         State_NoMedia           0
#define         State_Idle              1
#define         State_Pending           2
#define         State_Mounted           4
#define         State_Unmounting        5
#define         FALSE                   0
#define         TRUE                    1

struct ucred {
        pid_t pid;
        uid_t uid;
        gid_t gid;
};
static const char MEDIA_DIR[]  = "/mnt/media_rw";
static const char DEV_SDCARD[] = "/devices/platform/ocp/400d0000.sdioh";
static const char DEV_USB[]    = "/devices/platform/ocp/40080000.usb";

#define HAS_CONST_PREFIX(str,end,prefix)  has_prefix((str),(end),prefix,(sizeof(prefix)-1))
#define GET_CUR_MOUNT_POINT     gStorageStruct.mount_point
#define GET_CUR_STATUE          gStorageStruct.storage_state

typedef struct {
    int                     uevent_action;
    char                    *uevent_subsystem;
    char                    *uevent_params[NL_PARAMS_MAX];
}struct_uevent_t;

typedef struct {
    unsigned char           is_sdcard_dev ;
    unsigned char           storage_state ;
    dev_t                   current_mount_kdev;
    unsigned char           retry_mount_num;
    const char             *mount_point;
    int                     disk_major;
    int                     disk_minor;
    int                     part_minors[STORAGE_MAX_PARTITIONS];
    int                     disk_num_parts;
    unsigned int            pending_part_map;
}struct_storage_t;

static storage_fun_ptr      gStorageCall = NULL ;
static struct_storage_t     gStorageStruct ;
static int                  gNetlinkSock = 0 ;

static const char* has_prefix(const char* str, const char* end, const char* prefix, size_t prefixlen) {
    if ((end - str) >= (ptrdiff_t)prefixlen &&
        (prefixlen == 0 || !memcmp(str, prefix, prefixlen))) {
        return str + prefixlen;
    } else {
        return NULL;
    }
}
static void report_message(int state, const char* path) {
    if(NULL != gStorageCall ) {
        if( State_Mounted == state ){
            gStorageCall(STORAGE_MOUNT_EVENT , path);
        }
    }
}
static ssize_t read_uevent(int fd, void *buf, size_t len) {
    struct iovec iov = { buf, len };
    struct sockaddr_nl addr;
    char control[CMSG_SPACE(sizeof(struct ucred))];
    struct msghdr hdr = { &addr, sizeof(addr), &iov, 1, control, sizeof(control), 0, };
    ssize_t n = recvmsg(fd, &hdr, 0);
    if (n <= 0) {
        return n;
    }
    struct cmsghdr *cMsg = CMSG_FIRSTHDR(&hdr);
    if (cMsg == NULL || cMsg->cmsg_type != 0x02) {
        bzero(buf, len);
        errno = -EIO;
        return n;
    }
    struct ucred *cRed = (struct ucred *)CMSG_DATA(cMsg);
    if (cRed->uid != 0) {
        bzero(buf, len);
        errno = -EIO;
        return n;
    }

    if (addr.nl_groups == 0 || addr.nl_pid != 0) {
        bzero(buf, len);
        errno = -EIO;
        return n;
    }
    return n;
}
static const char *find_uevent_param(struct_uevent_t *evt ,const char *paramName) {
    size_t len = strlen(paramName);
    if(NULL==evt)
        return NULL ;
    for (int i = 0; i < NL_PARAMS_MAX && evt->uevent_params[i] != NULL; ++i) {
        const char *ptr = evt->uevent_params[i] + len;
        if (!strncmp(evt->uevent_params[i], paramName, len) && *ptr == '=')
            return ++ptr;
    }
    return NULL;
}
static unsigned char check_dev_path(const char* dp) {
    if (!strncmp(dp, DEV_SDCARD, strlen(DEV_SDCARD))) {
        gStorageStruct.is_sdcard_dev = 1 ;
        return 1;
    } else if (!strncmp(dp, DEV_USB, strlen(DEV_USB))) {
        gStorageStruct.is_sdcard_dev = 0 ;
        return 1;
    }
    return  0 ;
}

int create_device_node(const char *path, int major, int minor) {
    mode_t mode = 0660 | S_IFBLK;
    dev_t dev = (major << 8) | minor;
    if (mknod(path, mode, dev) < 0) {
        if (errno != EEXIST) {
            return -1;
        }
    }
    return 0;
}

static void set_state(int state) {
    if (GET_CUR_STATUE == state) {
        return;
    }
    if ((GET_CUR_STATUE == State_Pending) && (state != State_Idle)) {
        gStorageStruct.retry_mount_num = FALSE;
    }

    GET_CUR_STATUE = state;
    report_message(GET_CUR_STATUE,GET_CUR_MOUNT_POINT);
}
static int get_device_nodes(dev_t *devs, int max) {
    if (!(gStorageStruct.disk_num_parts)) {
        devs[0] = MKDEV(gStorageStruct.disk_major, gStorageStruct.disk_minor);
        return 1;
    }

    int i;
    for (i = 0; i < gStorageStruct.disk_num_parts; i++) {
        if (i == max)
            break;
        devs[i] = MKDEV(gStorageStruct.disk_major, gStorageStruct.part_minors[i]);
    }
    return gStorageStruct.disk_num_parts;
}
static int do_mount(const char *fsPath, const char *mountPoint) {
    int rc;
    char mountData[255];
    unsigned long flags = MS_NODEV | MS_NOSUID | MS_DIRSYNC | MS_NOEXEC;

    sprintf(mountData, "utf8,uid=%d,gid=%d,fmask=%o,dmask=%o,shortname=mixed",1023, 1023, 0, 0);
    OTA_USB_DEBUG("will do mount [fs=%s][mp=%s] vfat mountData=%s\n", fsPath, mountPoint, mountData);
    rc = mount(fsPath, mountPoint, "vfat", flags, mountData);

    if (rc && errno == EROFS) {
        OTA_USB_DEBUG("%s appears to be a read only filesystem - retrying mount RO\n", fsPath);
        flags |= MS_RDONLY;
        rc = mount(fsPath, mountPoint, "vfat", flags, mountData);
    }

    return rc;
}

static int mount_volume() {
    dev_t deviceNodes[4];
    int n, i;
    if (GET_CUR_STATUE != State_Idle) {
        errno = EBUSY;
        if (GET_CUR_STATUE == State_Pending) {
            gStorageStruct.retry_mount_num = TRUE;
        }
        OTA_USB_DEBUG("error mount_volume status=%d\n", GET_CUR_STATUE);
        return -1;
    }

    n = get_device_nodes((dev_t *) &deviceNodes, 4);
    if (!n) {
        OTA_USB_DEBUG("Failed to get device nodes (%s)\n", strerror(errno));
        return -1;
    }

    for (i = 0; i < n; i++) {
        errno = 0;
        char devicePath[255];
        sprintf(devicePath, "/dev/block/storaged/%d:%d", (int)MAJOR(deviceNodes[i]), (int)MINOR(deviceNodes[i]));
        if (do_mount(devicePath, GET_CUR_MOUNT_POINT)) {
            OTA_USB_DEBUG("%s failed to mount via VFAT (%s)\n", devicePath, strerror(errno));
            continue;
        }

        set_state(State_Mounted);
        gStorageStruct.current_mount_kdev = deviceNodes[i];
        return 0;
    }

    OTA_USB_DEBUG("BaseVolume %s found no suitable devices for mounting :(\n", GET_CUR_MOUNT_POINT);
    set_state(State_Idle);

    return -1;
}

static void handle_disk_added(const char *devpath, struct_uevent_t *evt) {
    gStorageStruct.disk_major = atoi(find_uevent_param(evt, "MAJOR"));
    gStorageStruct.disk_minor = atoi(find_uevent_param(evt, "MINOR"));
    const char *tmp = find_uevent_param(evt, "NPARTS");

    if (tmp) {
        gStorageStruct.disk_num_parts = atoi(tmp);
    } else {
        OTA_USB_DEBUG("Kernel block uevent missing 'NPARTS' %d\n", gStorageStruct.is_sdcard_dev);
        if (gStorageStruct.is_sdcard_dev) {
            gStorageStruct.disk_num_parts = 0;
        } else {
            gStorageStruct.disk_num_parts = 1;
        }
    }

    int partmask = 0;
    int i;
    for (i = 1; i <= gStorageStruct.disk_num_parts; i++) {
        partmask |= (1 << i);
    }
    gStorageStruct.pending_part_map = partmask;

    if (gStorageStruct.disk_num_parts == 0) {
        set_state(State_Idle);
    } else {
        set_state(State_Pending);
    }
}

void handle_partition_added(const char *devpath, struct_uevent_t *evt) {
    int major = atoi(find_uevent_param(evt, "MAJOR"));
    int minor = atoi(find_uevent_param(evt, "MINOR"));
    const char *tmp = find_uevent_param(evt, "PARTN");
    int part_num;
    if (tmp) {
        part_num = atoi(tmp);
    } else {
        OTA_USB_DEBUG("Kernel block uevent missing 'PARTN'\n");
        part_num = 1;
    }

    if (part_num > STORAGE_MAX_PARTITIONS || part_num < 1) {
        OTA_USB_DEBUG("Invalid 'PARTN' value\n");
        return;
    }

    if (part_num > gStorageStruct.disk_num_parts) {
        gStorageStruct.disk_num_parts = part_num;
    }

    if (major != gStorageStruct.disk_major) {
        OTA_USB_DEBUG("Partition '%s' has a different major than its disk!\n", devpath);
        return;
    }
    if (part_num >= STORAGE_MAX_PARTITIONS) {
        OTA_USB_DEBUG("Dv:partAdd: ignoring part_num = %d (max: %d)\n", part_num, STORAGE_MAX_PARTITIONS-1);
    } else {
        gStorageStruct.part_minors[part_num -1] = minor;
    }
    gStorageStruct.pending_part_map &= ~(1 << part_num);

    if (!(gStorageStruct.pending_part_map)) {
        set_state(State_Idle);
        if (gStorageStruct.retry_mount_num == TRUE) {
            gStorageStruct.retry_mount_num = FALSE;
            mount_volume();
        }
    } 
}

int do_unmount(const char *path) {
    int retries = 10;
    while (retries--) {
        if (!umount(path) || errno == EINVAL || errno == ENOENT) {
            OTA_USB_DEBUG("%s sucessfully unmounted\n", path);
            return 0;
        }
        OTA_USB_DEBUG("Failed to unmount %s (%s, retries %d)\n", path, strerror(errno), retries);
        usleep(1000*1000);
    }
    errno = EBUSY;
    OTA_USB_DEBUG("Giving up on unmount %s (%s)\n", path, strerror(errno));
    return -1;
}

int unmount_volume() {
    if (GET_CUR_STATUE != State_Mounted) {
        OTA_USB_DEBUG("BaseVolume %s unmount request when not mounted\n", GET_CUR_MOUNT_POINT);
        errno = EINVAL;
        return -2;
    }

    set_state(State_Unmounting);
    usleep(1000 * 1000);
    if (do_unmount(GET_CUR_MOUNT_POINT) != 0) {
        OTA_USB_DEBUG("Failed to unmount %s (%s)\n", GET_CUR_MOUNT_POINT, strerror(errno));
        goto fail_remount_secure;
    }

    set_state(State_Idle);
    if(gStorageCall) {
        gStorageCall(STORAGE_UNMOUNT_EVENT , GET_CUR_MOUNT_POINT);
    }
    gStorageStruct.current_mount_kdev = -1;
    return 0;

fail_remount_secure:
    set_state(State_Mounted);
    return -1;
}

static inline void handle_disk_removed(const char *devpath, struct_uevent_t *evt) {
    set_state(State_NoMedia);
}

static void handle_partition_removed(const char *devpath, struct_uevent_t *evt) {
    int major = atoi(find_uevent_param(evt, "MAJOR"));
    int minor = atoi(find_uevent_param(evt, "MINOR"));

    if (GET_CUR_STATUE != State_Mounted) {
        return;
    }

    if ((dev_t) MKDEV(major, minor) == gStorageStruct.current_mount_kdev) {
        if (unmount_volume()) {
            OTA_USB_DEBUG("Failed to unmount BaseVolume on bad removal (%s)\n", strerror(errno));
        } else {
            OTA_USB_DEBUG("unmount success\n");
        }
    }
}
int handle_block_event(struct_uevent_t *evt) {
    const char *dp = find_uevent_param(evt, "DEVPATH");
    if (NULL != dp && check_dev_path(dp)) {
        int action = evt->uevent_action;
        const char *devtype = find_uevent_param(evt, "DEVTYPE");
        if (action == STORAGE_ADD) {
            int major = atoi(find_uevent_param(evt, "MAJOR"));
            int minor = atoi(find_uevent_param(evt, "MINOR"));
            char nodepath[255];
            snprintf(nodepath, sizeof(nodepath), "/dev/block/storaged/%d:%d", major, minor);
            if (create_device_node(nodepath, major, minor)) {
                OTA_USB_DEBUG("Error making device node '%s' (%s)\n", nodepath,strerror(errno));
            }
            if (!strcmp(devtype, "disk") ) {
                handle_disk_added(dp, evt);
            } else {
                handle_partition_added(dp, evt);
            }
            if (GET_CUR_STATUE == State_Idle) {
                mount_volume();
            }
        } else if (action == STORAGE_REMOVE) {
            if (!strcmp(devtype, "disk") && !gStorageStruct.is_sdcard_dev) {
                handle_disk_removed(dp, evt);
            } else {
                handle_partition_removed(dp, evt);
            }
        } else {
                OTA_USB_DEBUG("Ignoring non add/remove/change event\n");
        }
        return 0;
    }
    errno = ENODEV;
    return -1;
}
static int parse_netlink_message(char *buffer, int size,struct_uevent_t* puevent) {
    const char *s = buffer;
    const char *end;
    int param_idx = 0;
    int first = 1;

    if (size == 0)
        return FALSE;

    buffer[size-1] = '\0';
    end = s + size;
    while (s < end) {
        if (first) {
            const char *p;
            for (p = s; *p != '@'; p++) {
                if (!*p) {
                    return FALSE;
                }
            }
            first = 0;
        } else {
            const char* a;
            if ((a = HAS_CONST_PREFIX(s, end, "ACTION=")) != NULL) {
                if (!strcmp(a, "add"))
                    puevent->uevent_action = STORAGE_ADD;
                else if (!strcmp(a, "remove"))
                    puevent->uevent_action = STORAGE_REMOVE;
            } else if ((a = HAS_CONST_PREFIX(s, end, "SUBSYSTEM=")) != NULL) {
                puevent->uevent_subsystem = strdup(a);
            } else if (param_idx < NL_PARAMS_MAX) {
                puevent->uevent_params[param_idx++] = strdup(s);
            }
        }
        s += strlen(s) + 1;
    }
    return TRUE;
}
static void destory_uevent_struct(struct_uevent_t* puevent) {
    int i;
    if(NULL == puevent)
        return ;
    if (puevent -> uevent_subsystem)
        free(puevent -> uevent_subsystem);
    for (i = 0; i < NL_PARAMS_MAX && puevent->uevent_params[i]; i++) {
        free(puevent -> uevent_params[i]);
    }
}
static void *create_netlink_socket_thread(void *obj) {
    char buf[BASE_BUFFER_SIZE+2];
    while(1) {
        int ret;
        while ((ret = read_uevent(gNetlinkSock, buf, BASE_BUFFER_SIZE)) > 0) {
            if (ret >= BASE_BUFFER_SIZE) {
                continue;
            }
            buf[ret] = '\0';
            buf[ret + 1] = '\0';
            OTA_USB_DEBUG("buffer[%d] = %s \n", ret, buf);
            struct_uevent_t uevent ={0,0,{0,}} ;
            if (parse_netlink_message(buf, ret, &uevent)) {
                if(NULL != uevent.uevent_subsystem && !strcmp(uevent.uevent_subsystem, "block") ) {
                    handle_block_event(&uevent);
                }
            } else {
                OTA_USB_DEBUG("Error decoding struct_uevent_t\n");
            }
            destory_uevent_struct(&uevent);
        }
    }

    pthread_exit(NULL);
    return NULL;
}
static int create_netlink_socket() {
    pthread_t thread_id_;
    struct sockaddr_nl nladdr;
    int sz = 64 * 1024, on = 1;

    memset(&nladdr, 0, sizeof(nladdr));
    nladdr.nl_family = AF_NETLINK;
    nladdr.nl_pid = getpid();
    nladdr.nl_groups = 0xffffffff;
    if ((gNetlinkSock = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT)) < 0) {
        OTA_USB_DEBUG("Unable to create uevent socket: %s\n", strerror(errno));
        return -1;
    }
    if (setsockopt(gNetlinkSock, SOL_SOCKET, SO_RCVBUFFORCE, &sz, sizeof(sz)) < 0) {
        OTA_USB_DEBUG("Unable to set uevent socket SO_RCVBUFFORCE option: %s\n", strerror(errno));
        goto out;
    }
    if (setsockopt(gNetlinkSock, SOL_SOCKET, SO_PASSCRED, &on, sizeof(on)) < 0) {
        OTA_USB_DEBUG("Unable to set uevent socket SO_PASSCRED option: %s\n", strerror(errno));
        goto out;
    }
    if (bind(gNetlinkSock, (struct sockaddr *) &nladdr, sizeof(nladdr)) < 0) {
        OTA_USB_DEBUG("Unable to bind uevent socket: %s\n", strerror(errno));
        goto out;
    }
    if (pthread_create(&thread_id_, NULL, create_netlink_socket_thread, NULL)) {
        OTA_USB_DEBUG("pthread_create (%s)\n", strerror(errno));
        return -1;
    }
    return 0;

out:
    close(gNetlinkSock);
    return -1;
}

static void do_trigger(DIR *dir, const char *path, int32_t pathLen)
{
    if (pathLen < 0) {
        return;
    }
    struct dirent *dirent = NULL;
    char ueventPath[512];
    snprintf(ueventPath, sizeof(ueventPath) - 1, "%s/uevent", path);
    int fd = open(ueventPath, O_WRONLY);
    if (fd >= 0) {
        write(fd, "add\n", 4);
        close(fd);
    }
    while ((dirent = readdir(dir)) != NULL) {
        if (dirent->d_name[0] == '.' || dirent->d_type != DT_DIR) {
            continue;
        }
        char tmpPath[512];
        snprintf(tmpPath, sizeof(tmpPath) - 1, "%s/%s", path, dirent->d_name);
        DIR *dir2 = opendir(tmpPath);
        if (dir2) {
            do_trigger(dir2, tmpPath, strlen(tmpPath));
            closedir(dir2);
        }
    }
}
static void retrigger(const char *sysPath)
{
    DIR *dir = opendir(sysPath);
    if (dir) {
        do_trigger(dir, sysPath, strlen(sysPath));
        closedir(dir);
    }
}
static void storage_retrigger_uevent()
{
    retrigger("/sys/class");
    retrigger("/sys/block");
    retrigger("/sys/devices");
}

static void do_storage_init() {
    mkdir("/dev", 0755);
    mkdir("/dev/block", 0755);
    mkdir("/dev/block/storaged", 0755);

    memset(&gStorageStruct, 0x00, sizeof(struct_storage_t));
    char mount[PATH_MAX];
    snprintf(mount, PATH_MAX, "%s/%s", MEDIA_DIR, "usb");
    mkdir(MEDIA_DIR, 0755);
    mkdir(mount, 0755);
    GET_CUR_MOUNT_POINT = strdup(mount);
    set_state(State_NoMedia);
}
static void register_storage_state_callback(storage_fun_ptr pfunc) {
    gStorageCall = pfunc ;
}

int mount_storage(storage_fun_ptr pfunc)
{
    do_storage_init();
    register_storage_state_callback(pfunc);
    create_netlink_socket();
    storage_retrigger_uevent();

    return 0;
}
