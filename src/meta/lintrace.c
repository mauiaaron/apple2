/*
 * Apple // emulator for *ix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 2013-2015 Aaron Culliney
 *
 */

#if !defined(__linux__) || defined(NDEBUG)
// Linux tracing should not be enabled for release builds
#else

#include "common.h"
#include "trace.h"

#define MAX_MSG_LEN 1024
#define MAX_ARG_LEN 768

#define TRACING_FILE "/sys/kernel/debug/tracing/trace_marker"

#define FMT_TRACE_BEGIN       "B|%d|%s"
#define FMT_TRACE_COUNT_BEGIN "C|%d|%s|%d"

static int trace_fd = -1;
static int trace_pid = -1;

__attribute__((constructor(CTOR_PRIORITY_LATE)))
static void _trace_init(void) {
    TEMP_FAILURE_RETRY(trace_fd = open(TRACING_FILE, O_WRONLY));
    if (trace_fd == -1) {
        ERRLOG("Could not open kernel trace file");
    } else {
        LOG("Initialized Linux tracing facility");
    }
    trace_pid = getpid();
}

__attribute__((destructor(255)))
static void _trace_shutdown(void) {
    if (trace_fd != -1) {
        TEMP_FAILURE_RETRY(close(trace_fd));
        trace_fd = -1;
    }
}

void trace_begin(const char *fmt, ...) {
    if (trace_fd == -1) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    char args_buf[MAX_ARG_LEN] = { 0 };
    vsnprintf(args_buf, MAX_ARG_LEN, fmt, args);
    va_end(args);

    char buf[MAX_MSG_LEN] = { 0 };
    size_t length = snprintf(buf, MAX_MSG_LEN, FMT_TRACE_BEGIN, trace_pid, args_buf);
    TEMP_FAILURE_RETRY(write(trace_fd, buf, length));
}

void trace_begin_count(uint32_t count, const char *fmt, ...) {
    if (trace_fd == -1) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    char args_buf[MAX_ARG_LEN] = { 0 };
    vsnprintf(args_buf, MAX_ARG_LEN, fmt, args);
    va_end(args);

    char buf[MAX_MSG_LEN] = { 0 };
    size_t length = snprintf(buf, MAX_MSG_LEN, FMT_TRACE_COUNT_BEGIN, trace_pid, args_buf, count);
    TEMP_FAILURE_RETRY(write(trace_fd, buf, length));
}

void trace_end(void) {
    if (trace_fd == -1) {
        return;
    }
    TEMP_FAILURE_RETRY(write(trace_fd, "E", 1));
}

void _trace_cleanup(void *token) {
    trace_end();
}

#endif // whole file

