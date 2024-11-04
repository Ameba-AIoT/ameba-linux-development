#include"captouch_util.h"

/* Private types ---------------------------------*/
enum config_dir {
	COMMON,
	ETC,
	CHX,
};
enum operation {
	CAPT_READ,
	CAPT_WRITE,
};

static void assert_param(uint8_t check_result)
{
	if (!check_result) {
		printf("Error: illegal parameter.\n");
	}
}

static int attr_config(enum config_dir config, const char *attr, enum operation op, char *val)
{
	char file_path[100];
	int fd;
	int len;
	int ret;
	switch (config) {
	case COMMON:
		sprintf(file_path, "%s/%s", "/sys/devices/platform/ocp/42010000.captouch/common_config", attr);
		break;
	case ETC:
		sprintf(file_path, "%s/%s", "/sys/devices/platform/ocp/42010000.captouch/etc_config", attr);
		break;
	case CHX:
		sprintf(file_path, "%s/%s", "/sys/devices/platform/ocp/42010000.captouch/chx_config", attr);
		break;
	default:
		printf("ERROR-DIR!!");
		return -1;
	}

	switch (op) {
	case CAPT_READ:
		if (0 > (fd = open(file_path, O_RDONLY))) {
			CAP_LOG_ERROR("open %s error", file_path);
			return fd;
		}
		if (0 > read(fd, val, sizeof(val))) {
			CAP_LOG_ERROR("read %s error,read size: %d, read val:%s\n", attr, sizeof(val), val);
			close(fd);
			exit(-1);
		}
		CAP_LOG_DEBUG("#get attr %s:%s\n", attr, val);
		break;
	case CAPT_WRITE:
		if (0 > (fd = open(file_path, O_WRONLY))) {
			CAP_LOG_ERROR("open %s error", file_path);
			return fd;
		}
		CAP_LOG_DEBUG("#set attr %s:%s\n", attr, val);
		len = strlen(val);
		if (len > write(fd, val, len)) {
			CAP_LOG_ERROR("write %s error\n", attr);
			close(fd);
			return -1;
		}
		break;
	default:
		CAP_LOG_ERROR("ERROR-operation!!");
		return -1;
	}

	close(fd);
	return 0;
}

static void ctc_set_ctc_enable(int state)
{
	if (state != 0) {
		/* Enable Captouch */
		attr_config(COMMON, "ctc_enable", CAPT_WRITE, "1");
	} else {
		/* Disable Captouch */
		attr_config(COMMON, "ctc_enable", CAPT_WRITE, "0");
	}
}

static void ctc_set_debounce_enable(int state)
{
	if (state != 0) {
		/* Enable debounce */
		attr_config(COMMON, "debounce_en", CAPT_WRITE, "1");
	} else {
		/* Disable debounce */
		attr_config(COMMON, "debounce_en", CAPT_WRITE, "0");
	}
}
static int ctc_get_debounce_enable(void)
{
	char buf[2] = {0};
	attr_config(COMMON, "debounce_en", CAPT_READ, buf);
	return atoi(buf);
}

static void ctc_set_debounce_times(int times)
{
	int ret;
	char buf[2] = {0};
	assert_param(times < 4);

	sprintf(buf, "%d", times);
	ret = attr_config(COMMON, "debounce_times", CAPT_WRITE, buf);
	if (ret < 0) {
		CAP_LOG_ERROR("%s\n", __func__);
	}
}
static int ctc_get_debounce_times(void)
{
	char buf[2] = {0};
	attr_config(COMMON, "debounce_times", CAPT_READ, buf);
	return atoi(buf);
}

uint16_t ctc_get_snr(void)
{
	int touch_data = 0;
	int noise_data = 0;
	int snr;

	char buf[5] = {0};
	attr_config(COMMON, "snr_touch", CAPT_READ, buf);
	touch_data =  atoi(buf);

	memset(buf, 0, 5);
	attr_config(COMMON, "snr_noise", CAPT_READ, buf);
	noise_data =  atoi(buf);
	CAP_LOG_DEBUG("SNR:n_0x%X:t_0x%X\n",  noise_data, touch_data);

	if (noise_data != 0) {
		snr = (touch_data * 100 + noise_data / 2) / noise_data; // 2-decimal float xx.xx
		if (snr > 0xFFFF) {
			snr = 0xFFFF;
		}
	} else {
		snr = 0xFFFF;
	}

	return (uint16_t)snr;

}

