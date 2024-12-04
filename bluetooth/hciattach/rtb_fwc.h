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


bool bluetooth_is_mp_mode(void);
void hci_platform_get_iqk_data(uint8_t *phy_efuse, uint8_t *data, uint8_t len);
struct patch_info *get_patch_entry(struct rtb_struct *btrtl);
uint8_t *rtb_read_config(const char *file, int *cfg_len, uint8_t chip_type, uint8_t *efuse_config);
uint8_t *rtb_read_firmware(struct rtb_struct *btrtl, int *fw_len);
#if (USE_FW_FILE_INSTEAD_OF_ARRAY == 0)
extern unsigned char rtl8730_hci_init_config[];
extern uint32_t rtl8730_hci_init_config_len;
extern const unsigned char rtl8730_rtlbt_fw[];
extern uint32_t rtl8730_rtlbt_fw_len;
#endif

#define HCI_FAIL      0
#define HCI_SUCCESS   1
#define HCI_IGNORE    2
#define PATCH_VERSION_INVALID   0
#define PATCH_VERSION_V1        1
#define PATCH_VERSION_V2        2
#define PATCH_VERSION_V3        3
#define RESERVE_LEN 1
#define HCI_PATCH_PROJECT_ID       0x28

#define LE_TO_UINT16(_data, _array)  {              \
        _data = ((uint16_t)(*((uint8_t *)(_array) + 0)) << 0) |        \
                ((uint16_t)(*((uint8_t *)(_array) + 1)) << 8);         \
    }

#define LE_TO_UINT32(_data, _array)    {            \
        _data = ((uint32_t)(*((uint8_t *)(_array) + 0)) <<  0) |       \
                ((uint32_t)(*((uint8_t *)(_array) + 1)) <<  8) |       \
                ((uint32_t)(*((uint8_t *)(_array) + 2)) << 16) |       \
                ((uint32_t)(*((uint8_t *)(_array) + 3)) << 24);        \
    }

#define LE_TO_UINT64(_data, _array)    {            \
        _data = ((uint64_t)(*((uint8_t *)(_array) + 0)) <<  0) |       \
                ((uint64_t)(*((uint8_t *)(_array) + 1)) <<  8) |       \
                ((uint64_t)(*((uint8_t *)(_array) + 2)) << 16) |       \
                ((uint64_t)(*((uint8_t *)(_array) + 3)) << 24) |       \
                ((uint64_t)(*((uint8_t *)(_array) + 4)) << 32) |       \
                ((uint64_t)(*((uint8_t *)(_array) + 5)) << 40) |       \
                ((uint64_t)(*((uint8_t *)(_array) + 6)) << 48) |       \
                ((uint64_t)(*((uint8_t *)(_array) + 7)) << 56);        \
    }

/* list head from kernel */
struct list_head {
	struct list_head *next, *prev;
};

#define offsetof(TYPE, MEMBER)	((size_t)&((TYPE *)0)->MEMBER)

#define container_of(ptr, type, member) ({                      \
	const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
	(type *)( (char *)__mptr - offsetof(type,member) );})

#define list_entry(ptr, type, member) \
	container_of(ptr, type, member)

#define list_for_each_safe(pos, n, head) \
	for (pos = (head)->next, n = pos->next; pos != (head); \
		pos = n, n = pos->next)

inline void INIT_LIST_HEAD(struct list_head *list)
{
	list->next = list;
	list->prev = list;
}

inline int list_empty(const struct list_head *head)
{
	return head->next == head;
}

inline void __list_add(struct list_head *_new,
							  struct list_head *prev,
							  struct list_head *next)
{
	next->prev = _new;
	_new->next = next;
	_new->prev = prev;
	prev->next = _new;
}

inline void list_add_tail(struct list_head *_new, struct list_head *head)
{
	__list_add(_new, head->prev, head);
}

inline void __list_del(struct list_head *prev, struct list_head *next)
{
	next->prev = prev;
	prev->next = next;
}

inline bool __list_del_entry_valid(struct list_head *entry)
{
	return true;
}

inline void __list_del_entry(struct list_head *entry)
{
	if (!__list_del_entry_valid(entry))
		return;

	__list_del(entry->prev, entry->next);
}

inline void list_del_init(struct list_head *entry)
{
	__list_del_entry(entry);
	INIT_LIST_HEAD(entry);
}

#define LIST_POISON1  ((void *) 0x00100100)
#define LIST_POISON2  ((void *) 0x00200200)
inline void list_del(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
	entry->next = (struct list_head *)LIST_POISON1;
	entry->prev = (struct list_head *)LIST_POISON2;
}

inline void __list_splice(const struct list_head *list,
								 struct list_head *prev,
								 struct list_head *next)
{
	struct list_head *first = list->next;
	struct list_head *last = list->prev;

	first->prev = prev;
	prev->next = first;

	last->next = next;
	next->prev = last;
}

inline void list_splice_tail(struct list_head *list,
									struct list_head *head)
{
	if (!list_empty(list)) {
		__list_splice(list, head->prev, head);
	}
}

inline void list_replace(struct list_head *old,
								struct list_head *new)
{
	new->next = old->next;
	new->next->prev = new;
	new->prev = old->prev;
	new->prev->next = new;
}

inline void list_replace_init(struct list_head *old,
									 struct list_head *new)
{
	list_replace(old, new);
	INIT_LIST_HEAD(old);
}