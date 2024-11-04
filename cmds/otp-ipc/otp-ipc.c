#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <linux/rtc.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/timerfd.h>
#include <asm/types.h>

#define LINUX_IPC_OTP_PHY_READ8			0
#define LINUX_IPC_OTP_PHY_WRITE8		1

#define OTP_WRITE_START_BYTE			5
#define OTP_LMAP_LEN					0x400
#define OPT_REQ_MSG_PARAM_NUM			OTP_LMAP_LEN

struct otp_ipc_tx_msg {
	int otp_id;
	int offset;
	int len;
	__u8 tx_value[OPT_REQ_MSG_PARAM_NUM];
	int tx_lock;
};

struct user_space_request {
	__u8	*result;
	struct otp_ipc_tx_msg otp_order;
};

typedef struct otp_ipc_rx_res {
	int ret;
	int value;
} otp_ipc_rx_res_t;

enum realtek_otp_operation {
	RTK_OTP_READ,
	RTK_OTP_WRITE,
};

static void pabort(const char *s)
{
	perror(s);
	abort();
}

int main(int argc, char **argv[])
{
	int ret = 0;
	int i = 0;
	int fd = 0;
	char *ptr;
	struct user_space_request test_data;
	otp_ipc_rx_res_t *p_recv_msg;
	int otp_cmd;
	__u8 result[OPT_REQ_MSG_PARAM_NUM];

	if (!argv[1]) {
		printf("NOTE: otp-ipc [physic] [read/write] [addr hex] [bytes dec] [write value hex]\n");
		pabort("Current mode not supported.\n");
	}

	if (!strcmp((char *)argv[1], "physic") && !strcmp((char *)argv[2], "read")) {
		otp_cmd = LINUX_IPC_OTP_PHY_READ8;
		test_data.result = result;
		test_data.otp_order.len = strtoul((char *)argv[4], &ptr, 10);
		test_data.otp_order.offset = strtoul((char *)argv[3], &ptr, 16);
	} else if (!strcmp((char *)argv[1], "physic") && !strcmp((char *)argv[2], "write")) {
		otp_cmd = LINUX_IPC_OTP_PHY_WRITE8;
		test_data.otp_order.len = strtoul((char *)argv[4], &ptr, 10);
		test_data.otp_order.offset = strtoul((char *)argv[3], &ptr, 16);
	} else {
		printf("NOTE: otp-ipc [physic/logic] [read/write] [addr hex] [bytes dec] [write value hex]\n");
		pabort("Current mode not supported.\n");
	}
	test_data.otp_order.otp_id = otp_cmd;

	if (!strcmp((char *)argv[2], "write")) {
		if (argc != OTP_WRITE_START_BYTE + test_data.otp_order.len)
			pabort("Write value is not correct.");
		else {
			test_data.otp_order.tx_lock = 0;
			printf("OTP values will be write into Efuse.\n");
		}

		for (i = 0; i < test_data.otp_order.len; i++) {
			test_data.otp_order.tx_value[i] = strtoul((char *)argv[OTP_WRITE_START_BYTE + i], &ptr, 16);
		}
	}

	fd = open("/dev/otp-ctrl", 2); // 2: O_RDWR   1:WR only
	if (fd == -1) {
		pabort("Can't open device");
	}

	ret = ioctl(fd, otp_cmd, &test_data);
	printf("%s %s %s\n", (char *)argv[1], (char *)argv[2], (ret < 0) ? "fail" : "success");
	if (ret < 0) goto out;

	if (!strcmp((char *)argv[2], "read")) {
		printf("userspace value: \n");
		for (i = 0; i < test_data.otp_order.len; i++) {
			printf("%02x ", result[i]);
		}
		printf("\n");
	}
out:
	close(fd);
}
