#include "captouch_app.h"
#include "captouch_util.h"
/* Private defines -----------------------------------------------------------*/

#define CAP_CH_DATA_SIZE		9
#define CAP_TOTAL_DATA_SIZE		(CAP_MAX_CH * CAP_CH_DATA_SIZE)
#define CAP_DATA_PAGE_SIZE		128 // Shall be larger than CAP_TOTAL_DATA_SIZE
#define CAP_DATA_PAGE_NUM		2
#define CAP_RX_BUF_LEN			1024

#define CAP_CH_INVALID			0xFF

#define CAP_MSG_ACK			0
#define CAP_MSG_ERROR			1
#define CAP_MSG_QUERY			2
#define CAP_MSG_QUERY_CH		3
#define CAP_MSG_CFG			4
#define CAP_MSG_CH_CFG			5
#define CAP_MSG_START			6
#define CAP_MSG_STOP			7
#define CAP_MSG_RESET			8
#define CAP_MSG_MBIAS 			9
#define CAP_MSG_INVALID		255

#define CAP_STS_IDLE			0
#define CAP_STS_BUSY			1


/* Private function prototypes ----------------------------------------------*/
static void cap_uart_send_string(char *pstr, int len);
/* Private variables -------------------------------------------------------*/
static uint8_t cap_monitor_buf[CAP_DATA_PAGE_NUM * CAP_DATA_PAGE_SIZE] __attribute__((aligned(64)));
static uint8_t cap_tx_data_header_buf[32] __attribute__((aligned(64)));
static uint8_t cap_rx_buf[CAP_RX_BUF_LEN];
static uint8_t cap_tx_buf[CAP_DATA_PAGE_SIZE];
static cap_ctrl_t cap_ctrl = {0};
pthread_attr_t uart_tx_attr;
unsigned char uart_dev[20];

static const cap_cmd_t cap_cmd_table[] = {
	{CAP_MSG_ACK,		"ack"},
	{CAP_MSG_ERROR,		"error"},
	{CAP_MSG_QUERY,	"query"},
	{CAP_MSG_QUERY_CH,	"query_ch"},
	{CAP_MSG_CFG,		"config"},
	{CAP_MSG_CH_CFG,	"config_ch"},
	{CAP_MSG_START,		"start"},
	{CAP_MSG_STOP,		"stop"},
	{CAP_MSG_RESET,		"reset"},
	{CAP_MSG_MBIAS,		"tune_mbias_ch"},
};

static void show_monitor_buf(void)
{
	int ch;
	uint8_t *buf;
	int offset = 0;
	int index = (cap_ctrl.rx_cnt) % CAP_DATA_PAGE_NUM;

	for (int i = 0; i < CAP_DATA_PAGE_NUM; i++) {
		CAP_LOG_DEBUG("-------cap_monitor_buf [page:%d]--- --%s-----\n", i, i == index ? "rx" : "");
		buf = cap_monitor_buf + i * CAP_DATA_PAGE_SIZE;
		offset = 0;
		for (ch = 0; ch < CAP_MAX_CH; ++ch) {
			if (cap_ctrl.monitor_cfg.channels[ch] != 0) {
				CAP_LOG_DEBUG("ch %d: ch[%d],raw_data[0x%02x%02x],baseline[0x%02x%02x],diff_data[0x%02x%02x],[%x][%x]\n", ch,
							  buf[0 + offset], buf[2 + offset], buf[1 + offset], buf[4 + offset], buf[3 + offset],
							  buf[6 + offset], buf[5 + offset], buf[7 + offset], buf[8 + offset]);
				offset += CAP_CH_DATA_SIZE;
			}
		}
	}
}

