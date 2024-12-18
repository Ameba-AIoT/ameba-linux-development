#include <ctype.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>

#include "trace.h"
#include "ttydevice.h"

#ifndef FIXED_DEVICE_CONFIG
#define FIXED_DEVICE_CONFIG 0
#endif

#define DEVICE_NODE_PATH_LEAN_MAX 48
#define kMaxWaitTime 2000 // ms

static char *usbinterfaceName1 = "ttyACM";
static char *usbinterfaceName2 = "ttyRTKRCP";

typedef enum
{
    NO_ERR,
    ERR_TIMEOUT,
    ERR_DISCONN,
    ERR_UNEXPECTED,
    ERR_UNKNOWN,
} T_EXEC_RESULT;

struct CfuTTYDeviceConfiguration
{
    char  device_node[DEVICE_NODE_PATH_LEAN_MAX];
    int baudrate;
    char parity[8];
    int flowctl;
    int stopbit;
};
struct CfuTTYDeviceConfiguration ttydevice = {"/dev/ttyUSB0", 921600, "none", 0, 1};
char ttyConfigPath[256] = {'\0'};
int ttyfd = -1;

static char *trim(char *str)
{
    while (isspace(*str))
    {
        ++str;
    }

    if (!*str)
    {
        return str;
    }

    char *end_str = str + strlen(str) - 1;
    while (end_str > str && isspace(*end_str))
    {
        --end_str;
    }

    end_str[1] = '\0';
    return str;
}

bool ttyGetDeviceName(char dev_name[48], uint16_t *len)
{
    if (*len < strlen(ttydevice.device_node) + 1)
    {
        return false;
    }
    memcpy(dev_name, ttydevice.device_node, strlen(ttydevice.device_node) + 1);
    *len = strlen(ttydevice.device_node) + 1;
    return true;
}

bool ttydevice_baud_set(int baudrate)
{
    ttydevice.baudrate = baudrate;

    log_info("device %s", ttydevice.device_node);
    log_info("baudrate %d", ttydevice.baudrate);
    log_info("parity %s", ttydevice.parity);
    log_info("flowctl %d", ttydevice.flowctl);
    log_info("stopbit %d", ttydevice.stopbit);

    return true;
}

bool ttydevice_flowctl_set(bool enable)
{
    ttydevice.flowctl = (int)enable;

    log_info("device %s", ttydevice.device_node);
    log_info("baudrate %d", ttydevice.baudrate);
    log_info("parity %s", ttydevice.parity);
    log_info("flowctl %d", ttydevice.flowctl);
    log_info("stopbit %d", ttydevice.stopbit);

    return true;
}

bool parse_device_config(char *configPath)
{
    FILE *fconfig;
    char line[512];
    int line_num = 0;
    struct CfuTTYDeviceConfiguration ttydevice_tmp;
    if (!configPath)
    {
        log_warn("use defalut uart config.");
        log_prompt("use defalut uart config.");
        return false;
    }

    fconfig = fopen(configPath, "r");
    if (!fconfig)
    {
        goto error;
    }

    memcpy(&ttydevice_tmp, &ttydevice, sizeof(ttydevice));
    while (fgets(line, 512, fconfig))
    {
        char *line_ptr = trim(line);
        line_num ++;
        // Skip blank and comment lines.
        if (*line_ptr == '\0' || *line_ptr == '#')
        {
            continue;
        }

        char *split = strchr(line_ptr, '=');
        if (!split)
        {
            log_warn("%s no key/value separator found on line %d.", __func__, line_num);
            return false;
        }
        *split = '\0';

        const char *tag =  trim(line_ptr);
        const char *value = trim(split + 1);

        if (!strcmp(tag, "device"))
        {
            if (strlen(tag) + 1 <= DEVICE_NODE_PATH_LEAN_MAX)
            {
                strcpy(ttydevice_tmp.device_node, value);
            }
            else
            {
                log_warn("%s(): fail to parse device.", __func__);
                goto error;
            }
        }
        else if (!strcmp(tag, "baudrate"))
        {
            ttydevice_tmp.baudrate = strtoul(value, NULL, 10);
        }
        else if (!strcmp(tag, "parity"))
        {
            if (!strcmp(value, "odd") ||
                !strcmp(value, "even") ||
                !strcmp(value, "none"))
            {
                strcpy(ttydevice_tmp.parity, value);
            }
            else
            {
                log_warn("%s(): fail to parse parity.", __func__);
                goto error;
            }
        }
        else if (!strcmp(tag, "flowctl"))
        {
            if (!strcmp(value, "enable"))
            {
                ttydevice_tmp.flowctl = 1;
            }
            else if (!strcmp(value, "disable"))
            {
                ttydevice_tmp.flowctl = 0;
            }
            else
            {
                log_warn("%s(): fail to parse flowctl.", __func__);
                goto error;
            }
        }
        else if (!strcmp(tag, "stopbit"))
        {
            if (!strcmp(value, "1") ||
                !strcmp(value, "2"))
            {
                ttydevice_tmp.stopbit = strtoul(value, NULL, 10);
            }
            else
            {
                log_warn("%s(): fail to parse stopbit.", __func__);
                goto error;
            }
        }
        else if (!strcmp(tag, "ConfigBin"))
        {
            extern char RcpConfigBin[512];
            strcpy(RcpConfigBin, value);
            log_warn("RcpConfigBin: %s.", RcpConfigBin);
            log_prompt("RcpConfigBin: %s. \n", RcpConfigBin);
        }
    }
    memcpy(&ttydevice, &ttydevice_tmp, sizeof(ttydevice));

    log_info("device %s", ttydevice.device_node);
    log_info("baudrate %d", ttydevice.baudrate);
    log_info("parity %s", ttydevice.parity);
    log_info("flowctl %d", ttydevice.flowctl);
    log_info("stopbit %d", ttydevice.stopbit);
    fclose(fconfig);
    return true;

error:
    log_info("use default device config");
    log_info("device %s", ttydevice.device_node);
    log_info("baudrate %d", ttydevice.baudrate);
    log_info("parity %s", ttydevice.parity);
    log_info("flowctl %d", ttydevice.flowctl);
    log_info("stopbit %d", ttydevice.stopbit);
    return false;
}

