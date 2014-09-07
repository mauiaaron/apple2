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

#define _LOG(...) \
    int saverr = errno; \
    errno = 0; \
    fprintf(error_log, "%s:%d - ", __FILE__, __LINE__); \
    fprintf(error_log, __VA_ARGS__); \
    if (saverr) { \
        fprintf(error_log, " (syserr: %s)", strerror(saverr)); \
    } \
    fprintf(error_log, "\n");

#ifndef NDEBUG

#define ERRLOG(...) \
    if (do_logging) { \
        _LOG(__VA_ARGS__); \
    }

#define ERRQUIT(...) \
    if (do_logging) { \
        _LOG(__VA_ARGS__); \
    } \
    exit(1);

#define LOG(...) \
    if (do_logging) { \
        errno = 0; \
        _LOG(__VA_ARGS__); \
    }

#else // NDEBUG

#define ERRLOG(...) \
    do { } while(0);

#define ERRQUIT(...) \
    do { } while(0);

#define LOG(...) \
    do { } while(0);

#endif

#define RELEASE_ERRLOG(...) \
    do { \
        _LOG(__VA_ARGS__); \
    } while (0);

#define RELEASE_LOG(...) \
    do { \
        errno = 0; \
        _LOG(__VA_ARGS__); \
    } while(0);

#define FREE(x) \
    do { \
        free((x)); \
        (x) = NULL; \
    } while (0);

#endif // whole file
