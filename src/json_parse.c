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
#include "json_parse_private.h"

#define JSON_LENGTH 16
#define DEFAULT_NUMTOK 16
#define MAX_INDENT 16
#define STACK_BUFSIZ 256

#define _JSMN_NONSTRING (JSMN_OBJECT)

#define QUOTE "\""
#define QUOTE_LEN (sizeof(QUOTE)-1)
#define COLON ":"
#define COLON_LEN (sizeof(COLON)-1)
#define COMMA ","
#define COMMA_LEN (sizeof(COMMA)-1)

#define DEBUG_JSON 1
#if DEBUG_JSON
#   define JSON_LOG(...) LOG(__VA_ARGS__)
#else
#   define JSON_LOG(...)
#endif

#define ASSERT_TOKEN_VALID(TOK) \
    do { \
        assert((TOK).start >= 0 && "bug"); \
        assert((TOK).end >= 0 && "bug"); \
        assert((TOK).end >= (TOK).start && "bug"); \
    } while (0)

static bool _json_write(const char *jsonString, size_t buflen, int fd) {
    ssize_t idx = 0;
    size_t chunk = buflen;
    do {
        ssize_t outlen = 0;
        TEMP_FAILURE_RETRY(outlen = write(fd, jsonString+idx, chunk));
        if (outlen <= 0) {
            break;
        }
        idx += outlen;
        chunk -= outlen;
    } while (idx < buflen);
    return idx == buflen;
}

static char *_json_copyAndQuote(const char *str, size_t *newLen) {

    char *qCopy = NULL;
    do {
        if (!str) {
            break;
        }

        size_t len = strlen(str);

        qCopy = MALLOC(QUOTE_LEN + len + QUOTE_LEN + 1);
        if (!qCopy) {
            break;
        }
        *newLen = QUOTE_LEN + len + QUOTE_LEN;

        char *p = qCopy;
        memcpy(p, QUOTE, QUOTE_LEN);
        p += QUOTE_LEN;
        memcpy(p, str, len);
        p += len;
        memcpy(p, QUOTE, QUOTE_LEN);
        p += QUOTE_LEN;
        *p = '\0';
    } while (0);

    return qCopy;
}

// recursive
static bool _json_prettyPrint(JSON_s *parsedData, int start, int end, const unsigned int indent, int fd) {

    char indentBuf[MAX_INDENT+1];
    indentBuf[MAX_INDENT] = '\0';

    bool success = false;
    do {
        if (indent > MAX_INDENT) {
            break;
        }
        memset(indentBuf, '\t', indent);

        jsmntok_t parent = { -1 };
        int idx = start;
        if (idx < end) {
            jsmntok_t tok = parsedData->jsonTokens[idx];
            if (tok.parent >= 0) {
                parent = parsedData->jsonTokens[tok.parent];
            }
        }

        bool isKey = true;

        while (idx < end) {

            jsmntok_t tok = parsedData->jsonTokens[idx];
            bool isFirst = (idx == start);

            // print finishing ", \n" stuff ...
            if (parent.type == JSMN_OBJECT) {
                if (!isKey) {
                    if (!_json_write(" : ", 3, fd)) {
                        break;
                    }
                } else {
                    if (!isFirst) {
                        if (!_json_write(",\n", 2, fd)) {
                            break;
                        }
                    }
                    if (!_json_write(indentBuf, indent, fd)) {
                        break;
                    }
                }
            } else if (parent.type == JSMN_ARRAY) {
                if (!isFirst) {
                    if (!_json_write(", ", 2, fd)) {
                        break;
                    }
                }
            }

            jsmntype_t type = parsedData->jsonTokens[idx].type;

            if (type == JSMN_PRIMITIVE) {
                char lastChar = parsedData->jsonString[tok.end];
                parsedData->jsonString[tok.end] = '\0';
                if (!_json_write(&parsedData->jsonString[tok.start], tok.end-tok.start, fd)) {
                    break;
                }
                parsedData->jsonString[tok.end] = lastChar;
                ++idx;
            } else if (type == JSMN_STRING) {
                char lastChar = parsedData->jsonString[tok.end];
                parsedData->jsonString[tok.end] = '\0';
                if (!_json_write("\"", 1, fd)) {
                    break;
                }
                if (!_json_write(&parsedData->jsonString[tok.start], tok.end-tok.start, fd)) {
                    break;
                }
                if (!_json_write("\"", 1, fd)) {
                    break;
                }
                parsedData->jsonString[tok.end] = lastChar;
                ++idx;
            } else if (type == JSMN_OBJECT) {
                if (!_json_write("{\n", 2, fd)) {
                    break;
                }
                if (!_json_prettyPrint(parsedData, idx+1, idx+tok.skip, indent+1, fd)) {
                    break;
                }
                if (!_json_write(indentBuf, indent, fd)) {
                    break;
                }
                if (!_json_write("}", 1, fd)) {
                    break;
                }
                idx += tok.skip;
            } else if (type == JSMN_ARRAY) {
                if (!_json_write("[ ", 2, fd)) {
                    break;
                }
                if (!_json_prettyPrint(parsedData, idx+1, idx+tok.skip, indent+1, fd)) {
                    break;
                }
                if (!_json_write(" ]", 2, fd)) {
                    break;
                }
                idx += tok.skip;
            } else {
                assert(false);
            }

            isKey = !isKey;
        }

        if (idx != end) {
            break;
        }

        if (parent.type != JSMN_ARRAY) {
            if (!_json_write("\n", 1, fd)) {
                break;
            }
        }

        success = true;
    } while (0);

    return success;
}