int device_open()
{
    int fd   = -1;
    int rval = 0;

    fd = open(ttydevice.device_node, O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
    if (fd == -1)
    {
        log_err("fail to open device %s, errno %d(%s)", ttydevice.device_node, errno, strerror(errno));
        return -1;
    }

    if (isatty(fd))
    {
        struct termios tios;
        speed_t        speed;

        int      stopBit  = 1;
        uint32_t baudrate = 115200;

        if ((rval = tcflush(fd, TCIOFLUSH)) != 0)
        {
            log_err("fail to flush the device 1 %s, errno %d(%s)", ttydevice.device_node, errno,
                    strerror(errno));
            goto exit;
        }

        rval = tcgetattr(fd, &tios);
        if (rval != 0)
        {
            log_err("fail to get attribute of the device %s, errno %d(%s)", ttydevice.device_node, errno,
                    strerror(errno));
            goto exit;
        }

        cfmakeraw(&tios);

        tios.c_cflag = CS8 | HUPCL | CREAD | CLOCAL;


        if (strncmp(ttydevice.parity, "odd", 3) == 0)
        {
            tios.c_cflag |= PARENB;
            tios.c_cflag |= PARODD;
        }
        else if (strncmp(ttydevice.parity, "even", 4) == 0)
        {
            tios.c_cflag |= PARENB;
        }
        else
        {
            tios.c_cflag &= ~PARENB;
        }

        stopBit = ttydevice.stopbit;

        switch (stopBit)
        {
        case 1:
            tios.c_cflag &= (unsigned long)(~CSTOPB);
            break;
        case 2:
            tios.c_cflag |= CSTOPB;
            break;
        default:
            exit(2);
            break;
        }

        baudrate = ttydevice.baudrate;

        switch (baudrate)
        {
        case 9600:
            speed = B9600;
            break;
        case 19200:
            speed = B19200;
            break;
        case 38400:
            speed = B38400;
            break;
        case 57600:
            speed = B57600;
            break;
        case 115200:
            speed = B115200;
            break;
        case 230400:
            speed = B230400;
            break;
        case 460800:
            speed = B460800;
            break;
        case 500000:
            speed = B500000;
            break;
        case 576000:
            speed = B576000;
            break;
        case 921600:
            speed = B921600;
            break;
        case 1000000:
            speed = B1000000;
            break;
        case 1152000:
            speed = B1152000;
            break;
        case 1500000:
            speed = B1500000;
            break;
        case 2000000:
            speed = B2000000;
            break;
        case 2500000:
            speed = B2500000;
            break;
        case 3000000:
            speed = B3000000;
            break;
        case 3500000:
            speed = B3500000;
            break;
        case 4000000:
            speed = B4000000;
            break;
        default:
            exit(2);
            break;
        }

        if (ttydevice.flowctl == 1)
        {
            tios.c_cflag |= CRTSCTS;
        }
        else
        {
            tios.c_cflag &= ~CRTSCTS;
        }

        if ((rval = tcsetattr(fd, TCSANOW, &tios)) != 0)
        {
            log_err("fail to set attribute of the device 1 %s, errno %d(%s)", ttydevice.device_node, errno,
                    strerror(errno));
            goto exit;
        }

        if ((rval = cfsetspeed(&tios, speed)) != 0)
        {
            log_err("fail to set uart baudrate %d, errno, %d(%s)", baudrate, errno, strerror(errno));
            goto exit;
        }

        if ((rval = tcsetattr(fd, TCSANOW, &tios)) != 0)
        {
            log_err("fail to set attribute of the device 2 %s, errno %d(%s)", ttydevice.device_node, errno,
                    strerror(errno));
            goto exit;
        }

        if ((rval = tcflush(fd, TCIOFLUSH)) != 0)
        {
            log_err("fail to flush the device 2 %s, errno %d(%s)", ttydevice.device_node, errno,
                    strerror(errno));
            goto exit;
        }
    }

exit:
    if (rval != 0)
    {
        close(fd);
        fd = -1;
        ttyfd = -1;
    }
    else
    {
        ttyfd = fd;
        log_debug("open tty successfully, baudrate %d ", ttydevice.baudrate);
        log_prompt("open tty successfully, baudrate %d \n", ttydevice.baudrate);
    }

    return fd;
}

void device_close(void)
{
    close(ttyfd);
    ttyfd = -1;

    log_debug("close tty successfully");
    log_prompt("close tty successfully \n");
    return;
}

bool rtkrcp_usb_interface_check()
{
    if (strstr(ttydevice.device_node, usbinterfaceName1))
    {
        return true;
    }
    if (strstr(ttydevice.device_node, usbinterfaceName2))
    {
        return true;
    }
    return false;
}

static uint64_t PlatTimeGet(void)
{
    struct timespec now;
    uint64_t ret;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
    {
        log_err("fail to get time");
        exit(1);
    }
    ret = now.tv_sec * 1000000;
    ret = ret + now.tv_nsec / 1000;
    return ret;
}

static int ttyWaitForWritable(void)
{
    T_EXEC_RESULT error   = NO_ERR;
    struct timeval timeout = {kMaxWaitTime / 1000, (kMaxWaitTime % 1000) * 1000};
    uint64_t       now     = PlatTimeGet(); //us
    uint64_t       end     = now + kMaxWaitTime * 1000; //us
    fd_set         writeFds;
    fd_set         errorFds;
    int            rval;

    while (true)
    {
        FD_ZERO(&writeFds);
        FD_ZERO(&errorFds);
        FD_SET(ttyfd, &writeFds);
        FD_SET(ttyfd, &errorFds);

        rval = select(ttyfd + 1, NULL, &writeFds, &errorFds, &timeout);

        if (rval > 0)
        {
            if (FD_ISSET(ttyfd, &writeFds))
            {
                error = NO_ERR;
                goto exit;
            }
            else if (FD_ISSET(ttyfd, &errorFds))
            {
                log_err("%s():tty device error", __func__);
                error = ERR_DISCONN;
                goto exit;
            }
            else
            {
                //assert(false);
                log_err("%s(): unknown error", __func__);
                error = ERR_UNKNOWN;
                goto exit;
            }
        }
        else if ((rval < 0) && (errno != EINTR))
        {
            log_err("%s(): system error, errno %d, %s", __func__, errno, strerror(errno));
            error = ERR_UNEXPECTED;
            goto exit;
        }

        now = PlatTimeGet();

        if (end > now)
        {
            uint64_t remain = end - now;

            timeout.tv_sec  = (time_t)(remain / 1000000);
            timeout.tv_usec = (suseconds_t)(remain % 1000000);
        }
        else
        {
            log_err("wait for writable timeout");
            error = ERR_TIMEOUT;
            goto exit;
        }
    }

exit:
    return (int)error;
}

int ttyWrite(const uint8_t *aFrame, uint16_t aLength)
{
    T_EXEC_RESULT error = NO_ERR;

    //debug
    char send_s[2048] = {'\0'};
    int n = 0;
    int count = 0;

    log_debug("---------------------------------------");

    n = 0;
    count = 0;
    log_debug("%s(): uart send (%d):",  __func__, aLength);

    for (uint16_t i = 0; i < aLength; i++)
    {
        n = snprintf(&send_s[count], sizeof(send_s) - 1 - count, " %02x ", aFrame[i]);
        count += n;
    }
    log_debug("%s(): %s",  __func__, send_s);
    log_debug("---------------------------------------");

    while (aLength)
    {
        ssize_t rval = write(ttyfd, aFrame, aLength);

        if (rval == aLength)
        {
            error = NO_ERR;
            goto exit;
        }
        else if (rval > 0)
        {
            log_warn("ttyWrite resend");
            aLength -= (uint16_t)rval;
            aFrame += (uint16_t)rval;
        }
        else if (rval < 0)
        {
            if ((errno != EAGAIN) && (errno != EWOULDBLOCK) && (errno != EINTR))
            {
                log_err("fail to send frame by tty device, errno %d, %s ", errno, strerror(errno));
                error = ERR_UNEXPECTED;
                goto exit;
            }
        }

        if ((error = ttyWaitForWritable()) != NO_ERR)
        {
            goto exit;
        }
    }

exit:
    if (error != NO_ERR)
    {
        log_err("%s(): error %d", __func__, error);
    }

    return (int)error;
}

static bool ttyRead(FrameHandler handler)
{
    uint8_t buffer[kMaxFrameSize];
    ssize_t rval;

    rval = read(ttyfd, buffer, sizeof(buffer));

    if (rval > 0)
    {
        //debug
        char recv_s[2048] = {'\0'};
        int n = 0;
        int count = 0;

        log_debug("%s(): uart receive (%d):",  __func__, rval);

        for (uint16_t i = 0; i < rval; i++)
        {
            n = snprintf(&recv_s[count], sizeof(recv_s) - 1 - count, " %02x ", buffer[i]);
            count += n;
        }
        log_debug("%s(): %s",  __func__, recv_s);

        if (handler)
        {
            return handler(buffer, rval);
        }
        else
        {
            log_prompt("uart receive:\n");
            for (uint8_t i = 0; i < rval; i++)
            {
                log_prompt(" %02x ", buffer[i]);
            }
            log_prompt("\n");
            return true;
        }
    }
    else if ((rval < 0) && (errno != EAGAIN) && (errno != EINTR))
    {
        log_err("%s(): system error, errno %d, %s", __func__, errno, strerror(errno));
        return false;
    }
    return false;
}

int ttyWaitForFrame(uint64_t aTimeoutUs, FrameHandler handler)
{
    T_EXEC_RESULT error = NO_ERR;
    struct timeval timeout;

    timeout.tv_sec = (time_t)(aTimeoutUs / 1000000);
    timeout.tv_usec = (suseconds_t)(aTimeoutUs % 1000000);

    uint64_t now = PlatTimeGet(); //us
    uint64_t end = now + aTimeoutUs; //us
    fd_set read_fds;
    fd_set error_fds;
    int rval;

    log_debug("-----------------start---------------------");
    while (true)
    {
        FD_ZERO(&read_fds);
        FD_ZERO(&error_fds);
        FD_SET(ttyfd, &read_fds);
        FD_SET(ttyfd, &error_fds);

        rval = select(ttyfd + 1, &read_fds, NULL, &error_fds, &timeout);

        if (rval > 0)
        {
            if (FD_ISSET(ttyfd, &read_fds))
            {
                if (ttyRead(handler))
                {
                    error = NO_ERR;
                    goto exit;
                }
            }
            else if (FD_ISSET(ttyfd, &error_fds))
            {
                log_err("%s(): error hanpaned", __func__);
                error = ERR_DISCONN;
                goto exit;
            }
            else
            {
                //assert(false);
                log_err("%s(): unknown error", __func__);
                error = ERR_UNKNOWN;
                goto exit;
            }
        }
        else if ((rval < 0) && (errno != EINTR))
        {
            log_err("%s(): system error, errno %d, %s", __func__, errno, strerror(errno));
            error = ERR_UNEXPECTED;
            goto exit;
        }

        now = PlatTimeGet();
        if (end > now)
        {
            uint64_t remain = end - now;

            timeout.tv_sec  = (time_t)(remain / 1000000);
            timeout.tv_usec = (suseconds_t)(remain % 1000000);
        }
        else
        {
            log_err("wait for readable timeout %ldms", aTimeoutUs / 1000);
            error = ERR_TIMEOUT;
            goto exit;
        }
    }
exit:
    log_debug("-----------------end---------------------");
    return (int)error;
}