static void ctc_set_scan_interval(int interval)
{
	int ret;
	char buf[5] = {0};

	sprintf(buf, "%d", interval);
	ret = attr_config(COMMON, "scan_interval", CAPT_WRITE, buf);
	if (ret < 0) {
		CAP_LOG_ERROR("%s\n", __func__);
	}
}

static int ctc_get_scan_interval()
{
	char buf[5] = {0};
	attr_config(COMMON, "scan_interval", CAPT_READ, buf);
	return atoi(buf);
}

static void ctc_set_sample_num(int sample)
{
	int ret;
	char buf[2] = {0};

	assert_param(sample < 8);

	sprintf(buf, "%d", sample);
	ret = attr_config(COMMON, "sample_num", CAPT_WRITE, buf);
	if (ret < 0) {
		CAP_LOG_ERROR("%s\n", __func__);
	}
}

static int ctc_get_sample_num(void)
{
	char buf[2] = {0};
	attr_config(COMMON, "sample_num", CAPT_READ, buf);
	return atoi(buf);
}

static void ctc_set_etc_enable(int state)
{
	if (state != 0) {
		/* Enable ETC */
		attr_config(ETC, "etc_enable", CAPT_WRITE, "1");
	} else {
		/* Disable ETC */
		attr_config(ETC, "etc_enable", CAPT_WRITE, "0");
	}
}

static int ctc_get_etc_enable(void)
{
	char state[4] = {0};
	attr_config(ETC, "etc_enable", CAPT_READ, state);
	return atoi(state);
}

static void ctc_set_etc_interval(int interval)
{
	int ret;
	char buf[4] = {0};

	assert_param(interval <= 127);

	sprintf(buf, "%d", interval);
	ret = attr_config(ETC, "etc_interval", CAPT_WRITE, buf);
	if (ret < 0) {
		CAP_LOG_ERROR("%s\n", __func__);
	}
}

static int ctc_get_etc_interval(void)
{
	char interval[4] = {0};
	attr_config(ETC, "etc_interval", CAPT_READ, interval);
	return atoi(interval);
}

static void ctc_set_etc_factor(int factor)
{
	int ret;
	char buf[2] = {0};

	assert_param(factor <= 15);

	sprintf(buf, "%d", factor);
	ret = attr_config(ETC, "etc_factor", CAPT_WRITE, buf);
	if (ret < 0) {
		CAP_LOG_ERROR("%s\n", __func__);
	}
}

static int ctc_get_etc_factor(void)
{
	char buf[2] = {0};
	attr_config(ETC, "etc_factor", CAPT_READ, buf);
	return atoi(buf);
}

static void ctc_set_etc_step(int step)
{
	int ret;
	char buf[2] = {0};

	assert_param(step <= 15);

	sprintf(buf, "%d", step);
	ret = attr_config(ETC, "etc_step", CAPT_WRITE, buf);
	if (ret < 0) {
		CAP_LOG_ERROR("%s\n", __func__);
	}
}

static int ctc_get_etc_step(void)
{
	char buf[2] = {0};
	attr_config(ETC, "etc_step", CAPT_READ, buf);
	return atoi(buf);
}

void ctc_set_cur_chx(int ch_num)
{
	char buf[2] = {0};
	sprintf(buf, "%d", ch_num);
	attr_config(CHX, "ch_num", CAPT_WRITE, buf);
}

static void ctc_set_chx_enable(int state)
{
	if (state != 0) {
		attr_config(CHX, "chx_enable", CAPT_WRITE, "1");
	} else {
		attr_config(CHX, "chx_enable", CAPT_WRITE, "0");
	}
}

static int ctc_get_chx_enable()
{
	char buf[2] = {0};
	attr_config(CHX, "chx_enable", CAPT_READ, buf);
	return atoi(buf);
}

static void ctc_set_chx_mbias(int mbias)
{
	int ret;
	char buf[4] = {0};

	sprintf(buf, "%d", mbias);

	ret = attr_config(CHX, "chx_mbias", CAPT_WRITE, buf);
	if (ret < 0) {
		CAP_LOG_ERROR("%s\n", __func__);
	}
}

