#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "trace.h"
#include "ttydevice.h"

#include "CFUonHDLC.h"

#define CONFIG_RESPONSE_DATA_LENGTH_MAX  512

//spinel header 1B + spinel cmd 1B + prop id 2B + data len 2B + sizeof(rtk_config_param)
#define READ_CONFIG_RESPONSE_DATA_LENGTH    34
//spinel header 1B + spinel cmd 1B + prop id 0 1B + status 1B
#define COMMON_CONFIG_RESPONSE_DATA_LENGTH    4

//spinel header 1B + spinel cmd 1B + prop id 2B + data len 2B + sizeof(rtk_config_param)
#define WRITE_CONFIG_COMMAND_DATA_LENGTH    34
//spinel header 1B + spinel cmd 1B + prop id 2B
#define READ_CONFIG_COMMAND_DATA_LENGTH     4
//spinel header 1B + spinel cmd 1B + prop id 2B + data len 2B + flowctl enable 1B
#define FLOWCTL_CONFIG_COMMAND_DATA_LENGTH  7
//spinel header 1B + spinel cmd 1B + prop id 2B + data len 2B + flowctl enable 1B
#define REBOOT_CONFIG_COMMAND_DATA_LENGTH   7

#define SPINEL_HEADER_FLAG 0x80
#define SPINEL_HEADER_IID_SHIFT 4
#define SPINEL_HEADER_IID(iid) ((uint8_t)((iid) << SPINEL_HEADER_IID_SHIFT))

#define SPINEL_PROP_VENDOR_RTK_CONFIG_READ      0x3c0a
#define SPINEL_PROP_VENDOR_RTK_CONFIG_WRITE     0x3c0b
#define SPINEL_PROP_VENDOR_RTK_FLOW_CONTROL     0x3c0c
#define SPINEL_PROP_VENDOR_RTK_REBOOT           0x3c0d

enum
{
    SPINEL_CMD_NOOP = 0,
    SPINEL_CMD_RESET = 1,
    SPINEL_CMD_PROP_VALUE_GET = 2,
    SPINEL_CMD_PROP_VALUE_SET = 3,
    SPINEL_CMD_PROP_VALUE_INSERT = 4,
    SPINEL_CMD_PROP_VALUE_REMOVE = 5,
    SPINEL_CMD_PROP_VALUE_IS = 6,
    SPINEL_CMD_PROP_VALUE_INSERTED = 7,
    SPINEL_CMD_PROP_VALUE_REMOVED = 8,
};

typedef struct _rtk_config_param
{
    uint32_t sign;
    uint32_t vid: 16;
    uint32_t pid: 16;
    uint32_t tx_pin: 8;
    uint32_t rx_pin: 8;
    uint32_t rts_pin: 8;
    uint32_t cts_pin: 8;
    uint32_t func_msk: 8;
    uint32_t baud_rate: 24;
    uint32_t pta_dis: 8;
    uint32_t pta_wl_act: 8;
    uint32_t pta_bt_act: 8;
    uint32_t pta_bt_stat: 8;
    uint32_t pta_bt_clk: 8;
    uint32_t ext_pa: 8;
    uint32_t ext_lna: 8;
    uint32_t ext_pa_lna_ploatiry: 8;
    uint32_t ic_type: 16;
    uint32_t reserved: 16;
} __attribute__((packed)) rtk_config_param;

rtk_config_param config_param;
char RcpConfigBin[512] = {'\0'};
int RcpConfigBin_fd = -1;

static bool flowctl_enable = false; //must be false as init
static bool parse_file_ok = false;

static uint8_t configResponse[CONFIG_RESPONSE_DATA_LENGTH_MAX];
static uint16_t configResponseLen;
static bool getConfigReponseHandle(uint8_t *buff, uint16_t len)
{
    uint16_t declen = 0;

    //log_prompt("%s(): uart receive: %d\n", __func__, len);
    log_debug("%s(): uart receive: %d",  __func__, len);

    //HDLC decode
    {
        hdlc_decode_ret ret = HDLD_DECODE_PENDING;
        declen = hdlc_decode(buff, len, &ret);

        if (ret != HDLD_DECODE_COMPLETE)
        {
            return false;
        }
    }

    memcpy(configResponse, decodeBuffer, declen);
    configResponseLen = declen;
    //debug
    {
        char recv_s[512] = {'\0'};
        int n = 0;
        int count = 0;

        n = snprintf(recv_s, sizeof(recv_s) - 1, "%s(): uart receive: ", __func__);
        count += n;
        for (uint8_t i = 0; i < configResponseLen; i++)
        {
            n = snprintf(&recv_s[count], sizeof(recv_s) - 1 - count, " %02x ", configResponse[i]);
            count += n;
        }
        //log_prompt("%s(): %s\n", __func__, recv_s);
        log_debug("%s", recv_s);

    }

    return true;
}

