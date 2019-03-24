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

#include "testcommon.h"

static bool test_thread_running = false;

extern pthread_mutex_t interface_mutex; // TODO FIXME : raw access to CPU mutex because stepping debugger ...

static void testdisplay_setup(void *arg) {
    test_setup_boot_disk("testdisplay1.dsk.gz", 1);
    test_common_setup();
    apple_ii_64k[0][MIXSWITCH_ADDR] = 0x00;
    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    run_args.joy_button0 = 0xff; // OpenApple
    if (test_do_reboot) {
        cpu65_interrupt(ResetSig);
    }
}

static void testdisplay_teardown(void *arg) {
    test_do_reboot = true;
}

// ----------------------------------------------------------------------------
// Various Display Tests ...

#if VIDEO_TRACING

#if 0
#   define EXPECTED_BOOT_VIDEO_TRACE_FILE_SIZE 1484244660
#   define EXPECTED_BOOT_VIDEO_TRACE_FILE_SHA "26b9e21914d4047e6b190fb4f6fcb6854a4eaa25"
// NOTE that CONFORMANT_TRACKS codepaths will change the output of this tracing (just like cpu tracing)
//  - Data in screen holes (e.g., 0478-047F is used by Disk ][
//  - Longer boot time with CONFORMANT_TRACKS will add more frames of output
#   if CONFORMANT_TRACKS
TEST test_boot_video_trace(void) {

    const char *homedir = HOMEDIR;
    char *traceFile = NULL;
    ASPRINTF(&traceFile, "%s/a2_boot_video_trace.txt", homedir);
    ASSERT(traceFile);
    unlink(traceFile);
    video_scannerTraceBegin(traceFile, 0);

    BOOT_TO_DOS();

    video_scannerTraceEnd();
    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(traceFile, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == EXPECTED_BOOT_VIDEO_TRACE_FILE_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = MALLOC(EXPECTED_BOOT_VIDEO_TRACE_FILE_SIZE);
        if (fread(buf, 1, EXPECTED_BOOT_VIDEO_TRACE_FILE_SIZE, fp) != EXPECTED_BOOT_VIDEO_TRACE_FILE_SIZE) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, EXPECTED_BOOT_VIDEO_TRACE_FILE_SIZE, md);
        FREE(buf);

        sha1_to_str(md, mdstr0);
        ASSERT(strcasecmp(mdstr0, EXPECTED_BOOT_VIDEO_TRACE_FILE_SHA) == 0);
    } while(0);

    unlink(traceFile);
    FREE(traceFile);

    PASS();
}
#   endif
#   undef EXPECTED_BOOT_VIDEO_TRACE_FILE_SIZE
#   undef EXPECTED_BOOT_VIDEO_TRACE_FILE_SHA
#endif // 0

#   define EXPECTED_TRACE_40COL_FILE_SIZ 698230
#   define EXPECTED_TRACE_40COL_FILE_SHA "2B8C050F84776A78F73A7AE803A474820E11B3C1"
TEST test_video_trace_40col(void) {

    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    test_type_input("CATALOG\r\rPOKE7987,255:REM TRIGGER DEBUGGER\r");
    debugger_go();
    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;

    const char *homedir = HOMEDIR;
    char *traceFile = NULL;
    ASPRINTF(&traceFile, "%s/a2_video_trace_40col.txt", homedir);
    ASSERT(traceFile);

    unlink(traceFile);

    video_scannerTraceBegin(traceFile, 1);

    debugger_setBreakCallback(&video_scannerTraceShouldStop);
    debugger_go();
    debugger_setBreakCallback(NULL);

    video_scannerTraceEnd();

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(traceFile, "r");
        ASSERT(fp);

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == EXPECTED_TRACE_40COL_FILE_SIZ);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = MALLOC(EXPECTED_TRACE_40COL_FILE_SIZ);
        if (fread(buf, 1, EXPECTED_TRACE_40COL_FILE_SIZ, fp) != EXPECTED_TRACE_40COL_FILE_SIZ) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, EXPECTED_TRACE_40COL_FILE_SIZ, md);
        FREE(buf);

        sha1_to_str(md, mdstr0);
        ASSERT(strcasecmp(mdstr0, EXPECTED_TRACE_40COL_FILE_SHA) == 0);
    } while(0);

    unlink(traceFile);
    FREE(traceFile);

    PASS();
}
#   undef EXPECTED_TRACE_40COL_FILE_SIZ
#   undef EXPECTED_TRACE_40COL_FILE_SHA