int ctc_get_chx_mbias(void)
{
	char buf[4] = {0};
	attr_config(CHX, "chx_mbias", CAPT_READ, buf);
	return atoi(buf);
}

static void ctc_set_chx_baseline(int baseline)
{
	int ret;
	char buf[3] = {0};

	sprintf(buf, "%d", baseline);

	ret = attr_config(CHX, "chx_baseline", CAPT_WRITE, buf);
	if (ret < 0) {
		CAP_LOG_ERROR("%s\n", __func__);
	}
}

static int ctc_get_chx_baseline(void)
{
	char buf[5] = {0};
	attr_config(CHX, "chx_baseline", CAPT_READ, buf);
	return atoi(buf);
}

static void ctc_set_chx_diff_th(int diff_th)
{
	int ret;
	char buf[5] = {0};

	sprintf(buf, "%d", diff_th);

	ret = attr_config(CHX, "chx_diff_th", CAPT_WRITE, buf);
	if (ret < 0) {
		CAP_LOG_ERROR("%s\n", __func__);
	}
}

static int ctc_get_chx_diff_th(void)
{
	char buf[5] = {0};
	attr_config(CHX, "chx_diff_th", CAPT_READ, buf);
	return atoi(buf);
}

static void ctc_set_chx_abs_th(int abs_th)
{
	int ret;
	char buf[5] = {0};

	assert_param(abs_th < 4096);

	sprintf(buf, "%d", abs_th);
	ret = attr_config(CHX, "chx_abs_th", CAPT_WRITE, buf);
	if (ret < 0) {
		CAP_LOG_ERROR("%s\n", __func__);
	}
}

static int ctc_get_chx_abs_th(void)
{
	char buf[5] = {0};
	attr_config(CHX, "chx_abs_th", CAPT_READ, buf);
	return atoi(buf);
}

static void ctc_set_chx_n_noise_th(int n_noise_th)
{
	int ret;
	char buf[5] = {0};

	sprintf(buf, "%d", n_noise_th);

	ret = attr_config(CHX, "chx_n_noise_th", CAPT_WRITE, buf);
	if (ret < 0) {
		CAP_LOG_ERROR("%s\n", __func__);
	}
}

static int ctc_get_chx_n_noise_th(void)
{
	char buf[5] = {0};
	attr_config(CHX, "chx_n_noise_th", CAPT_READ, buf);
	return atoi(buf);
}

static void ctc_set_chx_p_noise_th(int p_noise_th)
{
	int ret;
	char buf[4] = {0};

	sprintf(buf, "%d", p_noise_th);

	ret = attr_config(CHX, "chx_p_noise_th", CAPT_WRITE, buf);
	if (ret < 0) {
		CAP_LOG_ERROR("%s\n", __func__);
	}
}

static int ctc_get_chx_p_noise_th(void)
{
	char buf[5] = {0};
	attr_config(CHX, "chx_n_noise_th", CAPT_READ, buf);
	return atoi(buf);
}

static int ctc_get_chx_ave_data(void)
{
	char buf[5] = {0};
	attr_config(CHX, "chx_ave_data", CAPT_READ, buf);
	return atoi(buf);
}

/**
  * @brief   Tune mbias
  * @param   channel, channel index 0~8
  * @retval  Mbias
  */
uint8_t ctc_tune_mbias(uint8_t channel, uint16_t mbias_tune_target, uint16_t mbias_tune_error)
{
	/* check the parameters */
	assert_param(IS_CT_CHANNEL(channel));
	assert_param(mbias_tune_target <= 4095);
	assert_param(mbias_tune_error <= 4095);

	uint32_t scan_period, etc_interval, delay_etc;
	uint8_t mbias = 0;
	uint32_t ch_data;

	/* enable specified channel */
	ctc_set_cur_chx(channel);
	ctc_set_chx_enable(1);

	/* get scan_period and etc_inetrval */
	scan_period = ctc_get_scan_interval();
	etc_interval = ctc_get_etc_interval();
	delay_etc = (etc_interval + 1) * scan_period;//ms

	ch_data = ctc_get_chx_ave_data();
	if (abs(mbias_tune_target - ch_data) > mbias_tune_error) {
		while (mbias < MBIAS_MAX_VALUE) {
			ctc_set_chx_mbias(mbias);
			usleep(delay_etc * 1000);

			ch_data = ctc_get_chx_ave_data();
			if (ch_data > mbias_tune_target) {
				if (ch_data - mbias_tune_target < mbias_tune_error) {
					break;
				} else {
					mbias--;
				}
			} else {
				if (mbias_tune_target - ch_data < mbias_tune_error) {
					break;
				} else {
					mbias++;
				}
			}
		}
	} else {
		mbias = (int)ctc_get_chx_mbias();
	}
	if (mbias >= MBIAS_MAX_VALUE) {
		CAP_LOG_ERROR("%s MBias auto-tune @CH%d failed!!!\r\n", __func__, channel);
	}

	return mbias;
}

