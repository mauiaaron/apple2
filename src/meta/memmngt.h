/*
 * Apple // emulator for *ix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 2015-2016 Aaron Culliney
 *
 */

#ifndef _MEMMNGT_H_
#define _MEMMNGT_H_

// Simple memory management routines

#define _FREE(ptr, free_func) \
    do { \
        free_func((ptr)); \
        /* WARNING : code may depend on NULLification, even in release builds */ \
        (ptr) = NULL; \
    } while (0)

// A way to formalize intentional leaks
#define LEAK(ptr) \
    do { \
        /* WARNING : code may depend on NULLification, even in release builds */ \
        (ptr) = NULL; \
    } while (0)

#ifdef NDEBUG
#   define MALLOC(size)         malloc((size))
#   define CALLOC(nmemb, size)  calloc((nmemb), (size))
#   define REALLOC(ptr, size)   realloc((ptr), (size))
#   define STRDUP(str)          strdup((str))
#   define STRNDUP(str, n)      strndup((str), (n))
#   define ASPRINTF(s, f, ...)  asprintf((s), (f), __VA_ARGS__)
#   define FREE(ptr)            _FREE((ptr), free)
#else

// NOTE: debug builds use a simplistic inline *alloc() fence and a scribbling free() to pinpoint out-of-bounds heap
// writes.  We still need to use Valgrind to pinpoint oob-reads and other issues =)

#   define MALLOC(size)         _a2_malloc((size))
#   define CALLOC(nmemb, size)  _a2_calloc((nmemb), (size))
#   define REALLOC(ptr, size)   _a2_realloc((ptr), (size))
#   define STRDUP(str)          _a2_strndup((str), 0)
#   define STRNDUP(str, n)      _a2_strndup((str), (n))
#   define ASPRINTF(s, f, ...)  _a2_asprintf((s), (f), __VA_ARGS__)
#   define FREE(ptr)            _FREE((ptr), _a2_free)

#   define _BUF_SENTINEL 0xDEADC0DEUL
#   define _BUF_FENCE_SZ (sizeof(_BUF_SENTINEL))

void *_a2_malloc(size_t size);

void *_a2_calloc(size_t nmemb, size_t size);

void _a2_free(void *ptr);

void *_a2_realloc(void *ptr, size_t size);

char *_a2_strndup(const char *s, size_t size);

int _a2_asprintf(char **strp, const char *fmt, ...);

#endif

#if TARGET_OS_MAC || TARGET_OS_PHONE
#define CFRELEASE(x) \
    do { \
        CFRelease((x)); \
        (x) = NULL; \
    } while (0)
#endif

#endif // whole file

