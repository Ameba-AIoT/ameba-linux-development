#ifndef __RTKRCP_CONFIG_H__
#define __RTKRCP_CONFIG_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

extern char RcpConfigBin[512];

bool rtkrcp_config_parse();
bool rtkrcp_config();
bool rtkrcp_config_flowctl();
bool test_after_config_cfu_flowctl();

#ifdef __cplusplus
}
#endif

#endif