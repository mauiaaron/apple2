#ifndef _ZLIB_HELPERS_H_
#define _ZLIB_HELPERS_H_

#include "common.h"

const char* const inf(const char* const source); // source : foo.dsk.gz --> foo.dsk
const char* const def(const char* const source); // source : foo.dsk    --> foo.dsk.gz

#endif
