#ifndef __TRACE_H__
#define __TRACE_H__

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define log_level_debug  0x08
#define log_level_info   0x04
#define log_level_warn   0x02
#define log_level_err    0x01

void log_module_trace_init(uint8_t mask);

bool log_module_trace_set(uint8_t mask);

uint8_t log_module_trace_get(void);

void log_disable(void);

void log_print(uint8_t level, char *fmt, ...);

#ifndef SHOW_PROMPT_TO_STDOUT
#define SHOW_PROMPT_TO_STDOUT 1
#endif

#if SHOW_PROMPT_TO_STDOUT
#define log_prompt(fmt, ...)    printf(fmt, ##__VA_ARGS__)
#else
#define log_prompt(fmt, ...)
#endif

#if 0
#define log_err(fmt, ...)    log_print(log_level_err, fmt, ##__VA_ARGS__)
#define log_warn(fmt, ...)    log_print(log_level_warn, fmt, ##__VA_ARGS__)
#define log_info(fmt, ...)    log_print(log_level_info, fmt, ##__VA_ARGS__)
#define log_debug(fmt, ...)    log_print(log_level_debug, fmt, ##__VA_ARGS__)
#else
#define log_err(fmt, ...)       log_print(log_level_err, fmt, ##__VA_ARGS__)
#define log_warn(fmt, ...)      log_print(log_level_warn, fmt, ##__VA_ARGS__)
#define log_info(fmt, ...)      log_print(log_level_info, fmt, ##__VA_ARGS__)
#define log_debug(fmt, ...)     log_print(log_level_info, fmt, ##__VA_ARGS__)
#endif

#ifdef __cplusplus
}
#endif

#endif