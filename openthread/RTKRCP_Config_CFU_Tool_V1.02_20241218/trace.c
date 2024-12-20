/**
 * Copyright (c) 2015, Realsil Semiconductor Corporation. All rights reserved.
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>

#include <assert.h>
#include "trace.h"

#ifndef SYSLOG_PRINT
#define SYSLOG_PRINT 0
#endif

#if SYSLOG_PRINT
#include <syslog.h>

#define SPRINTF_BUFFER_LEN     256

pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

uint8_t trace_mask = 0x0f;

static char *level_prompt[4] = {"!!! ", "*!! ", "**! ", "*** "};

void log_module_trace_init(uint8_t mask)
{
    pthread_mutex_lock(&log_mutex);
    trace_mask = mask;
    pthread_mutex_unlock(&log_mutex);
    return;
}

bool log_module_trace_set(uint8_t mask)
{
    pthread_mutex_lock(&log_mutex);
    trace_mask = mask;
    pthread_mutex_unlock(&log_mutex);
    return true;
}

uint8_t log_module_trace_get(void)
{
    return trace_mask;
}

void log_disable(void)
{
    pthread_mutex_lock(&log_mutex);
    trace_mask = 0;
    pthread_mutex_unlock(&log_mutex);
}

void log_print(uint8_t level, char *fmt, ...)
{
    char data_buffer[SPRINTF_BUFFER_LEN];
    uint8_t prompt_index = 0;

    pthread_mutex_lock(&log_mutex);
    if ((trace_mask & level) == 0)
    {
        pthread_mutex_unlock(&log_mutex);
        return;
    }
    pthread_mutex_unlock(&log_mutex);

    struct tm *tm_now;
    struct timeval tv;
    uint32_t mill_time;
    gettimeofday(&tv, NULL);
    tm_now = localtime(&tv.tv_sec);
    mill_time = tv.tv_usec / 1000;

    va_list args;
    int n = 0;
    int count = 0;
    char *pbuf = data_buffer;
    n = sprintf(pbuf, "[%02u:%02u:%02u %03u]", tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec,
                mill_time);
    pbuf += n;
    count += n;

    n = sprintf(pbuf, "%s", "[RTKCFU]");
    pbuf += n;
    count += n;

    if (level == log_level_debug)
    {
        prompt_index = 3;
    }
    else if (level == log_level_info)
    {
        prompt_index = 2;
    }
    else if (level == log_level_warn)
    {
        prompt_index = 1;
    }
    else if (level == log_level_err)
    {
        prompt_index = 0;
    }
    n = sprintf(pbuf, "%s", level_prompt[prompt_index]);
    pbuf += n;
    count += n;

    va_start(args, fmt);
    n = vsnprintf(pbuf, SPRINTF_BUFFER_LEN - count - 1, fmt, args);
    count += n;
    va_end(args);

    if (count >= SPRINTF_BUFFER_LEN)
    {
        data_buffer[SPRINTF_BUFFER_LEN - 1] = '\0';
    }
    else
    {
        data_buffer[count] = '\0';
    }

    syslog(LOG_ERR, "%s", data_buffer);
    return;
}

#else
#define SPRINTF_BUFFER_LEN     256

#define MAX_MESH_LOG_SIZE   (8*1024*1024)

#ifdef _ANDROID_
static const char *log_path = "/sdcard/rtkcfu_log.txt";
static const char *log_path_old = "/sdcard/rtkcfu_log0.txt";
#else
static const char *log_path = "rtkcfu_log.txt";
static const char *log_path_old = "rtkcfu_log0.txt";
#endif

pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

uint8_t trace_mask = 0x0f;

static FILE *log_fp = NULL;

static char *level_prompt[4] = {"!!! ", "*!! ", "**! ", "*** "};

void log_module_trace_init(uint8_t mask)
{
    trace_mask = mask;

    pthread_mutex_lock(&log_mutex);
    if (log_fp == NULL)
    {
#ifdef _ANDROID_
        log_fp = fopen(log_path, "a");
        ALOGD("%s(): open/create %s %d", __func__, log_path, log_fp != NULL ? 1 : 0);
#else
        log_fp = fopen(log_path, "a");
#endif
        assert(log_fp != NULL);
    }
    pthread_mutex_unlock(&log_mutex);
    return;
}

bool log_module_trace_set(uint8_t mask)
{
    pthread_mutex_lock(&log_mutex);
    trace_mask = mask;
    pthread_mutex_unlock(&log_mutex);
    return true;
}

uint8_t log_module_trace_get(void)
{
    return trace_mask;
}

void log_disable(void)
{
    pthread_mutex_lock(&log_mutex);
    trace_mask = 0;
    if (log_fp != NULL)
    {
        fclose(log_fp);
        log_fp = NULL;
    }
    pthread_mutex_unlock(&log_mutex);
}

void log_print(uint8_t level, char *fmt, ...)
{
    char data_buffer[SPRINTF_BUFFER_LEN];
    uint8_t prompt_index = 0;

    pthread_mutex_lock(&log_mutex);
    if ((!log_fp) || (trace_mask & level) == 0)
    {
        pthread_mutex_unlock(&log_mutex);
        return;
    }
    pthread_mutex_unlock(&log_mutex);

    struct tm *tm_now;
    struct timeval tv;
    uint32_t mill_time;
    gettimeofday(&tv, NULL);
    tm_now = localtime(&tv.tv_sec);
    mill_time = tv.tv_usec / 1000;

    va_list args;
    int n = 0;
    int count = 0;
    char *pbuf = data_buffer;
    n = sprintf(pbuf, "[%02u:%02u:%02u %03u]", tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec,
                mill_time);
    pbuf += n;
    count += n;

    n = sprintf(pbuf, "%s", "[RTKCFU]");
    pbuf += n;
    count += n;

    if (level == log_level_debug)
    {
        prompt_index = 3;
    }
    else if (level == log_level_info)
    {
        prompt_index = 2;
    }
    else if (level == log_level_warn)
    {
        prompt_index = 1;
    }
    else if (level == log_level_err)
    {
        prompt_index = 0;
    }
    n = sprintf(pbuf, "%s", level_prompt[prompt_index]);
    pbuf += n;
    count += n;

    va_start(args, fmt);
    n = vsnprintf(pbuf, SPRINTF_BUFFER_LEN - count - 1, fmt, args);
    count += n;
    va_end(args);

    if (count >= SPRINTF_BUFFER_LEN)
    {
        data_buffer[SPRINTF_BUFFER_LEN - 1] = '\0';
    }
    else
    {
        data_buffer[count] = '\0';
    }

    pthread_mutex_lock(&log_mutex);
    if (log_fp)
    {
        fprintf(log_fp, "%s\n", data_buffer);
        if (ftell(log_fp) >= MAX_MESH_LOG_SIZE / 2)
        {
            fclose(log_fp);
            log_fp = NULL;
#ifdef _ANDROID_
            if (rename(log_path, log_path_old) == -1)
            {
                //TO DO
            }
            log_fp = fopen(log_path, "a");
#else
            if (rename(log_path, log_path_old) == -1)
            {
                //TO DO
            }
            log_fp = fopen(log_path, "a");
#endif
            //fseek(log_fp, 0, SEEK_SET);
        }
    }
    pthread_mutex_unlock(&log_mutex);
    return;
}
#endif