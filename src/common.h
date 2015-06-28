/*
 * Apple // emulator for *nix
 *
 * This software package is subject to the GNU General Public License
 * version 2 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * THERE ARE NO WARRANTIES WHATSOEVER.
 *
 */

#ifndef _COMMON_H_
#define _COMMON_H_

#if defined(__GNUC__) && !defined(_GNU_SOURCE)
#   define _GNU_SOURCE
#endif

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

#ifdef __APPLE__
#include "darwin-shim.h"
#import <CoreFoundation/CoreFoundation.h>
#endif

// custom annotations
#define INPARM
#define OUTPARM
#define INOUT
#define PRIVATE
#define PUBLIC

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
#include "../Android/jni/android_globals.h"
#endif

#define PATH_SEPARATOR "/" // =P

#if !defined(MIN)
#define MIN(a,b) (((a) <= (b)) ? (a) : (b))
#endif

#if !defined(MAX)
#define MAX(a,b) (((a) >= (b)) ? (a) : (b))
#endif

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
#   define _LOG_CMD(str) fprintf(error_log, "%s", str)
#endif

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
        asprintf(&buf, "%s:%d -%s%s %s%s", __FILE__, __LINE__, syserr_str ? : "", glerr_str ? : "", buf0, log_end); \
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
