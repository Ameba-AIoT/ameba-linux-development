// SPDX-License-Identifier: GPL-2.0
#include <stdint.h>
int h4_download_patch(int fd, int index, uint8_t *data, int len, struct termios *ti);
int h4_vendor_change_speed(int fd, uint32_t baudrate);
int h4_hci_reset(int fd);
int h4_read_local_ver(int fd);
int h4_vendor_read_rom_ver(int fd);
int h4_write_iqk(int fd, uint8_t *phy_efuse);
int start_xfer_wait(int fd, uint8_t *cmd, uint16_t len, uint32_t msec, int retry, uint8_t *resp, uint16_t *resp_len);