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
            } else if (type ==  JSMN_ARRAY) {
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

    jsmnerr_t errCount = JSMN_ERROR_NOMEM;
    do {
        jsmn_parser parser = { 0 };

        if (!jsonRef) {
            break;
        }

        JSON_s *parsedData = MALLOC(sizeof(*parsedData));
        if (!parsedData) {
            break;
        }
        *jsonRef = parsedData;

        if (!parsedData) {
            break;
        }
        parsedData->jsonString = STRDUP(jsonString);
        parsedData->jsonTokens = NULL;

        unsigned int numTokens = DEFAULT_NUMTOK;
        do {
            if (!parsedData->jsonTokens) {
                parsedData->jsonTokens = CALLOC(numTokens, sizeof(jsmntok_t));
                if (!parsedData->jsonTokens) {
                    ERRLOG("WHOA3 : %s", strerror(errno));
                    break;
                }
            } else {
                //LOG("reallocating json tokens ...");
                numTokens <<= 1;
                jsmntok_t *newTokens = REALLOC(parsedData->jsonTokens, numTokens * sizeof(jsmntok_t));
                if (!newTokens) {
                    ERRLOG("WHOA4 : %s", strerror(errno));
                    break;
                }
                memset(newTokens, '\0', numTokens * sizeof(jsmntok_t));
                parsedData->jsonTokens = newTokens;
            }
            jsmn_init(&parser);
            errCount = jsmn_parse(&parser, parsedData->jsonString, jsonLen, parsedData->jsonTokens, numTokens-(numTokens/2));
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
                default:
                    ERRLOG("UNKNOWN errCount : %d", errCount);
                    break;
            }
            break;
        }

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
    jsmnerr_t errCount = JSMN_ERROR_NOMEM;

    do {
        if (!jsonRef) {
            break;
        }

        jsonLen = JSON_LENGTH*2;
        jsonString = MALLOC(jsonLen);
        if (jsonString == NULL) {
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
                    if (!newString) {
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
    jsmnerr_t errCount = JSMN_ERROR_NOMEM;
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

static bool _json_mapGetStringValue(const JSON_s *map, const char *key, INOUT char **val, INOUT int *len) {
    bool foundMatch = false;

    do {
        if (!val) {
            break;
        }
        if (!len) {
            break;
        }

        *val = NULL;
        *len = -1;

        int tokCount = map->numTokens;
        if (tokCount < 0) {
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

            int start = keyTok.start;
            int end   = keyTok.end;
            assert(end >= start && "bug");
            const int size = end - start;

            ++idx;
            jsmntok_t valTok = map->jsonTokens[idx];
            if (size == keySize) {
                foundMatch = (strncmp(key, &map->jsonString[start], size) == 0);
                if (foundMatch) {
                    start = valTok.start;
                    end = valTok.end;
                    assert(end >= start && "bug");
                    *len = end - start;
                    *val = &map->jsonString[start];
                    break;
                }
            }

            skip = valTok.skip;
        }
    } while (0);

    return foundMatch;
}

bool json_mapCopyStringValue(const JSON_ref jsonRef, const char *key, INOUT char **val) {
    JSON_s *map = (JSON_s *)jsonRef;
    int len = 0;
    bool foundMatch = _json_mapGetStringValue(map, key, val, &len);
    if (foundMatch) {
        *val = len>0 ? STRNDUP(*val, len) : STRDUP("");
    }
    return foundMatch;
}

bool json_mapParseLongValue(const JSON_ref jsonRef, const char *key, INOUT long *val, const long base) {
    JSON_s *map = (JSON_s *)jsonRef;
    bool foundMatch = false;

    do {
        if (!val) {
            break;
        }

        int len = 0;
        char *str = NULL;
        foundMatch = _json_mapGetStringValue(map, key, &str, &len);
        if (foundMatch) {
            char ch = str[len];
            str[len] = '\0';
            *val = strtol(str, NULL, base);
            str[len] = ch;
        }
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

        int len = 0;
        char *str = NULL;
        foundMatch = _json_mapGetStringValue(map, key, &str, &len);
        if (foundMatch) {
            char ch = str[len];
            str[len] = '\0';
            *val = strtof(str, NULL);
            str[len] = ch;
        }
    } while (0);

    return foundMatch;
}

bool json_serialize(JSON_ref jsonRef, int fd, bool pretty) {
    JSON_s *parsedData = (JSON_s *)jsonRef;
    if (pretty) {
        return _json_prettyPrint(parsedData, /*start:*/0, /*end:*/parsedData->numTokens, /*indent:*/0, fd);
    } else {
        return _json_write(parsedData->jsonString, strlen(parsedData->jsonString), fd);
    }
}

void json_destroy(JSON_ref *jsonRef) {
    if (!jsonRef) {
        return;
    }

    JSON_s *parsedData = (JSON_s *)*jsonRef;
    FREE(parsedData->jsonString);
    FREE(parsedData->jsonTokens);
    FREE(*jsonRef);
}

