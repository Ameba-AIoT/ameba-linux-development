#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/time.h>
#include <time.h>

#include "trace.h"
#include "FwUpdate.h"
#include "RcpConfig.h"
#include "ttydevice.h"

static void timestamp_print()
{
    struct tm *tm_now;
    struct timeval tv;
    uint32_t mill_time;
    int n = 0;
    char data_buffer[256] = {'\0'};

    gettimeofday(&tv, NULL);
    tm_now = localtime(&tv.tv_sec);
    mill_time = tv.tv_usec / 1000;

    n = sprintf(data_buffer, "time: [%02u:%02u:%02u %03u]", tm_now->tm_hour, tm_now->tm_min,
                tm_now->tm_sec, mill_time);
    sprintf(&data_buffer[n], "rtk_cfu");
    log_prompt("%s \n", data_buffer);

    return;
}

void Usage()
{
    log_prompt("\n"
               "USAGE:\n"
               "    To make Component Firmware Update with Offer File \"*.offer.bin\" and Firmware image \".paylod.bin\".\n"
               "       Optional arguments \"forceIgnoreVersion\" is the flags to use to set those conditions\n"
               "    ./rtkcfu update ./config.txt [folder path to offer bins] [folder path to payload bins] <forceIgnoreVersion> \n"
               "\n"
              );
}

int main(int argc, char *argv[])
{
    int ret = 0;
    uint8_t retry = 0;
#if defined(_DEBUG)
    for (int i = 1; i < argc; i++)
    {
        log_prompt("Argv #%d is: %s\n", i, argv[i]);
    }
#endif

    // argv[0] is the program name.
    if (argc == 1)
    {
        Usage();
        return 0;
    }
    log_module_trace_init(0x0F);

    parse_device_config(argv[2]);
    rtkrcp_config_parse();

    timestamp_print();
    retry = 0;
    do
    {
        log_prompt("== start rtkrcp_config (Retry count %d) == \n", retry);
        log_info("== start rtkrcp_config (Retry count %d) == \n", retry);
        ret = rtkrcp_config();
        retry ++;
    }
    while (!ret && retry < 3);

    timestamp_print();
    ret = rtk_cfu_update(argc, argv);

    timestamp_print();
    retry = 0;
    do
    {
        log_prompt("== start rtkrcp_config_flowctl (Retry count %d) == \n", retry);
        log_info("== start rtkrcp_config_flowctl (Retry count %d) == \n", retry);
        ret = rtkrcp_config_flowctl();
        retry ++;
    }
    while (!ret && retry < 3);

    timestamp_print();
    test_after_config_cfu_flowctl();

    return ret;
}
