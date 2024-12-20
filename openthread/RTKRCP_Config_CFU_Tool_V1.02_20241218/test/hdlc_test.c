#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "CFUonHDLC.h"
#include "trace.h"

void main(void)
{
    uint8_t test_bin[61] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
                            0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13,
                            0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e,
                            0x7d, 0x7d, 0x7d, 0x7d, 0x7d, 0x7d, 0x7d, 0x7d, 0x7d, 0x7d,
                            0xf8, 0xf8, 0xf8, 0xf8, 0xf8, 0xf8, 0xf8, 0xf8, 0xf8, 0xf8,
                            0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0x2a, 0x2d,
                            0x2b
                           };

    uint8_t test_bin2[8] = { 0x7e, 0x85, 0x03, 0x83, 0x7a, 0xfa, 0x2a, 0x7e};
    uint8_t  *pencbuf;
    uint8_t decbuf[256];
    uint16_t enclen = 0;
    uint16_t declen = 0;
    uint16_t i = 0;

    hdlc_decode_ret ret;

    hdlc_encode(test_bin, sizeof(test_bin), &pencbuf, &enclen);
    log_prompt("encode data length is %d \n", enclen);
    for (i = 0; i < enclen; i++)
    {
        log_prompt("0x%02x ", pencbuf[i]);
    }
    log_prompt("\n");

    hdlc_decoder_reset();
    declen = hdlc_decode(pencbuf, enclen, &ret);
    if (ret == HDLD_DECODE_COMPLETE)
    {
        if (declen == sizeof(test_bin))
        {
            if (!memcmp(test_bin, decodeBuffer, 61))
            {
                log_prompt("parse successfully \n");
            }
            else
            {
                log_prompt(" parse error \n");
            }
        }
        else
        {
            log_prompt("declen %d is not %ld \n", declen, sizeof(test_bin));
        }
    }
    else
    {
        log_prompt("fail to parse the data \n");
    }

    hdlc_decoder_reset();
    declen = hdlc_decode(test_bin2, sizeof(test_bin2), &ret);
    if (ret == HDLD_DECODE_COMPLETE)
    {
        log_prompt("2 decode data length is %d \n", declen);
        for (i = 0; i < declen; i++)
        {
            log_prompt("0x%02x ", decodeBuffer[i]);
        }
        log_prompt("\n");
    }
    else
    {
        log_prompt("2 fail to parse the data \n");
    }

    hdlc_decoder_reset();
    declen = hdlc_decode(pencbuf, enclen, &ret);
    if (ret == HDLD_DECODE_COMPLETE)
    {
        if (declen == sizeof(test_bin))
        {
            if (!memcmp(test_bin, decodeBuffer, 61))
            {
                log_prompt("3 parse successfully \n");
            }
            else
            {
                log_prompt("3 parse error \n");
            }
        }
        else
        {
            log_prompt("3 declen %d is not %ld \n", declen, sizeof(test_bin));
        }
    }
    else
    {
        log_prompt("3 fail to parse the data \n");
    }
}