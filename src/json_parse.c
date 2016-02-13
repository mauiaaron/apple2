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
        parsedData->jsonString = strdup(jsonString);
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
        *val = len>0 ? strndup(*val, len) : strdup("");
    }
    return foundMatch;
}

bool json_mapParseLongValue(const JSON_s *map, const char *key, INOUT long *val, const int base) {
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
    STRDUP_FREE(parsedData->jsonString);
    FREE(parsedData->jsonTokens);
}

#if TEST_JSON
bool do_logging = true;
FILE *error_log = NULL;
// Runs jsmn against an arbitrary a test_file.json
static void _dumpJSON(JSON_s parsedData) {

    fprintf(stderr, "TOK COUNT:%d\n", parsedData.numTokens);

    for (int i=0; i<parsedData.numTokens; i++) {
        fprintf(stderr, "%6d %s", i, "type:");
        switch(parsedData.jsonTokens[i].type) {
            case JSMN_PRIMITIVE:
                fprintf(stderr, "%s", "PRIMITIVE ");
                break;
            case JSMN_OBJECT:
                fprintf(stderr, "%s", "OBJECT    ");
                break;
            case JSMN_ARRAY:
                fprintf(stderr, "%s", "ARRAY     ");
                break;
            case JSMN_STRING:
                fprintf(stderr, "%s", "STRING    ");
                break;
            default:
                fprintf(stderr, "%s", "UNKNOWN   ");
                break;
        }
        jsmntok_t tok = parsedData.jsonTokens[i];
        int start = tok.start;
        int end   = tok.end;
        fprintf(stderr, "start:%6d end:%6d size:%6d skipIdx:%6d parent:%3d", start, end, tok.size, tok.skip, tok.parent);
        if (tok.type == JSMN_STRING || tok.type == JSMN_PRIMITIVE) {
            char lastChar = parsedData.jsonString[end];
            parsedData.jsonString[end] = '\0';
            fprintf(stderr, " VALUE:%s", &parsedData.jsonString[start]);
            parsedData.jsonString[end] = lastChar;
        }
        fprintf(stderr, "%s", "\n");
    }
}

