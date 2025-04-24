/*
 *  Copyright (C) 2013 Realtek Semiconductor Corp.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <termios.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <endian.h>
#include <byteswap.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/timerfd.h>
#include <sys/epoll.h>
#include <stdbool.h>
#include <linux/wireless.h>
#include "rtb_fwc.h"
#include "hciattach.h"
#include "hciattach_h4.h"

#define RTK_VERSION "3.1.089fdb9.20220718-164930"

/* #define SERIAL_NONBLOCK_READ */

#ifdef SERIAL_NONBLOCK_READ
#define FD_BLOCK	0
#define FD_NONBLOCK	1
#endif

#define RTW_IOCTL_MP 	SIOCDEVPRIVATE + 1
#define BUF_SIZE   64

/* #define RTL_8703A_SUPPORT */
/* #define RTL8723DSH4_UART_HWFLOWC */ /* 8723DS H4 special */

uint8_t DBG_ON = 1;

#define PATCH_DATA_FIELD_MAX_SIZE   252

struct rtb_struct rtb_cfg;
struct rtk_bt_power_info bt_power_info;

#ifdef SERIAL_NONBLOCK_READ
static int set_fd_nonblock(int fd)
{
	long arg;
	int old_fl;

	arg = fcntl(fd, F_GETFL);
	if (arg < 0) {
		return -errno;
	}

	/* Return if already nonblock */
	if (arg & O_NONBLOCK) {
		return FD_NONBLOCK;
	}
	old_fl = FD_BLOCK;

	arg |= O_NONBLOCK;
	if (fcntl(fd, F_SETFL, arg) < 0) {
		return -errno;
	}

	return old_fl;
}

static int set_fd_block(int fd)
{
	long arg;

	arg = fcntl(fd, F_GETFL);
	if (arg < 0) {
		return -errno;
	}

	/* Return if already block */
	if (!(arg & O_NONBLOCK)) {
		return 0;
	}

	arg &= ~O_NONBLOCK;
	if (fcntl(fd, F_SETFL, arg) < 0) {
		return -errno;
	}

	return 0;
}
#endif

/*
 * Download Realtek Firmware and Config
 */
static uint8_t _get_patch_project_id(uint8_t *p_buf)
{
	uint8_t opcode;
	uint8_t length;
	uint8_t data;

	opcode = *(--p_buf);

	while (opcode != 0xFF) {
		length = *(--p_buf);
		if (opcode == 0x00) {
			if (length != 1) {
				RS_ERR("Project ID length error!");
				return 0xFF;
			} else {
				data = *(--p_buf);
				return data;
			}
		} else {
			p_buf -= length;
			opcode = *(--p_buf);
		}
	}

	RS_ERR("Project ID not found!");
	return 0xFF;
}

static uint8_t hci_patch_get_patch_version(struct rtb_struct* rtb_cfg)
{
	const uint8_t patch_sig_v1[] = {0x52, 0x65, 0x61, 0x6C, 0x74, 0x65, 0x63, 0x68}; // V1 signature: Realtech
	const uint8_t patch_sig_v2[] = {0x52, 0x54, 0x42, 0x54, 0x43, 0x6F, 0x72, 0x65}; // V2 signature: RTBTCore
	const uint8_t patch_sig_v3[] = {0x42, 0x54, 0x4E, 0x49, 0x43, 0x30, 0x30, 0x33}; // V2 signature: BTNIC003
	const uint8_t ext_section_sig[] = {0x51, 0x04, 0xFD, 0x77};                      // Extension section signature
	uint8_t project_id;
	uint8_t *p_patch = NULL;
	uint32_t patch_len;
	uint8_t patch_version;

	p_patch = rtb_cfg->fw_buf;
	patch_len = rtb_cfg->fw_len;

	if ((!memcmp(p_patch, patch_sig_v1, sizeof(patch_sig_v1))) &&
		(!memcmp(p_patch + patch_len - sizeof(ext_section_sig), ext_section_sig, sizeof(ext_section_sig)))) {
		patch_version = PATCH_VERSION_V1;
	} else if ((!memcmp(p_patch, patch_sig_v2, sizeof(patch_sig_v2))) &&
				(!memcmp(p_patch + patch_len - sizeof(ext_section_sig), ext_section_sig, sizeof(ext_section_sig)))) {
		project_id = _get_patch_project_id(p_patch + patch_len - sizeof(ext_section_sig));
		if (project_id != HCI_PATCH_PROJECT_ID) {
			RS_ERR("Project ID 0x%02x check fail, No available patch!", project_id);
			return PATCH_VERSION_INVALID;
		}
		patch_version = PATCH_VERSION_V2;
	} else if ((!memcmp(p_patch, patch_sig_v3, sizeof(patch_sig_v3)))) {
		patch_version = PATCH_VERSION_V3;
	} else {
		RS_ERR("Signature check fail, No available patch!");
	}

	return patch_version;
}