static int cap_monitor_send_data(int seq, uint8_t *data, uint16_t len)
{
	uint8_t *buf = cap_tx_data_header_buf;
	uint8_t checksum = 0;
	int index = 0;
	int snr;
	int ret;
	for (int i = 0; i < len; i++) {
		checksum ^= data[i];
	}

	memcpy(buf, "data", strlen("data"));
	index += strlen("data");
	*(uint32_t *)(buf + index) = seq;
	index += 4;
	*(uint8_t *)(buf + index) = cap_ctrl.monitor_channels;
	index += 1;
	snr = ctc_get_snr();
	*(uint16_t *)(buf + index) = snr;
	index += 2;
	*(uint16_t *)(buf + index) = len;
	index += 2;
	*(uint8_t *)(buf + index) = checksum;
	index += 1;

	CAP_LOG_DEBUG("TX data seq=%d chs=%d len=%d checksum=0x%02X\n", seq, cap_ctrl.monitor_channels, len, checksum);
	if (cap_ctrl.monitor_channels > 0) {
		CAP_LOG_DEBUG("Data: %02X %02X %02X %02X %02X %02X %02X %02X %02X\n", data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], data[8]);
	}
	if (cap_ctrl.monitor_channels > 1) {
		CAP_LOG_DEBUG("Data: %02X %02X %02X %02X %02X %02X %02X %02X %02X\n", data[9], data[10], data[11], data[12], data[13], data[14], data[15], data[16], data[17]);
	}

	pthread_mutex_lock(&cap_ctrl.tx_mutex);
	ret = write(cap_ctrl.uart_fd, (char *)buf, index);
	if (ret != index) {
		CAP_LOG_ERROR("Fail to write buf: %d", ret);
	}
	ret = write(cap_ctrl.uart_fd, (char *)data, len);
	if (ret != len) {
		CAP_LOG_ERROR("Fail to write data: %d", ret);
	}

	pthread_mutex_unlock(&cap_ctrl.tx_mutex);

	return 0;
}

static inline void cap_push_channel_data(uint8_t *buf, int index, cap_ch_rt_data_t *data)
{
	memcpy((void *)(buf + index), (void *)data, sizeof(cap_ch_rt_data_t));
}

static void cap_monitor_get_data(void)
{
	int ch;
	int valid_ch_cnt = 0;
	cap_ch_rt_data_t data;

	int index = cap_ctrl.rx_cnt % CAP_DATA_PAGE_NUM;
	uint8_t *buf = cap_monitor_buf + index * CAP_DATA_PAGE_SIZE;

	for (ch = 0; ch < CAP_MAX_CH; ++ch) {
		if (cap_ctrl.monitor_cfg.channels[ch] != 0) {
			data.ch = ch;
			ctc_get_ch_rt_data(&data);
			cap_push_channel_data(buf, valid_ch_cnt * CAP_CH_DATA_SIZE, &data);
			valid_ch_cnt++;
		}
	}

	show_monitor_buf();
	cap_ctrl.rx_cnt++;
	CAP_LOG_DEBUG("%s tx_cnt:%d,rx_cnt:%d\n", __func__, cap_ctrl.tx_cnt, cap_ctrl.rx_cnt);
}

void *monitor_tx_thread(void *arg)
{
	int index;
	uint8_t *buf;

	while (1) {
		sem_wait(&cap_ctrl.timer_sema);

		CAP_LOG_DEBUG("%s tx_cnt:%d,rx_cnt:%d\n", __func__, cap_ctrl.tx_cnt, cap_ctrl.rx_cnt);
		if (cap_ctrl.tx_cnt < cap_ctrl.rx_cnt) {
			index = cap_ctrl.tx_cnt % CAP_DATA_PAGE_NUM;
			buf = cap_monitor_buf + index * CAP_DATA_PAGE_SIZE;

			memcpy(cap_tx_buf, buf, cap_ctrl.monitor_channels * CAP_CH_DATA_SIZE);
			cap_ctrl.tx_cnt++;
			cap_monitor_send_data(cap_ctrl.tx_cnt, cap_tx_buf, cap_ctrl.monitor_channels * CAP_CH_DATA_SIZE);
		}

		if (cap_ctrl.monitor_stop) {
			break;
		}
	}

	CAP_LOG_WARN("TX task stopped\n");
	pthread_attr_destroy(&uart_tx_attr);
	pthread_exit(NULL);
}