int main(int argc, char **argv) {

    error_log = stderr;

    if (argc < 2) {
        fprintf(stderr, "Please specify a JSON file to parse on CLI, e.g. : %s path/to/some_file.json\n", argv[0]);
        exit(1);
    }

    JSON_s parsedData = { 0 };
    int tokCount = json_createFromFile(argv[1], &parsedData);

    if (tokCount < 0) {
        return 1;
    }

    fprintf(stderr, "-------------------------------------------------------------------------------\n");
    fprintf(stderr, "DUMPING FILE %s ...\n", argv[1]);
    fprintf(stderr, "-------------------------------------------------------------------------------\n");
    fprintf(stderr, "\n");

    _dumpJSON(parsedData);

    json_destroy(&parsedData);

    fprintf(stderr, "\n");
    fprintf(stderr, "-------------------------------------------------------------------------------\n");
    fprintf(stderr, "NOW TESTING MAP FUNCTIONS...\n");
    fprintf(stderr, "-------------------------------------------------------------------------------\n");
    fprintf(stderr, "\n");

    const char *testMapStr0 =
"    {                                             "
"        \"key0\"    : \"a value zero\",           "
"        \"key1\"  : \"  	 \",               "
"        \"key2\" : {    	 \n"
"}                  ,                              "
"    	 \"key3\" : {   	 \n"
"  \"subkey0\" : \"subval0\", "
"  \"subkey1\" : { \"moar\" : \"recursion\" }  , "
"  \"subkey2\" : \"line0      \n"
"   	line1   "
"   	line2\"  \n"
"},                                                "
"        \"key4\"  : [ \"Q\",    \"W\",  \"E\",    "
"        \"R\",  \"T\",  \"Y\",  \"U\",   \"I\",   "
"        \"O\",   \"P\", { \"x\" : [ 22, 4, \"ab\" ] } ],"
"        \"intKey0\"  : 42     ,                   "
"        \"key5\"  : \"\"                   ,      "
"        \"intKey1\"  : -101,                      "
"        \"intKey2\"  : -000000000,                "
"        \"intKey3\"  :  0x2400,                   "
"        \"intKey4\"  :  10111111,                 "
"        \"floatKey0\"      : 0.0   ,              "
"        \"floatKey1\"      : -.0001220703125     ,"
"        \"floatKey2\"     :  3.1415928 ,          "
"        \"floatKey3\"     :  -3.1e2               "
"    }                                             "
    ;

    tokCount = json_createFromString(testMapStr0, &parsedData);
    if (tokCount < 0) {
        return 1;
    }

    _dumpJSON(parsedData);

    long lVal;
    float fVal;
    char *val;

    json_mapParseLongValue(&parsedData, "intKey2", &lVal, 10);
    assert(lVal == 0);

    json_mapCopyStringValue(&parsedData, "key0", &val);
    assert(strcmp(val, "a value zero") == 0);
    STRDUP_FREE(val);

    json_mapCopyStringValue(&parsedData, "key1", &val);
    assert(strcmp(val, "  \t ") == 0);
    STRDUP_FREE(val);

    json_mapCopyStringValue(&parsedData, "key2", &val);
    assert(strcmp(val, "{    \t \n}") == 0);
    STRDUP_FREE(val);

    json_mapCopyStringValue(&parsedData, "key3", &val);
    assert(strcmp(val, "{   \t \n  \"subkey0\" : \"subval0\",   \"subkey1\" : { \"moar\" : \"recursion\" }  ,   \"subkey2\" : \"line0      \n   \tline1      \tline2\"  \n}") == 0);
    {
        JSON_s parsedSubData = { 0 };
        int tokSubCount = json_createFromString(val, &parsedSubData);
        if (tokSubCount < 0) {
            return 1;
        }

        char *subval;
        json_mapCopyStringValue(&parsedSubData, "subkey0", &subval);
        assert(strcmp(subval, "subval0") == 0);
        STRDUP_FREE(subval);

        json_mapCopyStringValue(&parsedSubData, "subkey1", &subval);
        assert(strcmp(subval, "{ \"moar\" : \"recursion\" }") == 0);
        STRDUP_FREE(subval);

        json_mapCopyStringValue(&parsedSubData, "subkey2", &subval);
        assert(strcmp(subval, "line0      \n   \tline1      \tline2") == 0);
        STRDUP_FREE(subval);
    }
    STRDUP_FREE(val);

    json_mapCopyStringValue(&parsedData, "key4", &val);
    assert(strcmp(val, "[ \"Q\",    \"W\",  \"E\",            \"R\",  \"T\",  \"Y\",  \"U\",   \"I\",           \"O\",   \"P\", { \"x\" : [ 22, 4, \"ab\" ] } ]") == 0);
    // TODO : subarray checks
    STRDUP_FREE(val);

    json_mapCopyStringValue(&parsedData, "key5", &val);
    assert(strcmp(val, "") == 0);
    STRDUP_FREE(val);

    json_mapParseLongValue(&parsedData, "intKey0", &lVal, 10);
    assert(lVal == 42);

    json_mapParseLongValue(&parsedData, "intKey1", &lVal, 10);
    assert(lVal == -101);

    json_mapParseLongValue(&parsedData, "intKey3", &lVal, 16);
    assert(lVal == 0x2400);

    json_mapParseLongValue(&parsedData, "intKey4", &lVal, 2);
    assert(lVal == 191);

    json_mapParseLongValue(&parsedData, "intKey4", &lVal, 10);
    assert(lVal == 10111111);

    json_mapParseFloatValue(&parsedData, "floatKey0", &fVal);
    assert(fVal == 0.f);

    json_mapParseFloatValue(&parsedData, "floatKey1", &fVal);
    assert(fVal == -.0001220703125);

    json_mapParseFloatValue(&parsedData, "floatKey2", &fVal);
    assert((long)(fVal*10000000) == 31415928);

    json_mapParseFloatValue(&parsedData, "floatKey3", &fVal);
    assert((long)fVal == -310);

    json_destroy(&parsedData);
    fprintf(stderr, "...map functions passed\n");

    return 0;
}
#endif
