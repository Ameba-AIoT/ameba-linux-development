#ifndef CAP_UTIL_H
#define CAP_UTIL_H
#include "captouch_app.h"

void ctc_start(cap_start_req_t *req);
void ctc_get_config(cap_cfg_req_t *cfg);
void ctc_set_config(cap_cfg_req_t *cfg);
void ctc_set_cur_chx(int ch_num);
void ctc_get_ch_config(cap_ch_cfg_req_t *cfg);
void ctc_set_ch_config(cap_ch_cfg_req_t *cfg);
void ctc_get_ch_rt_data(cap_ch_rt_data_t *data);
int ctc_get_chx_mbias(void);
void ctc_tune_ch_mbias(cap_ch_cfg_req_t *cfg, uint16_t target, uint16_t error);
void ctc_reset_baseline(void);
uint16_t ctc_get_snr(void);

#endif /* CAP_UTIL_H */