static bool ConfigRequest(uint8_t *cmdBuffer, uint16_t cmdLen, uint8_t *responseBuffer,
                          uint16_t *responseLen)
{
    int err = 0;

    uint8_t *aFrame = cmdBuffer;
    uint16_t aLength = cmdLen;

    //debug
    char send_s[2048] = {'\0'};
    char recv_s[512] = {'\0'};
    int n = 0;
    int count = 0;

    if (!cmdBuffer)
    {
        log_prompt("%s(): the pointer of cmdBuffer is NULL \n", __func__);
        log_err("%s(): the pointer of cmdBuffer is NULL \n", __func__);
        return false;
    }

    if (!responseBuffer)
    {
        log_prompt("%s(): the pointer of responseBuffer is NULL \n", __func__);
        log_err("%s(): the pointer of responseBuffer is NULL \n", __func__);
        return false;
    }

    if (!responseLen)
    {
        log_prompt("%s(): the pointer of responseLen is NULL \n", __func__);
        log_err("%s(): the pointer of responseLen is NULL \n", __func__);
        return false;
    }

    //debug tx
    {
        n = 0;
        count = 0;
        log_debug("%s(): uart send original data(%d):",  __func__, aLength);

        for (uint16_t i = 0; i < aLength; i++)
        {
            n = snprintf(&send_s[count], sizeof(send_s) - 1 - count, " %02x ", aFrame[i]);
            count += n;
        }
        log_debug("%s() original: %s \n",  __func__, send_s);
    }

    //HDLC encode
    {
        uint8_t *pEncData = NULL;
        uint16_t encDataLen = 0;
        bool ret;
        ret = hdlc_encode(cmdBuffer, cmdLen, &pEncData, &encDataLen);
        if (ret)
        {
            aLength = encDataLen;
            aFrame = pEncData;
        }
        else
        {
            log_prompt("%s(): fail to decode data \n", __func__);
            log_err("%s(): fail to decode data \n", __func__);
            exit(1);
        }
    }

    if (ttyWrite(aFrame, aLength) !=  0)
    {
        log_err("%s(): failed to send uart data", __func__);
        log_prompt("%s(): failed to send uart data", __func__);
        return false;
    }

    hdlc_decoder_reset();
    err = ttyWaitForFrame(1000000, getConfigReponseHandle); // 1s
    if (err != 0)
    {
        log_err("error %d happened while waiting for GetVerison Command Response Report", err);
        return false;
    }

    //debug rx
    {
        n = 0;
        count = 0;
        log_debug("%s(): uart recv original data(%d):",  __func__, aLength);

        for (uint8_t i = 0; i < configResponseLen; i++)
        {
            n = snprintf(&recv_s[count], sizeof(recv_s) - 1 - count, " %02x ", configResponse[i]);
            count += n;
        }
        log_debug("%s() original: %s \n",  __func__, recv_s);
    }

    if (*responseLen < configResponseLen)
    {
        return false;
    }
    memcpy(responseBuffer, configResponse, configResponseLen);
    *responseLen = configResponseLen;

    return true;
}

static bool getRedundantDataHandle(uint8_t *buff, uint16_t len)
{
    return false;
}

static void readtoEmpty(uint64_t aTimeoutUs)
{
    ttyWaitForFrame(aTimeoutUs, getRedundantDataHandle); // 100ms
}

static int spinel_packed_uint_size(unsigned int value)
{
    int ret;

    if (value < (1 << 7))
    {
        ret = 1;
    }
    else if (value < (1 << 14))
    {
        ret = 2;
    }
    else if (value < (1 << 21))
    {
        ret = 3;
    }
    else if (value < (1 << 28))
    {
        ret = 4;
    }
    else
    {
        ret = 5;
    }

    return ret;
}