void ctc_tune_ch_mbias(cap_ch_cfg_req_t *cfg, uint16_t target, uint16_t error)
{
	cfg->mbias = ctc_tune_mbias(cfg->ch, target, error);
}

void ctc_start(cap_start_req_t *req)
{
	int ch;

	for (ch = 0; ch < CAP_MAX_CH; ++ch) {
		ctc_set_cur_chx(ch);
		ctc_set_chx_enable((req->channels[ch] != 0) ? 1 : 0);
	}
	ctc_set_ctc_enable(1);
}

static void ctc_stop(void)
{
	int ch;

	for (ch = 0; ch < CAP_MAX_CH; ++ch) {
		ctc_set_cur_chx(ch);
		ctc_set_chx_enable(0);
	}

	ctc_set_ctc_enable(0);
}

void ctc_set_config(cap_cfg_req_t *cfg)
{
	ctc_set_scan_interval(cfg->scan_int);
	ctc_set_sample_num(cfg->avg_samp);
	ctc_set_debounce_times(cfg->deb_cnt);
	ctc_set_debounce_enable(cfg->deb_en);
	ctc_set_etc_enable(cfg->etc_en);
	ctc_set_etc_interval(cfg->etc_int);
	ctc_set_etc_factor(cfg->etc_factor);
	ctc_set_etc_step(cfg->etc_step);
}

void ctc_get_config(cap_cfg_req_t *cfg)
{
	cfg->scan_int = ctc_get_scan_interval();
	cfg->avg_samp = ctc_get_sample_num();
	cfg->etc_en = ctc_get_etc_enable();
	cfg->etc_int = ctc_get_etc_interval();
	cfg->etc_step = ctc_get_etc_step();
	cfg->etc_factor = ctc_get_etc_factor();
	cfg->deb_cnt = ctc_get_debounce_times();
	cfg->deb_en = ctc_get_debounce_enable();
	cfg->chip_name = CHIP_NAME;
}

void ctc_set_ch_config(cap_ch_cfg_req_t *cfg)
{
	ctc_set_cur_chx(cfg->ch);
	ctc_set_chx_mbias(cfg->mbias);
	ctc_set_chx_abs_th(cfg->abs_th);
	ctc_set_chx_diff_th(cfg->diff_th);
	ctc_set_chx_p_noise_th(cfg->pn_th);
	ctc_set_chx_n_noise_th(cfg->nn_th);
	ctc_set_chx_enable(cfg->enable);
}

void ctc_get_ch_config(cap_ch_cfg_req_t *cfg)
{
	ctc_set_cur_chx(cfg->ch);
	cfg->mbias = ctc_get_chx_mbias();
	cfg->enable = ctc_get_chx_enable();
	cfg->diff_th = ctc_get_chx_diff_th();
	cfg->abs_th = ctc_get_chx_abs_th();
	cfg->pn_th = ctc_get_chx_p_noise_th();
	cfg->nn_th = ctc_get_chx_n_noise_th();
}

void ctc_get_ch_rt_data(cap_ch_rt_data_t *data)
{
	ctc_set_cur_chx(data->ch);
	data->raw_data = ctc_get_chx_ave_data();
	data->baseline = ctc_get_chx_baseline();
	data->diff_data = (int)data->baseline - (int)data->raw_data;
	data->cap = 0; // Reserved
}

void ctc_reset_baseline(void)
{
	ctc_set_ctc_enable(0);
	ctc_set_ctc_enable(1);
}

