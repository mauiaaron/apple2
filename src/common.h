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

#ifndef _COMMON_H_
#define _COMMON_H_

#if defined(__GNUC__) && !defined(_GNU_SOURCE)
#   define _GNU_SOURCE 1
#endif

#ifdef __APPLE__
#   warning DEFINING CUSTOM TEMP_FAILURE_RETRY(x) macro
#   define TEMP_FAILURE_RETRY(exp) ({ \
        typeof (exp) _rc; \
        do { \
            _rc = (exp); \
            if (_rc == -1 && (errno == EINTR || errno == EAGAIN) ) { \
                usleep(10); \
            } else { \
                break; \
            } \
        } while (1); \
        _rc; })
#endif

// custom annotations
#define INPARM
#define OUTPARM
#define INOUT
#define PRIVATE
#define PUBLIC
#define READONLY

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>

#include <zlib.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "misc.h"
#include "vm.h"
#include "timing.h"
#include "cpu.h"
#include "video/video.h"
#include "disk.h"
#include "interface.h"
#include "keys.h"
#include "joystick.h"
#include "glue.h"
#include "prefs.h"
#include "zlib-helpers.h"

#include "meta/trace.h"

#ifdef __APPLE__
#include "darwin-shim.h"
#import <CoreFoundation/CoreFoundation.h>
#endif

#define CTOR_PRIORITY_FIRST  101
#define CTOR_PRIORITY_EARLY  111
#define CTOR_PRIORITY_LATE   201

#if VIDEO_OPENGL
#include "video_util/glUtil.h"
// 2015/04/01 ... early calls to glGetError()--before a context exists--causes segfaults on MacOS X
extern bool safe_to_do_opengl_logging;
static inline GLenum safeGLGetError(void) {
    if (safe_to_do_opengl_logging) {
        return glGetError();
    }
    return (GLenum)0;
}
#else
#define GLenum int
#define safeGLGetError() 0
#define glGetError() 0
#endif

#ifdef DEBUGGER
#include "meta/debug.h"
#endif

#ifdef AUDIO_ENABLED
#include "audio/soundcore.h"
#include "audio/speaker.h"
#include "audio/mockingboard.h"
#endif

#ifdef ANDROID
#   include "../Android/jni/android_globals.h"
#   define USE_SIMD 1
#   define SIMD_IS_AVAILABLE() (android_armNeonEnabled/* || android_x86SSSE3Enabled*/)
#else
#   define SIMD_IS_AVAILABLE()
#endif

#define PATH_SEPARATOR "/" // =P

#if !defined(MIN)
#define MIN(a,b) (((a) <= (b)) ? (a) : (b))
#endif

#if !defined(MAX)
#define MAX(a,b) (((a) >= (b)) ? (a) : (b))
#endif

#define SPINLOCK_INIT 0
#define SPINLOCK_ACQUIRED -1
#define SPINLOCK_ACQUIRE(x) \
    do { \
        long val = __sync_sub_and_fetch((x), 1); \
        if (val == SPINLOCK_ACQUIRED) { \
            break; \
        } \
        __sync_add_and_fetch((x), 1); \
    } while (1);

#define SPINLOCK_RELINQUISH(x) \
    __sync_add_and_fetch((x), 1);


// cribbed from AOSP and modified with usleep() and to also ignore EAGAIN (should this be a different errno than EINTR)
#define TEMP_FAILURE_RETRY_FOPEN(exp) ({ \
    typeof (exp) _rc; \
    do { \
        _rc = (exp); \
        if (_rc == NULL && (errno == EINTR || errno == EAGAIN) ) { \
            usleep(10); \
        } else { \
            break; \
        } \
    } while (1); \
    _rc; })


extern bool do_logging;

#ifdef ANDROID
static const char *log_end = "";
#   include <android/log.h>
#   define QUIT_FUNCTION(x) exit(x)
#   define _LOG_CMD(str) __android_log_print(ANDROID_LOG_ERROR, "apple2ix", "%s", str)
#else
extern FILE *error_log;
static const char *log_end = "\n";
#   define QUIT_FUNCTION(x) exit(x)
#   define _LOG_CMD(str) \
        do { \
            if (UNLIKELY(!error_log)) { \
                error_log = stderr; \
            } \
            fprintf(error_log, "%s", str); \
        } while (0);
