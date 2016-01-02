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

#ifndef _MEMMNGT_H_
#define _MEMMNGT_H_

// Simple memory management routines

#define _FREE(ptr, free_func) \
    do { \
        free_func((ptr)); \
        /* WARNING : code may depend on NULLification, even in release builds */ \
        (ptr) = NULL; \
    } while (0)

#define ASPRINTF_FREE(ptr) _FREE((ptr), free)
#define STRDUP_FREE(ptr) _FREE((ptr), free)

#ifdef NDEBUG
#   define MALLOC(size)         malloc((size))
#   define CALLOC(nmemb, size)  calloc((nmemb), (size))
#   define REALLOC(ptr, size)   realloc((ptr), (size))
#   define FREE(ptr)            _FREE((ptr), free)
#else

// NOTE: debug builds use a simplistic inline *alloc() fence and a scribbling free() to pinpoint out-of-bounds heap
// writes.  We still need to use Valgrind to pinpoint oob-reads and other issues =)

#   define MALLOC(size)         _a2_malloc((size))
#   define CALLOC(nmemb, size)  _a2_calloc((nmemb), (size))
#   define REALLOC(ptr, size)   _a2_realloc((ptr), (size))
#   define FREE(ptr)            _FREE((ptr), _a2_free)

#   define _BUF_SENTINEL 0xDEADC0DEUL
#   define _BUF_FENCE_SZ (sizeof(_BUF_SENTINEL))

static inline void *_a2_malloc(size_t size) {
    const size_t totalSize = sizeof(size_t)+_BUF_FENCE_SZ+size+_BUF_FENCE_SZ;
    char *p = (char *)malloc(totalSize);
    if (p) {
        *((size_t *)p) = totalSize;
        *((uint32_t *)(p+sizeof(size_t))) = _BUF_SENTINEL;
        *((uint32_t *)(p+totalSize-_BUF_FENCE_SZ)) = _BUF_SENTINEL;
        p += sizeof(size_t)+_BUF_FENCE_SZ;
    }
    return p;
}

static inline void *_a2_calloc(size_t nmemb, size_t size) {
    size *= nmemb;
    const size_t totalSize = sizeof(size_t)+_BUF_FENCE_SZ+size+_BUF_FENCE_SZ;
    char *p = (char *)calloc(totalSize, 1);
    if (p) {
        *((size_t *)p) = totalSize;
        *((uint32_t *)(p+sizeof(size_t))) = _BUF_SENTINEL;
        *((uint32_t *)(p+totalSize-_BUF_FENCE_SZ)) = _BUF_SENTINEL;
        p += sizeof(size_t)+_BUF_FENCE_SZ;
    }
    return p;
}

static inline void _a2_free(void *ptr) {
    char *p = (char *)ptr;
    if (!p) {
        return;
    }
    p = p-_BUF_FENCE_SZ-sizeof(size_t);
    const size_t totalSize = *((size_t *)p);
    assert( *((uint32_t *)(p+sizeof(size_t))) == _BUF_SENTINEL && "1st memory sentinel invalid!" );
    assert( *((uint32_t *)(p+totalSize-_BUF_FENCE_SZ)) == _BUF_SENTINEL && "2nd memory sentinel invalid!" );
    memset(p, 0xAA, totalSize);
    free(p);
}

static inline void *_a2_realloc(void *ptr, size_t size) {
    char *p = (char *)ptr;
    if (!p) {
        return _a2_malloc(size);
    }
    if (size == 0) {
        FREE(ptr);
        return NULL;
    }

    // verify prior allocation is sane
    p = p-_BUF_FENCE_SZ-sizeof(size_t);
    const size_t totalSizeBefore = *((size_t *)p);
    assert( *((uint32_t *)(p+sizeof(size_t))) == _BUF_SENTINEL && "1st memory sentinel invalid!" );
    assert( *((uint32_t *)(p+totalSizeBefore-_BUF_FENCE_SZ)) == _BUF_SENTINEL && "2nd memory sentinel invalid!" );

    const size_t totalSizeAfter = sizeof(size_t)+_BUF_FENCE_SZ+size+_BUF_FENCE_SZ;
    assert(totalSizeAfter > totalSizeBefore && "FIXME fenced realloc() to smaller sizes not implemented!");

    p = (char *)realloc(p, totalSizeAfter);
    if (p) {
        *((size_t *)p) = totalSizeAfter;
        assert( *((uint32_t *)(p+sizeof(size_t))) == _BUF_SENTINEL && "1st memory sentinel invalid!" );
        *((uint32_t *)(p+totalSizeAfter-_BUF_FENCE_SZ)) = _BUF_SENTINEL;
        p += sizeof(size_t)+_BUF_FENCE_SZ;
    }

    return p;
}

#endif

#ifdef __APPLE__
#define CFRELEASE(x) \
    do { \
        CFRelease((x)); \
        (x) = NULL; \
    } while (0)
#endif

#endif // whole file

