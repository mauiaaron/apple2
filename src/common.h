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

#ifndef NDEBUG
#       if defined(__GNUC__)
#               pragma GCC diagnostic push
#               pragma GCC diagnostic ignored "-Wunused-variable"
#       elif defined(__clang__)
#               pragma clang diagnostic push
#               pragma clang diagnostic ignored "-Wunused-variable"
#       endif

static FILE *error_log=0;
#define ERRLOG(/* err message format string, args */...)                                \
    do {                                                                                \
        int saverr = errno; errno = 0;                                                  \
        fprintf(error_log ? error_log : stderr, "%s:%d - ", __FILE__, __LINE__);        \
        fprintf(error_log ? error_log : stderr, __VA_ARGS__);                           \
        if (saverr) {                                                                   \
            fprintf(error_log ? error_log : stderr, " (syserr: %s)", strerror(saverr)); \
        }                                                                               \
        fprintf(error_log ? error_log : stderr, "\n");                                  \
    } while(0);

#define ERRQUIT(...) \
    do { \
        ERRLOG(__VA_ARGS__); \
        exit(1); \
    } while(0);

#else // NDEBUG

#if defined(__GNUC__)
#   pragma GCC diagnostic pop
#elif defined(__clang__)
#   pragma clang diagnostic pop
#endif

#define ERRLOG(...) \
    do \
    { \
    } while(0);

#endif

#define LOG(...) \
    do { \
        errno = 0; \
        ERRLOG(__VA_ARGS__); \
    } while(0);

#define Free(X) \
    do { \
        free(X); \
        X=NULL; \
    } while (0);

#endif // whole file