static void monitor_timer_handler(int signo)
{

	struct timeval tm;
	gettimeofday(&tm, NULL);

	if (signo == SIGALRM) {
		CAP_LOG_DEBUG("\r\nGet the SIGALRM signal! time[%ld.%03ld]\n", tm.tv_sec, tm.tv_usec / 1000);
		cap_monitor_get_data();
		sem_post(&cap_ctrl.timer_sema);
	}
}

void timer_stop()
{
	struct itimerval value;
	value.it_value.tv_sec = 0;
	value.it_value.tv_usec = 0;
	value.it_interval = value.it_value;
	setitimer(ITIMER_REAL, &value, NULL);
}

int timer_start(int interval)
{
	struct itimerval new_value = {0};

	CAP_LOG_INFO("Monitor timer_start ...\n");

	signal(SIGALRM, monitor_timer_handler);

	new_value.it_value.tv_sec = 0;//start time point
	new_value.it_value.tv_usec = 500;//us

	new_value.it_interval.tv_sec = (int)(interval / 1000); //timer interval
	new_value.it_interval.tv_usec = (interval % 1000) * 1000;

	if (setitimer(ITIMER_REAL, &new_value, NULL) < 0) {
		CAP_LOG_ERROR("Setitimer Failed : %s\n");
		return -1;
	}
	CAP_LOG_INFO("Monitor timer start OK\n");

	return 0;
}