static int _json_createFromString(const char *jsonString, INOUT JSON_ref *jsonRef, ssize_t jsonLen) {

    int errCount = JSMN_ERROR_NOMEM;
    do {
        jsmn_parser parser = { 0 };

        if (!jsonRef) {
            break;
        }

        JSON_s *parsedData = CALLOC(sizeof(*parsedData), 1);
        if (!parsedData) {
            break;
        }
        *jsonRef = parsedData;

        if (!parsedData) {
            break;
        }
        parsedData->jsonTokens = NULL;

        unsigned int numTokens = DEFAULT_NUMTOK;
        do {
            if (!parsedData->jsonTokens) {
                parsedData->jsonTokens = CALLOC(numTokens, sizeof(jsmntok_t));
                if (UNLIKELY(!parsedData->jsonTokens)) {
                    ERRLOG("WHOA3 : %s", strerror(errno));
                    break;
                }
            } else {
                //LOG("reallocating json tokens ...");
                numTokens <<= 1;
                jsmntok_t *newTokens = REALLOC(parsedData->jsonTokens, numTokens * sizeof(jsmntok_t));
                if (UNLIKELY(!newTokens)) {
                    ERRLOG("WHOA4 : %s", strerror(errno));
                    break;
                }
                memset(newTokens, '\0', numTokens * sizeof(jsmntok_t));
                parsedData->jsonTokens = newTokens;
            }
            jsmn_init(&parser);
            errCount = jsmn_parse(&parser, jsonString, jsonLen, parsedData->jsonTokens, numTokens);
        } while (errCount == JSMN_ERROR_NOMEM);

        if (errCount == JSMN_ERROR_NOMEM) {
            break;
        }

        if (errCount < 0) {
            ERRLOG("%s", "OOPS error parsing JSON : ");
            switch (errCount) {
                case JSMN_ERROR_NOMEM:
                    assert(0 && "should not happen");
                    break;
                case JSMN_ERROR_INVAL:
                    ERRLOG("%s", "Invalid character inside JSON string");
                    break;
                case JSMN_ERROR_PART:
                    ERRLOG("%s", "String is not a complete JSON packet, moar bytes expected");
                    break;
                case JSMN_ERROR_PRIMITIVE_INVAL:
                    ERRLOG("%s", "Invalid character inside JSON primitive");
                    break;
                default:
                    ERRLOG("UNKNOWN errCount : %d", errCount);
                    break;
            }
            break;
        }

        parsedData->jsonString = STRDUP(jsonString);
        parsedData->numTokens = errCount;
        parsedData->jsonLen = jsonLen;

    } while (0);

    if (errCount < 0) {
        if (*jsonRef) {
            json_destroy(jsonRef);
        }
    }

    return errCount;
}

