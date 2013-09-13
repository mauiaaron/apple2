/*
 * Apple // emulator : misc defines
 *
 * Copyright 2013 Aaron Culliney
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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#ifndef NDEBUG

#if defined(__GNUC__)
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wunused-variable"
#elif defined(__clang__)
#   pragma clang diagnostic push
#   pragma clang diagnostic ignored "-Werror=unused-variable"
#endif

static FILE *error_log=0;
#define ERRLOG(/* err message format string, args */...)                                \
    {                                                                                   \
        int saverr = errno; errno = 0;                                                  \
        fprintf(error_log ? error_log : stderr, "%s:%d - ", __FILE__, __LINE__);        \
        fprintf(error_log ? error_log : stderr, __VA_ARGS__);                           \
        if (saverr) {                                                                   \
            fprintf(error_log ? error_log : stderr, " (syserr: %s)", strerror(saverr)); \
        }                                                                               \
        fprintf(error_log ? error_log : stderr, "\n");                                  \
    }

#define ERRQUIT(...) \
    { \
        ERRLOG(__VA_ARGS__); \
        exit(0); \
    }

#else // NDEBUG

#if defined(__GNUC__)
#   pragma GCC diagnostic pop
#elif defined(__clang__)
#   pragma clang diagnostic pop
#endif

#define ERRLOG(...)                                                                     \
    do                                                                                  \
    {                                                                                   \
    } while(0);

#endif

#define LOG(...) ERRLOG(__VA_ARGS__)

static void inline Free(void *x) { free(x); x=NULL; }

#endif // whole file
