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

#include "json_parse.h"

#define JSON_LENGTH 16
#define DEFAULT_NUMTOK 16

static int _json_createFromString(const char *jsonString, INOUT JSON_s *parsedData, ssize_t jsonLen) {

    jsmnerr_t errCount = JSMN_ERROR_NOMEM;
    do {
        jsmn_parser parser = { 0 };

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

    } while (0);

    if (errCount < 0) {
        if (parsedData) {
            json_destroy(parsedData);
        }
    }

    return errCount;
}

int json_createFromFile(const char *filePath, INOUT JSON_s *parsedData) {

    int fd = -1;
    ssize_t jsonIdx = 0;
    ssize_t jsonLen = 0;

    char *jsonString = NULL;

    do {
        if (!filePath) {
            break;
        }

        if (!parsedData) {
            break;
        }

        TEMP_FAILURE_RETRY(fd = open(filePath, O_RDONLY));
        if (fd < 0) {
            ERRLOG("Error opening file : %s", strerror(errno));
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
        TEMP_FAILURE_RETRY(close(fd));
        fd = -1;

        // now parse the string
        jsmnerr_t errCount = _json_createFromString(jsonString, parsedData, jsonLen);
        FREE(jsonString);
        return errCount;

    } while (0);

    if (fd > 0) {
        TEMP_FAILURE_RETRY(close(fd));
        fd = -1;
    }

    if (jsonString) {
        FREE(jsonString);
    }

    return JSMN_ERROR_NOMEM;
}

int json_createFromString(const char *jsonString, INOUT JSON_s *parsedData) {
    return _json_createFromString(jsonString, parsedData, strlen(jsonString));
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

bool json_mapCopyStringValue(const JSON_s *map, const char *key, INOUT char **val) {
    int len = 0;
    bool foundMatch = _json_mapGetStringValue(map, key, val, &len);
    if (foundMatch) {
        *val = len>0 ? STRNDUP(*val, len) : STRDUP("");
    }
    return foundMatch;
}

bool json_mapParseLongValue(const JSON_s *map, const char *key, INOUT long *val, const long base) {
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

bool json_mapParseFloatValue(const JSON_s *map, const char *key, INOUT float *val) {
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

void json_destroy(JSON_s *parsedData) {
    FREE(parsedData->jsonString);
    FREE(parsedData->jsonTokens);
}