extern uint8_t hci_download_patch_v2(int fd, struct rtb_struct* rtb_cfg, struct termios *ti);
extern uint8_t hci_download_patch_v3(int fd, struct rtb_struct* rtb_cfg, struct termios *ti);
static int rtb_download_fwc(int fd, struct rtb_struct* rtb_cfg, struct termios *ti)
{
	uint8_t patch_version;
	uint8_t *p_patch;
	uint32_t patch_len;
	uint8_t ret = HCI_FAIL;

	patch_version = hci_patch_get_patch_version(rtb_cfg);

	switch (patch_version) {
	case PATCH_VERSION_V1:
		RS_ERR("Signature check success: Merge patch v1 not support");
		break;

	case PATCH_VERSION_V2:
		RS_INFO("Signature check success: Merge patch v2");
		ret = hci_download_patch_v2(fd, rtb_cfg, ti);
		break;

	case PATCH_VERSION_V3:
		RS_INFO("Signature check success: Merge patch v3");
		ret = hci_download_patch_v3(fd, rtb_cfg, ti);
		break;

	default:
		RS_ERR("Signature check fail, No available patch!");
		break;
	}

	return ret;
}

#define ARRAY_SIZE(a)	(sizeof(a)/sizeof(a[0]) )
struct rtb_baud {
	uint32_t rtb_speed;
	int uart_speed;
};

#ifdef BAUDRATE_4BYTES
struct rtb_baud baudrates[] = {
	{0x0000701d, 115200},
	{0x0252C00A, 230400},
	{0x03F75004, 921600},
	{0x05F75004, 921600},
	{0x00005004, 1000000},
	{0x04928002, 1500000},
	{0x00005002, 2000000},
	{0x0000B001, 2500000},
	{0x04928001, 3000000},
	{0x052A6001, 3500000},
	{0x00005001, 4000000},
};
#else
struct rtb_baud baudrates[] = {
	{0x701d, 115200}
	{0x6004, 921600},
	{0x4003, 1500000},
	{0x5002, 2000000},
	{0x8001, 3000000},
	{0x9001, 3000000},
	{0x7001, 3500000},
	{0x5001, 4000000},
};
#endif

static void vendor_speed_to_std(uint32_t rtb_speed, uint32_t *uart_speed)
{
	*uart_speed = 115200;

	unsigned int i;
	for (i = 0; i < ARRAY_SIZE(baudrates); i++) {
		if (baudrates[i].rtb_speed == rtb_speed) {
			*uart_speed = baudrates[i].uart_speed;
			return;
		}
	}
	return;
}

static inline void std_speed_to_vendor(int uart_speed, uint32_t *rtb_speed)
{
	*rtb_speed = 0x701D;

	unsigned int i;
	for (i = 0; i < ARRAY_SIZE(baudrates); i++) {
		if (baudrates[i].uart_speed == uart_speed) {
			*rtb_speed = baudrates[i].rtb_speed;
			return;
		}
	}

	return;
}

/*
 * Config Realtek Bluetooth.
 * Config parameters are got from Realtek Config file and FW.
 *
 * speed is the init_speed in uart struct
 * Returns 0 on success
 */