static int cap_monitor_start_daemon(void)
{
	int ret = 0;

	CAP_LOG_INFO("Monitor daemon started...\n");

	cap_ctrl.monitor_stop = 0;
	cap_ctrl.monitor_status = CAP_STS_BUSY;

	/*1.timer*/
	timer_start(cap_ctrl.monitor_cfg.samp_int);//ms

	/*2.UART-TX task*/
	struct sched_param schedule_param;

	pthread_attr_init(&uart_tx_attr);
	schedule_param.sched_priority = 1;/*for set priority*/
	pthread_attr_setinheritsched(&uart_tx_attr, PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setschedpolicy(&uart_tx_attr, SCHED_RR);
	pthread_attr_setschedparam(&uart_tx_attr, &schedule_param);

	ret = pthread_create(&cap_ctrl.uart_tx_id, &uart_tx_attr, monitor_tx_thread, (void *)(&cap_ctrl.uart_tx_id));
	if (ret) {
		CAP_LOG_ERROR("Fail to create tx task");
		return ret;
	}

	pthread_detach(cap_ctrl.uart_tx_id);
	return ret;
}

static int cap_monitor_start(cap_start_req_t *req)
{
	int i;
	int enable;
	cap_start_req_t *cfg = &cap_ctrl.monitor_cfg;

	CAP_LOG_DEBUG("Monitor start request\n");

	memcpy((void *)cfg, (void *)req, sizeof(cap_start_req_t));

	CAP_LOG_DEBUG("Monitor cfg: channels=%d %d %d %d %d %d %d %d %d\n",
				  cfg->channels[0], cfg->channels[1], cfg->channels[2], cfg->channels[3], cfg->channels[4],
				  cfg->channels[5], cfg->channels[6], cfg->channels[7], cfg->channels[8]);

	cap_ctrl.monitor_channels = 0;
	for (i = 0; i < CAP_MAX_CH; ++i) {
		enable = req->channels[i];
		if (enable != 0) {
			cap_ctrl.monitor_channels++;
		}
	}

	cap_ctrl.rx_cnt = 0;
	cap_ctrl.tx_cnt = 0;

	ctc_start(cfg);
	return 0;
}

static int cap_monitor_stop(void)
{
	CAP_LOG_DEBUG("Monitor stop request\n");

	if (cap_ctrl.monitor_stop) {
		CAP_LOG_INFO("Monitor already stopped\n");
		return 0;
	}

	cap_ctrl.monitor_stop = 1;
	cap_ctrl.monitor_status = CAP_STS_IDLE;

	timer_stop();

	CAP_LOG_INFO("Monitor stopped\n");

	return 0;
}

static void cap_uart_send_string(char *pstr, int len)
{
	int ret;
	CAP_LOG_INFO("Query response: %s (%d)\n", pstr, len);

	pthread_mutex_lock(&cap_ctrl.tx_mutex);
	ret = write(cap_ctrl.uart_fd, pstr, len);
	if (ret != len) {
		CAP_LOG_ERROR("Fail to write: %d", ret);
	}
	pthread_mutex_unlock(&cap_ctrl.tx_mutex);
}

static void cap_monitor_send_response(uint8_t ret)
{
	cJSON *msg_obj;
	char *msg_js;

	msg_obj = cJSON_CreateObject();
	if (msg_obj == NULL) {
		CAP_LOG_ERROR("Fail to create cjson object\n");
		return;
	}

	if (ret == 0) {
		cJSON_AddStringToObject(msg_obj, "type", "ack");
	} else {
		cJSON_AddStringToObject(msg_obj, "type", "error");
	}

	msg_js = cJSON_Print(msg_obj);
	cJSON_Delete(msg_obj);

	cap_uart_send_string(msg_js, strlen(msg_js));

	free(msg_js);
}

static int cap_monitor_reset_baseline(void)
{
	CAP_LOG_DEBUG("Baseline reset request\n");

	ctc_reset_baseline();

	return 0;
}

static int cap_moniter_tune_channel_mbias(cap_ch_cfg_req_t *cfg)
{
	cJSON *msg_obj;
	char *msg_js;
	int ch_data;

	CAP_LOG_DEBUG("Query channel mbias...\n");

	if ((msg_obj = cJSON_CreateObject()) == NULL) {
		CAP_LOG_ERROR("Fail to create cjson object\n");
		return -1;
	}

	ctc_tune_ch_mbias(cfg, MBIAS_TUNE_TARGET, MBIAS_TUNE_ERROR);
	ctc_set_cur_chx(cfg->ch);
	ch_data = ctc_get_chx_mbias();
	cJSON_AddStringToObject(msg_obj, "type", "tune_mbias_ch");
	cJSON_AddNumberToObject(msg_obj, "ch", cfg->ch);
	cJSON_AddNumberToObject(msg_obj, "mbias", cfg->mbias);
	cJSON_AddNumberToObject(msg_obj, "baseline", ch_data);

	msg_js = cJSON_Print(msg_obj);
	cJSON_Delete(msg_obj);

	cap_uart_send_string(msg_js, strlen(msg_js));

	free(msg_js);

	return 0;
}

static int cap_monitor_config_channel(cap_ch_cfg_req_t *cfg)
{
	CAP_LOG_DEBUG("Channel config request\n");

	ctc_set_ch_config(cfg);

	return 0;
}

static int cap_monitor_config(cap_cfg_req_t *cfg)
{
	CAP_LOG_DEBUG("Config request\n");

	ctc_set_config(cfg);

	return 0;
}

static int cap_monitor_query_channel(int ch)
{
	cJSON *msg_obj;
	char *msg_js;
	cap_ch_cfg_req_t cfg;

	CAP_LOG_DEBUG("Query channel configurations...\n");

	if ((msg_obj = cJSON_CreateObject()) == NULL) {
		CAP_LOG_ERROR("Fail to create cjson object\n");
		return -1;
	}

	cfg.ch = ch;
	ctc_get_ch_config(&cfg);

	cJSON_AddStringToObject(msg_obj, "type", "query_ch");
	cJSON_AddNumberToObject(msg_obj, "ch", ch);
	cJSON_AddNumberToObject(msg_obj, "enable", cfg.enable);
	cJSON_AddNumberToObject(msg_obj, "mbias", cfg.mbias);
	cJSON_AddNumberToObject(msg_obj, "diff_th", cfg.diff_th);
	cJSON_AddNumberToObject(msg_obj, "abs_th", cfg.abs_th);
	cJSON_AddNumberToObject(msg_obj, "nn_th", cfg.nn_th);
	cJSON_AddNumberToObject(msg_obj, "pn_th", cfg.pn_th);

	msg_js = cJSON_Print(msg_obj);
	cJSON_Delete(msg_obj);

	cap_uart_send_string(msg_js, strlen(msg_js));

	free(msg_js);
	return 0;
}

static int cap_monitor_query(void)
{
	cJSON *msg_obj;
	char *msg_js;
	cap_cfg_req_t cfg;

	CAP_LOG_DEBUG("Query general configurations...\n");

	if ((msg_obj = cJSON_CreateObject()) == NULL) {
		CAP_LOG_ERROR("Fail to create cjson object\n");
		return 1;
	}

	ctc_get_config(&cfg);

	cJSON_AddStringToObject(msg_obj, "type", "query");
	cJSON_AddStringToObject(msg_obj, "version", CAP_SW_VERSION);

	switch (cap_ctrl.monitor_status) {
	case CAP_STS_BUSY:
		cJSON_AddStringToObject(msg_obj, "status", "busy");
		break;
	default:
		cJSON_AddStringToObject(msg_obj, "status", "idle");
		break;
	}

	cJSON_AddNumberToObject(msg_obj, "scan_int", cfg.scan_int);
	cJSON_AddNumberToObject(msg_obj, "avg_samp", cfg.avg_samp);
	cJSON_AddNumberToObject(msg_obj, "etc_en", cfg.etc_en);
	cJSON_AddNumberToObject(msg_obj, "step", cfg.etc_step);
	cJSON_AddNumberToObject(msg_obj, "etc_int", cfg.etc_int);
	cJSON_AddNumberToObject(msg_obj, "etc_factor", cfg.etc_factor);
	cJSON_AddNumberToObject(msg_obj, "deb_en", cfg.deb_en);
	cJSON_AddNumberToObject(msg_obj, "deb_cnt", cfg.deb_cnt);
	cJSON_AddStringToObject(msg_obj, "chip_name", cfg.chip_name);

	msg_js = cJSON_Print(msg_obj);
	cJSON_Delete(msg_obj);

	cap_uart_send_string(msg_js, strlen(msg_js));

	free(msg_js);

	return 0;
}

static void cap_rx_process(void)
{
	cJSON *root_obj;
	cJSON *type_obj;
	cJSON *item_obj;
	unsigned int i;
	uint8_t tmp_buf[CAP_RX_BUF_LEN];
	int ret = -1;
	int cmd = CAP_MSG_INVALID;

	memset(tmp_buf, 0x00, CAP_RX_BUF_LEN);
	memcpy(tmp_buf, cap_rx_buf, cap_ctrl.cmd_rx_cnt);
	memset(cap_rx_buf, 0x00, CAP_RX_BUF_LEN);

	cap_ctrl.cmd_rx_cnt = 0;

	CAP_LOG_INFO("Got request: %s\n", tmp_buf);

	if ((root_obj = cJSON_Parse((const char *)tmp_buf)) != NULL) {
		if ((type_obj = cJSON_GetObjectItem(root_obj, "type")) != NULL) {
			CAP_LOG_DEBUG("Request type: %s\n", type_obj->valuestring);
			for (i = 0; i < sizeof(cap_cmd_table) / sizeof(cap_cmd_t); i++) {
				if (!strcmp(type_obj->valuestring, cap_cmd_table[i].txt)) {
					cmd = cap_cmd_table[i].cmd;
					break;
				}
			}
		}
	}

	switch (cmd) {
	case CAP_MSG_QUERY: {/*{"type":"query"}*/
		ret = cap_monitor_query();
		if (ret != 0) {
			cap_monitor_send_response(ret);
		}
		break;
	}
	case CAP_MSG_QUERY_CH: {/* {"type":"query_ch","ch":8}*/
		item_obj = cJSON_GetObjectItem(root_obj, "ch");
		ret = cap_monitor_query_channel(item_obj->valueint);
		if (ret != 0) {
			cap_monitor_send_response(ret);
		}
		break;
	}
	case CAP_MSG_CFG: {/*{"type":"config","scan_int":60,"avg_samp":6,"etc_en":1,"step":1,"etc_int":3,"etc_factor":4,"deb_en":1,"deb_cnt":0}*/
		cap_cfg_req_t cfg = {0};
		item_obj = cJSON_GetObjectItem(root_obj, "scan_int");
		cfg.scan_int = item_obj->valueint;
		item_obj = cJSON_GetObjectItem(root_obj, "step");/*etc_step*/
		cfg.etc_step = item_obj->valueint;
		item_obj = cJSON_GetObjectItem(root_obj, "avg_samp");
		cfg.avg_samp = item_obj->valueint;
		item_obj = cJSON_GetObjectItem(root_obj, "etc_en");
		cfg.etc_en = item_obj->valueint;
		item_obj = cJSON_GetObjectItem(root_obj, "etc_int");
		cfg.etc_int = item_obj->valueint;
		item_obj = cJSON_GetObjectItem(root_obj, "etc_factor");
		cfg.etc_factor = item_obj->valueint;
		item_obj = cJSON_GetObjectItem(root_obj, "deb_en");
		cfg.deb_en = item_obj->valueint;
		item_obj = cJSON_GetObjectItem(root_obj, "deb_cnt");
		cfg.deb_cnt = item_obj->valueint;
		ret = cap_monitor_config(&cfg);
		cap_monitor_send_response(ret);
		break;
	}
	case CAP_MSG_CH_CFG: {/*{"type":"config_ch","ch":0,"enable":1,"mbias":34,"diff_th":100,"abs_th":123,"nn_th":40,"pn_th":40}*/
		cap_ch_cfg_req_t cfg = {0};
		item_obj = cJSON_GetObjectItem(root_obj, "ch");
		cfg.ch = item_obj->valueint;
		item_obj = cJSON_GetObjectItem(root_obj, "enable");
		cfg.enable = item_obj->valueint;
		item_obj = cJSON_GetObjectItem(root_obj, "mbias");
		cfg.mbias = item_obj->valueint;
		item_obj = cJSON_GetObjectItem(root_obj, "diff_th");
		cfg.diff_th = item_obj->valueint;
		item_obj = cJSON_GetObjectItem(root_obj, "abs_th");
		cfg.abs_th = item_obj->valueint;
		item_obj = cJSON_GetObjectItem(root_obj, "nn_th");
		cfg.nn_th = item_obj->valueint;
		item_obj = cJSON_GetObjectItem(root_obj, "pn_th");
		cfg.pn_th = item_obj->valueint;

		ret = cap_monitor_config_channel(&cfg);
		cap_monitor_send_response(ret);
		break;
	}

	case CAP_MSG_START: {/*{"type":"start","samp_int":30,"ch0":1,"ch1":1,"ch2":1,"ch3":1,"ch4":0,"ch5":0,"ch6":0,"ch7":0,"ch8":0}*/
		cap_start_req_t req = {0};
		item_obj = cJSON_GetObjectItem(root_obj, "samp_int");/*for sw timer interval*/
		req.samp_int = item_obj->valueint;
		item_obj = cJSON_GetObjectItem(root_obj, "ch0");
		req.channels[0] = item_obj->valueint;
		item_obj = cJSON_GetObjectItem(root_obj, "ch1");
		req.channels[1] = item_obj->valueint;
		item_obj = cJSON_GetObjectItem(root_obj, "ch2");
		req.channels[2] = item_obj->valueint;
		item_obj = cJSON_GetObjectItem(root_obj, "ch3");
		req.channels[3] = item_obj->valueint;
		item_obj = cJSON_GetObjectItem(root_obj, "ch4");
		req.channels[4] = item_obj->valueint;
		item_obj = cJSON_GetObjectItem(root_obj, "ch5");
		req.channels[5] = item_obj->valueint;
		item_obj = cJSON_GetObjectItem(root_obj, "ch6");
		req.channels[6] = item_obj->valueint;
		item_obj = cJSON_GetObjectItem(root_obj, "ch7");
		req.channels[7] = item_obj->valueint;
		item_obj = cJSON_GetObjectItem(root_obj, "ch8");
		req.channels[8] = item_obj->valueint;

		ret = cap_monitor_start(&req);
		cap_monitor_send_response(ret);
		if (ret == 0) {
			cap_monitor_start_daemon();
		}
		break;
	}
	case CAP_MSG_STOP:/*{"type":"stop"}*/
		ret = cap_monitor_stop();
		cap_monitor_send_response(ret);
		break;
	case CAP_MSG_RESET:/*{"type":"reset"}*/
		ret = cap_monitor_reset_baseline();
		cap_monitor_send_response(ret);
		break;
	case CAP_MSG_MBIAS: {/*{"type":"tune_mbias_ch","ch":6}*/
		cap_ch_cfg_req_t cfg = {0};
		item_obj = cJSON_GetObjectItem(root_obj, "ch");
		cfg.ch = item_obj->valueint;
		ret = cap_moniter_tune_channel_mbias(&cfg);
		if (ret != 0) {
			cap_monitor_send_response(ret);
		}
		break;
	}
	default:
		CAP_LOG_WARN("Unsupport cmd type: %d\n", cmd);
		cap_monitor_send_response(ret);
		CAP_LOG_DEBUG("get:%d\n", ctc_get_snr());
		break;
	}
	cJSON_Delete(root_obj);

}

void *cap_rx_thread(void *arg)
{
	(void)arg;
	while (1) {
		sem_wait(&cap_ctrl.rx_sema);
		cap_rx_process();
	}

	CAP_LOG_WARN("CAPT RX task stopped\n");
	pthread_exit(NULL);
}

void *uart_rx_thread(void *arg)
{
	int ret, nByte;
	char sig;
	fd_set rfds;

	while (1) {
		FD_ZERO(&rfds);
		FD_SET(cap_ctrl.uart_fd, &rfds);
		ret = select(cap_ctrl.uart_fd + 1, &rfds, NULL, NULL, NULL);

		if ((ret > 0)  && (FD_ISSET(cap_ctrl.uart_fd, &rfds))) {
			nByte = read(cap_ctrl.uart_fd, &cap_rx_buf[cap_ctrl.cmd_rx_cnt], CAP_RX_BUF_LEN);
			if (nByte > 0) {
				CAP_LOG_DEBUG("capt rx:%s(len:%d)\n", cap_rx_buf, nByte);

				for (int i = 0 ; i < nByte; i++) {
					sig = cap_rx_buf[cap_ctrl.cmd_rx_cnt + i];
					if (sig == '{') {
						cap_ctrl.cmd_start_cnt++;
					} else if (sig == '}') {
						cap_ctrl.cmd_end_cnt++;
					}
				}

				cap_ctrl.cmd_rx_cnt += nByte;
				CAP_LOG_DEBUG("start:%d,end:%d\n\r", cap_ctrl.cmd_start_cnt, cap_ctrl.cmd_end_cnt);

				if ((cap_ctrl.cmd_start_cnt != 0) && cap_ctrl.cmd_start_cnt == cap_ctrl.cmd_end_cnt) {
					cap_ctrl.cmd_start_cnt = 0;
					cap_ctrl.cmd_end_cnt = 0;
					sem_post(&cap_ctrl.rx_sema);
				}

			}
		}
	}

	CAP_LOG_WARN("UART RX task stopped\n");

	pthread_exit(NULL);
}

int cap_uart_init(void)
{
	int nByte;
	int ret;
	char buffer[512];
	char *uart_out = "start uart OK!\n";
	struct termios newtio, oldtio;

	cap_ctrl.uart_fd = open(uart_dev, O_RDWR | O_NOCTTY);
	if (cap_ctrl.uart_fd < 0) {
		CAP_LOG_ERROR("Fail to open device node %s\n", uart_dev);
		return -1;
	}

	ret = tcgetattr(cap_ctrl.uart_fd, &oldtio);
	if (ret) {
		CAP_LOG_ERROR("Fail to get attribute: %d\n", ret);
		return ret;
	}

	memset(&newtio, 0, sizeof(newtio));

	/*local receive*/
	newtio.c_cflag |= (CLOCAL | CREAD);

	/*8bit data*/
	newtio.c_cflag &= ~CSIZE;
	newtio.c_cflag |= CS8;

	/*no parity and ignore parity error*/
	newtio.c_cflag &= ~PARENB;
	newtio.c_iflag = IGNPAR;

	/*set baudrate*/
	newtio.c_cflag |=  B1500000;

	/*1 stop bit*/
	newtio.c_cflag &=  ~CSTOPB;

	/*time out*/
	newtio.c_cc[VTIME] = 0;
	newtio.c_cc[VMIN] = 0;

	/*flush intput queue*/
	tcflush(cap_ctrl.uart_fd, TCIFLUSH);

	/*enable ternios setting*/
	ret = tcsetattr(cap_ctrl.uart_fd, TCSANOW, &newtio);
	if (ret)	{
		CAP_LOG_ERROR("Fail to set attribute: %d\n", ret);
		return ret;
	}

	ret = write(cap_ctrl.uart_fd, uart_out, strlen(uart_out));
	if (ret != strlen(uart_out)) {
		CAP_LOG_ERROR("Fail to write: %d", ret);
		return ret;
	}

	CAP_LOG_INFO("Capt Tool %s init OK!\n", uart_dev);
	return 0;
}

int app_init()
{
	int ret;

	ret = cap_uart_init();
	if (ret) {
		return -1;
	}
	cap_ctrl.monitor_stop = 0;
	cap_ctrl.monitor_status = CAP_STS_IDLE;
	cap_ctrl.rx_cnt = 0;
	cap_ctrl.tx_cnt = 0;

	memset(cap_monitor_buf, 0x00, CAP_MAX_CH * CAP_CH_DATA_SIZE);
	memset(cap_rx_buf, 0, CAP_RX_BUF_LEN);

	ret = sem_init(&cap_ctrl.rx_sema, 0, 0);
	if (ret == -1) {
		CAP_LOG_ERROR("sem_init failed \n");
		return -1;
	}

	ret = sem_init(&cap_ctrl.timer_sema, 0, 0);
	if (ret == -1) {
		CAP_LOG_ERROR("sem_init failed \n");
		return -1;
	}

	ret = pthread_mutex_init(&cap_ctrl.tx_mutex, NULL);
	if (0 != ret) {
		CAP_LOG_ERROR("mutex init failed \n");
		return -1;
	}

	return 0;
}

int main(int argc, char **argv)
{
	int ret;
	pthread_t capt_rx_tid;
	pthread_t uart_rx_tid;

	if (argc < 2) {
		CAP_LOG_ERROR("cmd: captouch <UART device node> &\n");
		return 1;
	}  else {
	        strcpy(uart_dev, (char*)argv[1]);
	}

	ret = app_init();
	if (ret) {
		goto cap_test_daemon_exit;
	}
	CAP_LOG_INFO("Capt Tool App Start!\n");

	/*CAPT-RX task*/
	if (pthread_create(&capt_rx_tid, NULL, cap_rx_thread, NULL) != 0) {
		perror("pthread_create cap_rx_thread error");
		goto cap_test_daemon_exit;
	}

	/*UART-RX task*/
	if (pthread_create(&uart_rx_tid, NULL, uart_rx_thread, NULL) != 0) {
		perror("pthread_create uart_rx_thread error");
		goto cap_test_daemon_exit;
	}

	/*wait till thread exit*/
	pthread_join(capt_rx_tid, NULL);
	pthread_join(uart_rx_tid, NULL);

	CAP_LOG_INFO("Capt Tool App Stop!\n");

cap_test_daemon_exit:
	CAP_LOG_ERROR("Capt Tool App EXIT\n");
	pthread_mutex_destroy(&cap_ctrl.tx_mutex);
	sem_destroy(&cap_ctrl.timer_sema);
	sem_destroy(&cap_ctrl.rx_sema);
	return 0;
}