int json_createFromFD(int fd, INOUT JSON_ref *jsonRef) {

    ssize_t jsonIdx = 0;
    ssize_t jsonLen = 0;

    char *jsonString = NULL;
    int errCount = JSMN_ERROR_NOMEM;

    do {
        if (!jsonRef) {
            break;
        }

        jsonLen = JSON_LENGTH*2;
        jsonString = MALLOC(jsonLen);
        if (UNLIKELY(jsonString == NULL)) {
            ERRLOG("WHOA : %s", strerror(errno));
            break;
        }

        ssize_t bytesRead = 0;
        do {
            TEMP_FAILURE_RETRY(bytesRead = read(fd, jsonString+jsonIdx, JSON_LENGTH));
            if (bytesRead < 0) {
                ERRLOG("Error reading file : %s", strerror(errno));
                break;
            }
            if (bytesRead) {
                jsonIdx += bytesRead;
                if (jsonLen - jsonIdx < JSON_LENGTH) {
                    //LOG("reallocating json string ...");
                    jsonLen <<= 1;
                    char *newString = REALLOC(jsonString, jsonLen);
                    if (UNLIKELY(!newString)) {
                        ERRLOG("WHOA2 : %s", strerror(errno));
                        bytesRead = -1;
                        break;
                    }
                    jsonString = newString;
                }
            }
        } while (bytesRead);

        if (bytesRead < 0) {
            break;
        }

        jsonLen = jsonIdx;
        // now parse the string
        errCount = _json_createFromString(jsonString, jsonRef, jsonLen);

    } while (0);

    if (jsonString) {
        FREE(jsonString);
    }

    return errCount;
}

int json_createFromFile(const char *filePath, INOUT JSON_ref *jsonRef) {

    int fd = -1;
    int errCount = JSMN_ERROR_NOMEM;
    do {
        if (!filePath) {
            break;
        }
        if (!jsonRef) {
            break;
        }

        TEMP_FAILURE_RETRY(fd = open(filePath, O_RDONLY));
        if (fd < 0) {
            ERRLOG("Error opening file : %s", strerror(errno));
            break;
        }

        errCount = json_createFromFD(fd, jsonRef);

    } while (0);

    if (fd > 0) {
        TEMP_FAILURE_RETRY(close(fd));
        fd = -1;
    }

    return errCount;
}

int json_createFromString(const char *jsonString, INOUT JSON_ref *jsonRef) {
    return _json_createFromString(jsonString, jsonRef, strlen(jsonString));
}

// ----------------------------------------------------------------------------

static bool _json_mapGetStringValue(const JSON_s *map, const char *key, INOUT int *index) {
    bool foundMatch = false;

    do {
        if (!index) {
            break;
        }

        *index = -1;

        if (!map) {
            break;
        }

        int tokCount = map->numTokens;
        if (tokCount <= 0) {
            break;
        }

        if (!key) {
            break;
        }

        int idx=0;
        const int keySize = strlen(key);

        // should begin as map ...
        if (map->jsonTokens[idx].type != JSMN_OBJECT) {
            ERRLOG("Map JSON : must start with begin map token");
            break;
        }
        ++idx;

        // Linear parse/search for key ...
        for (int skip=0; idx<tokCount; idx+=skip) {
            jsmntok_t keyTok = map->jsonTokens[idx];

            assert(keyTok.parent == 0);

            if (keyTok.type != JSMN_STRING) {
                ERRLOG("Map JSON : expecting a string key at map position %d", idx);
                break;
            }

            ASSERT_TOKEN_VALID(keyTok);
            const int size = keyTok.end - keyTok.start;

            ++idx;
            jsmntok_t valTok = map->jsonTokens[idx];
            if (size == keySize) {
                foundMatch = (strncmp(key, &map->jsonString[keyTok.start], size) == 0);
                if (foundMatch) {
                    *index = idx;
                    break;
                }
            }

            skip = valTok.skip;
        }
    } while (0);

    return foundMatch;
}

bool json_isMap(const JSON_ref jsonRef) {
    JSON_s *map = (JSON_s *)jsonRef;

    bool ret = false;
    do {
        if (!map) {
            break;
        }
        if (map->numTokens <= 0) {
            break;
        }
        ret = map->jsonTokens[0].type == JSMN_OBJECT;
    } while (0);

    return ret;
}

