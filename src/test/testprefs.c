/*
 * Apple // emulator for *ix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 2016 Aaron Culliney
 *
 */

#include "testcommon.h"
#include "json_parse_private.h"

static bool test_thread_running = false;

extern pthread_mutex_t interface_mutex; // TODO FIXME : raw access to CPU mutex because stepping debugger ...

static void testprefs_setup(void *unused) {
}

static void testprefs_teardown(void *unused) {
}

static const char *get_default_preferences(void) {
    return
    "{"
    "    \"audio\" : {"
    "        \"speakerVolume\" : 4,"
    "        \"mbVolume\" : 2"
    "    },"
    "    \"interface\" : {"
    "        \"diskPath\" : \"/usr/local/games/apple2/disks\""
    "    },"
    "    \"joystick\" : {"
    "        \"joystickMode\" : 1,"
    "        \"pcJoystickParms\" : \"128 128 255 1 255 1\","
    "        \"kpJoystickParms\" : \"8 1\""
    "    },"
    "    \"keyboard\" : {"
    "        \"caps\" : true"
    "    },"
    "    \"video\" : {"
    "        \"colorMode\" : \"2\""
    "    },"
    "    \"vm\" : {"
    "        \"cpuScale\" : 1.0,"
    "        \"cpuScaleAlt\" : 4.0"
    "    }"
    "}"
    ;
}

static const char *get_sample_json_0(void) {
    return
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
"        \"intKey2\"  : \"-000000000\",                "
"        \"intKey3\"  :  \"0x2400\",                   "
"        \"intKey4\"  :  10111111,                 "
"        \"floatKey0\"      : 0.0   ,              "
"        \"floatKey1\"      : -0.0001220703125     ,"
"        \"floatKey2\"     :  3.1415928 ,          "
"        \"floatKey3\"     :  -3.1e2               "
"    }                                             "
    ;
}

// ----------------------------------------------------------------------------
// JSON/prefs tests ...

TEST test_json_map_0(JSON_ref parsedData) {

    long lVal;
    float fVal;
    char *val;

    json_mapParseLongValue(parsedData, "intKey2", &lVal, 10);
    ASSERT(lVal == 0);

    json_mapCopyStringValue(parsedData, "key0", &val);
    ASSERT(strcmp(val, "a value zero") == 0);
    FREE(val);

    json_mapCopyStringValue(parsedData, "key1", &val);
    ASSERT(strcmp(val, "  \t ") == 0);
    FREE(val);

    json_mapCopyStringValue(parsedData, "key2", &val);
    ASSERT(strcmp(val, "{    \t \n}") == 0);
    FREE(val);

    json_mapCopyStringValue(parsedData, "key3", &val);
    ASSERT(strcmp(val, "{   \t \n  \"subkey0\" : \"subval0\",   \"subkey1\" : { \"moar\" : \"recursion\" }  ,   \"subkey2\" : \"line0      \n   \tline1      \tline2\"  \n}") == 0);
    do {
        JSON_ref parsedSubData = NULL;
        int tokSubCount = json_createFromString(val, &parsedSubData);
        ASSERT(tokSubCount > 0);

        char *subval;
        json_mapCopyStringValue(parsedSubData, "subkey0", &subval);
        ASSERT(strcmp(subval, "subval0") == 0);
        FREE(subval);

        json_mapCopyStringValue(parsedSubData, "subkey1", &subval);
        ASSERT(strcmp(subval, "{ \"moar\" : \"recursion\" }") == 0);
        FREE(subval);

        json_mapCopyStringValue(parsedSubData, "subkey2", &subval);
        ASSERT(strcmp(subval, "line0      \n   \tline1      \tline2") == 0);
        FREE(subval);

        json_destroy(&parsedSubData);
    } while (0);
    FREE(val);

    json_mapCopyStringValue(parsedData, "key4", &val);
    ASSERT(strcmp(val, "[ \"Q\",    \"W\",  \"E\",            \"R\",  \"T\",  \"Y\",  \"U\",   \"I\",           \"O\",   \"P\", { \"x\" : [ 22, 4, \"ab\" ] } ]") == 0);
    // TODO : subarray checks
    FREE(val);

    json_mapCopyStringValue(parsedData, "key5", &val);
    ASSERT(strcmp(val, "") == 0);
    FREE(val);

    json_mapParseLongValue(parsedData, "intKey0", &lVal, 10);
    ASSERT(lVal == 42);

    json_mapParseLongValue(parsedData, "intKey1", &lVal, 10);
    ASSERT(lVal == -101);

    json_mapParseLongValue(parsedData, "intKey3", &lVal, 16);
    ASSERT(lVal == 0x2400);

    json_mapParseLongValue(parsedData, "intKey4", &lVal, 2);
    ASSERT(lVal == 191);

    json_mapParseLongValue(parsedData, "intKey4", &lVal, 10);
    ASSERT(lVal == 10111111);

    json_mapParseFloatValue(parsedData, "floatKey0", &fVal);
    ASSERT(fVal == 0.f);

    json_mapParseFloatValue(parsedData, "floatKey1", &fVal);
    ASSERT(fVal == -.0001220703125);

    json_mapParseFloatValue(parsedData, "floatKey2", &fVal);
    ASSERT((long)(fVal*10000000) == 31415928);

    json_mapParseFloatValue(parsedData, "floatKey3", &fVal);
    ASSERT((long)fVal == -310);

    PASS();
}

