#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>

#include "hciattach.h"
#include "hciattach_h4.h"
#include "rtb_fwc.h"


#define HCI_PATCH_FRAG_SIZE 252
#define HCI_PATCH_IMAGE_INFO_LEN 1024

#define HCI_BT_REG_KEY_ID 0xb000ada4

#define REG_ADDR 0
#define REG_VALUE 1

#define TRIM_INDEX(num, index)    {    \
        num = (index >= 0x80) ? ((index - 0x80) % 0x7f + 1) : (index % 0x80); \
    }

typedef enum {
	INFO_REG_PATCH_START_ADDRESS = 0x00,
	INFO_REG_SECURITY_HEADER_BUF_REMAIN_SIZE = 0x01,
	INFO_REG_NUM,
} PATCH_IMAGE_INFO_REG_NAME;

typedef enum {
	OPCODE_PATCH_IMAGE = 0x01,
} SECTION_OPCODE;

typedef struct {
	struct list_head list;
	bool new_section_check;
	uint8_t priority;
	uint16_t cur_index;
	uint16_t end_index;
	uint8_t last_pkt;
	uint32_t reg_addr[INFO_REG_NUM];
    uint32_t reg_value[INFO_REG_NUM];
	uint32_t image_len;
	uint8_t *image_payload;
	uint32_t sent_image_len;
} SECTION_NODE;

typedef struct {
	uint32_t fw_len;
	uint32_t sent_fw_len;
	uint8_t section_cnt;
	uint8_t *config_buf;
	uint16_t config_len;
	uint16_t sent_config_len;
	uint8_t *patch_buf;
	uint32_t patch_len;
	SECTION_NODE head_node;
	SECTION_NODE *security_node;
} PATCH_INFO;

static PATCH_INFO *patch_info = NULL;
static uint8_t patch_key_id = 0;

static uint8_t chipid;

static uint8_t hci_vendor_read(int fd, uint32_t addr, uint32_t *value)
{
    uint8_t buf[9];
    uint8_t resp[12];
	uint16_t rlen = sizeof(resp);
    int result;

    buf[0] = 0x01;
    buf[1] = 0x61;
    buf[2] = 0xfc;
    buf[3] = (uint8_t)(5);
    buf[4] = (uint8_t)(0x21);
    for (uint8_t i = 0; i < 4; i++) {
		buf[5 + i] = (uint8_t)(addr >> (8 * i));
	}

    result = start_xfer_wait(fd, buf, sizeof(buf), 1000, 0, resp, &rlen);
	if (result < 0) {
		RS_ERR("HCI vendor read error!");
		return HCI_FAIL;
	}

    /* Check Resp: OpCode and Status */
	if (resp[4] != 0x61 || resp[5] != 0xfc || resp[6] != 0x00) {
		return HCI_FAIL;
	}

    LE_TO_UINT32(*value, &resp[7]);

	return HCI_SUCCESS;
}

static uint8_t hci_vendor_write(int fd, uint32_t addr, uint32_t value)
{
    uint8_t buf[13];
    uint8_t resp[8];
	uint16_t rlen = sizeof(resp);
    int result;

    buf[0] = 0x01;
    buf[1] = 0x62;
    buf[2] = 0xfc;
    buf[3] = (uint8_t)(9);
    buf[4] = (uint8_t)(0x21);
    for (uint8_t i = 0; i < 4; i++) {
		buf[5 + i] = (uint8_t)(addr >> (8 * i));
		buf[9 + i] = (uint8_t)(value >> (8 * i));
	}

    result = start_xfer_wait(fd, buf, sizeof(buf), 1000, 0,
							 resp, &rlen);
	if (result < 0) {
		RS_ERR("HCI Read local version info error");
		return HCI_FAIL;
	}

     /* Check Resp: OpCode and Status */
	if (resp[4] != 0x62 || resp[5] != 0xfc || resp[6] != 0x00) {
		return HCI_FAIL;
	}

    return HCI_SUCCESS;
}

static void _insert_patch_queue(struct list_head *head, SECTION_NODE *p_node)
{
	struct list_head *pos, *next;
	SECTION_NODE *node;

	if (!head || !p_node) {
		return;
	}

	list_for_each_safe(pos, next, head) {
		node = list_entry(pos, SECTION_NODE, list);
		if (node->priority >= p_node->priority) {
			break;
		}
	}

	__list_add(&p_node->list, pos->prev, pos);
}