static int spinel_packed_uint_encode(uint8_t *bytes, unsigned int len, unsigned int value)
{
    const int encoded_size = spinel_packed_uint_size(value);

    if ((int)len >= encoded_size)
    {
        int i;

        for (i = 0; i != encoded_size - 1; ++i)
        {
            *bytes++ = (value & 0x7F) | 0x80;
            value    = (value >> 7);
        }

        *bytes++ = (value & 0x7F);
    }

    return encoded_size;
}

static bool rtkrcp_config_check(bool *is_config_same)
{
    uint8_t tid = 5 & 0x0f;
    uint8_t CmdBuff[READ_CONFIG_COMMAND_DATA_LENGTH];

    uint8_t RespBuff[READ_CONFIG_RESPONSE_DATA_LENGTH];
    uint16_t RespBuffLen = sizeof(RespBuff);

    if (!is_config_same)
    {
        log_prompt("%s(): the pointer of is_config_same is NULL \n", __func__);
        log_err("%s(): the pointer of is_config_same is NULL \n", __func__);
        return false;
    }

    CmdBuff[0] = SPINEL_HEADER_FLAG | SPINEL_HEADER_IID(0) | tid;
    CmdBuff[1] = SPINEL_CMD_PROP_VALUE_GET;
    spinel_packed_uint_encode(&CmdBuff[2], 2, SPINEL_PROP_VENDOR_RTK_CONFIG_READ);

    if (!ConfigRequest(CmdBuff, sizeof(CmdBuff), RespBuff, &RespBuffLen))
    {
        return false;
    }

    if (RespBuffLen == READ_CONFIG_RESPONSE_DATA_LENGTH &&
        RespBuff[0] == CmdBuff[0] && RespBuff[1] == SPINEL_CMD_PROP_VALUE_IS &&
        RespBuff[2] == CmdBuff[2] && RespBuff[3] == CmdBuff[3] &&
        RespBuff[4] == sizeof(config_param) && RespBuff[5] == 00)
    {
        rtk_config_param *p = (rtk_config_param *)&RespBuff[6];

        if (parse_file_ok)
        {
            if (config_param.ic_type != p->ic_type)
            {
                log_prompt("%s(): ConfigBin is not for this RCP \n", __func__);
                log_err("%s(): ConfigBin is not for this RCP \n", __func__);
                exit(255);
            }
            if (config_param.vid != p->vid || config_param.pid != p->pid ||
                config_param.func_msk != p->func_msk || config_param.baud_rate != p->baud_rate ||
                config_param.pta_dis != p->pta_dis || config_param.ext_pa != p->ext_pa ||
                config_param.ext_lna != p->ext_lna || config_param.ext_pa_lna_ploatiry != p->ext_pa_lna_ploatiry)
            {
                *is_config_same = false;
            }
            else
            {
                //if this config support UART flowctl, we should disable flowctl and reset RCP config
                if ((config_param.func_msk & 0x01) == 0)
                {
                    *is_config_same = false;
                }
                else
                {
                    *is_config_same = true;
                }
            }
        }
        else
        {
            //if there is no ConfigBin in the host
            //load config from the RCP
            //if this config support UART flowctl, we should disable flowctl and reset RCP config
            memcpy(&config_param, p, sizeof(rtk_config_param));
            if ((config_param.func_msk & 0x01) == 0)
            {
                *is_config_same = false;
            }
            else
            {
                *is_config_same = true;
            }
        }
    }
    else
    {
        *is_config_same = false;
    }
    return true;
}