int json_mapCopyJSON(const JSON_ref jsonRef, const char *key, INOUT JSON_ref *val) {
    JSON_s *map = (JSON_s *)jsonRef;

    int idx = 0;
    int errCount = JSMN_ERROR_NOMEM;
    do {
        if (!val) {
            break;
        }

        bool foundMatch = _json_mapGetStringValue(map, key, &idx);
        if (!foundMatch) {
            break;
        }

        jsmntok_t tok = map->jsonTokens[idx];
        ASSERT_TOKEN_VALID(tok);
        int len = tok.end - tok.start;
        if (len<=0) {
            break;
        }

        char *str = STRNDUP(&map->jsonString[tok.start], len);
        if (!str) {
            break;
        }

        errCount = json_createFromString(str, val);
        assert(errCount >= 0);
        FREE(str);

    } while (0);

    return errCount;
}

bool json_mapCopyStringValue(const JSON_ref jsonRef, const char *key, INOUT char **val) {
    JSON_s *map = (JSON_s *)jsonRef;
    bool foundMatch = false;

    do {
        if (!val) {
            break;
        }

        int idx = 0;
        if (!_json_mapGetStringValue(map, key, &idx)) {
            break;
        }

        jsmntok_t tok = map->jsonTokens[idx];
        ASSERT_TOKEN_VALID(tok);
        int len = tok.end - tok.start;
        *val = len>0 ? STRNDUP(&map->jsonString[tok.start], len) : STRDUP("");

        foundMatch = true;

    } while (0);

    return foundMatch;
}

bool json_mapParseLongValue(const JSON_ref jsonRef, const char *key, INOUT long *val, const long base) {
    JSON_s *map = (JSON_s *)jsonRef;
    bool foundMatch = false;

    do {
        if (!val) {
            break;
        }

        int idx = 0;
        if (!_json_mapGetStringValue(map, key, &idx)) {
            break;
        }

        jsmntok_t tok = map->jsonTokens[idx];
        ASSERT_TOKEN_VALID(tok);
        int len = tok.end - tok.start;
        char *str = &map->jsonString[tok.start];
        char ch = str[len];
        str[len] = '\0';
        *val = strtol(str, NULL, base);
        str[len] = ch;

        foundMatch = true;
    } while (0);

    return foundMatch;
}

bool json_mapParseBoolValue(const JSON_ref jsonRef, const char *key, INOUT bool *val) {
    JSON_s *map = (JSON_s *)jsonRef;
    bool foundMatch = false;

    do {
        if (!val) {
            break;
        }

        int idx = 0;
        if (!_json_mapGetStringValue(map, key, &idx)) {
            break;
        }

        jsmntok_t tok = map->jsonTokens[idx];
        ASSERT_TOKEN_VALID(tok);
        int len = tok.end - tok.start;
        char *str = &map->jsonString[tok.start];
        char ch = str[len];
        str[len] = '\0';
        *val = (strncasecmp("false", str, sizeof("false")) != 0);
        str[len] = ch;

        foundMatch = true;
    } while (0);

    return foundMatch;
}

bool json_mapParseFloatValue(const JSON_ref jsonRef, const char *key, INOUT float *val) {
    JSON_s *map = (JSON_s *)jsonRef;
    bool foundMatch = false;

    do {
        if (!val) {
            break;
        }

        int idx = 0;
        if (!_json_mapGetStringValue(map, key, &idx)) {
            break;
        }

        jsmntok_t tok = map->jsonTokens[idx];
        ASSERT_TOKEN_VALID(tok);
        int len = tok.end - tok.start;
        char *str = &map->jsonString[tok.start];
        char ch = str[len];
        str[len] = '\0';
        *val = strtof(str, NULL);
        str[len] = ch;

        foundMatch = true;
    } while (0);

    return foundMatch;
}

