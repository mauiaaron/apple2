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
#define INOUT
#define INPARM
#define _NONNULL
#define _NULLABLE
#define OUTPARM
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

#include "json_parse.h"
#include "misc.h"
#include "vm.h"
#include "timing.h"
#include "cpu.h"
#include "interface.h"
#include "display.h"
#include "video/video.h"
#include "disk.h"
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

#if VIDEO_OPENGL
#   include "video_util/glUtil.h"
#endif

#include "meta/log.h"
#include "meta/debug.h"

#include "audio/soundcore.h"
#include "audio/speaker.h"
#include "audio/mockingboard.h"

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


// memory management
#include "meta/memmngt.h"

// branch prediction
#define LIKELY(x)   __builtin_expect((x), true)
#define UNLIKELY(x) __builtin_expect((x), false)

#endif // whole file