static bool rtkrcp_config_read(rtk_config_param *param)
{
    uint8_t tid = 5 & 0x0f;
    uint8_t CmdBuff[READ_CONFIG_COMMAND_DATA_LENGTH];

    uint8_t RespBuff[READ_CONFIG_RESPONSE_DATA_LENGTH];
    uint16_t RespBuffLen = sizeof(RespBuff);

    if (!param)
    {
        log_prompt("%s(): the pointer of param is NULL \n", __func__);
        log_err("%s(): the pointer of param is NULL \n", __func__);
        return false;
    }

    CmdBuff[0] = SPINEL_HEADER_FLAG | SPINEL_HEADER_IID(0) | tid;
    CmdBuff[1] = SPINEL_CMD_PROP_VALUE_GET;
    spinel_packed_uint_encode(&CmdBuff[2], 2, SPINEL_PROP_VENDOR_RTK_CONFIG_READ);

    if (!ConfigRequest(CmdBuff, sizeof(CmdBuff), RespBuff, &RespBuffLen))
    {
        return false;
    }

    if (RespBuffLen == READ_CONFIG_RESPONSE_DATA_LENGTH &&
        RespBuff[0] == CmdBuff[0] && RespBuff[1] == SPINEL_CMD_PROP_VALUE_IS &&
        RespBuff[2] == CmdBuff[2] && RespBuff[3] == CmdBuff[3] &&
        RespBuff[4] == sizeof(config_param) && RespBuff[5] == 00)
    {
        rtk_config_param *p = (rtk_config_param *)&RespBuff[6];
        memcpy(param, p, sizeof(rtk_config_param));
    }
    else
    {
        log_prompt("%s(): unexpected Read Config Response \n", __func__);
        log_err("%s(): unexpected Read Config Response \n", __func__);
        return false;
    }
    return true;
}

static bool rtkrcp_config_write(bool *response_ok)
{
    uint8_t tid = 5 & 0x0f;
    uint8_t CmdBuff[WRITE_CONFIG_COMMAND_DATA_LENGTH];

    uint8_t RespBuff[COMMON_CONFIG_RESPONSE_DATA_LENGTH];
    uint16_t RespBuffLen = sizeof(RespBuff);

    if (!response_ok)
    {
        log_prompt("%s(): the pointer of response_ok is NULL \n", __func__);
        log_err("%s(): the pointer of response_ok is NULL \n", __func__);
        return false;
    }

    CmdBuff[0] = SPINEL_HEADER_FLAG | SPINEL_HEADER_IID(0) | tid;
    CmdBuff[1] = SPINEL_CMD_PROP_VALUE_SET;
    spinel_packed_uint_encode(&CmdBuff[2], 2, SPINEL_PROP_VENDOR_RTK_CONFIG_WRITE);
    CmdBuff[4] = sizeof(config_param);
    CmdBuff[5] = 00;
    memcpy(&CmdBuff[6], (uint8_t *)&config_param, sizeof(config_param));

    if (!ConfigRequest(CmdBuff, sizeof(CmdBuff), RespBuff, &RespBuffLen))
    {
        return false;
    }
    if (RespBuffLen == COMMON_CONFIG_RESPONSE_DATA_LENGTH &&
        RespBuff[0] == CmdBuff[0] && RespBuff[1] == SPINEL_CMD_PROP_VALUE_IS &&
        RespBuff[2] == 00)
    {
        if (RespBuff[3] == 00)
        {
            *response_ok = true;
        }
        else
        {
            *response_ok = false;
        }
    }
    else
    {
        *response_ok = false;
    }
    return true;
}

static bool rtkrcp_flowctl_set(uint8_t flowctl_enable, bool *response_ok)
{
    uint8_t tid = 5 & 0x0f;
    uint8_t CmdBuff[FLOWCTL_CONFIG_COMMAND_DATA_LENGTH];

    uint8_t RespBuff[COMMON_CONFIG_RESPONSE_DATA_LENGTH];
    uint16_t RespBuffLen = sizeof(RespBuff);

    if (!response_ok)
    {
        log_prompt("%s(): the pointer of response_ok is NULL \n", __func__);
        log_err("%s(): the pointer of response_ok is NULL \n", __func__);
        return false;
    }

    CmdBuff[0] = SPINEL_HEADER_FLAG | SPINEL_HEADER_IID(0) | tid;
    CmdBuff[1] = SPINEL_CMD_PROP_VALUE_SET;
    spinel_packed_uint_encode(&CmdBuff[2], 2, SPINEL_PROP_VENDOR_RTK_FLOW_CONTROL);
    CmdBuff[4] = sizeof(uint8_t);
    CmdBuff[5] = 00;
    CmdBuff[6] = flowctl_enable;

    if (!ConfigRequest(CmdBuff, sizeof(CmdBuff), RespBuff, &RespBuffLen))
    {
        return false;
    }
    if (RespBuffLen == COMMON_CONFIG_RESPONSE_DATA_LENGTH &&
        RespBuff[0] == CmdBuff[0] && RespBuff[1] == SPINEL_CMD_PROP_VALUE_IS &&
        RespBuff[2] == 00)
    {
        if (RespBuff[3] == 00)
        {
            *response_ok = true;
        }
        else
        {
            *response_ok = false;
        }
    }
    else
    {
        *response_ok = false;
    }
    return true;
}