#   define EXPECTED_TRACE_LILTEXWIN_FILE_SIZ 613920
#   define EXPECTED_TRACE_LILTEXWIN_FILE_SHA "97cf740a3697d8046b0756c7c2baedc893d46237"
TEST test_video_trace_liltexwin(void) {
    test_setup_boot_disk("testdisplay2.dsk.gz", 1); // boots directly into LILTEXWIN

    BOOT_TO_DOS();

    debugger_setTimeout(5);
    debugger_clearWatchpoints();
    debugger_go();
    debugger_setTimeout(0);

    const char *homedir = HOMEDIR;
    char *traceFile = NULL;
    ASPRINTF(&traceFile, "%s/a2_video_trace_liltexwin.txt", homedir);
    ASSERT(traceFile);

    unlink(traceFile);

    video_scannerTraceBegin(traceFile, 1);

    debugger_setBreakCallback(&video_scannerTraceShouldStop);
    debugger_go();
    debugger_setBreakCallback(NULL);
    debugger_setWatchpoint(WATCHPOINT_ADDR);

    video_scannerTraceEnd();

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(traceFile, "r");
        ASSERT(fp);

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == EXPECTED_TRACE_LILTEXWIN_FILE_SIZ);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = MALLOC(EXPECTED_TRACE_LILTEXWIN_FILE_SIZ);
        if (fread(buf, 1, EXPECTED_TRACE_LILTEXWIN_FILE_SIZ, fp) != EXPECTED_TRACE_LILTEXWIN_FILE_SIZ) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, EXPECTED_TRACE_LILTEXWIN_FILE_SIZ, md);
        FREE(buf);

        sha1_to_str(md, mdstr0);
        ASSERT(strcasecmp(mdstr0, EXPECTED_TRACE_LILTEXWIN_FILE_SHA) == 0);
    } while(0);

    unlink(traceFile);
    FREE(traceFile);

    PASS();
}
#   undef EXPECTED_TRACE_LILTEXWIN_FILE_SIZ
#   undef EXPECTED_TRACE_LILTEXWIN_FILE_SHA

#endif

// ----------------------------------------------------------------------------
// TEXT

TEST test_40col_normal() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    test_type_input("RUN TESTNORMALTEXT\r");
    debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    WAIT_FOR_FB_SHA("D676FAFEF4FE5B31832EF875285B7E3A87E47689");

    PASS();
}

TEST test_80col_normal() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    test_type_input("PR#3\rRUN TESTNORMALTEXT\r");
    debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    WAIT_FOR_FB_SHA("BB63B7206CD8741270791872CCD5B77C08169850");

    PASS();
}

TEST test_40col_inverse() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    test_type_input("RUN TESTINVERSETEXT\r");
    debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    WAIT_FOR_FB_SHA("137B1F840ACC1BD23F9636153AAD93CD0FB60E97");

    PASS();
}

TEST test_80col_inverse() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    test_type_input("PR#3\rRUN TESTINVERSETEXT\r");
    debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    WAIT_FOR_FB_SHA("CDAB6BCA6DA883049AF1431EF408F8994615B24A");

    PASS();
}

// ----------------------------------------------------------------------------
// LORES
//

TEST test_lores_with_80col() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    test_type_input("TEXT\rPR#3\rRUN TESTLORES\r");
    debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    WAIT_FOR_FB_SHA("22867871D3E9F26DAB99286724CD24114D185930");

    PASS();
}

TEST test_lores_with_40col() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    test_type_input("TEXT\rHOME\rLOAD TESTLORES\rLIST\rLIST\rRUN\r");
    debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    WAIT_FOR_FB_SHA("89CB9CAB3EAFC24CB3995B2B3846DF7E62C2106F");

    PASS();
}

TEST test_lores_with_40col_2() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    test_type_input("RUNTESTLORES_2\r");
    debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    WAIT_FOR_FB_SHA("0CE19F2C173A7EBAC293198CF0E15A8BE495C9FA");

    PASS();
}

