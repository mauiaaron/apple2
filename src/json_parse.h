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

#include "common.h"
#include "../externals/jsmn/jsmn.h"

typedef struct JSON_s {
    char *jsonString;
    jsmntok_t *jsonTokens;
} JSON_s;

// parses file into string and tokens.  returns positive token count or negative jsmnerr_t error code.
int json_createFromFile(const char *filePath, INOUT JSON_s *parsedData);

// destroys internal allocated memory (if any)
void json_destroy(JSON_s *parsedData);