static bool rtkrcp_reboot_set(uint8_t reboot, bool *response_ok)
{
    uint8_t tid = 5 & 0x0f;
    uint8_t CmdBuff[REBOOT_CONFIG_COMMAND_DATA_LENGTH];

    uint8_t RespBuff[COMMON_CONFIG_RESPONSE_DATA_LENGTH];
    uint16_t RespBuffLen = sizeof(RespBuff);

    if (!response_ok)
    {
        log_prompt("%s(): the pointer of response_ok is NULL \n", __func__);
        log_err("%s(): the pointer of response_ok is NULL \n", __func__);
        return false;
    }

    CmdBuff[0] = SPINEL_HEADER_FLAG | SPINEL_HEADER_IID(0) | tid;
    CmdBuff[1] = SPINEL_CMD_PROP_VALUE_SET;
    spinel_packed_uint_encode(&CmdBuff[2], 2, SPINEL_PROP_VENDOR_RTK_REBOOT);
    CmdBuff[4] = sizeof(uint8_t);
    CmdBuff[5] = 00;
    CmdBuff[6] = reboot;

    if (!ConfigRequest(CmdBuff, sizeof(CmdBuff), RespBuff, &RespBuffLen))
    {
        return false;
    }
    if (RespBuffLen == COMMON_CONFIG_RESPONSE_DATA_LENGTH &&
        RespBuff[0] == CmdBuff[0] && RespBuff[1] == SPINEL_CMD_PROP_VALUE_IS &&
        RespBuff[2] == 00)
    {
        if (RespBuff[3] == 00)
        {
            *response_ok = true;
        }
        else
        {
            *response_ok = false;
        }
    }
    else
    {
        *response_ok = false;
    }
    return true;
}

bool rtkrcp_config_parse()
{
    int ret;
    rtk_config_param config_param_temp;
    if (!access(RcpConfigBin, F_OK))
    {
        RcpConfigBin_fd = open(RcpConfigBin, O_RDONLY);
        if (RcpConfigBin_fd == -1)
        {
            log_err("%s: open %s error, %s", __func__, RcpConfigBin, strerror(errno));
            parse_file_ok = false;
            return false;
        }
        ret = read(RcpConfigBin_fd, &config_param_temp, sizeof(rtk_config_param));
        if (ret == -1)
        {
            log_err("%s: read error, %s", __func__, strerror(errno));
            close(RcpConfigBin_fd);
            parse_file_ok = false;
            return false;
        }
        if (ret == sizeof(rtk_config_param))
        {
            memcpy(&config_param, &config_param_temp, sizeof(rtk_config_param));
            log_info("%s: read RcpConfig Bin successfully, baudrate %d", __func__, config_param.baud_rate);
            log_prompt("%s: read RcpConfig Bin successfully, baudrate %d \n", __func__, config_param.baud_rate);
        }
        else
        {
            log_info("%s: RcpConfig Bin is not expected", __func__);
            log_prompt("%s: RcpConfig Bin is not expected \n", __func__);
        }
        close(RcpConfigBin_fd);
        parse_file_ok = true;
        return true;
    }
    log_warn("%s: fail to access %s", __func__, RcpConfigBin);
    parse_file_ok = false;
    return false;
}

