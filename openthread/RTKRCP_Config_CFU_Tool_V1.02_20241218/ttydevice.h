#ifndef __TTYDEVICE_H__
#define __TTYDEVICE_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define kMaxFrameSize 512

typedef bool (*FrameHandler)(uint8_t *buff, uint16_t len);

int device_open();
void device_close();
int ttyWrite(const uint8_t *aFrame, uint16_t aLength);
int ttyWaitForFrame(uint64_t aTimeoutUs, FrameHandler handler);
bool ttyGetDeviceName(char dev_name[48], uint16_t *len);
bool parse_device_config(char *configPath);
bool ttydevice_baud_set(int baudrate);
bool ttydevice_flowctl_set(bool enable);
bool rtkrcp_usb_interface_check();

#ifdef __cplusplus
}
#endif

#endif