/*
 *  Copyright (C) 2018 Realtek Semiconductor Corporation.
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

struct rtb_struct;

#define USE_FW_FILE_INSTEAD_OF_ARRAY 1
#define BAUDRATE_4BYTES

#define ROM_LMP_NONE            0x0000
#define ROM_LMP_8730           0x8730

/* Chip type */
/* software id */
#define CHIP_UNKNOWN	0x00
#define CHIP_8730  0x1

#define RTL_FW_MATCH_CHIP_TYPE  (1 << 0)
#define RTL_FW_MATCH_HCI_VER    (1 << 1)
#define RTL_FW_MATCH_HCI_REV    (1 << 2)
struct patch_info {
	uint32_t    match_flags;
	uint8_t     chip_type;
	uint16_t    lmp_subver;
	uint16_t    proj_id;
	uint8_t     hci_ver;
	uint16_t    hci_rev;
	char        *patch_file;
	char        *config_file;
	char        *ic_name;
};

#define OPT_REQ_MSG_PARAM_NUM			0x400
#define HCI_LGC_EFUSE_LEN          0x50
#define HCI_LGC_EFUSE_OFFSET       0x1B0
#define HCI_LGC_ANT_OFFSET       0x133
#define HCI_PHY_EFUSE_LEN          0x70
#define HCI_WRITE_IQK_DATA_LEN 	 0x6D
#define HCI_PHY_EFUSE_BASE         0x740
#define BIT(__n)       (1<<(__n))
#define HCI_CONFIG_SIGNATURE       0x8723ab55
#define HCI_MAC_ADDR_LEN           6
#define LEFUSE(x)                  ((x)-HCI_LGC_EFUSE_OFFSET)

struct rtb_otp_ipc_tx_msg {
	int otp_id;
	int offset;
	int len;
	uint8_t tx_value[OPT_REQ_MSG_PARAM_NUM];
	int tx_lock;
};

struct rtb_otp_request {
	uint8_t	*result;
	struct rtb_otp_ipc_tx_msg otp_order;
};

bool bluetooth_is_mp_mode(void);
void hci_platform_get_iqk_data(uint8_t *phy_efuse, uint8_t *data, uint8_t len);
struct patch_info *get_patch_entry(struct rtb_struct *btrtl);
uint8_t *rtb_read_config(const char *file, int *cfg_len, uint8_t chip_type, uint8_t *efuse_config);
uint8_t *rtb_read_firmware(struct rtb_struct *btrtl, int *fw_len);
uint8_t *rtb_get_final_patch(int fd, int proto, int *rlen);
#if (USE_FW_FILE_INSTEAD_OF_ARRAY == 0)
extern unsigned char rtl8730_hci_init_config[];
extern uint32_t rtl8730_hci_init_config_len;
extern const unsigned char rtl8730_rtlbt_fw[];
extern uint32_t rtl8730_rtlbt_fw_len;
#endif