uint8_t bt_ant_switch = 0;//default 0, means bt_rfafe
static int rtb_config(int fd, int proto, int speed, struct termios *ti)
{
	int final_speed = 0;
	int ret = 0;
	int max_patch_size = 0;
	uint8_t *hci_lgc_efuse = NULL;
	uint8_t *hci_phy_efuse = NULL;
	uint8_t bt_ant_val = 0;
	rtb_cfg.proto = proto;

	/* bt  efuse read */
	uint8_t lgc_efuse_data[OPT_REQ_MSG_PARAM_NUM] = {0};
	uint8_t phy_efuse_data[HCI_PHY_EFUSE_LEN] = {0};
	memset(lgc_efuse_data, 0xff, OPT_REQ_MSG_PARAM_NUM);
	int phy_otp_fd = open("/sys/bus/nvmem/devices/otp_raw0/nvmem", 2); // 2: O_RDWR   1:WR only
	int lgc_otp_fd = open("/sys/bus/nvmem/devices/otp_map0/nvmem", 2); // 2: O_RDWR   1:WR only
	if (phy_otp_fd < 0 || lgc_otp_fd < 0) {
		RS_ERR("Open otp devices error, please confirm your configurations, %d, %s", errno,
			   strerror(errno));
		memset(lgc_efuse_data, 0xff, OPT_REQ_MSG_PARAM_NUM);
	} else {
		ret = pread(lgc_otp_fd, lgc_efuse_data, OPT_REQ_MSG_PARAM_NUM, 0);
		if (ret < 0)
			RS_ERR("Read logical map data failed");
		/*printf("logic read value: \n");
		for (int i = 0; i < OPT_REQ_MSG_PARAM_NUM; i++) {
			printf("%02x ", lgc_efuse_data[i]);
		}
		printf("\nend\n");*/

		ret = pread(phy_otp_fd, phy_efuse_data, HCI_PHY_EFUSE_LEN, HCI_PHY_EFUSE_BASE);
		if (ret < 0)
			RS_ERR("Read physical map data failed");
		hci_phy_efuse = phy_efuse_data;
		/*printf("physical read value: \n");
		for (int i = 0; i < HCI_PHY_EFUSE_LEN; i++) {
			printf("%02x ", phy_efuse_data[i]);
		}
		printf("\nend\n");*/
	}
	hci_lgc_efuse = lgc_efuse_data + HCI_LGC_EFUSE_OFFSET;
	close(phy_otp_fd);
	close(lgc_otp_fd);
	/* bt  efuse read */

	/* bt  power on */
	bt_ant_val = lgc_efuse_data[HCI_LGC_ANT_OFFSET];
	//bt_ant_switch 0, means bt_rfafe        1, means wl_rfafe
	if (bluetooth_is_mp_mode()) {
		bt_ant_switch = mp_ant_switch;
	} else {
		if (bt_ant_val != 0xff) {
			if (bt_ant_val & (BIT(5))) {
				bt_ant_switch = 0;
			} else {
				bt_ant_switch = 1;
			}
		}
	}
	int bt_fd = open("/dev/bt-cdev", O_RDWR); // 2: O_RDWR   1:WR only
	if (bt_fd < 0) {
		RS_ERR("Open bt cdev device error,, please confirm your configurations, %d, %s", errno,
			   strerror(errno));
		return -1;
	}
	bt_power_info.power_on = 1;
	bt_power_info.bt_ant_switch = bt_ant_switch;
	ret = ioctl(bt_fd, RTK_BT_IOC_SET_BT_POWER, &bt_power_info);
	if(ret < 0) {
		RS_ERR("Fail to get RTK_BT_IOC_SET_BT_POWER, %d, %s", errno,
			   strerror(errno));
		return -1;
	}
	close(bt_fd);
	/* bt  power on */

	/* Read Local Version Information and RTK ROM version */
	RS_INFO("Realtek H4 IC");

	h4_read_local_ver(fd);
	h4_vendor_read_rom_ver(fd);
	if (rtb_cfg.lmp_subver == ROM_LMP_8730) {
		rtb_cfg.chip_type = CHIP_8730;
	} else {
		RS_ERR("H4: unknown chip");
		return -1;
	}

	RS_INFO("LMP Subversion 0x%04x", rtb_cfg.lmp_subver);
	RS_INFO("EVersion %u", rtb_cfg.eversion);

	rtb_cfg.patch_ent = get_patch_entry(&rtb_cfg);
	if (bluetooth_is_mp_mode()) {
		rtb_cfg.patch_ent->patch_file = "rtl8730_mp_fw";
	}
	if (rtb_cfg.patch_ent) {
		if(bt_ant_switch == 1)
			rtb_cfg.patch_ent->config_file = "rtl8730_config_s1";
		else if(bt_ant_switch == 0)
			rtb_cfg.patch_ent->config_file = "rtl8730_config_s0";
		RS_INFO("IC: %s", rtb_cfg.patch_ent->ic_name);
		RS_INFO("Firmware/config: %s, %s",
				rtb_cfg.patch_ent->patch_file,
				rtb_cfg.patch_ent->config_file);
	} else {
		RS_ERR("Can not find firmware/config entry");
		return -1;
	}

	rtb_cfg.config_buf = rtb_read_config(rtb_cfg.patch_ent->config_file,
										 &rtb_cfg.config_len,
										 rtb_cfg.patch_ent->chip_type, hci_lgc_efuse);
	if (!rtb_cfg.config_buf) {
		RS_ERR("Read Config file error, use eFuse settings");
		rtb_cfg.config_len = 0;
	}

	rtb_cfg.fw_buf = rtb_read_firmware(&rtb_cfg, &rtb_cfg.fw_len);
	if (!rtb_cfg.fw_buf) {
		RS_ERR("Read Bluetooth firmware error");
		rtb_cfg.fw_len = 0;
		/* Free config buf */
		if (rtb_cfg.config_buf) {
			free(rtb_cfg.config_buf);
			rtb_cfg.config_buf = NULL;
			rtb_cfg.config_len = 0;
		}
		return -1;
	}
change_baud:
	/* change baudrate if needed
	 * rtb_cfg.vendor_baud is a __u32/__u16 vendor-specific variable
	 * parsed from config file
	 * */
	if (rtb_cfg.vendor_baud == 0) {
		/* No baud setting in Config file */
		std_speed_to_vendor(speed, &rtb_cfg.vendor_baud);
		RS_INFO("No baud from Config file, set baudrate: %d, 0x%08x",
				speed, rtb_cfg.vendor_baud);
		goto start_download;
	} else
		vendor_speed_to_std(rtb_cfg.vendor_baud,
							(uint32_t *) & (rtb_cfg.final_speed));

	final_speed = rtb_cfg.final_speed ? rtb_cfg.final_speed : speed;
	if (final_speed != 115200) {
		h4_vendor_change_speed(fd, rtb_cfg.vendor_baud);

		/* Make sure the ack for cmd complete event is transmitted */
		tcdrain(fd);
		usleep(50000); /* The same value as before */
		RS_INFO("Fw download speed %d", final_speed);
		if (set_speed(fd, ti, final_speed) < 0) {
			RS_ERR("fw download can't set baud rate: %d", final_speed);
			goto buf_free;
		}
	} else {
		RS_INFO("Fw download speed is %d, no baud change needs",
				rtb_cfg.final_speed);
		//goto start_download;
	}	

start_download:
		ret = rtb_download_fwc(fd, &rtb_cfg, ti);

		if (hci_phy_efuse != NULL) {
			h4_write_iqk(fd, hci_phy_efuse);
		}

		/* Make hci reset after Controller applies the Firmware and Config */
		if (ret == 0) {
			ret = h4_hci_reset(fd);
		}

		if (ret < 0) {
			return ret;
		} else {
			if (bluetooth_is_mp_mode() && (final_speed != 115200)) {
				final_speed = 115200;
				rtb_cfg.final_speed = 115200;
				rtb_cfg.vendor_baud = 0x0000701d;

				h4_vendor_change_speed(fd, rtb_cfg.vendor_baud);

				/* Make sure the ack for cmd complete event is transmitted */
				tcdrain(fd);
				usleep(50000); /* The same value as before */
				if (set_speed(fd, ti, final_speed) < 0) {
					RS_ERR("Can't set baud rate: %d, %d, %d", final_speed,
						rtb_cfg.final_speed, speed);
				}
			}
			RS_INFO("Final speed %d", final_speed);
		}

	/* Notify WIFI BT antenna when in mp mode*/
	if(bluetooth_is_mp_mode()) {
		struct ifreq ifr;
		union iwreq_data u;

		char ibuf[BUF_SIZE] = "mp_bt ant,s";
		ibuf[strlen(ibuf)] = (char) (bt_ant_switch + '0');
		ibuf[strlen(ibuf)] = '\0';
		char* ifname = "wlan0";

		int sock = socket(AF_INET, SOCK_DGRAM, 0);
		if (sock < 0) {
			RS_ERR("create socket error\n");
			return -1;
		}

		memset(&ifr, 0, sizeof(struct ifreq));
		strcpy(ifr.ifr_name, ifname);

		if (ioctl(sock, SIOCGIFFLAGS, &ifr) != 0) {
			RS_ERR("could not read interface %s flags: %s", ifname, strerror(errno));
			close(sock);
			return -1;
		}

		if (!(ifr.ifr_flags & IFF_UP)) {
			RS_ERR("%s is not up!\n", ifname);
			close(sock);
			return -1;
		}

		memset(&u, 0, sizeof(union iwreq_data));
		u.data.pointer = ibuf;
		u.data.length = strlen(ibuf) + 1;

		ifr.ifr_data = (void*)&u;
		
		if (ioctl(sock, RTW_IOCTL_MP, &ifr) < 0) {
			RS_ERR("ioctl error %s\n", strerror(errno));
			close(sock);
			return -1;
		}
		RS_INFO("Private Message: %s\n", ibuf);
		close(sock);
	}

done:

	RS_DBG("Init Process finished");
	return 0;

buf_free:
	free(rtb_cfg.total_buf);
	return -1;
}

