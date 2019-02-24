/*
 * Apple // emulator for *ix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 2013-2018 Aaron Culliney
 *
 */

#include "common.h"

#if ENABLE_SYSTRACING

#if defined(__linux__)
// Linux (and Android) have kernel support via ftrace (atrace) : https://www.kernel.org/doc/Documentation/trace/ftrace.txt
#else
#   if !USE_CUSTOM_TRACE_FILE
#       error not on Linux, must use USE_CUSTOM_TRACE_FILE ...
#   endif
#endif

#include <stdarg.h>

#define MAX_MSG_LEN 1024
#define MAX_ARG_LEN 768
#define CLOCK_GETTIME() \
    struct timespec ts; \
    clock_gettime(CLOCK_MONOTONIC, &ts); \
    long secs = ts.tv_sec; \
    long usecs = ts.tv_nsec / 1000;

static int trace_fd = -1;

#if !USE_CUSTOM_TRACE_FILE

#   define FMT_TRACE_BEGIN  "B|%lu|%s"
#   define FMT_TRACE_CBEGIN "C|%lu|%s|%d"
#   define FMT_TRACE_END    "E"
#   define TRACING_PATH     "/sys/kernel/debug/tracing/trace_marker"
#   define OPEN_FLAGS       (O_WRONLY)
#   define MODE_FLAGS       (0)

#else

#   define FMT_TRACE_BEGIN  "a2ix-%lu  [000] ...1 %ld%c%06ld: tracing_mark_write: B|%lu|%s\n"
#   define FMT_TRACE_CBEGIN "a2ix-%lu  [000] ...1 %ld%c%06ld: tracing_mark_write: C|%lu|%s|%d\n"
#   define FMT_TRACE_END    "a2ix-%lu  [000] ...1 %ld%c%06ld: tracing_mark_write: E\n"
#   define TRACE_BUF_SIZ    (2*1024*1024)
#   define OPEN_FLAGS       (O_RDWR|O_CREAT|O_TRUNC)
#   define MODE_FLAGS       (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)
#   define FILE_WRITER_USLEEP 10

static pthread_t systrace_thread_id = 0;
static bool systrace_thread_initialized = false;

static long writerGate = 0;    // prevent writing in systrace_thread critical section
static char *writeBuf0 = NULL; // allocated
static char *writeBuf1 = NULL; // (non-allocated contiguous) == writeBuf0 + TRACE_BUF_SIZ
static char *writeHead = NULL;

static void _write_trace(char *srcBuf, unsigned long length) {
    if (UNLIKELY(length <= 0)) {
        assert(false && "invalid input");
        return;
    }

    // gated swizzle writeHead (wait if necessary for systrace_thread to exit critical section)
    long gate;
    do {
        gate = __sync_add_and_fetch(&writerGate, 1);
        if (LIKELY(gate > 0)) {
            break;
        }
        __sync_sub_and_fetch(&writerGate, 1);
        usleep(1);
    } while(1);
    char *dstBuf = __sync_fetch_and_add(&writeHead, (void *)length);
    __sync_sub_and_fetch(&writerGate, 1);

    // sanity check buffer overflow
    uintptr_t idx0 = dstBuf - writeBuf0;
    uintptr_t idx1 = dstBuf - writeBuf1;
    uintptr_t idx = idx0 < idx1 ? idx0 : idx1;
    if (UNLIKELY((idx + length) > TRACE_BUF_SIZ)) {
        assert(false && "avoided buffer overflow, please adjust buffer size or FILE_WRITER_USLEEP ...");
        return;
    }

    memcpy(dstBuf, srcBuf, length);
}

static void *systrace_thread(void *ignored) {
    (void)ignored;

    LOG("Starting systrace writer thread ...");

    writeBuf0 = CALLOC(1, TRACE_BUF_SIZ);
    assert(writeBuf0);
    writeBuf1 = CALLOC(1, TRACE_BUF_SIZ);
    assert(writeBuf1);

    bool swapped = __sync_bool_compare_and_swap(&writeHead, /*oldval:*/NULL, /*newval:*/writeBuf0);
    assert(swapped);

    systrace_thread_initialized = true;

    char *currBuf = writeBuf0;
    char *otherBuf = writeBuf1;
    while (LIKELY(systrace_thread_initialized)) {

        // gated swap of writeHead to otherBuf ... wait for writers if necessary
        long gate;
        do {
            gate = __sync_sub_and_fetch(&writerGate, 10000);
            if (LIKELY(gate == -10000)) {
                break;
            }
            __sync_add_and_fetch(&writerGate, 10000);
            usleep(FILE_WRITER_USLEEP);
        } while(1);
        char *p = __sync_val_compare_and_swap(&writeHead, /*oldval:*/writeHead, /*newval:*/otherBuf);
        __sync_add_and_fetch(&writerGate, 10000);

        // swap active buffer ...
        char *oldBuf = currBuf;
        currBuf = otherBuf;
        otherBuf = oldBuf;

        size_t length = p - oldBuf;
        if (length == 0) {
            // no traces to write
            usleep(FILE_WRITER_USLEEP);
            continue;
        }

        if (UNLIKELY(length > TRACE_BUF_SIZ)) {
            length = TRACE_BUF_SIZ;
        }

        // heuristic : if NUL bytes found in trace buffer, we need to wait for lingering writers to finish
        while (1) {
            p = (char *)memchr(oldBuf, 0x00, length);
            if (LIKELY(p == NULL)) {
                break;
            }
            usleep(1);
        }

        // now write the old buffer to the trace file on disk
        TEMP_FAILURE_RETRY(write(trace_fd, oldBuf, length));

        // clean buffer for next time
        memset(oldBuf, 0x00, TRACE_BUF_SIZ);
    }

    LOG("Stopping systrace writer thread ...");

    return NULL;
}