TEST test_lores_40colmix_normal() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    test_type_input("TEXT\rHOME\rLOAD TESTLORES\rLIST\rLIST\rPOKE 7986,127\rRUN\r");
    debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    WAIT_FOR_FB_SHA("C0D1C54E3CD4D8D39B5174AFCB4E1815F3749614");

    PASS();
}

TEST test_lores_40colmix_inverse() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    test_type_input("TEXT\rINVERSE\rHOME\rLOAD TESTLORES\rLIST\rLIST\rPOKE 7986,127\rRUN\r");
    debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    WAIT_FOR_FB_SHA("F2332A32E34F6CF61A69C0C278105F348870E5C6");

    PASS();
}

TEST test_lores_80colmix_normal() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    test_type_input("TEXT\rPR#3\rLOAD TESTLORES\rLIST\rLIST\rPOKE 7986,127\rRUN\r");
    debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    WAIT_FOR_FB_SHA("EC819006C66EFE5CFF45BADC7B2ECA078268B1D3");

    PASS();
}

TEST test_lores_80colmix_inverse() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    test_type_input("TEXT\rINVERSE\rPR#3\rLOAD TESTLORES\rLIST\rLIST\rPOKE 7986,127\rRUN\r");
    debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    WAIT_FOR_FB_SHA("8681289778AB74083712DFF225A2CADADAE9C6DA");

    PASS();
}

// ----------------------------------------------------------------------------
// HIRES
//

#define MOIRE_SHA_BW "04C6F5968C56A07FE19B9AB010B052C6218C8E79"

TEST test_hires_with_80col() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    test_type_input("PR#3\rRUN TESTHIRES_2\r");
    debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    WAIT_FOR_FB_SHA(MOIRE_SHA_BW);

    PASS();
}

TEST test_hires_with_40col() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    test_type_input("RUN TESTHIRES_2\r");
    debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    WAIT_FOR_FB_SHA(MOIRE_SHA_BW);

    PASS();
}

TEST test_hires_with_40col_page2() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    test_type_input("RUN TESTHIRES_PG2\r");
    debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    WAIT_FOR_FB_SHA(MOIRE_SHA_BW);

    PASS();
}

TEST test_hires_40colmix_normal() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    test_type_input("POKE 7986,127\rRUN TESTHIRES\r");
    debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    WAIT_FOR_FB_SHA("EEA7E0FBE38543CEAC0C9B599977C414D32F453C");

    PASS();
}

TEST test_hires_40colmix_inverse() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    test_type_input("INVERSE\rPOKE 7986,127\rRUN TESTHIRES\r");
    debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    WAIT_FOR_FB_SHA("29EE37FF777100486AE0706E1ACAE233F74A8C26");

    PASS();
}

TEST test_hires_80colmix_normal() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    test_type_input("PR#3\rLOAD TESTHIRES\rLIST\rLIST\rPOKE 7986,127\rRUN\r");
    debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    WAIT_FOR_FB_SHA("BE548CA56A0194580637B488D7376D8DBCBC71C8");

    PASS();
}

TEST test_hires_80colmix_inverse() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    test_type_input("INVERSE\rPR#3\rLOAD TESTHIRES\rLIST\rLIST\rPOKE 7986,127\rRUN\r");
    debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    WAIT_FOR_FB_SHA("7B1F9B4BDDE9E0AAA8895B5D9CC9D9126A612244");

    PASS();
}

// ----------------------------------------------------------------------------
// 80LORES & 80HIRES
//

TEST test_80col_lores() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    test_type_input("RUN TESTLORES80\r");
    debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    WAIT_FOR_FB_SHA("4565AB42DDDEB0FC49A5E868BDE4513BD407F84B");

    PASS();
}

TEST test_80col_lores_2() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    test_type_input("RUN TESTLORES80_2\r");
    debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    WAIT_FOR_FB_SHA("55ECD4261611E1209915AB4052FFA3203C075EF1");

    PASS();
}

TEST test_80col_hires() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    test_type_input("RUN TESTHIRES80_2\r");
    debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    WAIT_FOR_FB_SHA("0BBBDB9EB3D68C54E4791F78CA9865214B879118");

    PASS();
}

// ----------------------------------------------------------------------------
// Test Suite