bool rtkrcp_config()
{
    bool ret = false;
    bool is_config_same = false;
    bool response_ok = false;
    int baudrate = 912600;
    bool flowctl_tmp = false;
    int baudrate_table[] = {921600,
                            115200, 230400, 460800, 921600, 1000000, 2000000,
                            115200, 230400, 460800, 921600, 1000000, 2000000,
                            115200, 230400, 460800, 921600, 1000000, 2000000,
                           };
    uint8_t baudrate_count = sizeof(baudrate_table) / sizeof(baudrate_table[0]);
    uint8_t i = 0;
    rtk_config_param param;


    log_prompt("%s(): Config Func V1.02 (20241218) \n", __func__);
    log_err("%s(): Config Func V1.02 (20241218) \n", __func__);

    if (parse_file_ok)
    {
        //set uart parameters as Config.bin
        baudrate = (int)config_param.baud_rate;
        ttydevice_baud_set(baudrate);

        do
        {
            //try flowctl disable
            flowctl_tmp = false;
            ttydevice_flowctl_set(flowctl_tmp);
            if (device_open() == -1)
            {
                log_prompt("%s(): 0 fail to open ttydevice \n", __func__);
                log_err("%s(): 0 fail to open ttydevice \n", __func__);
                ret = false;
            }
            else
            {
                usleep(20000); //20ms
                rtkrcp_config_read(&param);
                readtoEmpty(100000);
                if (rtkrcp_config_check(&is_config_same))
                {
                    ret = true;
                    break;
                }
                else
                {
                    device_close();
                    ret = false;
                }
            }

            //try flowctl enable
            flowctl_tmp = true;
            ttydevice_flowctl_set(flowctl_tmp);
            if (device_open() == -1)
            {
                log_prompt("%s(): 1 fail to open ttydevice \n", __func__);
                log_err("%s(): 1 fail to open ttydevice \n", __func__);
                ret = false;
            }
            else
            {
                usleep(20000); //20ms
                rtkrcp_config_read(&param);
                readtoEmpty(100000);
                if (rtkrcp_config_check(&is_config_same))
                {
                    ret = true;
                    break;
                }
                else
                {
                    device_close();
                    ret = false;
                }
            }
        }
        while (0);
    }

    if (ret == false)
    {
        // try to hit uart baudrate
        for (i = 0; i < baudrate_count; i++)
        {
            baudrate = baudrate_table[i];
            ttydevice_baud_set(baudrate);

            //flowctl disabled
            flowctl_tmp = false;
            ttydevice_flowctl_set(flowctl_tmp);
            usleep(20000); //20ms
            if (device_open() == -1)
            {
                log_prompt("%s(): fail to open ttydevice \n", __func__);
                log_err("%s(): fail to open ttydevice \n", __func__);
                return false;
            }
            usleep(20000); //20ms
            rtkrcp_config_read(&param);
            readtoEmpty(100000);
            if (rtkrcp_config_check(&is_config_same))
            {
                break;
            }
            else
            {
                device_close();
            }

            //flowctl enabled
            flowctl_tmp = true;
            ttydevice_flowctl_set(flowctl_tmp);
            usleep(20000); //20ms
            if (device_open() == -1)
            {
                log_prompt("%s(): fail to open ttydevice \n", __func__);
                log_err("%s(): fail to open ttydevice \n", __func__);
                return false;
            }
            usleep(20000); //20ms
            rtkrcp_config_read(&param);
            readtoEmpty(100000);
            if (rtkrcp_config_check(&is_config_same))
            {
                break;
            }
            else
            {
                device_close();
            }
        }
        if (i == baudrate_count)
        {
            log_prompt("%s(): fail to hit UART baudrate\n", __func__);
            log_err("%s(): fail to hit UART baudrate\n", __func__);
            exit(254);
            return false;
        }
    }

    log_prompt("%s(): uart baudrate is %d bps, flowctl %s \n", __func__, baudrate,
               flowctl_tmp ? "enable" : "disable");
    log_info("%s(): uart baudrate is %d bps, flowctl %s \n", __func__, baudrate,
             flowctl_tmp ? "enable" : "disable");

    if (is_config_same == true)
    {
        log_prompt("%s(): no need to set UART config \n", __func__);
        log_info("%s(): no need to set UART config \n", __func__);
        device_close();
        return true;
    }

    if ((config_param.func_msk & 0x01) == 0)
    {
        flowctl_enable = true;
    }
    else
    {
        flowctl_enable = false;
    }
    //in this phase, force flowctl disable
    config_param.func_msk |= 0x01;

    readtoEmpty(100000);
    if (!rtkrcp_config_write(&response_ok) || !response_ok)
    {
        log_prompt("%s(): fail to write config to ttydevice \n", __func__);
        log_err("%s(): fail to write config to ttydevice \n", __func__);
        device_close();
        return false;
    }

    //no response
    //rtkrcp_reboot_set(1, &response_ok);

    log_prompt("%s(): write config successfully, wait rcp reboot \n", __func__);
    log_info("%s(): write config successfully, wait rcp reboot \n", __func__);
    device_close();
    usleep(500000); //400ms

    //will reboot and no flowctl after write config
    ttydevice_baud_set((int)config_param.baud_rate);
    ttydevice_flowctl_set(false);
    if (device_open() == -1)
    {
        log_prompt("%s(): fail to reopen ttydevice \n", __func__);
        log_err("%s(): fail to reopen ttydevice \n", __func__);
        return false;
    }
    usleep(20000); //20ms
    readtoEmpty(200000);
    if (!rtkrcp_config_check(&is_config_same))
    {
        log_prompt("%s(): 2 fail to send  read_config cmd \n", __func__);
        log_err("%s(): 2 fail to send  read_config cmd \n", __func__);
        device_close();
        return false;
    }
    if (is_config_same != true)
    {
        log_prompt("%s(): fail to change UART config \n", __func__);
        log_err("%s(): fail to change UART config \n", __func__);
        device_close();
        return false;
    }
    else
    {
        log_prompt("%s(): config successfully \n", __func__);
        log_err("%s(): config successfully \n", __func__);
    }

    device_close();

    //back to original config load from Host's ConfigBin or RCP response data
    if (flowctl_enable)
    {
        config_param.func_msk &= 0xfe;
    }
    else
    {
        config_param.func_msk |= 0x01;
    }

    return true;
}

