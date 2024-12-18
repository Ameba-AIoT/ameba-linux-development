#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/time.h>
#include <time.h>

#include "trace.h"
#include "ttydevice.h"

static uint8_t hci_evt[20];
bool testHandler(uint8_t *buff, uint16_t len)
{
    static uint8_t index = 0;
    log_prompt("*uart receive: %d\n", len);
    memcpy(&hci_evt[index], buff, len);
    index += len;
    if (index == 14)
    {
        log_prompt("*uart receive:\n");
        for (uint8_t i = 0; i < index; i++)
        {
            log_prompt(" %02x ", hci_evt[i]);
        }
        log_prompt("\n");
        memset(hci_evt, 0, sizeof(hci_evt));
        index = 0;
        return true;
    }
    return false;
}

bool uart_test(int argc, char *argv[])
{
    char *offerPath;
    char *srecBinPath;

    int error = 0;
    uint8_t hci_cmd[20] = {0x01, 0x01, 0x10, 0x00};
    if (argc < 3)
    {
        log_prompt("Error, too few parameters.\n");
        return false;
    }

    error = ttyWrite(hci_cmd, 4);
    if (error != 0)
    {
        log_prompt("Error, uart send fail.\n");
        return false;
    }

    error = ttyWaitForFrame(5000000, testHandler);
    if (error == 0)
    {
        log_prompt("uart receive success.\n");
    }
    else
    {
        log_prompt("Error, uart receive fail.\n");
        return false;
    }
    return true;
}

static uint8_t version_evt[61];
bool rcptestHandler(uint8_t *buff, uint16_t len)
{
    static uint8_t index = 0;
    log_prompt("*uart receive: %d\n", len);
    memcpy(&version_evt[index], buff, len);
    index += len;
    if (index == sizeof(version_evt))
    {
        log_prompt("*uart receive:\n");
        for (uint8_t i = 0; i < index; i++)
        {
            log_prompt(" %02x ", version_evt[i]);
        }
        log_prompt("\n");
        memset(version_evt, 0, sizeof(version_evt));
        index = 0;
        return true;
    }
    return false;
}

bool rcp_test(int argc, char *argv[])
{
    char *offerPath;
    char *srecBinPath;

    int error = 0;
    uint8_t hci_cmd[20];
    if (argc < 3)
    {
        log_prompt("Error, too few parameters.\n");
        return false;
    }

    hci_cmd[0] = 0x2A;
    error = ttyWrite(hci_cmd, 1);
    if (error != 0)
    {
        log_prompt("Error, uart send fail.\n");
        return false;
    }

    error = ttyWaitForFrame(5000000, rcptestHandler);
    if (error == 0)
    {
        log_prompt("uart receive success.\n");
    }
    else
    {
        log_prompt("Error, uart receive fail.\n");
        return false;
    }
    return true;
}

int main(int argc, char *argv[])
{

#if defined(_DEBUG)
    for (int i = 1; i < argc; i++)
    {
        log_prompt("Argv #%d is: %s\n", i, argv[i]);
    }
#endif

    // argv[0] is the program name.
    if (argc == 1)
    {
        return 0;
    }

    log_module_trace_init(0x0F);

    if (!strcmp(argv[1], "uarttest"))
    {
        uart_test(argc, argv);
        log_prompt("uarttest.\n");
    }
    else if (!strcmp(argv[1], "rcptest"))
    {
        rcp_test(argc, argv);
        log_prompt("rcptest.\n");
    }
    else
    {
        log_prompt("Failed to parse input tokens.\n");
    }

    device_close();
    return 0;
}