#endif // USE_CUSTOM_TRACE_FILE

static void _trace_init(void) {
#if !USE_CUSTOM_TRACE_FILE
    TEMP_FAILURE_RETRY(trace_fd = open(TRACING_PATH, O_WRONLY));
#else
    char path[PAGE_SIZE];
    snprintf(path, sizeof(path), "%s/systrace.txt", HOMEDIR);

    trace_fd = TEMP_FAILURE_RETRY(open(path, OPEN_FLAGS, MODE_FLAGS));
    assert(trace_fd >= 0);

    assert(systrace_thread_id == 0);
    int err = TEMP_FAILURE_RETRY(pthread_create(&systrace_thread_id, NULL, (void *)&systrace_thread, (void *)NULL));
    assert(!err);

    while (!systrace_thread_initialized) {
        usleep(FILE_WRITER_USLEEP);
    }
#endif

    assert(trace_fd >= 0);

    if (trace_fd < 0) {
        LOG("Could not open kernel trace file");
    } else {
        LOG("Initialized Linux tracing facility");
    }
}

static __attribute__((constructor)) void __trace_init(void) {
    emulator_registerStartupCallback(CTOR_PRIORITY_LATE, &_trace_init);
}

__attribute__((destructor(255)))
static void _trace_shutdown(void) {
    if (trace_fd < 0) {
        return;
    }
    int fd = trace_fd;
    trace_fd = -1;
    TEMP_FAILURE_RETRY(close(fd));

#if USE_CUSTOM_TRACE_FILE
    systrace_thread_initialized = false;
    if (pthread_join(systrace_thread_id, NULL)) {
        LOG("OOPS: pthread_join of CPU thread ...");
    }
    systrace_thread_id = 0;

    FREE(writeBuf0);
    FREE(writeBuf1);
    writeHead = NULL;
#endif
}

void _trace_begin(const char *fmt, ...) {
    if (UNLIKELY(trace_fd == -1)) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    char args_buf[MAX_ARG_LEN] = { 0 };
    vsnprintf(args_buf, MAX_ARG_LEN, fmt, args);
    va_end(args);

    char buf[MAX_MSG_LEN] = { 0 };

    unsigned long thread_id = (unsigned long)pthread_self();

#if USE_CUSTOM_TRACE_FILE
    CLOCK_GETTIME();
    size_t length = snprintf(buf, MAX_MSG_LEN, FMT_TRACE_BEGIN, thread_id, secs, '.', usecs, thread_id, args_buf);
    _write_trace(buf, length);
#else
    size_t length = snprintf(buf, MAX_MSG_LEN, FMT_TRACE_BEGIN, thread_id, args_buf);
    TEMP_FAILURE_RETRY(write(trace_fd, buf, length));
#endif
}

void _trace_begin_count(uint32_t count, const char *fmt, ...) {
    if (UNLIKELY(trace_fd == -1)) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    char args_buf[MAX_ARG_LEN] = { 0 };
    vsnprintf(args_buf, MAX_ARG_LEN, fmt, args);
    va_end(args);

    char buf[MAX_MSG_LEN] = { 0 };

    unsigned long thread_id = (unsigned long)pthread_self();

#if USE_CUSTOM_TRACE_FILE
    CLOCK_GETTIME();
    size_t length = snprintf(buf, MAX_MSG_LEN, FMT_TRACE_CBEGIN, thread_id, secs, '.', usecs, thread_id, args_buf, count);
    _write_trace(buf, length);
#else
    size_t length = snprintf(buf, MAX_MSG_LEN, FMT_TRACE_CBEGIN, thread_id, args_buf, count);
    TEMP_FAILURE_RETRY(write(trace_fd, buf, length));
#endif
}

void _trace_end(void) {
    if (UNLIKELY(trace_fd == -1)) {
        return;
    }

#if !USE_CUSTOM_TRACE_FILE
    TEMP_FAILURE_RETRY(write(trace_fd, FMT_TRACE_END, sizeof(FMT_TRACE_END) - 1));
#else
    char buf[MAX_MSG_LEN];
    buf[0] = '\0';

    CLOCK_GETTIME();
    unsigned long thread_id = (unsigned long)pthread_self();
    size_t length = snprintf(buf, MAX_MSG_LEN, FMT_TRACE_END, thread_id, secs, '.', usecs);
    _write_trace(buf, length);
#endif
}

void _trace_cleanup(void *token) {
    _trace_end();
}

#endif // whole file
