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

#define BLANK_DSK "blank.dsk.gz"

static bool test_thread_running = false;

extern pthread_mutex_t interface_mutex; // TODO FIXME : raw access to CPU mutex because stepping debugger ...

static void testspeaker_setup(void *unused) {
    test_common_setup();
    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    apple_ii_64k[0][TESTOUT_ADDR] = 0x00;
    joy_button0 = 0xff; // OpenApple
    if (test_do_reboot) {
        cpu65_interrupt(ResetSig);
    }
}

static void testspeaker_teardown(void *unused) {
}

// ----------------------------------------------------------------------------
// Spaker tests ...

extern unsigned long (*testing_getCyclesCount)(void);
extern void (*testing_cyclesOverflow)(void);
static bool cycles_overflowed = false;

static unsigned long testspeaker_getCyclesCount(void) {
    // advance cycles count to near overflow
    return ULONG_MAX - (CLK_6502_INT);
}

static void testspeaker_cyclesOverflow(void) {
    cycles_overflowed = true;
}

// ----------------------------------------------------------------------------
// TESTS ...

TEST test_timing_overflow() {

    // force an almost overflow

    testing_getCyclesCount = &testspeaker_getCyclesCount;
    testing_cyclesOverflow = &testspeaker_cyclesOverflow;

    ASSERT(!cycles_overflowed);
    test_setup_boot_disk(BLANK_DSK, /*readonly:*/1);
    BOOT_TO_DOS();
    ASSERT(cycles_overflowed);

    // appears emulator handled cycle count overflow gracefully ...
    testing_getCyclesCount = NULL;
    testing_cyclesOverflow = NULL;

    PASS();
}

#define EXPECTED_BEEP_TRACE_FILE_SIZE 770
#define EXPECTED_BEEP_TRACE_SHA "69C728A65B5933D73F91D77694BEE7F674C9EDF7"
TEST test_boot_sound() {

    const char *homedir = HOMEDIR;
    char *testout = NULL;
    ASPRINTF(&testout, "%s/a2_speaker_beep_test.txt", homedir);
    if (testout) {
        unlink(testout);
        speaker_traceBegin(testout);
    }

    test_setup_boot_disk(BLANK_DSK, /*readonly:*/1);
    BOOT_TO_DOS();

    speaker_traceEnd();
    disk6_eject(0);

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(testout, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == EXPECTED_BEEP_TRACE_FILE_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = MALLOC(EXPECTED_BEEP_TRACE_FILE_SIZE);
        if (fread(buf, 1, EXPECTED_BEEP_TRACE_FILE_SIZE, fp) != EXPECTED_BEEP_TRACE_FILE_SIZE) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, EXPECTED_BEEP_TRACE_FILE_SIZE, md);
        FREE(buf);

        sha1_to_str(md, mdstr0);
        ASSERT(strcmp(mdstr0, EXPECTED_BEEP_TRACE_SHA) == 0);
    } while(0);

    unlink(testout);
    FREE(testout);

    PASS();
}

// ----------------------------------------------------------------------------
// Test Suite

GREATEST_SUITE(test_suite_speaker) {
    pthread_mutex_lock(&interface_mutex);

    test_thread_running = true;

    GREATEST_SET_SETUP_CB(testspeaker_setup, NULL);
    GREATEST_SET_TEARDOWN_CB(testspeaker_teardown, NULL);
    //GREATEST_SET_BREAKPOINT_CB(test_breakpoint, NULL);

    // TESTS --------------------------

    RUN_TESTp(test_timing_overflow);
    RUN_TESTp(test_boot_sound);

    // --------------------------------
    pthread_mutex_unlock(&interface_mutex);
}

SUITE(test_suite_speaker);
GREATEST_MAIN_DEFS();

static char **test_argv = NULL;
static int test_argc = 0;

static int _test_thread(void) {
    int argc = test_argc;
    char **argv = test_argv;
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(test_suite_speaker);
    GREATEST_MAIN_END();
}

static void *test_thread(void *dummyptr) {
    _test_thread();
    return NULL;
}

void test_speaker(int _argc, char **_argv) {
    test_argc = _argc;
    test_argv = _argv;

    test_common_init();

    pthread_t p;
    pthread_create(&p, NULL, (void *)&test_thread, (void *)NULL);
    while (!test_thread_running) {
        struct timespec ts = { .tv_sec=0, .tv_nsec=33333333 };
        nanosleep(&ts, NULL);
    }
    pthread_detach(p);
}