int rtb_init(int fd, int proto, int speed, struct termios *ti)
{
	struct epoll_event ev;
	int result;

	RS_INFO("Realtek hciattach version %s \n", RTK_VERSION);

	memset(&rtb_cfg, 0, sizeof(rtb_cfg));
	rtb_cfg.serial_fd = fd;
	rtb_cfg.dl_fw_flag = 1;

	rtb_cfg.epollfd = epoll_create(64);
	if (rtb_cfg.epollfd == -1) {
		RS_ERR("epoll_create1, %s (%d)", strerror(errno), errno);
		exit(EXIT_FAILURE);
	}

	ev.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP;
	ev.data.fd = fd;
	if (epoll_ctl(rtb_cfg.epollfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
		RS_ERR("epoll_ctl: epoll ctl add, %s (%d)", strerror(errno),
			   errno);
		exit(EXIT_FAILURE);
	}

	rtb_cfg.timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
	if (rtb_cfg.timerfd == -1) {
		RS_ERR("timerfd_create error, %s (%d)", strerror(errno), errno);
		return -1;
	}

	if (rtb_cfg.timerfd > 0) {
		ev.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP;
		ev.data.fd = rtb_cfg.timerfd;
		if (epoll_ctl(rtb_cfg.epollfd, EPOLL_CTL_ADD,
					  rtb_cfg.timerfd, &ev) == -1) {
			RS_ERR("epoll_ctl: epoll ctl add, %s (%d)",
				   strerror(errno), errno);
			exit(EXIT_FAILURE);
		}
	}

	RS_INFO("Use epoll");

	result = rtb_config(fd, proto, speed, ti);

	epoll_ctl(rtb_cfg.epollfd, EPOLL_CTL_DEL, fd, NULL);
	epoll_ctl(rtb_cfg.epollfd, EPOLL_CTL_DEL, rtb_cfg.timerfd, NULL);
	close(rtb_cfg.timerfd);
	rtb_cfg.timerfd = -1;

	return result;
}

int rtb_deinit(int fd, int proto, int speed, struct termios *ti)
{
	/* bt  power off */
	int bt_fd = open("/dev/bt-cdev", O_RDWR); // 2: O_RDWR   1:WR only
	if (bt_fd < 0) {
		RS_ERR("Open bt-cdev error, please confirm your configurations, %d, %s", errno,
			   strerror(errno));
		return -1;
	}
	bt_power_info.power_on = 0;
	bt_power_info.bt_ant_switch = bt_ant_switch;
	int ret = ioctl(bt_fd, RTK_BT_IOC_SET_BT_POWER, &bt_power_info);
	if(ret < 0) {
		RS_ERR("Fail to get RTK_BT_IOC_SET_BT_POWER, %d, %s", errno,
			   strerror(errno));
		return -1;
	}
	close(bt_fd);
	/* bt  power off */

	return 0;
}

int rtb_post(int fd, int proto, struct termios *ti)
{
	/* No need to change baudrate */
	/* if (rtb_cfg.final_speed)
	 * 	return set_speed(fd, ti, rtb_cfg.final_speed);
	 */

	return 0;
}
