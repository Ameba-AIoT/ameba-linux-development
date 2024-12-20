#ifndef __HDLC_H__
#define __HDLC_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    HDLD_DECODE_COMPLETE = 0,
    HDLD_DECODE_PENDING = 1,
    HDLD_DECODE_NO_BUFS = 2,
    HDLD_DECODE_PARSE_ERR = 3,
} hdlc_decode_ret;

extern uint8_t decodeBuffer[256];

bool hdlc_encode(const uint8_t *aData, uint16_t aLength, uint8_t **encData, uint16_t *encLength);
void hdlc_decoder_reset(void);
uint16_t hdlc_decode(const uint8_t *aData, uint16_t aLength, hdlc_decode_ret *ret);

#ifdef __cplusplus
}
#endif

#endif