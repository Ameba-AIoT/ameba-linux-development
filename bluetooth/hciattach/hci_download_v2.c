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

static uint8_t chipid;

typedef enum {
	OPCODE_PATCH_SNIPPETS = 0x01,
	OPCODE_DUMMY_HEADER = 0x02,
	OPCODE_SECURITY_HEADER = 0x03,
	OPCODE_OTA_FLAG = 0x04,
	OPCODE_CONTROLLER_RSVD = 0x08
} SECTION_OPCODE;

typedef struct {
	struct list_head list;
	uint8_t eco;
	uint8_t priority;
	uint8_t key_id;
	uint8_t reserve;
	uint32_t payload_len;
	uint8_t *payload;
	uint32_t sent_payload_len;
} PATCH_NODE;

typedef struct {
	uint32_t fw_len;
	uint32_t sent_fw_len;
	uint8_t *config_buf;
	uint16_t config_len;
	uint16_t sent_config_len;
	uint16_t cur_index;
	uint16_t end_index;
	uint8_t last_pkt;
	uint8_t *patch_buf;
	uint32_t patch_len;
	PATCH_NODE head_node;
} PATCH_INFO;

static PATCH_INFO *patch_info = NULL;
static uint8_t patch_key_id = 0;

static uint8_t hci_patch_downlod_init(void)
{
	patch_info = (PATCH_INFO*) malloc(sizeof(PATCH_INFO));
	if (!patch_info) {
		return HCI_FAIL;
	}

	memset(patch_info, 0, sizeof(PATCH_INFO));
	INIT_LIST_HEAD(&patch_info->head_node.list);

	return HCI_SUCCESS;
}

static void hci_patch_download_done(void)
{
	struct list_head *pos, *next;
	PATCH_NODE *node;

	list_for_each_safe(pos, next, &patch_info->head_node.list) {
		node = list_entry(pos, PATCH_NODE, list);
		list_del_init(pos);
		free(node);
	}

	if (patch_info) {
		free(patch_info);
	}
	patch_info = NULL;
}

static void _insert_patch_queue(struct list_head *head, PATCH_NODE *p_patch_node)
{
	struct list_head *pos, *next;
	PATCH_NODE *node;

	if (!head || !p_patch_node) {
		return;
	}

	list_for_each_safe(pos, next, head) {
		node = list_entry(pos, PATCH_NODE, list);
		if (node->priority >= p_patch_node->priority) {
			break;
		}
	}

	__list_add(&p_patch_node->list, pos->prev, pos);
}

static void _parse_patch_section(uint8_t *p_buf, uint32_t *p_fw_len, SECTION_OPCODE opcode, bool *p_found_security_header,
								 PATCH_NODE *p_patch_node_head)
{
	PATCH_NODE *patch_node;
	uint16_t number, reserve;
	uint8_t *position;
	uint8_t eco;
	uint32_t payload_len;

	LE_TO_UINT16(number, p_buf);

	position = p_buf + sizeof(number) + sizeof(reserve);
	for (uint16_t i = 0; i < number; i++) {
		eco = *(position);
		LE_TO_UINT32(payload_len, position + sizeof(patch_node->eco) + sizeof(patch_node->priority) +
					 sizeof(patch_node->key_id) + sizeof(patch_node->reserve));

		if (eco == chipid) {
			patch_node = (PATCH_NODE *) malloc(sizeof(PATCH_NODE));
			if (patch_node == NULL) {
				RS_ERR("patch_node allocate fail!");
				return;
			}
			memset(patch_node, 0, sizeof(PATCH_NODE));

			patch_node->eco = eco;
			patch_node->priority = *(position + sizeof(patch_node->eco));
			if (opcode == OPCODE_SECURITY_HEADER) {
				patch_node->key_id = *(position + sizeof(patch_node->eco) + sizeof(patch_node->priority));
			}
			patch_node->payload_len = payload_len;
			patch_node->payload = position + sizeof(patch_node->eco) + sizeof(patch_node->priority) +
								  sizeof(patch_node->key_id) + sizeof(patch_node->reserve) + sizeof(patch_node->payload_len);

			if (opcode == OPCODE_PATCH_SNIPPETS || opcode == OPCODE_DUMMY_HEADER) {
				_insert_patch_queue(&p_patch_node_head->list, patch_node);
				*p_fw_len += payload_len;
			} else if (opcode == OPCODE_SECURITY_HEADER) {
				if (patch_node->key_id == patch_key_id) {
					_insert_patch_queue(&p_patch_node_head->list, patch_node);
					*p_fw_len += payload_len;
					*p_found_security_header = true;
				} else {
					RS_ERR("patch_node->key_id = 0x%x mismatch patch_key_id = 0x%x", patch_node->key_id, patch_key_id);
					free(patch_node);
				}
			}
		}

		position += sizeof(patch_node->eco) + sizeof(patch_node->priority) +
					sizeof(patch_node->key_id) + sizeof(patch_node->reserve) + sizeof(patch_node->payload_len) + payload_len;
	}
}