// 2016/03/02 : HACK NOTE TODO FIXME : we do nasty string splicing because I haven't settled on a C library to use for
// collections.  It would be nice to be able to use something like CoreFoundation (CFArray, CFDictionary, etc), but
// don't believe the APSL license is GPL-compatible.  (Should investigate whether Swift OpenSource drop includes
// low-level C libs with a compatible FOSS license ;-)
static bool _json_mapSetValue(const JSON_ref jsonRef, const char *key, const char *val, jsmntype_t valType) {
    JSON_s *map = (JSON_s *)jsonRef;

    bool didSetValue = false;
    char *newVal = NULL;
    do {
        if (!map) {
            break;
        }
        if (!key) {
            break;
        }
        if (!val) {
            break;
        }

        if (map->jsonTokens[0].type != JSMN_OBJECT) {
            ERRLOG("Map JSON : object not a map!");
            break;
        }

        size_t spliceBegin = 0;
        size_t spliceEnd = 0;
        size_t valLen = strlen(val);

        int idx = 0;
        bool foundMatch = _json_mapGetStringValue(map, key, &idx);
        if (foundMatch) {
            jsmntok_t tok = map->jsonTokens[idx];
            ASSERT_TOKEN_VALID(tok);
            spliceBegin = tok.start;
            spliceEnd = tok.end;

            if (tok.type == JSMN_STRING) {
                if (valType == JSMN_STRING) {
                    // string -> string
                    assert(map->jsonString[spliceBegin-1] == '"');
                    assert(map->jsonString[spliceEnd] == '"');
                } else {
                    // string -> non-string (strip previous quotes)
                    --spliceBegin;
                    ++spliceEnd;
                    assert(map->jsonString[spliceBegin] == '"');
                    assert(map->jsonString[spliceEnd-1] == '"');
                }
            } else {
                if (valType == JSMN_STRING) {
                    // non-string -> string
                    newVal = _json_copyAndQuote(val, &valLen);
                    if (!newVal) {
                        break;
                    }
                    val = newVal;
                }
            }

        } else {
            jsmntok_t tok = map->jsonTokens[0];

            size_t keyLen = strlen(key);
            size_t quoLen = (valType == JSMN_STRING) ? QUOTE_LEN : 0;
            size_t comLen = tok.size ? COMMA_LEN : 0;

            int keyValLen = QUOTE_LEN + keyLen + QUOTE_LEN + COLON_LEN + quoLen + valLen + quoLen + comLen;
            newVal = MALLOC(keyValLen+1);
            if (!newVal) {
                break;
            }

            char *p = newVal;

            memcpy(p, QUOTE, QUOTE_LEN);
            p += QUOTE_LEN;
            memcpy(p, key, keyLen);
            p += keyLen;
            memcpy(p, QUOTE, QUOTE_LEN);
            p += QUOTE_LEN;

            memcpy(p, COLON, COLON_LEN);
            p += COLON_LEN;

            if (quoLen) {
                memcpy(p, QUOTE, quoLen);
                p += quoLen;
            }
            memcpy(p, val, valLen);
            p += valLen;
            if (quoLen) {
                memcpy(p, QUOTE, quoLen);
                p += quoLen;
            }

            if (comLen) {
                memcpy(p, COMMA, comLen);
                p += comLen;
            }
            newVal[keyValLen] = '\0';

            spliceBegin = tok.start+1; // insert at beginning
            spliceEnd = spliceBegin;

            val = newVal;
            valLen = keyValLen;
        }

        assert(spliceBegin <= spliceEnd);
        assert(spliceBegin > 0);            // must always begin with '{'
        assert(spliceEnd < map->jsonLen);   // must always close with '}'

        int prefixLen = spliceBegin;
        int suffixLen = (map->jsonLen - spliceEnd);

        int newLen = prefixLen+valLen+suffixLen;

        char *jsonString = MALLOC(newLen + 1);
        if (!jsonString) {
            break;
        }
        memcpy(jsonString, &map->jsonString[0], prefixLen);
        memcpy(jsonString+prefixLen, val, valLen);
        memcpy(jsonString+prefixLen+valLen, &map->jsonString[spliceEnd], suffixLen);
        jsonString[newLen] = '\0';

        JSON_ref newRef = NULL;
        int errCount = json_createFromString(jsonString, &newRef);
        if (errCount < 0) {
            ERRLOG("Cannot set new JSON value err : %d", errCount);
        } else {
            JSON_s *newMap = (JSON_s *)newRef;
            FREE(map->jsonString);
            FREE(map->jsonTokens);
            *map = *newMap;
            newMap->jsonString = NULL;
            newMap->jsonTokens = NULL;
            json_destroy(&newRef);
            didSetValue = true;
        }

        FREE(jsonString);
    } while (0);

    if (newVal) {
        FREE(newVal);
    }

    return didSetValue;
}

bool json_mapSetStringValue(const JSON_ref jsonRef, const char *key, const char *val) {
    return _json_mapSetValue(jsonRef, key, val, JSMN_STRING);
}