TEST test_json_map_1() {

    const char *testMapStr0 = get_sample_json_0();

    JSON_ref parsedData = NULL;
    int tokCount = json_createFromString(testMapStr0, &parsedData);
    ASSERT(tokCount > 0);

    test_json_map_0(parsedData);

    json_destroy(&parsedData);
    PASS();
}

TEST test_json_serialization() {

    const char *testMapStr0 = get_sample_json_0();

    JSON_ref parsedData = NULL;
    int tokCount = json_createFromString(testMapStr0, &parsedData);
    ASSERT(tokCount > 0);

    char *str = STRDUP("/tmp/json-XXXXXX");
    int fd = mkstemp(str);
    ASSERT(fd > 0);
    FREE(str);

    json_serialize(parsedData, fd, /*pretty:*/false);
    json_destroy(&parsedData);
    lseek(fd, 0, SEEK_SET);
    tokCount = json_createFromFD(fd, &parsedData);
    ASSERT(tokCount > 0);

    test_json_map_0(parsedData);

    TEMP_FAILURE_RETRY(close(fd));

    json_destroy(&parsedData);
    PASS();
}

TEST test_json_serialization_pretty() {

    const char *testMapStr0 = get_sample_json_0();

    JSON_ref parsedData = NULL;
    int tokCount = json_createFromString(testMapStr0, &parsedData);
    ASSERT(tokCount > 0);

    char *str = STRDUP("/tmp/json-pretty-XXXXXX");
    int fd = mkstemp(str);
    ASSERT(fd > 0);
    FREE(str);

    json_serialize(parsedData, fd, /*pretty:*/true);
    json_destroy(&parsedData);
    lseek(fd, 0, SEEK_SET);
    tokCount = json_createFromFD(fd, &parsedData);
    ASSERT(tokCount > 0);

    do {
        long lVal;
        float fVal;
        char *val;

        json_mapParseLongValue(parsedData, "intKey2", &lVal, 10);
        ASSERT(lVal == 0);

        json_mapCopyStringValue(parsedData, "key0", &val);
        ASSERT(strcmp(val, "a value zero") == 0);
        FREE(val);

        json_mapCopyStringValue(parsedData, "key1", &val);
        ASSERT(strcmp(val, "  \t ") == 0);
        FREE(val);

        json_mapCopyStringValue(parsedData, "key2", &val);
        do {
            JSON_ref parsedSubData = NULL;
            int tokSubCount = json_createFromString(val, &parsedSubData);
            ASSERT(tokSubCount == 1);
            ASSERT(((JSON_s *)parsedSubData)->jsonTokens[0].type == JSMN_OBJECT);
            json_destroy(&parsedSubData);
        } while (0);
        FREE(val);

        json_mapCopyStringValue(parsedData, "key3", &val);
        do {
            JSON_ref parsedSubData = NULL;
            int tokSubCount = json_createFromString(val, &parsedSubData);
            ASSERT(tokSubCount == 9);

            char *subval;
            json_mapCopyStringValue(parsedSubData, "subkey0", &subval);
            ASSERT(strcmp(subval, "subval0") == 0);
            FREE(subval);

            json_mapCopyStringValue(parsedSubData, "subkey1", &subval);
            do {
                JSON_ref parsedSubSubData = NULL;
                int tokSubSubCount = json_createFromString(subval, &parsedSubSubData);
                ASSERT(tokSubSubCount == 3);

                char *subsubval;
                json_mapCopyStringValue(parsedSubSubData, "moar", &subsubval);
                ASSERT(strcmp(subsubval, "recursion") == 0);
                FREE(subsubval);

                json_destroy(&parsedSubSubData);
            } while (0);
            FREE(subval);

            json_mapCopyStringValue(parsedSubData, "subkey2", &subval);
            ASSERT(strcmp(subval, "line0      \n   \tline1      \tline2") == 0);
            FREE(subval);

            json_destroy(&parsedSubData);
        } while (0);
        FREE(val);

        json_mapCopyStringValue(parsedData, "key4", &val);
        do {
            JSON_ref parsedSubData = NULL;
            int tokSubCount = json_createFromString(val, &parsedSubData);
            ASSERT(tokSubCount == 17);
            // TODO : subarray checks
            json_destroy(&parsedSubData);
        } while (0);
        FREE(val);

        json_mapCopyStringValue(parsedData, "key5", &val);
        ASSERT(strcmp(val, "") == 0);
        FREE(val);

        json_mapParseLongValue(parsedData, "intKey0", &lVal, 10);
        ASSERT(lVal == 42);

        json_mapParseLongValue(parsedData, "intKey1", &lVal, 10);
        ASSERT(lVal == -101);

        json_mapParseLongValue(parsedData, "intKey3", &lVal, 16);
        ASSERT(lVal == 0x2400);

        json_mapParseLongValue(parsedData, "intKey4", &lVal, 2);
        ASSERT(lVal == 191);

        json_mapParseLongValue(parsedData, "intKey4", &lVal, 10);
        ASSERT(lVal == 10111111);

        json_mapParseFloatValue(parsedData, "floatKey0", &fVal);
        ASSERT(fVal == 0.f);

        json_mapParseFloatValue(parsedData, "floatKey1", &fVal);
        ASSERT(fVal == -.0001220703125);

        json_mapParseFloatValue(parsedData, "floatKey2", &fVal);
        ASSERT((long)(fVal*10000000) == 31415928);

        json_mapParseFloatValue(parsedData, "floatKey3", &fVal);
        ASSERT((long)fVal == -310);
    } while (0);

    TEMP_FAILURE_RETRY(close(fd));

    json_destroy(&parsedData);
    PASS();
}