#endif

#define _MYFILE_ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define _LOG(...) \
    do { \
        int _err = errno; \
        errno = 0; \
        \
        char *syserr_str = NULL; \
        char *glerr_str = NULL; \
        if (_err) { \
            asprintf(&syserr_str, " (syserr:%s)", strerror(_err)); \
        } \
        if (_glerr) { \
            asprintf(&glerr_str, " (glerr:%04X)", _glerr); \
        } \
        \
        char *buf0 = NULL; \
        asprintf(&buf0, __VA_ARGS__); \
        \
        char *buf = NULL; \
        asprintf(&buf, "%s:%d (%s) -%s%s %s%s", _MYFILE_, __LINE__, __func__, syserr_str ? : "", glerr_str ? : "", buf0, log_end); \
        \
        _LOG_CMD(buf); \
        \
        free(buf0); \
        free(buf); \
        if (syserr_str) { \
            free(syserr_str); \
        } \
        if (glerr_str) { \
            free(glerr_str); \
        } \
    } while (0)

#ifndef NDEBUG

#ifdef ANDROID
// Apparently some non-conformant Android devices (ahem, Spamsung, ahem) do not actually let me see what the assert
// actually was before aborting/segfaulting ...
#   undef assert
#   define assert(e) \
    do { \
        if ((e)) { \
            /* ... */ \
        } else { \
            LOG( "!!! ASSERT !!! : " #e ); \
            sleep(1); \
            __assert2(_MYFILE_, __LINE__, __func__, #e); \
        } \
    } while (0)
#endif

#define LOG(...) \
    if (do_logging) { \
        errno = 0; \
        GLenum _glerr = 0; \
        _LOG(__VA_ARGS__); \
    } //

#define ERRLOG(...) \
    if (do_logging) { \
        GLenum _glerr = safeGLGetError(); \
        _LOG(__VA_ARGS__); \
        while ( (_glerr = safeGLGetError()) ) { \
            _LOG(__VA_ARGS__); \
        } \
    } //

#define GL_ERRLOG(...) \
    if (do_logging) { \
        GLenum _glerr = 0; \
        while ( (_glerr = safeGLGetError()) ) { \
            _LOG(__VA_ARGS__); \
        } \
    } //

#define ERRQUIT(...) \
    do { \
        GLenum _glerr = safeGLGetError(); \
        _LOG(__VA_ARGS__); \
        while ( (_glerr = safeGLGetError()) ) { \
            _LOG(__VA_ARGS__); \
        } \
        QUIT_FUNCTION(1); \
    } while (0)

#define GL_ERRQUIT(...) \
    do { \
        GLenum _glerr = 0; \
        while ( (_glerr = safeGLGetError()) ) { \
            _LOG(__VA_ARGS__); \
            QUIT_FUNCTION(_glerr); \
        } \
    } while (0)

#else // NDEBUG

#define ERRLOG(...) \
    do { } while (0)

#define ERRQUIT(...) \
    do { } while (0)

#define LOG(...) \
    do { } while (0)

#define GL_ERRLOG(...) \
    do { } while (0)

#define GL_ERRQUIT(...) \
    do { } while (0)

#endif // NDEBUG

#define RELEASE_ERRLOG(...) \
    do { \
        GLenum _glerr = 0; \
        _LOG(__VA_ARGS__); \
    } while (0)

#define RELEASE_LOG(...) \
    do { \
        GLenum _glerr = safeGLGetError(); \
        errno = 0; \
        _LOG(__VA_ARGS__); \
    } while (0)

#define RELEASE_BREAK() \
    do { \
        /* BOOM */ \
        char *ptr = (char *)0xABADF000; \
        *ptr = '\0';\
        /* or if that worked, just deref NULL */ \
        ptr = (char *)0x0; \
        *ptr = '\0'; \
    } while (0);

#define FREE(x) \
    do { \
        free((x)); \
        (x) = NULL; \
    } while (0)

#ifdef __APPLE__
#define CFRELEASE(x) \
    do { \
        CFRelease((x)); \
        (x) = NULL; \
    } while (0)
#endif

// branch prediction
#define LIKELY(x)   __builtin_expect((x), true)
#define UNLIKELY(x) __builtin_expect((x), false)

#endif // whole file