static void _select_security_section_node(SECTION_NODE **dest_node, SECTION_NODE *p_node)
{
	if (!p_node) {
		return;
	}

	if ((p_node->reg_value[INFO_REG_SECURITY_HEADER_BUF_REMAIN_SIZE] != 0) &&
		(p_node->reg_addr[INFO_REG_SECURITY_HEADER_BUF_REMAIN_SIZE] != 0) &&
		(p_node->reg_addr[INFO_REG_SECURITY_HEADER_BUF_REMAIN_SIZE] != 0xffffffff) &&
		((*dest_node == NULL) || (p_node->priority > (*dest_node)->priority))) {
		*dest_node = p_node;
	}
}

static uint32_t _parse_patch_image(uint8_t *p_payload)
{
	PATCH_INFO *info = patch_info;
	SECTION_NODE *node;
	uint8_t *position;
	uint16_t chip_id;
	uint8_t ic_cut, key_id, ota_en;
	uint64_t temp_reg_addr, temp_reg_value, temp_image_len;
	uint32_t total_len;

	position = p_payload;
	LE_TO_UINT16(chip_id, position);
	ic_cut = *(position + sizeof(chip_id));
	key_id = *(position + sizeof(chip_id) + sizeof(ic_cut));
	ota_en = *(position + sizeof(chip_id) + sizeof(ic_cut) + sizeof(key_id));

	if (chip_id != HCI_PATCH_PROJECT_ID || ic_cut != chipid || key_id != patch_key_id || ota_en != 0) {
		RS_ERR("Missmatch chip id / ic cut / key id / ota en!");
		return 0;
	}

	if (info->section_cnt >= 64) {
		RS_ERR("Section number exceed 64!");
		return 0;
	}

	position += HCI_PATCH_IMAGE_INFO_LEN;
	LE_TO_UINT64(temp_image_len, position);
	if (temp_image_len == 0) {
		/* Drop section whose image_len is 0 */
		RS_ERR("Section data length is 0.");
		return 0;
	}

	node = (SECTION_NODE *)malloc(sizeof(SECTION_NODE));
	if (node == NULL) {
		RS_ERR("Section node allocate fail!");
		return 0;
	}
	memset(node, 0, sizeof(SECTION_NODE));
	info->section_cnt++;

	node->image_len = (uint32_t)(temp_image_len);
	node->image_payload = position + sizeof(temp_image_len);

	position -= sizeof(node->priority);
	node->priority = *(position);
	for (int i = (INFO_REG_NUM - 1); i >= 0; i--) {
		position -= sizeof(temp_reg_value);
		LE_TO_UINT64(temp_reg_value, position);
		node->reg_value[i] = (uint32_t)(temp_reg_value);

		position -= sizeof(temp_reg_addr);
		LE_TO_UINT64(temp_reg_addr, position);
		node->reg_addr[i] = (uint32_t)(temp_reg_addr);
	}

	total_len = (info->section_cnt == 1) ? node->image_len + info->config_len : node->image_len;
	node->cur_index = 1;
	node->end_index = (total_len - 1) / HCI_PATCH_FRAG_SIZE + 1;
	node->last_pkt = total_len % HCI_PATCH_FRAG_SIZE;
	if (node->last_pkt == 0) {
		node->last_pkt = HCI_PATCH_FRAG_SIZE;
	}

	_insert_patch_queue(&info->head_node.list, node);

	_select_security_section_node(&info->security_node, node);

	return node->image_len;
}

static uint32_t _parse_sections(uint8_t *p_buf)
{
	uint8_t *p_section;
	uint32_t section_num, opcode, i;
	uint32_t fw_len = 0;
	uint64_t payload_len;

	LE_TO_UINT32(section_num, p_buf);

	if (section_num == 0) {
		RS_ERR("Section num error!");
		return 0;
	}

	p_section = p_buf + sizeof(section_num);
	for (i = 0; i < section_num; i++) {
		LE_TO_UINT32(opcode, p_section);
		LE_TO_UINT64(payload_len, p_section + sizeof(opcode));

		switch (opcode) {
		case OPCODE_PATCH_IMAGE:
			fw_len += _parse_patch_image(p_section + sizeof(opcode) + sizeof(payload_len));
			break;
		default:
			RS_ERR("Unknown opcode 0x%x", opcode);
			break;
		}
		p_section += sizeof(opcode) + sizeof(payload_len) + payload_len;
	}

	return fw_len;
}