bool json_mapSetRawStringValue(const JSON_ref jsonRef, const char *key, const char *val) {
    return _json_mapSetValue(jsonRef, key, val, _JSMN_NONSTRING);
}

bool json_mapSetJSONValue(const JSON_ref jsonRef, const char *key, const JSON_ref jsonSubRef) {
    bool didSetValue = false;
    do {
        if (!jsonSubRef) {
            break;
        }

        didSetValue = _json_mapSetValue(jsonRef, key, ((JSON_s *)jsonSubRef)->jsonString, _JSMN_NONSTRING);
    } while (0);

    return didSetValue;
}

bool json_mapSetLongValue(const JSON_ref jsonRef, const char *key, long val) {
    char buf[STACK_BUFSIZ];
    buf[0] = '\0';
    snprintf(buf, STACK_BUFSIZ, "%ld", val);
    return _json_mapSetValue(jsonRef, key, buf, JSMN_PRIMITIVE);
}

bool json_mapSetBoolValue(const JSON_ref jsonRef, const char *key, bool val) {
    char buf[STACK_BUFSIZ];
    buf[0] = '\0';
    snprintf(buf, STACK_BUFSIZ, "%s", val ? "true" : "false");
    return _json_mapSetValue(jsonRef, key, buf, JSMN_PRIMITIVE);
}

bool json_mapSetFloatValue(const JSON_ref jsonRef, const char *key, float val) {
    char buf[STACK_BUFSIZ];
    buf[0] = '\0';
    snprintf(buf, STACK_BUFSIZ, "%f", val);
    return _json_mapSetValue(jsonRef, key, buf, JSMN_PRIMITIVE);
}

// ----------------------------------------------------------------------------

static bool _json_arrayCalcIndex(const JSON_s *array, INOUT unsigned long *index) {
    bool valid = false;

    do {
        if (!array) {
            break;
        }
        if (array->numTokens <= 0) {
            break;
        }
        jsmntok_t tok = array->jsonTokens[0];
        if (tok.type != JSMN_ARRAY) {
            break;
        }

        int count = tok.size;
        if ((*index) >= count) {
            break;
        }

        int idx = 1;
        for (int i=0; i<(*index); i++) {
            tok = array->jsonTokens[idx];
            idx += tok.skip;
        }

        *index = idx;
        valid = true;

    } while (0);

    return valid;
}

bool json_isArray(const JSON_ref jsonRef) {
    JSON_s *array = (JSON_s *)jsonRef;

    bool ret = false;
    do {
        if (!array) {
            break;
        }
        if (array->numTokens <= 0) {
            break;
        }
        ret = array->jsonTokens[0].type == JSMN_ARRAY;
    } while (0);

    return ret;
}

bool json_arrayCount(const JSON_ref jsonRef, INOUT long *count) {
    JSON_s *array = (JSON_s *)jsonRef;

    bool ret = false;
    do {
        if (!array) {
            break;
        }
        if (!count) {
            break;
        }
        if (array->numTokens <= 0) {
            break;
        }
        jsmntok_t tok = array->jsonTokens[0];
        if (tok.type != JSMN_ARRAY) {
            break;
        }

        *count = tok.size;
        ret = true;
    } while (0);

    return ret;
}

int json_arrayCopyJSONAtIndex(const JSON_ref jsonRef, unsigned long index, INOUT JSON_ref *val) {
    JSON_s *array = (JSON_s *)jsonRef;

    int errCount = JSMN_ERROR_NOMEM;
    do {
        if (!val) {
            break;
        }
        bool valid = _json_arrayCalcIndex(array, &index);
        if (!valid) {
            break;
        }

        jsmntok_t tok = array->jsonTokens[index];
        ASSERT_TOKEN_VALID(tok);
        int len = tok.end - tok.start;
        if (len <= 0) {
            break;
        }

        char *str = STRNDUP(&array->jsonString[tok.start], len);
        if (!str) {
            break;
        }

        errCount = json_createFromString(str, val);
        assert(errCount >= 0);
        FREE(str);

    } while (0);

    return errCount;
}

