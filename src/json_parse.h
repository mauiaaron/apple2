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

#ifndef _JSON_PARSE_H_
#define _JSON_PARSE_H_ 1

#include "common.h"
#include "../externals/jsmn/jsmn.h"

// opaque type
typedef const struct JSON_s *JSON_ref;

// ----------------------------------------------------------------------------
// constructors

// parses string into tokens.  returns positive token count or negative jsmnerr_t error code.
int json_createFromString(const char *jsonString, INOUT JSON_ref *jsonRef);

// parses file into string and tokens.  returns positive token count or negative jsmnerr_t error code.
int json_createFromFile(const char *filePath, INOUT JSON_ref *jsonRef);

// parses FD into string and tokens.  returns positive token count or negative jsmnerr_t error code.
int json_createFromFD(int fd, INOUT JSON_ref *jsonRef);

// ----------------------------------------------------------------------------
// map functions

// get JSON_ref value for key in map JSON, returns true upon success and allocated JSON_ref
int json_mapCopyJSON(const JSON_ref map, const char *key, INOUT JSON_ref *val);

// get string value for key in map JSON, returns true upon success and strdup()'d value in *val
bool json_mapCopyStringValue(const JSON_ref map, const char *key, INOUT char **val);

// get long value for key in map JSON, returns true upon success
bool json_mapParseLongValue(const JSON_ref map, const char *key, INOUT long *val, const long base);

// get bool value for key in map JSON, returns true upon success
bool json_mapParseBoolValue(const JSON_ref map, const char *key, INOUT bool *val);

// get float value for key in map JSON, returns true upon success
bool json_mapParseFloatValue(const JSON_ref map, const char *key, INOUT float *val);

// set string value for key in map JSON, returns true upon success
bool json_mapSetStringValue(const JSON_ref map, const char *key, const char *val);

// set raw string (does not add quotes) value for key in map JSON, returns true upon success
bool json_mapSetRawStringValue(const JSON_ref map, const char *key, const char *val);

// set JSON_ref value for key in map JSON, returns true upon success
bool json_mapSetJSONValue(const JSON_ref map, const char *key, const JSON_ref val);

// set long value for key in map JSON, returns true upon success
bool json_mapSetLongValue(const JSON_ref map, const char *key, long val);

// set bool value for key in map JSON, returns true upon success
bool json_mapSetBoolValue(const JSON_ref map, const char *key, bool val);

// set float value for key in map JSON, returns true upon success
bool json_mapSetFloatValue(const JSON_ref map, const char *key, float val);

// ----------------------------------------------------------------------------
// array functions

//bool json_arrayCopyStringValueAtIndex(const JSON_ref array, unsigned long index, INOUT const char **val);
//bool json_arrayParseLongValueAtIndex(const JSON_ref array, unsigned long index, INOUT const long *val, const long base);
//bool json_arrayParseFloatValueAtIndex(const JSON_ref array, unsigned long index, INOUT const float *val);

// ----------------------------------------------------------------------------
// serialization

// serialize to file descriptor
bool json_serialize(JSON_ref json, int fd, bool pretty);

// ----------------------------------------------------------------------------
// destructor
void json_destroy(JSON_ref *jsonRef);

#endif
