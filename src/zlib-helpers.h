/*
 * Apple // emulator for *ix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 2013-2017 Aaron Culliney
 *
 */

#ifndef _ZLIB_HELPERS_H_
#define _ZLIB_HELPERS_H_

#include "common.h"

// augmented error codes/strings (not actually from zlib routines)
#define ZERR_UNKNOWN "Unknown zlib error"
#define ZERR_INFLATE_OPEN_DEST   "Error opening destination file for inflation"
#define ZERR_INFLATE_WRITE_DEST  "Error writing destination file for inflation"
#define ZERR_DEFLATE_OPEN_SOURCE "Error opening source file for deflation"
#define ZERR_DEFLATE_READ_SOURCE "Error reading source file for deflation"

// inflate/uncompress from file descriptor into previously allocated buffer of expected_bytes length
const char *zlib_inflate_to_buffer(int fd, const int expected_bytescount, char *buf);

// inflate/uncompress from file descriptor back into itself
const char *zlib_inflate_inplace(int fd, const int expected_bytescount, bool *is_gzipped);

// inline deflate/compress from buffer to buffer
const char *zlib_deflate_buffer(const char *src, const int src_bytescount, char *dst, OUTPARM off_t *dst_size);

#endif