bool json_arrayCopyStringValueAtIndex(const JSON_ref jsonRef, unsigned long index, INOUT char **val) {
    JSON_s *array = (JSON_s *)jsonRef;

    bool ret = false;
    do {
        if (!val) {
            break;
        }
        bool valid = _json_arrayCalcIndex(array, &index);
        if (!valid) {
            break;
        }

        jsmntok_t tok = array->jsonTokens[index];
        ASSERT_TOKEN_VALID(tok);
        int len = tok.end - tok.start;
        if (len <= 0) {
            break;
        }

        *val = STRNDUP(&array->jsonString[tok.start], len);
        if (!*val) {
            break;
        }

        ret = true;
    } while (0);

    return ret;
}

bool json_arrayParseLongValueAtIndex(const JSON_ref jsonRef, unsigned long index, INOUT long *val, const long base) {
    JSON_s *array = (JSON_s *)jsonRef;

    bool ret = false;
    do {
        if (!val) {
            break;
        }
        bool valid = _json_arrayCalcIndex(array, &index);
        if (!valid) {
            break;
        }

        jsmntok_t tok = array->jsonTokens[index];
        ASSERT_TOKEN_VALID(tok);
        int len = tok.end - tok.start;
        if (len <= 0) {
            break;
        }

        char *str = &array->jsonString[tok.start];
        char ch = str[len];
        str[len] = '\0';
        *val = strtol(str, NULL, base);
        str[len] = ch;

        ret = true;
    } while (0);

    return ret;
}

bool json_arrayParseBoolValueAtIndex(const JSON_ref jsonRef, unsigned long index, INOUT bool *val) {
    JSON_s *array = (JSON_s *)jsonRef;

    bool ret = false;
    do {
        if (!val) {
            break;
        }
        bool valid = _json_arrayCalcIndex(array, &index);
        if (!valid) {
            break;
        }

        jsmntok_t tok = array->jsonTokens[index];
        ASSERT_TOKEN_VALID(tok);
        int len = tok.end - tok.start;
        if (len <= 0) {
            break;
        }

        char *str = &array->jsonString[tok.start];
        char ch = str[len];
        str[len] = '\0';
        *val = (strncasecmp("false", str, sizeof("false")) != 0);
        str[len] = ch;

        ret = true;
    } while (0);

    return ret;
}

bool json_arrayParseFloatValueAtIndex(const JSON_ref jsonRef, unsigned long index, INOUT float *val) {
    JSON_s *array = (JSON_s *)jsonRef;

    bool ret = false;
    do {
        if (!val) {
            break;
        }
        bool valid = _json_arrayCalcIndex(array, &index);
        if (!valid) {
            break;
        }

        jsmntok_t tok = array->jsonTokens[index];
        ASSERT_TOKEN_VALID(tok);
        int len = tok.end - tok.start;
        if (len <= 0) {
            break;
        }

        char *str = &array->jsonString[tok.start];
        char ch = str[len];
        str[len] = '\0';
        *val = strtof(str, NULL);
        str[len] = ch;

        ret = true;
    } while (0);

    return ret;
}

// ----------------------------------------------------------------------------

bool json_serialize(JSON_ref jsonRef, int fd, bool pretty) {
    JSON_s *parsedData = (JSON_s *)jsonRef;
    if (pretty) {
        return _json_prettyPrint(parsedData, /*start:*/0, /*end:*/parsedData->numTokens, /*indent:*/0, fd);
    } else {
        return _json_write(parsedData->jsonString, strlen(parsedData->jsonString), fd);
    }
}

bool json_unescapeSlashes(char **kbdPath) {
    // A big "fhank-you" to the Java org.json.JSONStringer API which "helpfully" escapes slash '/' characters
    if (!kbdPath) {
        return false;
    }

    char *str = *kbdPath;
    size_t len = strlen(str) + 1; // include termination \0
    char *p0 = NULL;
    char *p = str;
    while (*p) {
        if (*p == '/') {
            if (p0 && *p0 == '\\') {
                memmove(p0, p, ((str+len)-p));
                p = p0;
            }
        }
        p0 = p;
        ++p;
    }

    return true;
}

void json_destroy(JSON_ref *jsonRef) {
    if (!jsonRef) {
        return;
    }

    JSON_s *parsedData = (JSON_s *)*jsonRef;
    if (!parsedData) {
        return;
    }

    FREE(parsedData->jsonString);
    FREE(parsedData->jsonTokens);
    FREE(*((JSON_s **)jsonRef));
}