static uint8_t _get_patch_info(struct rtb_struct* rtb_cfg)
{
	PATCH_INFO *info = patch_info;
	uint32_t version_date, version_time, reserved;
	uint32_t fw_len;
	uint8_t sig_len = 8;

	info->patch_buf = rtb_cfg->fw_buf;
	info->patch_len = rtb_cfg->fw_len;

	LE_TO_UINT32(version_date, info->patch_buf + sig_len);
	LE_TO_UINT32(version_time, info->patch_buf + sig_len + sizeof(version_date));
	RS_INFO("FW Version: %d%d", version_date, version_time);

	info->config_buf = rtb_cfg->config_buf;
    info->config_len = rtb_cfg->config_len;
	RS_INFO("info->config_len = %d", info->config_len);

	fw_len = _parse_sections(info->patch_buf + sig_len + sizeof(version_date) + sizeof(version_time) + sizeof(reserved));
	if (fw_len == 0) {
		RS_ERR("Available patch not found!");
		return HCI_IGNORE;
	}
	RS_INFO("FW Length: %d", fw_len);

	info->fw_len = fw_len;

	return HCI_SUCCESS;
}

static uint8_t hci_patch_download_init_parse(struct rtb_struct* rtb_cfg)
{
	uint8_t ret;

	patch_info = malloc(sizeof(PATCH_INFO));
	if (!patch_info) {
		return HCI_FAIL;
	}

	memset(patch_info, 0, sizeof(PATCH_INFO));
	INIT_LIST_HEAD(&patch_info->head_node.list);

	/* Parse patch and get info */
	ret = _get_patch_info(rtb_cfg);
	if (HCI_SUCCESS != ret) {
		return ret;
	}

	return HCI_SUCCESS;
}

static void hci_patch_downlod_done(void)
{
	struct list_head *pos, *next;
	SECTION_NODE *node;

	list_for_each_safe(pos, next, &patch_info->head_node.list) {
		node = list_entry(pos, SECTION_NODE, list);
		list_del_init(pos);
		free(node);
	}

	if (patch_info) {
		free(patch_info);
	}
	patch_info = NULL;
}

static uint8_t hci_patch_get_cmd_buf(uint32_t (*reg_arr)[2], bool *new_section, uint8_t *cmd_buf)
{
	PATCH_INFO *info = patch_info;
	SECTION_NODE *node;
	struct list_head *pos, *next;
	uint8_t *data_buf = &cmd_buf[2]; /* cmd_buf[0]: len, cmd_buf[1]: index */
	uint8_t remain_len = 0, sending_len;

	/* Send each section image frag */
	list_for_each_safe(pos, next, &info->head_node.list) {
		node = list_entry(pos, SECTION_NODE, list);
		if (node->new_section_check == false) {
			for (uint8_t i = 0; i < INFO_REG_NUM; i++) {
				reg_arr[i][REG_ADDR] = node->reg_addr[i];
				reg_arr[i][REG_VALUE] = node->reg_value[i];
			}
			node->new_section_check = true;
			*new_section = true;
		}

		if (node->cur_index > node->end_index) {
			/* To check sending completion when cur_index greater than end_index */
			continue;
		} else if (node->cur_index == node->end_index) {
			cmd_buf[0] = node->last_pkt + 1;
		} else {
			cmd_buf[0] = HCI_PATCH_FRAG_SIZE + 1;
		}
		TRIM_INDEX(cmd_buf[1], node->cur_index);

		remain_len = cmd_buf[0] - 1;
		if (node->sent_image_len + remain_len <= node->image_len) {
			sending_len = remain_len;
			memcpy(data_buf, node->image_payload + node->sent_image_len, sending_len);
			info->sent_fw_len += sending_len;
			node->sent_image_len += sending_len;
			remain_len -= sending_len;
		} else {
			/* For first section last pkt, need add config data */
			sending_len = node->image_len - node->sent_image_len;
			memcpy(data_buf, node->image_payload + node->sent_image_len, sending_len);
			info->sent_fw_len += sending_len;
			node->sent_image_len += sending_len;
			remain_len -= sending_len;

			memcpy(data_buf + sending_len, info->config_buf + info->sent_config_len, remain_len);
			info->sent_config_len += remain_len;
			
			if (node->image_len != node->sent_image_len) {
				RS_ERR("Sextion node has not been sent completely! image_len = %d, sent_image_len = %d", node->image_len, node->sent_image_len);
				return HCI_FAIL;
			}
		}
		break;
	}

	if (node->cur_index > node->end_index) {
		/* Each section sent completed when exit the loop, last pkt need be sent */
		cmd_buf[0] = 1;
		cmd_buf[1] = 0x80;

		if (info->sent_fw_len != info->fw_len) {
			RS_ERR("Firmware has not been sent completely! fw_len = %d, sent_fw_len = %d", info->fw_len, info->sent_fw_len);
			return HCI_FAIL;
		}

		if (info->sent_config_len != info->config_len) {
			RS_ERR("Config data has not been sent completely! config_len = %d, sent_config_len = %d", info->config_len, info->sent_config_len);
			return HCI_FAIL;
		}
	} else {
		node->cur_index++;
	}

	return HCI_SUCCESS;
}

