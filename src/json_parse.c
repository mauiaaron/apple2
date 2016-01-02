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

int json_createFromFile(const char *filePath, INOUT JSON_s *parsedData) {

    int fd = -1;
    jsmnerr_t errCount = JSMN_ERROR_NOMEM;
    ssize_t jsonIdx = 0;
    ssize_t jsonLen = 0;

    char *jsonString = NULL;
    jsmntok_t *jsonTokens = NULL;

    do {
        if (!parsedData) {
            break;
        }
        parsedData->jsonString = NULL;
        parsedData->jsonTokens = NULL;

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
        fd = 0;

        // now parse the string

        jsmn_parser parser = { 0 };

        unsigned int numTokens = DEFAULT_NUMTOK;
        do {
            if (!jsonTokens) {
                jsonTokens = CALLOC(numTokens, sizeof(jsmntok_t));
                if (!jsonTokens) {
                    ERRLOG("WHOA3 : %s", strerror(errno));
                    break;
                }
            } else {
                //LOG("reallocating json tokens ...");
                numTokens <<= 1;
                jsmntok_t *newTokens = REALLOC(jsonTokens, numTokens * sizeof(jsmntok_t));
                if (!newTokens) {
                    ERRLOG("WHOA4 : %s", strerror(errno));
                    break;
                }
                memset(newTokens, '\0', numTokens * sizeof(jsmntok_t));
                jsonTokens = newTokens;
            }
            jsmn_init(&parser);
            errCount = jsmn_parse(&parser, jsonString, jsonLen, jsonTokens, numTokens-(numTokens/2));
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

        parsedData->jsonString = jsonString;
        parsedData->jsonTokens = jsonTokens;
    } while (0);

    if (fd > 0) {
        TEMP_FAILURE_RETRY(close(fd));
        fd = 0;
    }

    if (errCount < 0) {
        if (jsonString) {
            FREE(jsonString);
        }
        if (jsonTokens) {
            FREE(jsonTokens);
        }
    }

    return errCount;
}

void json_destroy(JSON_s *parsedData) {
    FREE(parsedData->jsonString);
    FREE(parsedData->jsonTokens);
}

#if TEST_JSON
bool do_logging = true;
FILE *error_log = NULL;
// Runs jsmn against an arbitrary a test_file.json
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

    fprintf(stderr, "TOK COUNT:%d\n", tokCount);

    for (int i=0; i<tokCount; i++) {
        fprintf(stderr, "%s", "type:");
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
        int start = parsedData.jsonTokens[i].start;
        int end   = parsedData.jsonTokens[i].end;
        fprintf(stderr, "start:%6d end:%6d size:%6d parent:%3d", start, end, parsedData.jsonTokens[i].size, parsedData.jsonTokens[i].parent);
        if (parsedData.jsonTokens[i].type == JSMN_STRING) {
            char lastChar = parsedData.jsonString[end];
            parsedData.jsonString[end] = '\0';
            fprintf(stderr, " VALUE:%s", &parsedData.jsonString[start]);
            parsedData.jsonString[end] = lastChar;
        }
        fprintf(stderr, "%s", "\n");
    }

    return 0;
}
#endif