static uint32_t _parse_patch(uint8_t *p_buf, PATCH_NODE *p_patch_node_head)
{
	uint32_t i;
	uint32_t section_num;
	uint8_t *p_section;
	uint32_t opcode, length;
	uint32_t fw_len = 0;
	bool found_security_header = false;

	LE_TO_UINT32(section_num, p_buf);

	if (section_num == 0) {
		RS_ERR("Section num error!");
		return 0;
	} else {
		p_section = p_buf + sizeof(section_num);
		for (i = 0; i < section_num; i++) {
			LE_TO_UINT32(opcode, p_section);
			LE_TO_UINT32(length, p_section + sizeof(opcode));

			switch (opcode) {
			case OPCODE_PATCH_SNIPPETS:
				_parse_patch_section(p_section + sizeof(opcode) + sizeof(length), &fw_len, OPCODE_PATCH_SNIPPETS, NULL, p_patch_node_head);
				break;
			case OPCODE_DUMMY_HEADER:
				if (patch_key_id != 0) {
					RS_ERR("patch_key_id = 0x%x, ignore", patch_key_id);
					break;
				}
				_parse_patch_section(p_section + sizeof(opcode) + sizeof(length), &fw_len, OPCODE_DUMMY_HEADER, NULL, p_patch_node_head);
				break;
			case OPCODE_SECURITY_HEADER:
				if (patch_key_id == 0) {
					RS_ERR("patch_key_id = 0x%x, ignore", patch_key_id);
					break;
				}
				_parse_patch_section(p_section + sizeof(opcode) + sizeof(length), &fw_len, OPCODE_SECURITY_HEADER, &found_security_header, p_patch_node_head);
				break;
			case OPCODE_OTA_FLAG:
				RS_ERR("OTA flag not support");
				break;
			case OPCODE_CONTROLLER_RSVD:
				break;
			default:
				RS_ERR("Unknown opcode 0x%x", opcode);
				break;
			}
			p_section += sizeof(opcode) + sizeof(length) + length;
		}

		// if has key id but not found security header, parse dummy header again
		if (patch_key_id != 0 && found_security_header == false) {
			p_section = p_buf + sizeof(section_num);
			for (i = 0; i < section_num; i++) {
				LE_TO_UINT32(opcode, p_section);
				LE_TO_UINT32(length, p_section + sizeof(opcode));

				if (opcode == OPCODE_DUMMY_HEADER) {
					_parse_patch_section(p_section + sizeof(opcode) + sizeof(length), &fw_len, OPCODE_DUMMY_HEADER, NULL, p_patch_node_head);
				}
				p_section += sizeof(opcode) + sizeof(length) + length;
			}
		}
	}

	return fw_len;
}

static uint8_t _get_patch_info(struct rtb_struct* rtb_cfg)
{
	PATCH_INFO *info = patch_info;
	uint32_t version_date, version_time;
	uint32_t fw_len;
	uint8_t sig_len = 8;

	info->patch_buf = rtb_cfg->fw_buf;
	info->patch_len = rtb_cfg->fw_len;

	LE_TO_UINT32(version_date, info->patch_buf + sig_len);
	LE_TO_UINT32(version_time, info->patch_buf + sig_len + sizeof(version_date));
	RS_INFO("FW Version: %d%d", version_date, version_time);

	fw_len = _parse_patch(info->patch_buf + sig_len + sizeof(version_date) + sizeof(version_time), &info->head_node);
	if (fw_len == 0) {
		RS_ERR("Available patch not found!");
		return HCI_IGNORE;
	}
	RS_INFO("FW Length: %d", fw_len);

	info->fw_len = fw_len;

    info->config_buf = rtb_cfg->config_buf;
    info->config_len = rtb_cfg->config_len;

	/* Calculate patch info */
	info->end_index = (info->fw_len + info->config_len - 1) / HCI_PATCH_FRAG_SIZE;
	info->last_pkt = (info->fw_len + info->config_len) % HCI_PATCH_FRAG_SIZE;
	if (info->last_pkt == 0) {
		info->last_pkt = HCI_PATCH_FRAG_SIZE;
	}

	return HCI_SUCCESS;
}