static bool hci_patch_found_security_section(uint32_t *reg_addr, uint32_t *reg_value)
{
	SECTION_NODE *node = patch_info->security_node;

	if (node == NULL) {
		return false;
	}

	*reg_addr = (uint32_t)(node->reg_addr[INFO_REG_PATCH_START_ADDRESS]);
	*reg_value = (uint32_t)(node->reg_value[INFO_REG_PATCH_START_ADDRESS] + node->image_len - node->reg_value[INFO_REG_SECURITY_HEADER_BUF_REMAIN_SIZE]);

	return true;
}

uint8_t hci_download_patch_v3(int fd, struct rtb_struct* rtb_cfg, struct termios *ti)
{
	uint8_t ret = HCI_SUCCESS;
	uint8_t buf[RESERVE_LEN + 256];
    uint8_t resp[8];
	uint16_t rlen = sizeof(resp);
    chipid = rtb_cfg->eversion + 1;

    buf[0] = 0x01;
    buf[1] = 0x20;
    buf[2] = 0xfc;

    bool new_section;
	uint32_t reg_arr[INFO_REG_NUM][2];
	uint32_t sec_reg_addr, sec_reg_value;
	uint32_t key_id_addr = HCI_BT_REG_KEY_ID, key_id_value;

	ret = hci_vendor_read(fd, key_id_addr, &key_id_value);
	if ((HCI_SUCCESS != ret) || (key_id_value != 0)) {
		return HCI_FAIL;
	}
	patch_key_id = key_id_value;

	ret = hci_patch_download_init_parse(rtb_cfg);
	if (HCI_SUCCESS != ret) {
		goto dl_patch_done;
	}

	while (1) {
		new_section = false;
		memset(reg_arr, 0, sizeof(reg_arr));
        memset(buf + 3, 0, 254);
		memset(resp, 0, 8);

		ret = hci_patch_get_cmd_buf(reg_arr, &new_section, &buf[3]);
		if (HCI_SUCCESS != ret) {
			goto dl_patch_done;
		}

		if (new_section) {
			for (uint8_t i = 0; i < INFO_REG_NUM; i++) {
				if (reg_arr[i][REG_ADDR] != 0 && reg_arr[i][REG_ADDR] != 0xffffffff) {
					ret = hci_vendor_write(fd, reg_arr[i][REG_ADDR], reg_arr[i][REG_VALUE]);
					if (HCI_SUCCESS != ret) {
						goto dl_patch_done;
					}
				}
			}
		}

		if (buf[3] == 1 && buf[4] & 0x80) {
			if (hci_patch_found_security_section(&sec_reg_addr, &sec_reg_value)) {
				ret = hci_vendor_write(fd, sec_reg_addr, sec_reg_value);
				if (HCI_SUCCESS != ret) {
					goto dl_patch_done;
				}
			}
		}

        RS_DBG("fd: %d, index: %d, len: %d", fd, buf[4], buf[3] - 1);
		if(buf[4] == 0x80) {
			tcdrain(fd);

			if (rtb_cfg->uart_flow_ctrl) {
				RS_INFO("Enable host hw flow control");
				ti->c_cflag |= CRTSCTS;
			} else {
				RS_INFO("Disable host hw flow control");
				ti->c_cflag &= ~CRTSCTS;
			}

			if (tcsetattr(fd, TCSANOW, ti) < 0) {
				RS_ERR("Can't set port settings");
				return -1;
			}
		}

        ret = start_xfer_wait(fd, buf, buf[3] + 4, 1000, 0, resp, &rlen);
        if (ret < 0) {
            RS_ERR("Transfer patch failed, index %d", index);
            goto dl_patch_done;
        }

		/* Check Resp: OpCode and Status */
		if (resp[4] != 0x20 || resp[5] != 0xfc || resp[6] != 0x00) {
			goto dl_patch_done;
		}

		/* Check the last download finished info */
		if (resp[7] == 0x80) {
			break;
		}
	}

dl_patch_done:
	hci_patch_downlod_done();

	return ret;
}
