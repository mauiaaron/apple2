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
#else
#define GLenum int
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
    }

#define ERRLOG(...) \
    if (do_logging) { \
        GLenum _glerr = glGetError(); \
        _LOG(__VA_ARGS__); \
        while ( (_glerr = glGetError()) ) { \
            _LOG(__VA_ARGS__); \
        } \
    }

#define GL_ERRLOG(...) \
    if (do_logging) { \
        GLenum _glerr = 0; \
        while ( (_glerr = glGetError()) ) { \
            _LOG(__VA_ARGS__); \
        } \
    }

#define ERRQUIT(...) \
    do { \
        GLenum _glerr = glGetError(); \
        _LOG(__VA_ARGS__); \
        while ( (_glerr = glGetError()) ) { \
            _LOG(__VA_ARGS__); \
        } \
        QUIT_FUNCTION(1); \
    } while(0)

#define GL_ERRQUIT(...) \
    do { \
        GLenum _glerr = 0; \
        while ( (_glerr = glGetError()) ) { \
            _LOG(__VA_ARGS__); \
            QUIT_FUNCTION(_glerr); \
        } \
    } while(0)

#else // NDEBUG

#define ERRLOG(...) \
    do { } while(0);

#define ERRQUIT(...) \
    do { } while(0);

#define LOG(...) \
    do { } while(0);

#endif // NDEBUG

#define RELEASE_ERRLOG(...) \
    do { \
        _LOG(__VA_ARGS__); \
    } while (0);

#define RELEASE_LOG(...) \
    do { \
        GLenum _glerr = glGetError(); \
        errno = 0; \
        _LOG(__VA_ARGS__); \
    } while(0);

#define FREE(x) \
    do { \
        free((x)); \
        (x) = NULL; \
    } while (0);

#ifdef __APPLE__
#define CFRELEASE(x) \
    do { \
        CFRelease((x)); \
        (x) = NULL; \
    } while (0);
#endif

#endif // whole file
