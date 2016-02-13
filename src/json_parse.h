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

#ifndef _JSON_PARSE_H_
#define _JSON_PARSE_H_ 1

#include "common.h"
#include "../externals/jsmn/jsmn.h"

typedef struct JSON_s {
    char *jsonString;
    int numTokens;
    jsmntok_t *jsonTokens;
} JSON_s;

// parses string into tokens.  returns positive token count or negative jsmnerr_t error code.
int json_createFromString(const char *jsonString, INOUT JSON_s *parsedData);

// parses file into string and tokens.  returns positive token count or negative jsmnerr_t error code.
int json_createFromFile(const char *filePath, INOUT JSON_s *parsedData);

// ----------------------------------------------------------------------------
// map functions

// get string value for key in map JSON, returns true upon success and strdup()'d value in *val
bool json_mapCopyStringValue(const JSON_s *map, const char *key, INOUT char **val);

// get int value for key in map JSON, returns true upon success
bool json_mapParseLongValue(const JSON_s *dict, const char *key, INOUT long *val, const int base);

// get float value for key in map JSON, returns true upon success
bool json_mapParseFloatValue(const JSON_s *dict, const char *key, INOUT float *val);

// ----------------------------------------------------------------------------
// array functions

//bool json_arrayCopytStringValueAtIndex(const JSON_s *array, unsigned long index, INOUT const char **val);
//bool json_arrayParseIntValueAtIndex(const JSON_s *array, unsigned long index, INOUT const int *val);
//bool json_arrayParseFloatValueAtIndex(const JSON_s *array, unsigned long index, INOUT const float *val);

// destroys internal allocated memory (if any)
void json_destroy(JSON_s *parsedData);

#endif
