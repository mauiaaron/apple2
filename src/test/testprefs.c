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
    "        \"variant\" : \"keypad\","
    "        \"pcJoystickParms\" : \"128 128 255 1 255 1\","
    "        \"kpJoystickParms\" : \"8 1\""
    "    },"
    "    \"keyboard\" : {"
    "        \"caps\" : true"
    "    }",
    "    \"video\" : {"
    "        \"color\" : \"interpolated\""
    "    },"
    "    \"vm\" : {"
    "        \"speed\" : 1.0,"
    "        \"altSpeed\" : 4.0"
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
"        \"intKey2\"  : -000000000,                "
"        \"intKey3\"  :  0x2400,                   "
"        \"intKey4\"  :  10111111,                 "
"        \"floatKey0\"      : 0.0   ,              "
"        \"floatKey1\"      : -.0001220703125     ,"
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

TEST test_json_invalid_bareKey() {

    JSON_ref parsedData = NULL;
    int errCount = json_createFromString("{ aBareKey : \"aNonBareVal\" }", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);

    json_destroy(&parsedData);

    PASS();
}

TEST test_json_invalid_bareVal() {

    JSON_ref parsedData = NULL;
    int errCount = json_createFromString("{ \"aNonBareKey\" : aBareVal }", &parsedData);
    ASSERT(errCount == JSMN_ERROR_INVAL);

    json_destroy(&parsedData);

    PASS();
}

TEST test_json_map_invalid_danglingComma() {

    JSON_ref parsedData = NULL;
    int errCount = json_createFromString("{ \"aNonBareKey\" : \"aNonBareVal\", }", &parsedData); // BUG IN JSMN ...
    ASSERT(errCount == JSMN_ERROR_INVAL);

    json_destroy(&parsedData);

    PASS();
}

TEST test_json_map_invalid_danglingKey() {

    JSON_ref parsedData = NULL;
    int errCount = json_createFromString("{ \"aNonBareKey\" : \"aNonBareVal\", \"aNoneBareButDanglingKey\" }", &parsedData); // BUG IN JSMN ...
    ASSERT(errCount == JSMN_ERROR_INVAL);

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

#if 0
TEST test_prefs_loadString_1() {
    const char *prefsJSON = get_default_preferences();
    bool ok = prefs_loadString(prefsJSON);
    ASSERT(ok);

    char *val = NULL;
    bool bVal = false;
    long lVal = 0;
    bool fVal = 0.f;

    ok = prefs_parseLongValue(PREF_DOMAIN_AUDIO, "speakerVolume", &lVal);
    ASSERT(ok);
    ASSERT(lVal == 4);

    ok = prefs_parseLongValue(PREF_DOMAIN_AUDIO, "mbVolume", &lVal);
    ASSERT(ok);
    ASSERT(lVal == 2);

    ok = prefs_copyStringValue(PREF_DOMAIN_INTERFACE, "diskPath", &val);
    ASSERT(ok);
    ASSERT(strcmp(val, "/usr/local/games/apple2/disks") == 0);
    FREE(val);

    ok = prefs_copyStringValue(PREF_DOMAIN_JOYSTICK, "variant", &val);
    ASSERT(ok);
    ASSERT(strcmp(val, "keypad") == 0);
    FREE(val);

    ok = prefs_copyStringValue(PREF_DOMAIN_JOYSTICK, "pcJoystickParms", &val);
    ASSERT(ok);
    ASSERT(strcmp(val, "128 128 255 1 255 1") == 0);
    FREE(val);

    ok = prefs_copyStringValue(PREF_DOMAIN_JOYSTICK, "kpJoystickParms", &val);
    ASSERT(ok);
    ASSERT(strcmp(val, "8 1") == 0);
    FREE(val);

    ok = prefs_parseBoolValue(PREF_DOMAIN_KEYBOARD, "caps", &bVal);
    ASSERT(ok);
    ASSERT(bVal == true);

    ok = prefs_copyStringValue(PREF_DOMAIN_VIDEO, "color", &val);
    ASSERT(ok);
    ASSERT(strcmp(val, "interpolated") == 0);
    FREE(val);

    ok = prefs_parseFloatValue(PREF_DOMAIN_VM, "speed", &fVal);
    ASSERT(ok);
    ASSERT(fVal == 1.f);

    ok = prefs_parseFloatValue(PREF_DOMAIN_VM, "altSpeed", &fVal);
    ASSERT(ok);
    ASSERT(fVal == 4.f);

    PASS();
}
#endif

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

    RUN_TESTp(test_json_invalid_bareKey);
    RUN_TESTp(test_json_invalid_bareVal);
    RUN_TESTp(test_json_map_invalid_danglingComma);
    RUN_TESTp(test_json_map_invalid_danglingKey);

    RUN_TESTp(test_json_map_mutation_1);

    //RUN_TESTp(test_prefs_loadString_1);

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