GREATEST_SUITE(test_suite_display) {
    pthread_mutex_lock(&interface_mutex);

    test_thread_running = true;

    GREATEST_SET_SETUP_CB(testdisplay_setup, NULL);
    GREATEST_SET_TEARDOWN_CB(testdisplay_teardown, NULL);
    GREATEST_SET_BREAKPOINT_CB(test_breakpoint, NULL);

    // TESTS --------------------------

#if VIDEO_TRACING && CONFORMANT_TRACKS
    //RUN_TESTp(test_boot_video_trace); -- Not valid presently
    //RUN_TESTp(test_video_trace_40col); -- Need more stable test : disk is settled and cursor flashing ...
    RUN_TESTp(test_video_trace_liltexwin);
#endif
    // text modes
    RUN_TESTp(test_40col_normal);
    test_do_reboot = false;
    RUN_TESTp(test_40col_normal);
    test_do_reboot = true;

    RUN_TESTp(test_80col_normal);
    test_do_reboot = false;
    RUN_TESTp(test_80col_normal);
    test_do_reboot = true;

    RUN_TESTp(test_40col_inverse);
    test_do_reboot = false;
    RUN_TESTp(test_40col_inverse);
    test_do_reboot = true;

    RUN_TESTp(test_80col_inverse);
    test_do_reboot = false;
    RUN_TESTp(test_80col_inverse);
    test_do_reboot = true;

    // lores

    test_do_reboot = true;
    RUN_TEST(test_lores_with_80col);
    test_do_reboot = false;
    RUN_TEST(test_lores_with_80col);

    test_do_reboot = true;
    RUN_TEST(test_lores_with_40col);
    test_do_reboot = false;
    RUN_TEST(test_lores_with_40col);

    test_do_reboot = true;
    RUN_TEST(test_lores_with_40col_2);
    test_do_reboot = false;
    RUN_TEST(test_lores_with_40col_2);

    test_do_reboot = true;
    RUN_TEST(test_lores_40colmix_normal);
    test_do_reboot = false;
    RUN_TEST(test_lores_40colmix_normal);

    test_do_reboot = true;
    RUN_TEST(test_lores_40colmix_inverse);
    test_do_reboot = false;
    RUN_TEST(test_lores_40colmix_inverse);

    test_do_reboot = true;
    RUN_TEST(test_lores_80colmix_normal);
    test_do_reboot = false;
    RUN_TEST(test_lores_80colmix_normal);

    test_do_reboot = true;
    RUN_TEST(test_lores_80colmix_inverse);
    test_do_reboot = false;
    RUN_TEST(test_lores_80colmix_inverse);

    // hires

    test_do_reboot = true;
    RUN_TEST(test_hires_with_80col);
    test_do_reboot = false;
    RUN_TEST(test_hires_with_80col);

    test_do_reboot = true;
    RUN_TEST(test_hires_with_40col);
    test_do_reboot = false;
    RUN_TEST(test_hires_with_40col);

    test_do_reboot = true;
    RUN_TEST(test_hires_with_40col_page2);
    test_do_reboot = false;
    RUN_TEST(test_hires_with_40col_page2);

    test_do_reboot = true;
    RUN_TEST(test_hires_40colmix_normal);
    test_do_reboot = false;
    RUN_TEST(test_hires_40colmix_normal);

    test_do_reboot = true;
    RUN_TEST(test_hires_40colmix_inverse);
    test_do_reboot = false;
    RUN_TEST(test_hires_40colmix_inverse);

    test_do_reboot = true;
    RUN_TEST(test_hires_80colmix_normal);
    test_do_reboot = false;
    RUN_TEST(test_hires_80colmix_normal);

    test_do_reboot = true;
    RUN_TEST(test_hires_80colmix_inverse);
    test_do_reboot = false;
    RUN_TEST(test_hires_80colmix_inverse);

    // double-lo/hi

    RUN_TEST(test_80col_lores);

    RUN_TEST(test_80col_lores_2);

    RUN_TEST(test_80col_hires);

    // ...
    disk6_eject(0);
    pthread_mutex_unlock(&interface_mutex);
}

SUITE(test_suite_display);
GREATEST_MAIN_DEFS();

static char **test_argv = NULL;
static int test_argc = 0;

static int _test_thread(void) {
    int argc = test_argc;
    char **argv = test_argv;
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(test_suite_display);
    GREATEST_MAIN_END();
}

static void *test_thread(void *dummyptr) {
    _test_thread();
    return NULL;
}

void test_display(int _argc, char **_argv) {
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

