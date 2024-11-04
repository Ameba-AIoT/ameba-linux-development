#ifndef _CAP_TOOL_H_
#define _CAP_TOOL_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include <semaphore.h>
#include <pthread.h>
#include <termios.h>
#include <sys/time.h>
#include <signal.h>

#include <cJSON.h>

#define CAP_DEBUG				0
#define CAP_SW_VERSION			"1.0"

#define CAP_MAX_CH				(9)

#define MBIAS_MAX_VALUE			64

#define CAP_UART_BAUDRATE		1500000
#define MBIAS_TUNE_TARGET		3300
#define MBIAS_TUNE_ERROR		300

#define CT_CHANNEL_NUM					(9)
#define IS_CT_CHANNEL(CHANNEL_NUM)		(CHANNEL_NUM < CT_CHANNEL_NUM)
#define MBIAS_MAX_VALUE			64

#if CAP_DEBUG
#define CAP_LOG_DEBUG(fmt, ...)	printf("[CAP][DEBUG] " fmt "\n", ##__VA_ARGS__)
#else
#define CAP_LOG_DEBUG(fmt, ...)	do { } while(0)
#endif
#define CAP_LOG_INFO(fmt, ...)	printf("[CAP][INFO] " fmt "\n", ##__VA_ARGS__)
#define CAP_LOG_WARN(fmt, ...)	printf("[CAP][WARN] " fmt "\n", ##__VA_ARGS__)
#define CAP_LOG_ERROR(fmt, ...)	printf("[CAP][ERROR] " fmt "\n", ##__VA_ARGS__)

#define CHIP_NAME				"AmebaSmart"

typedef struct {
	int cmd;
	char *txt;
} cap_cmd_t;


typedef struct {
	int scan_int;
	int avg_samp;
	int etc_en;
	int etc_int;
	int etc_step;
	int etc_factor;
	int deb_en;//debounce en
	int deb_cnt;
	char *chip_name;
} cap_cfg_req_t;


typedef struct {
	int ch;
	int enable;
	int mbias;
	int diff_th;
	int abs_th;
	int nn_th;
	int pn_th;
} cap_ch_cfg_req_t;

typedef struct {
	uint8_t ch;
	uint16_t raw_data;
	uint16_t baseline;
	int16_t diff_data;
	uint16_t cap;
} __attribute__((packed))  cap_ch_rt_data_t;

typedef struct {
	uint16_t samp_int;
	uint8_t channels[CAP_MAX_CH];
} cap_start_req_t;

typedef struct {
	uint32_t cmd_rx_cnt;
	uint32_t cmd_start_cnt;
	uint32_t cmd_end_cnt;

	uint32_t rx_cnt;
	uint32_t tx_cnt;

	uint8_t monitor_channels;
	uint8_t monitor_stop;
	uint8_t monitor_status;

	cap_start_req_t monitor_cfg;

	int uart_fd;
	pthread_t uart_tx_id;

	sem_t rx_sema;
	sem_t timer_sema;
	pthread_mutex_t tx_mutex;

} cap_ctrl_t;

#endif //_CAP_TOOL_H_