TEST test_json_raw_bool() {

    JSON_ref parsedData = NULL;
    int errCount = json_createFromString("true", &parsedData);
    ASSERT(errCount == 1);
    ASSERT(parsedData != NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString(" true ", &parsedData);
    ASSERT(errCount == 1);
    ASSERT(parsedData != NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("false", &parsedData);
    ASSERT(errCount == 1);
    ASSERT(parsedData != NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString(" false ", &parsedData);
    ASSERT(errCount == 1);
    ASSERT(parsedData != NULL);
    json_destroy(&parsedData);

    PASS();
}

TEST test_json_invalid_raw_bool() {

    JSON_ref parsedData = NULL;
    int errCount = json_createFromString("truthiness", &parsedData);
    ASSERT(errCount == JSMN_ERROR_PRIMITIVE_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("falsify", &parsedData);
    ASSERT(errCount == JSMN_ERROR_PRIMITIVE_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("truez", &parsedData);
    ASSERT(errCount == JSMN_ERROR_PRIMITIVE_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("falseX", &parsedData);
    ASSERT(errCount == JSMN_ERROR_PRIMITIVE_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("true false", &parsedData); // !!!
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("false true", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("true, false", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    PASS();
}

TEST test_json_raw_null() {

    JSON_ref parsedData = NULL;
    int errCount = json_createFromString("null", &parsedData);
    ASSERT(errCount == 1);
    ASSERT(parsedData != NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString(" null ", &parsedData);
    ASSERT(errCount == 1);
    ASSERT(parsedData != NULL);
    json_destroy(&parsedData);

    PASS();
}

TEST test_json_invalid_raw_null() {

    JSON_ref parsedData = NULL;
    int errCount = json_createFromString("nullX", &parsedData);
    ASSERT(errCount == JSMN_ERROR_PRIMITIVE_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString(" null null ", &parsedData); // !!!
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    PASS();
}

TEST test_json_raw_int() {

    JSON_ref parsedData = NULL;
    int errCount = json_createFromString("42", &parsedData);
    ASSERT(errCount == 1);
    ASSERT(parsedData != NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString(" 42 ", &parsedData);
    ASSERT(errCount == 1);
    ASSERT(parsedData != NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString(" 0 ", &parsedData);
    ASSERT(errCount == 1);
    ASSERT(parsedData != NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString(" -0 ", &parsedData);
    ASSERT(errCount == 1);
    ASSERT(parsedData != NULL);
    json_destroy(&parsedData);

    PASS();
}

TEST test_json_invalid_raw_int() {

    JSON_ref parsedData = NULL;
    int errCount = json_createFromString("42x", &parsedData);
    ASSERT(errCount == JSMN_ERROR_PRIMITIVE_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("00", &parsedData);
    ASSERT(errCount == JSMN_ERROR_PRIMITIVE_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("-00", &parsedData);
    ASSERT(errCount == JSMN_ERROR_PRIMITIVE_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString(" 1 2 ", &parsedData); // !!!
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString(" 1, 2 ", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    PASS();
}

TEST test_json_raw_float() {

    JSON_ref parsedData = NULL;
    int errCount = json_createFromString("-3.14159", &parsedData);
    ASSERT(errCount == 1);
    ASSERT(parsedData != NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString(" -3.14159  ", &parsedData);
    ASSERT(errCount == 1);
    ASSERT(parsedData != NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString(" -3.14e-2 ", &parsedData);
    ASSERT(errCount == 1);
    ASSERT(parsedData != NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString(" -0.000E+000 ", &parsedData);
    ASSERT(errCount == 1);
    ASSERT(parsedData != NULL);
    json_destroy(&parsedData);

    PASS();
}

TEST test_json_invalid_raw_float() {

    JSON_ref parsedData = NULL;
    int errCount = json_createFromString("-3.14159z", &parsedData);
    ASSERT(errCount == JSMN_ERROR_PRIMITIVE_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("2e1.25 }", &parsedData);
    ASSERT(errCount == JSMN_ERROR_PRIMITIVE_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("-3.14159e", &parsedData);
    ASSERT(errCount == JSMN_ERROR_PRIMITIVE_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString(" -3.14e ", &parsedData);
    ASSERT(errCount == JSMN_ERROR_PRIMITIVE_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString(" -3.14E- ", &parsedData);
    ASSERT(errCount == JSMN_ERROR_PRIMITIVE_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString(" -3.14E+ ", &parsedData);
    ASSERT(errCount == JSMN_ERROR_PRIMITIVE_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString(" 1.0 2.0 ", &parsedData); // !!!
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString(" 1.0, 2.0 ", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("2.0e2e2", &parsedData);
    ASSERT(errCount == JSMN_ERROR_PRIMITIVE_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("314-159", &parsedData);
    ASSERT(errCount == JSMN_ERROR_PRIMITIVE_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("314159-", &parsedData);
    ASSERT(errCount == JSMN_ERROR_PRIMITIVE_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("--314159", &parsedData);
    ASSERT(errCount == JSMN_ERROR_PRIMITIVE_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("0.0123.3", &parsedData);
    ASSERT(errCount == JSMN_ERROR_PRIMITIVE_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);
    PASS();

    parsedData = NULL;
    errCount = json_createFromString(".123", &parsedData);
    ASSERT(errCount == JSMN_ERROR_PRIMITIVE_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);
    PASS();

    parsedData = NULL;
    errCount = json_createFromString("123.", &parsedData);
    ASSERT(errCount == JSMN_ERROR_PRIMITIVE_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    PASS();
}

TEST test_json_raw_string() {

    JSON_ref parsedData = NULL;
    int errCount = json_createFromString("\"aString\"", &parsedData);
    ASSERT(errCount == 1);
    ASSERT(parsedData != NULL);

    json_destroy(&parsedData);

    PASS();
}

TEST test_json_invalid_raw_string() {

    JSON_ref parsedData = NULL;
    int errCount = json_createFromString("aString", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("flagrant", &parsedData);
    ASSERT(errCount == JSMN_ERROR_PRIMITIVE_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("nocturnal", &parsedData);
    ASSERT(errCount == JSMN_ERROR_PRIMITIVE_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("\"aString", &parsedData);
    ASSERT(errCount == JSMN_ERROR_PART);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    PASS();
}

TEST test_json_invalid_raw_comma() {

    JSON_ref parsedData = NULL;
    int errCount = json_createFromString(",", &parsedData); // !!!
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString(" , ", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    PASS();
}

TEST test_json_invalid_raw_quote() {

    JSON_ref parsedData = NULL;
    int errCount = json_createFromString("\"", &parsedData); // !!!
    ASSERT(errCount == JSMN_ERROR_PART);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString(" \" ", &parsedData);
    ASSERT(errCount == JSMN_ERROR_PART);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString(" \" \" \" ", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    PASS();
}

TEST test_json_invalid_raw_minus() {

    JSON_ref parsedData = NULL;
    int errCount = json_createFromString("-", &parsedData);
    ASSERT(errCount == JSMN_ERROR_PRIMITIVE_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString(" - ", &parsedData);
    ASSERT(errCount == JSMN_ERROR_PRIMITIVE_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString(" -- ", &parsedData);
    ASSERT(errCount == JSMN_ERROR_PRIMITIVE_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString(" -e ", &parsedData);
    ASSERT(errCount == JSMN_ERROR_PRIMITIVE_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("-0.25e+25-", &parsedData);
    ASSERT(errCount == JSMN_ERROR_PRIMITIVE_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    PASS();
}

TEST test_json_invalid_raw_e() {

    JSON_ref parsedData = NULL;
    int errCount = json_createFromString("e", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString(" e ", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("-e0", &parsedData);
    ASSERT(errCount == JSMN_ERROR_PRIMITIVE_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("-.e0", &parsedData);
    ASSERT(errCount == JSMN_ERROR_PRIMITIVE_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("1.e2", &parsedData); // should this be valid?
    ASSERT(errCount == JSMN_ERROR_PRIMITIVE_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("2e.02", &parsedData);
    ASSERT(errCount == JSMN_ERROR_PRIMITIVE_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("2e.-02", &parsedData);
    ASSERT(errCount == JSMN_ERROR_PRIMITIVE_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    PASS();
}

TEST test_json_empty() {

    JSON_ref parsedData = NULL;
    int errCount = json_createFromString("", &parsedData);
    ASSERT(errCount == 0);
    ASSERT(parsedData != NULL);

    json_destroy(&parsedData);

    PASS();
}

TEST test_json_map_null() {

    JSON_ref parsedData = NULL;
    int errCount = json_createFromString("{ \"aKey\" : null }", &parsedData);
    ASSERT(errCount == 3);
    ASSERT(parsedData != NULL);

    char *val = NULL;
    bool ok = json_mapCopyStringValue(parsedData, "aKey", &val);
    ASSERT(ok);
    ASSERT(strcmp(val, "null") == 0);
    FREE(val);

    json_destroy(&parsedData);

    PASS();
}

TEST test_json_map_bool() {

    JSON_ref parsedData = NULL;
    int errCount = json_createFromString("{ \"aKey\" : true }", &parsedData);
    ASSERT(errCount == 3);
    ASSERT(parsedData != NULL);

    bool bVal = false;
    bool ok = json_mapParseBoolValue(parsedData, "aKey", &bVal);
    ASSERT(ok);
    ASSERT(bVal == true);

    json_destroy(&parsedData);

    PASS();
}

TEST test_json_map_float() {

    JSON_ref parsedData = NULL;
    int errCount = json_createFromString("{ \"aKey\" : 2.5e-2 }", &parsedData);
    ASSERT(errCount == 3);
    ASSERT(parsedData != NULL);

    float fVal = 0.0;
    bool ok = json_mapParseFloatValue(parsedData, "aKey", &fVal);
    ASSERT(ok);
    ASSERT((int)(fVal * 1000) == 25);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("{ \"aKey\" : -0.25 }", &parsedData);
    ASSERT(errCount == 3);
    ASSERT(parsedData != NULL);

    fVal = 0.0;
    ok = json_mapParseFloatValue(parsedData, "aKey", &fVal);
    ASSERT(ok);
    ASSERT(fVal == -0.25);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("{ \"aKey\" : 25.5E+002 }", &parsedData);
    ASSERT(errCount == 3);
    ASSERT(parsedData != NULL);

    fVal = 0.0;
    ok = json_mapParseFloatValue(parsedData, "aKey", &fVal);
    ASSERT(ok);
    ASSERT(fVal == 2550.0);
    json_destroy(&parsedData);

    PASS();
}

TEST test_json_map_invalid_bareKey() {

    JSON_ref parsedData = NULL;
    int errCount = json_createFromString("{ aBareKey : \"aNonBareVal\" }", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);

    json_destroy(&parsedData);

    PASS();
}

TEST test_json_map_invalid_bareVal() {

    JSON_ref parsedData = NULL;
    int errCount = json_createFromString("{ \"aNonBareKey\" : aBareVal }", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);

    json_destroy(&parsedData);

    PASS();
}

TEST test_json_map_invalid_dangling() {

    JSON_ref parsedData = NULL;
    int errCount = json_createFromString("{ \"aNonBareKey\" : \"aNonBareVal\", }", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("{ , }", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("{ \"aNonBareKey\" : }", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("{ : \"aNonBareVal\" }", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("{ : }", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("{ : , }", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("{ , : }", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("{ , : , }", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("{ ::: }", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("{ :::: }", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("{ :::1 }", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("{ ::::1 }", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("{ ,,1, }", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("{ ,,,1, }", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("{ \"aNonBareButDanglingKey\" }", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("{ \"aNonBareButDanglingKey\" : }", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("{ \"aNonBareKey\" : \"aNonBareVal\", \"aNonBareButDanglingKey\" }", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("{ \"aNonBareKey\" : \"aNonBareVal\", \"aNonBareButDanglingKey\" : }", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("{ \"aNonBareButDanglingKey\" }", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("{ \"aNonBareButDanglingKey\" \"aNonBareButDanglingKey\" }", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    PASS();
}

TEST test_json_array_invalid_dangling() {

    JSON_ref parsedData = NULL;
    int errCount = json_createFromString("[ \"foo\", \"bar\", ]", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("[ , ]", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("[ ,,,, ]", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("[ ,,,1, ]", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    PASS();
}

TEST test_json_map_invalid_intKey() {

    JSON_ref parsedData = NULL;
    int errCount = json_createFromString("{ 42 : \"aNonBareVal\" }", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);

    json_destroy(&parsedData);

    PASS();
}

TEST test_json_map_invalid_floatKey() {

    JSON_ref parsedData = NULL;
    int errCount = json_createFromString("{ -1.0 : \"aNonBareVal\" }", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);

    json_destroy(&parsedData);

    PASS();
}

TEST test_json_map_invalid_boolKey() {

    JSON_ref parsedData = NULL;
    int errCount = json_createFromString("{ true : \"aNonBareVal\" }", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    parsedData = NULL;
    errCount = json_createFromString("{ false : \"aNonBareVal\" }", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);
    json_destroy(&parsedData);

    PASS();
}

TEST test_json_map_invalid_nullKey() {

    JSON_ref parsedData = NULL;
    int errCount = json_createFromString("{ null : \"aNonBareVal\" }", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);

    json_destroy(&parsedData);

    PASS();
}

TEST test_json_map_invalid_mapKey() {

    JSON_ref parsedData = NULL;
    int errCount = json_createFromString("{ { \"foo\" : \"bar\" } : \"aNonBareVal\" }", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);

    json_destroy(&parsedData);

    PASS();
}

TEST test_json_map_invalid_arrayKey() {

    JSON_ref parsedData = NULL;
    int errCount = json_createFromString("{ [ \"foo\", \"bar\" ] : \"aNonBareVal\" }", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);
    ASSERT(parsedData == NULL);

    json_destroy(&parsedData);

    PASS();
}

TEST test_json_map_mutation_1() {

    JSON_ref parsedData = NULL;
    int tokCount = json_createFromString("{}", &parsedData);
    ASSERT(tokCount == 1);

    bool ok = false;
    char *val = NULL;
    long lVal = 0;
    float fVal = 0.f;

    ok = json_mapSetStringValue(parsedData, "key0", "val0");
    ASSERT(ok);
    ok = json_mapCopyStringValue(parsedData, "key0", &val);
    ASSERT(ok);
    ASSERT(strcmp(val, "val0") == 0);
    FREE(val);

    ok = json_mapSetStringValue(parsedData, "key1", "val1");
    ASSERT(ok);
    ok = json_mapCopyStringValue(parsedData, "key1", &val);
    ASSERT(ok);
    ASSERT(strcmp(val, "val1") == 0);
    FREE(val);

    ok = json_mapSetLongValue(parsedData, "longKey0", 42);
    ASSERT(ok);
    ok = json_mapCopyStringValue(parsedData, "longKey0", &val);
    ASSERT(ok);
    ASSERT(strcmp(val, "42") == 0);
    FREE(val);
    ok = json_mapParseLongValue(parsedData, "longKey0", &lVal, 10);
    ASSERT(ok);
    ASSERT(lVal == 42);

    ok = json_mapSetStringValue(parsedData, "key0", "");
    ASSERT(ok);
    ok = json_mapCopyStringValue(parsedData, "key0", &val);
    ASSERT(ok);
    ASSERT(strlen(val) == 0);
    FREE(val);

    ok = json_mapSetStringValue(parsedData, "key1", "hello world");
    ASSERT(ok);
    ok = json_mapCopyStringValue(parsedData, "key1", &val);
    ASSERT(ok);
    ASSERT(strcmp(val, "hello world") == 0);
    FREE(val);

    ok = json_mapSetFloatValue(parsedData, "floatKey0", 0.25);
    ASSERT(ok);
    ok = json_mapParseFloatValue(parsedData, "floatKey0", &fVal);
    ASSERT(ok);
    ASSERT(fVal == 0.25);

    ok = json_mapSetFloatValue(parsedData, "floatKey0", -0.5);
    ASSERT(ok);
    ok = json_mapParseFloatValue(parsedData, "floatKey0", &fVal);
    ASSERT(ok);
    ASSERT(fVal == -0.5);

    // test sub maps ...

    do {
        JSON_ref parsedSubData = NULL;

        tokCount = json_createFromString("{}", &parsedSubData);
        ASSERT(tokCount == 1);

        ok = json_mapSetStringValue(parsedSubData, "foo", "bar");
        ASSERT(ok);

        do {
            JSON_ref parsedSubSubData = NULL;

            tokCount = json_createFromString("{}", &parsedSubSubData);
            ASSERT(tokCount == 1);

            ok = json_mapSetStringValue(parsedSubSubData, "subFoo", "subBar");
            ASSERT(ok);

            ok = json_mapSetJSONValue(parsedSubData, "subKey0", parsedSubSubData);
            ASSERT(ok);

            json_destroy(&parsedSubSubData);
        } while (0);

        ok = json_mapSetJSONValue(parsedData, "key2", parsedSubData);
        ASSERT(ok);

        json_destroy(&parsedSubData);
    } while (0);

    ok = json_mapCopyStringValue(parsedData, "key2", &val);
    ASSERT(ok);
    ASSERT(strcmp(val, "{\"subKey0\":{\"subFoo\":\"subBar\"},\"foo\":\"bar\"}") == 0); // HACK : testing whitespace implementation details
    FREE(val);

    do {
        JSON_ref parsedSubData = NULL;

        ok = json_mapCopyJSON(parsedData, "key2", &parsedSubData);
        ASSERT(ok);
        ASSERT(parsedSubData);

        ok = json_mapCopyStringValue(parsedSubData, "foo", &val);
        ASSERT(ok);
        ASSERT(strcmp(val, "bar") == 0);
        FREE(val);

        ok = json_mapCopyStringValue(parsedSubData, "subKey0", &val);
        ASSERT(ok);
        ASSERT(strcmp(val, "{\"subFoo\":\"subBar\"}") == 0); // HACK : testing whitespace implementation details
        FREE(val);
        do {
            JSON_ref parsedSubSubData = NULL;
            ok = json_mapCopyJSON(parsedSubData, "subKey0", &parsedSubSubData);
            ASSERT(ok);
            ASSERT(parsedSubSubData);

            ok = json_mapCopyStringValue(parsedSubSubData, "subFoo", &val);
            ASSERT(ok);
            ASSERT(strcmp(val, "subBar") == 0);
            FREE(val);

            json_destroy(&parsedSubSubData);
        } while (0);

        json_destroy(&parsedSubData);
    } while (0);

    // setting invalid JSON does not clobber existing data ...
    ok = json_mapSetRawStringValue(parsedData, "key2", "{ aBareKey : \"this is invalid\" }");
    ASSERT(!ok);

    ok = json_mapCopyStringValue(parsedData, "key2", &val);
    ASSERT(ok);
    ASSERT(strcmp(val, "{\"subKey0\":{\"subFoo\":\"subBar\"},\"foo\":\"bar\"}") == 0); // HACK : testing whitespace implementation details
    FREE(val);

    ok = json_mapSetRawStringValue(parsedData, "key2", "{ \"aNotBareKey\" : \"this should now be valid\" }");
    ASSERT(ok);

    ok = json_mapCopyStringValue(parsedData, "key2", &val);
    ASSERT(ok);
    ASSERT(strcmp(val, "{ \"aNotBareKey\" : \"this should now be valid\" }") == 0); // HACK : testing whitespace implementation details
    FREE(val);

    json_destroy(&parsedData);
    PASS();
}

TEST test_prefs_loadString_1() {
    const char *prefsJSON = get_default_preferences();
    prefs_loadString(prefsJSON);

    char *val = NULL;
    bool bVal = false;
    long lVal = 0;
    float fVal = 0.f;

    bool ok = prefs_parseLongValue(PREF_DOMAIN_AUDIO, PREF_SPEAKER_VOLUME, &lVal, /*base:*/10);
    ASSERT(ok);
    ASSERT(lVal == 4);

    ok = prefs_parseLongValue(PREF_DOMAIN_AUDIO, PREF_MOCKINGBOARD_VOLUME, &lVal, /*base:*/10);
    ASSERT(ok);
    ASSERT(lVal == 2);

    ok = prefs_copyStringValue(PREF_DOMAIN_INTERFACE, PREF_DISK_PATH, &val);
    ASSERT(ok);
    ASSERT(strcmp(val, "/usr/local/games/apple2/disks") == 0);
    FREE(val);

    ok = prefs_copyStringValue(PREF_DOMAIN_JOYSTICK, PREF_JOYSTICK_MODE, &val);
    ASSERT(ok);
    ASSERT(strcmp(val, "1") == 0);
    FREE(val);

    ok = prefs_copyStringValue(PREF_DOMAIN_JOYSTICK, "pcJoystickParms", &val);
    ASSERT(ok);
    ASSERT(strcmp(val, "128 128 255 1 255 1") == 0);
    FREE(val);

    ok = prefs_copyStringValue(PREF_DOMAIN_JOYSTICK, "kpJoystickParms", &val);
    ASSERT(ok);
    ASSERT(strcmp(val, "8 1") == 0);
    FREE(val);

    ok = prefs_parseBoolValue(PREF_DOMAIN_KEYBOARD, PREF_KEYBOARD_CAPS, &bVal);
    ASSERT(ok);
    ASSERT(bVal == true);

    ok = prefs_copyStringValue(PREF_DOMAIN_VIDEO, PREF_COLOR_MODE, &val);
    ASSERT(ok);
    ASSERT(strcmp(val, "2") == 0);
    FREE(val);

    ok = prefs_parseFloatValue(PREF_DOMAIN_VM, PREF_CPU_SCALE, &fVal);
    ASSERT(ok);
    ASSERT(fVal == 1.f);

    ok = prefs_parseFloatValue(PREF_DOMAIN_VM, PREF_CPU_SCALE_ALT, &fVal);
    ASSERT(ok);
    ASSERT(fVal == 4.f);

    PASS();
}

TEST test_prefs_set_props() {
    const char *prefsJSON = get_default_preferences();
    prefs_loadString(prefsJSON);

    char *val = NULL;
    bool bVal = true;
    long lVal = 0;
    float fVal = 0.f;

    bool ok = prefs_setLongValue(PREF_DOMAIN_AUDIO, PREF_SPEAKER_VOLUME, 8);
    ASSERT(ok);
    ok = prefs_parseLongValue(PREF_DOMAIN_AUDIO, PREF_SPEAKER_VOLUME, &lVal, /*base:*/10);
    ASSERT(ok);
    ASSERT(lVal == 8);

    ok = prefs_setStringValue(PREF_DOMAIN_INTERFACE, PREF_DISK_PATH, "/home/apple2ix/disks");
    ASSERT(ok);
    ok = prefs_copyStringValue(PREF_DOMAIN_INTERFACE, PREF_DISK_PATH, &val);
    ASSERT(ok);
    ASSERT(strcmp(val, "/home/apple2ix/disks") == 0);
    FREE(val);

    ok = prefs_setBoolValue(PREF_DOMAIN_KEYBOARD, PREF_KEYBOARD_CAPS, false);
    ASSERT(ok);
    ok = prefs_parseBoolValue(PREF_DOMAIN_KEYBOARD, PREF_KEYBOARD_CAPS, &bVal);
    ASSERT(ok);
    ASSERT(bVal == false);

    ok = prefs_setFloatValue(PREF_DOMAIN_VM, PREF_CPU_SCALE_ALT, 0.25f);
    ASSERT(ok);
    ok = prefs_parseFloatValue(PREF_DOMAIN_VM, PREF_CPU_SCALE_ALT, &fVal);
    ASSERT(ok);
    ASSERT(fVal == 0.25f);

    PASS();
}

#define TEST_JSON "test-apple2ix.json"
#define EXPECTED_TEST_PREFS_FILE_SIZE 178
#define EXPECTED_TEST_PREFS_SHA "263844f0177a9229eece7907cd9f6f72aef535f5"
TEST test_prefs_load_and_save() {
    unlink(TEST_JSON);
    putenv("APPLE2IX_JSON=" TEST_JSON);
    prefs_load();
    prefs_save();

    bool ok = false;
    char *val = (char *)0xdeadc0de;
    bool bVal = false;
    long lVal = 42;
    float fVal = 0.125;

    ok = prefs_copyStringValue(PREF_DOMAIN_INTERFACE, PREF_DISK_PATH, &val);
    ASSERT(!ok);
    ASSERT(val == (char *)0xdeadc0de);
    //FREE(val);
    ok = prefs_setStringValue(PREF_DOMAIN_INTERFACE, PREF_DISK_PATH, "/home/apple2ix/disks");
    ASSERT(ok);
    ok = prefs_copyStringValue(PREF_DOMAIN_INTERFACE, PREF_DISK_PATH, &val);
    ASSERT(ok);
    ASSERT(val);
    ASSERT(strcmp(val, "/home/apple2ix/disks") == 0);
    FREE(val);

    ok = prefs_parseLongValue(PREF_DOMAIN_AUDIO, PREF_SPEAKER_VOLUME, &lVal, /*base:*/10);
    ASSERT(!ok);
    ASSERT(lVal == 42);
    ok = prefs_setLongValue(PREF_DOMAIN_AUDIO, PREF_SPEAKER_VOLUME, 8);
    ASSERT(ok);
    ok = prefs_parseLongValue(PREF_DOMAIN_AUDIO, PREF_SPEAKER_VOLUME, &lVal, /*base:*/10);
    ASSERT(ok);
    ASSERT(lVal == 8);

    ok = prefs_parseBoolValue(PREF_DOMAIN_KEYBOARD, PREF_KEYBOARD_CAPS, &bVal);
    ASSERT(!ok);
    ASSERT(bVal == false);
    ok = prefs_setBoolValue(PREF_DOMAIN_KEYBOARD, PREF_KEYBOARD_CAPS, true);
    ASSERT(ok);
    ok = prefs_parseBoolValue(PREF_DOMAIN_KEYBOARD, PREF_KEYBOARD_CAPS, &bVal);
    ASSERT(ok);
    ASSERT(bVal == true);

    ok = prefs_parseFloatValue(PREF_DOMAIN_VM, PREF_CPU_SCALE_ALT, &fVal);
    ASSERT(!ok);
    ASSERT(fVal == 0.125f);
    ok = prefs_setFloatValue(PREF_DOMAIN_VM, PREF_CPU_SCALE_ALT, 0.25f);
    ASSERT(ok);
    ok = prefs_parseFloatValue(PREF_DOMAIN_VM, PREF_CPU_SCALE_ALT, &fVal);
    ASSERT(ok);
    ASSERT(fVal == 0.25f);

    prefs_save();

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(TEST_JSON, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == EXPECTED_TEST_PREFS_FILE_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = MALLOC(EXPECTED_TEST_PREFS_FILE_SIZE);
        if (fread(buf, 1, EXPECTED_TEST_PREFS_FILE_SIZE, fp) != EXPECTED_TEST_PREFS_FILE_SIZE) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, EXPECTED_TEST_PREFS_FILE_SIZE, md);
        FREE(buf);

        sha1_to_str(md, mdstr0);
        ASSERT(strcasecmp(mdstr0, EXPECTED_TEST_PREFS_SHA) == 0);
    } while(0);

    unlink(TEST_JSON);

    PASS();
}

// ----------------------------------------------------------------------------
// Test Suite

GREATEST_SUITE(test_suite_prefs) {
    GREATEST_SET_SETUP_CB(testprefs_setup, NULL);
    GREATEST_SET_TEARDOWN_CB(testprefs_teardown, NULL);
    //GREATEST_SET_BREAKPOINT_CB(test_breakpoint, NULL);

    // TESTS --------------------------
    test_thread_running = true;

    RUN_TESTp(test_json_map_1);

    RUN_TESTp(test_json_serialization);
    RUN_TESTp(test_json_serialization_pretty);

    RUN_TESTp(test_json_raw_bool);
    RUN_TESTp(test_json_invalid_raw_bool);
    RUN_TESTp(test_json_raw_null);
    RUN_TESTp(test_json_invalid_raw_null);
    RUN_TESTp(test_json_raw_int);
    RUN_TESTp(test_json_invalid_raw_int);
    RUN_TESTp(test_json_raw_float);
    RUN_TESTp(test_json_invalid_raw_float);
    RUN_TESTp(test_json_raw_string);
    RUN_TESTp(test_json_invalid_raw_string);
    RUN_TESTp(test_json_invalid_raw_comma);
    RUN_TESTp(test_json_invalid_raw_quote);
    RUN_TESTp(test_json_invalid_raw_minus);
    RUN_TESTp(test_json_invalid_raw_e);
    RUN_TESTp(test_json_empty);

    RUN_TESTp(test_json_map_null);
    RUN_TESTp(test_json_map_bool);
    RUN_TESTp(test_json_map_float);
    RUN_TESTp(test_json_map_invalid_bareKey);
    RUN_TESTp(test_json_map_invalid_bareVal);
    RUN_TESTp(test_json_map_invalid_dangling);
    RUN_TESTp(test_json_array_invalid_dangling);
    RUN_TESTp(test_json_map_invalid_intKey);
    RUN_TESTp(test_json_map_invalid_floatKey);
    RUN_TESTp(test_json_map_invalid_boolKey);
    RUN_TESTp(test_json_map_invalid_nullKey);
    RUN_TESTp(test_json_map_invalid_mapKey);
    RUN_TESTp(test_json_map_invalid_arrayKey);

    RUN_TESTp(test_json_map_mutation_1);

    RUN_TESTp(test_prefs_loadString_1);
    RUN_TESTp(test_prefs_set_props);
    RUN_TESTp(test_prefs_load_and_save);

    // --------------------------------
    pthread_mutex_unlock(&interface_mutex);
}

SUITE(test_suite_prefs);
GREATEST_MAIN_DEFS();

static char **test_argv = NULL;
static int test_argc = 0;

static int _test_thread(void) {
    int argc = test_argc;
    char **argv = test_argv;
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(test_suite_prefs);
    GREATEST_MAIN_END();
}

static void *test_thread(void *dummyptr) {
    _test_thread();
    return NULL;
}

void test_prefs(int argc, char **argv) {
    test_argc = argc;
    test_argv = argv;

    pthread_mutex_lock(&interface_mutex);

    test_common_init();

    pthread_t p;
    pthread_create(&p, NULL, (void *)&test_thread, (void *)NULL);

    while (!test_thread_running) {
        struct timespec ts = { .tv_sec=0, .tv_nsec=33333333 };
        nanosleep(&ts, NULL);
    }
    emulator_start();
    //pthread_join(p, NULL);
}

#if !defined(__APPLE__) && !defined(ANDROID)
int main(int argc, char **argv) {
    test_prefs(argc, argv);
}
#endif
