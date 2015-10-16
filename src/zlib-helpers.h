#ifndef _ZLIB_HELPERS_H_
#define _ZLIB_HELPERS_H_

#include "common.h"

// augmented error codes/strings (not actually from zlib routines)
#define ZERR_UNKNOWN "Unknown zlib error"
#define ZERR_INFLATE_OPEN_DEST   "Error opening destination file for inflation"
#define ZERR_INFLATE_WRITE_DEST  "Error writing destination file for inflation"
#define ZERR_DEFLATE_OPEN_SOURCE "Error opening source file for deflation"
#define ZERR_DEFLATE_READ_SOURCE "Error reading source file for deflation"

const char* zlib_inflate(const char* const src, const int expected_bytes); // src : foo.dsk.gz --> foo.dsk
const char* zlib_deflate(const char* const src, const int expected_bytes); // src : foo.dsk    --> foo.dsk.gz

#endif
