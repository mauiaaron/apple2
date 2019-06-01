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

// custom annotations
#define INOUT
#define INPARM
#define _NONNULL
#define _NULLABLE
#define OUTPARM
#define PRIVATE
#define PUBLIC
#define READONLY

#define CALL_ON_UI_THREAD   // function should only be called on UI thread
#define ASSERT_ON_UI_THREAD() \
    assert(video_isRenderThread())
#define ASSERT_NOT_ON_UI_THREAD() \
    assert(!video_isRenderThread())

#define CALL_ON_CPU_THREAD  // function should only be called on CPU thread
#define ASSERT_ON_CPU_THREAD() \
    assert(timing_isCPUThread())
#define ASSERT_NOT_ON_CPU_THREAD() \
    assert(!timing_isCPUThread())

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

#include "meta/systrace.h"

#if __APPLE__
#   include "meta/darwin-shim.h"
#   if TARGET_OS_MAC || TARGET_OS_PHONE
#       import <CoreFoundation/CoreFoundation.h>
#   endif
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

#define SPINLOCK_INIT 0L

/*
   https://gcc.gnu.org/onlinedocs/gcc/_005f_005fsync-Builtins.html

   "In most cases, these built-in functions are considered a full barrier. That is, no memory operand is moved across
   the operation, either forward or backward. Further, instructions are issued as necessary to prevent the processor
   from speculating loads across the operation and from queuing stores after the operation."
 */
#define SPIN_LOCK_FULL(x) \
    do { \
        long prev = __sync_fetch_and_or((x), 1L); \
        if (prev == SPINLOCK_INIT) { \
            break; \
        } \
        usleep(1); \
    } while (1)

#define SPIN_UNLOCK_FULL(x) \
    __sync_fetch_and_and((x), SPINLOCK_INIT)

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

#if !defined(TEMP_FAILURE_RETRY)
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


#endif // whole file