bool rtkrcp_config_flowctl()
{
    bool response_ok = true;
    if (rtkrcp_usb_interface_check())
    {
        log_prompt("%s(): there is no flowctl of USB interface to set \n", __func__);
        log_info("%s(): there is no flowctl of USB interface to set \n", __func__);
        return true;
    }

    if (flowctl_enable)
    {
        if (device_open() == -1)
        {
            log_prompt("%s(): fail to open ttydevice \n", __func__);
            log_err("%s(): fail to open ttydevice \n", __func__);
            return false;
        }
        if (rtkrcp_flowctl_set(!!flowctl_enable, &response_ok))
        {
            if (response_ok == false)
            {
                log_prompt("%s(): fail to enable/disable flowctl \n", __func__);
                log_err("%s(): fail to enable/disable flowctl \n", __func__);
                device_close();
                return false;
            }
        }
        else
        {
            log_prompt("%s(): fail to send flowctl_set cmd \n", __func__);
            log_err("%s(): fail to send flowctl_set cmd \n", __func__);
            device_close();
            return false;
        }
        log_prompt("%s(): set flowctl to %d \n", __func__, !!flowctl_enable);
        log_err("%s(): set flowctl to %d \n", __func__, !!flowctl_enable);
        ttydevice_flowctl_set(flowctl_enable);
        device_close();
        usleep(250000); //200ms
    }
    else
    {
        log_prompt("%s(): no need to enable UART flowctl \n", __func__);
        log_info("%s(): no need to enable UART flowctl \n", __func__);
    }

    return true;
}

#define TEST_FOR_CONFIG_CFU_FLOWCTL 1
bool test_after_config_cfu_flowctl()
{
#if TEST_FOR_CONFIG_CFU_FLOWCTL
    bool is_config_same = false;
    rtk_config_param param;

    memset(&param, 0, sizeof(rtk_config_param));
    usleep(100000); //100ms
    if (device_open() == -1)
    {
        log_prompt("%s(): 2 fail to open ttydevice \n", __func__);
        log_err("%s(): 2 fail to open ttydevice \n", __func__);
        return false;
    }
    usleep(100000); //100ms
    if (!rtkrcp_config_read(&param))
    {
        log_prompt("%s(): 2 fail to send  read_config cmd \n", __func__);
        log_err("%s(): 2 fail to send  read_config cmd \n", __func__);
        device_close();
        return false;
    }
    else
    {
        if (config_param.vid != param.vid || config_param.pid != param.pid ||
            config_param.func_msk != param.func_msk || config_param.baud_rate != param.baud_rate ||
            config_param.pta_dis != param.pta_dis || config_param.ext_pa != param.ext_pa ||
            config_param.ext_lna != param.ext_lna ||
            config_param.ext_pa_lna_ploatiry != param.ext_pa_lna_ploatiry)
        {
            log_prompt("%s(): fail to Config \n", __func__);
            log_err("%s(): fail to Config \n", __func__);
        }
        else
        {
            log_prompt("%s(): Config successfully \n", __func__);
            log_info("%s(): Config successfully \n", __func__);
        }
    }
    device_close();
#endif
    return true;
}