#ifndef _ZLIB_HELPERS_H_
#define _ZLIB_HELPERS_H_

#include "common.h"

const char* inf(const char* const src, int *rawcount); // src : foo.dsk.gz --> foo.dsk
const char* def(const char* const src, const int expected_bytes); // src : foo.dsk    --> foo.dsk.gz

#endif
