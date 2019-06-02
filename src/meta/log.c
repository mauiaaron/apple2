/*
 * Apple // emulator for *ix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 2017 Aaron Culliney
 *
 */

#include "common.h"

// log.c :
//  - simple and not-particularly-performant logging functions
//  - do not call in a tight loop!

#if defined(__ANDROID__)
#   include <android/log.h>
#endif

#define LOG_PATH_TEMPLATE "%s%sapple2ix_log.%u.txt"

bool do_logging = true;
bool do_std_logging = true;

static int logFd = -1;
static off_t logSiz = 0;
static const unsigned int logRotateSize = 1024 * 1024;
static const unsigned int logRotateCount = 4;

static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

// ----------------------------------------------------------------------------

static void _log_stopLogging(void) {
    if (logFd >= 0) {
        TEMP_FAILURE_RETRY(fsync(logFd));
        TEMP_FAILURE_RETRY(close(logFd));
        logFd = -1;
        logSiz = 0;
    }
}

static void _log_rotate(bool performRotation) {

    _log_stopLogging();

    int xflag = 0;
    if (LIKELY(performRotation)) {
        xflag = O_TRUNC;
        for (unsigned int i = logRotateCount; i>0; i--) {
            char *logPath0 = NULL;
            char *logPath1 = NULL;

            ASPRINTF(&logPath0, LOG_PATH_TEMPLATE, data_dir, PATH_SEPARATOR, (i-1));
            ASPRINTF(&logPath1, LOG_PATH_TEMPLATE, data_dir, PATH_SEPARATOR, i);
            assert((uintptr_t)logPath0);
            assert((uintptr_t)logPath1);

            int ret = -1;
            TEMP_FAILURE_RETRY(ret = rename(logPath0, logPath1));

            FREE(logPath0);
            FREE(logPath1);
        }
    }

    char *logPath = NULL;
    ASPRINTF(&logPath, LOG_PATH_TEMPLATE, data_dir, PATH_SEPARATOR, 0);
    assert((uintptr_t)logPath);

    TEMP_FAILURE_RETRY(logFd = open(logPath, O_WRONLY|O_CREAT|xflag, S_IRUSR|S_IWUSR));

    logSiz = lseek(logFd, 0L, SEEK_END);

    //log_outputString("-------------------------------------------------------------------------------"); -- do not write to logfile here unless lock is recursive

    FREE(logPath);
}

#if VIDEO_OPENGL
// 2015/04/01 ... early calls to glGetError()--before a context exists--causes segfaults on MacOS X
extern bool safe_to_do_opengl_logging;
GLenum safeGLGetError(void) {
    if (safe_to_do_opengl_logging && video_isRenderThread()) {
        return glGetError();
    }
    return (GLenum)0;
}
#endif

void log_init(void) {
    assert((uintptr_t)data_dir);
    _log_rotate(/*performRotation:*/false);
}

void log_outputString(const char * const str) {
    if (UNLIKELY(!str)) {
        return;
    }

    if (UNLIKELY(!do_logging)) {
        return;
    }

    if (do_std_logging) {
#if defined(__ANDROID__) && !defined(NDEBUG)
        __android_log_print(ANDROID_LOG_ERROR, "apple2ix", "%s", str);
#else
        fprintf(stderr, "%s\n", str);
#endif
    }

    if (UNLIKELY(logFd < 0)) {
        return;
    }

    pthread_mutex_lock(&log_mutex);

    size_t expected_bytescount = strlen(str);
    size_t bytescount = 0;

    do {
        ssize_t byteswritten = 0;
        TEMP_FAILURE_RETRY(byteswritten = write(logFd, str+bytescount, expected_bytescount-bytescount));
        if (UNLIKELY(byteswritten <= 0)) {
            break; // OOPS !
        }

        bytescount += byteswritten;
        if (bytescount >= expected_bytescount) {
            bytescount = expected_bytescount;
            TEMP_FAILURE_RETRY(write(logFd, "\n", 1));
            TEMP_FAILURE_RETRY(fsync(logFd));
            logSiz += bytescount + 1;
            if (UNLIKELY(logSiz >= logRotateSize)) {
                _log_rotate(/*performRotation:*/true);
            }

            break; // OKAY
        }
    } while (1);

    if (UNLIKELY(bytescount != expected_bytescount)) {
        // logging is b0rked, shut it down ...
        _log_stopLogging();
    }

    pthread_mutex_unlock(&log_mutex);
}

