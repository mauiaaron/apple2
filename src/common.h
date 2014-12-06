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
#include "uthash.h"
#include "zlib-helpers.h"


#if VIDEO_OPENGL
#include "video_util/glUtil.h"
#define CRASH_APP_ON_LOAD_BECAUSE_YAY_GJ_APPLE 0
// 2014/11/29 -- Yay GJ Apple! ... you would think that early app lifecycle calls to glGetError() would not segfault on Macs
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

#if !defined(MIN)
#define MIN(a,b) (((a) <= (b)) ? (a) : (b))
#endif

#if !defined(MAX)
#define MAX(a,b) (((a) >= (b)) ? (a) : (b))
#endif

extern bool do_logging;
extern FILE *error_log;

#define QUIT_FUNCTION(x) exit(x)

#define _LOG(...) \
    int _err = errno; \
    errno = 0; \
    fprintf(error_log, "%s:%d - ", __FILE__, __LINE__); \
    fprintf(error_log, __VA_ARGS__); \
    if (_err) { \
        fprintf(error_log, " (syserr: %s)", strerror(_err)); \
    } \
    if (_glerr) { \
        fprintf(error_log, " (OOPS glerr:%04X)", _glerr); \
    } \
    fprintf(error_log, "\n");

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

#endif // whole file