static uint8_t hci_get_patch_cmd_len(uint8_t *cmd_len, struct rtb_struct* rtb_cfg)
{
	uint8_t ret;
	PATCH_INFO *info = patch_info;

	/* Download FW partial patch first time, get patch and info */
	if (0 == info->cur_index) {
		ret = _get_patch_info(rtb_cfg);
		if (HCI_SUCCESS != ret) {
			return ret;
		}
	}

	if (info->cur_index == info->end_index) {
		*cmd_len = info->last_pkt + 1;
		return HCI_SUCCESS;
	}

	*cmd_len = HCI_PATCH_FRAG_SIZE + 1;

	return HCI_SUCCESS;
}

static uint8_t hci_get_patch_cmd_buf(uint8_t *cmd_buf, uint8_t cmd_len)
{
	PATCH_INFO *info = patch_info;
	uint8_t *data_buf = &cmd_buf[1];
	uint8_t data_len = cmd_len - 1;
	uint8_t remain_len = data_len;
	uint8_t sending_len = 0;
	uint32_t total_node_len = 0;
	struct list_head *pos, *next;
	PATCH_NODE *node;

	/* first byte is index */
	if (info->cur_index >= 0x80) {
		cmd_buf[0] = (info->cur_index - 0x80) % 0x7f + 1;
	} else {
		cmd_buf[0] = info->cur_index % 0x80;
	}
	if (info->cur_index == info->end_index) {
		cmd_buf[0] |= 0x80;
	}

	list_for_each_safe(pos, next, &info->head_node.list) {
		node = list_entry(pos, PATCH_NODE, list);
		total_node_len += node->payload_len;

		// Find the patch node need to be send
		if (info->sent_fw_len < total_node_len) {
			if (info->sent_fw_len + remain_len < total_node_len) {
				sending_len = remain_len;
				memcpy(data_buf + data_len - remain_len, node->payload + node->sent_payload_len, sending_len);
				info->sent_fw_len += sending_len;
				node->sent_payload_len += sending_len;
				remain_len -= sending_len;
				// data_buf is already full, break the loop
				break;
			} else {
				sending_len = node->payload_len - node->sent_payload_len;
				memcpy(data_buf + data_len - remain_len, node->payload + node->sent_payload_len, sending_len);
				info->sent_fw_len += sending_len;
				node->sent_payload_len += sending_len;
				remain_len -= sending_len;
				if (node->payload_len != node->sent_payload_len) {
					RS_ERR("Patch node has not been sent completely! payload_len = %d, sent_payload_len = %d", node->payload_len, node->sent_payload_len);
					return HCI_FAIL;
				}
				// data_buf is not full, jump to the next patch node
			}
		}
	}

	if (remain_len > 0) {
		if (info->fw_len != info->sent_fw_len) {
			RS_ERR("Firmware has not been sent completely! fw_len = %d, sent_fw_len = %d", info->fw_len, info->sent_fw_len);
			return HCI_FAIL;
		}

		// Add config data after firmware
		memcpy(data_buf + data_len - remain_len, info->config_buf + info->sent_config_len, remain_len);
		info->sent_config_len += remain_len;
	}

	if (info->cur_index == info->end_index) {
		if (info->config_len != info->sent_config_len) {
			RS_ERR("Config data has not been sent completely! config_len = %d, sent_config_len = %d\r", info->config_len, info->sent_config_len);
			return HCI_FAIL;
		}
	} else {
		info->cur_index++;
	}

	return HCI_SUCCESS;
}

uint8_t hci_download_patch_v2(int fd, struct rtb_struct* rtb_cfg, struct termios *ti)
{
	uint8_t ret = HCI_SUCCESS;
	uint8_t buf[RESERVE_LEN + 256];
    uint8_t resp[8];
	uint16_t rlen = sizeof(resp);

    buf[0] = 0x01;
    buf[1] = 0x20;
    buf[2] = 0xfc;

    chipid = rtb_cfg->eversion + 1;

	ret = hci_patch_downlod_init();
	if (HCI_SUCCESS != ret) {
		goto dl_patch_done;
	}

	while (1) {
        memset(buf + 3, 0, 254);
		memset(resp, 0, 8);
		ret = hci_get_patch_cmd_len(&buf[3], rtb_cfg);
		if (HCI_SUCCESS != ret) {
			goto dl_patch_done;
		}

		ret = hci_get_patch_cmd_buf(&buf[4], buf[3]);
		if (HCI_SUCCESS != ret) {
			goto dl_patch_done;
		}

		RS_DBG("fd: %d, index: %d, len: %d", fd, buf[4], buf[3] - 1);
		if(buf[4] & 0x80) {
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

		/* Check the last patch fragment */
		if (resp[7] & 0x80) {
			break;
		}
	}

dl_patch_done:
	hci_patch_download_done();

	return ret;
}