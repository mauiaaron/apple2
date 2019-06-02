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

#include "common.h"

#include <stdarg.h>

#ifndef NDEBUG

void *_a2_malloc(size_t size) {
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

void *_a2_calloc(size_t nmemb, size_t size) {
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

void *_a2_realloc(void *ptr, size_t size) {
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

void _a2_free(void *ptr) {
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

char *_a2_strndup(const char *str, size_t len) {
    if (len == 0) {
        len = strlen(str);
    }
    char *dst = NULL;

    do {
        dst = (char *)_a2_malloc(len+1);
        memcpy(dst, str, len);
        *(dst+len) = '\0';
    } while (0);

    return dst;
}

int _a2_asprintf(char **strp, const char *fmt, ...) {

    char *strp0 = NULL;
    int ret = -1;

    va_list args;
    va_start(args, fmt);
    ret = vasprintf(&strp0, fmt, args);
    va_end(args);

    if (ret > 0) {
        assert(*strp0);
        assert((uintptr_t)strp);
        *strp = _a2_malloc(ret+1);
        assert((uintptr_t)*strp);
        memcpy(*strp, strp0, ret);
        *((*strp)+ret) = '\0';
        free(strp0);
    }

    return ret;
}

#endif // NDEBUG

