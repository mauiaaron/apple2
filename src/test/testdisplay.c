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
    test_common_setup();
    apple_ii_64k[0][MIXSWITCH_ADDR] = 0x00;
    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    run_args.joy_button0 = 0xff; // OpenApple
    if (test_do_reboot) {
        cpu65_interrupt(ResetSig);
    }
}

static void testdisplay_teardown(void *arg) {
}

// ----------------------------------------------------------------------------
// Various Display Tests ...

TEST test_boot_disk() {
    test_setup_boot_disk("testdisplay1.dsk.gz", 1);

    BOOT_TO_DOS();

    PASS();
}

#if VIDEO_TRACING

#   define EXPECTED_BOOT_VIDEO_TRACE_FILE_SIZE 1484244660
#   define EXPECTED_BOOT_VIDEO_TRACE_FILE_SHA "26b9e21914d4047e6b190fb4f6fcb6854a4eaa25"
// NOTE that CONFORMANT_TRACKS codepaths will change the output of this tracing (just like cpu tracing)
//  - Data in screen holes (e.g., 0478-047F is used by Disk ][
//  - Longer boot time with CONFORMANT_TRACKS will add more frames of output
#   if CONFORMANT_TRACKS
TEST test_boot_video_trace(void) {
    test_setup_boot_disk("testdisplay1.dsk.gz", 1);

    srandom(0);
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

#   define EXPECTED_TRACE_40COL_FILE_SIZ 698230
#   define EXPECTED_TRACE_40COL_FILE_SHA "03dd130fa58c068d2434cf7fa244f64ec058290b"
TEST test_video_trace_40col(void) {
    test_setup_boot_disk("testdisplay1.dsk.gz", 1);

    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    test_type_input("CATALOG\rPOKE7987,255:REM TRIGGER DEBUGGER\r");
    c_debugger_go();
    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;

    const char *homedir = HOMEDIR;
    char *traceFile = NULL;
    ASPRINTF(&traceFile, "%s/a2_video_trace_40col.txt", homedir);
    ASSERT(traceFile);

    unlink(traceFile);

    video_scannerTraceBegin(traceFile, 1);

    debugger_setBreakCallback(&video_scannerTraceShouldStop);
    c_debugger_go();
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

    c_debugger_set_timeout(5);
    c_debugger_clear_watchpoints();
    c_debugger_go();
    c_debugger_set_timeout(0);

    const char *homedir = HOMEDIR;
    char *traceFile = NULL;
    ASPRINTF(&traceFile, "%s/a2_video_trace_liltexwin.txt", homedir);
    ASSERT(traceFile);

    unlink(traceFile);

    video_scannerTraceBegin(traceFile, 1);

    debugger_setBreakCallback(&video_scannerTraceShouldStop);
    c_debugger_go();
    debugger_setBreakCallback(NULL);
    c_debugger_set_watchpoint(WATCHPOINT_ADDR);

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
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA("51E5960115380C64351ED00A2ACAB0EB67970249");

    PASS();
}

TEST test_80col_normal() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    test_type_input("PR#3\rRUN TESTNORMALTEXT\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA("ED9CE59F41A51A5ABB1617383A411455677A78E3");

    PASS();
}

TEST test_40col_inverse() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    test_type_input("RUN TESTINVERSETEXT\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA("20957B960C3C0DE0ABEE0058A08C0DDA24AB31D8");

    PASS();
}

TEST test_80col_inverse() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    test_type_input("PR#3\rRUN TESTINVERSETEXT\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA("037798F4BCF740D0A7CBF7CDF5FC5D1B0B3C77A2");

    PASS();
}

// ----------------------------------------------------------------------------
// LORES
//
// 2014/04/05 NOTE : Tests may be successful but graphics display appears to be somewhat b0rken
//
// NOTE : These tests assume standard color mode (not b/w or interpolated)
//

TEST test_lores_with_80col() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    test_type_input("TEXT\rPR#3\rRUN TESTLORES\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
#if !CONFORMANT_TRACKS
    ASSERT_SHA("7B642FF04DE03142A2CE1062C28A4D92E492EDDC");
#endif

    WAIT_FOR_FB_SHA("09C732B37F9E482C8CBE38DA97C99EE640EB8913");

    PASS();
}

TEST test_lores_with_40col() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    test_type_input("TEXT\rHOME\rLOAD TESTLORES\rLIST\rLIST\rRUN\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA_OLD("D7DC78F5718B4CF8716614E79ADABCAB919FCE5D");

    WAIT_FOR_FB_SHA("36287232F7FD4574AA5E05F1C6CACB598C9C2903");
    ASSERT_SHA     ("36287232F7FD4574AA5E05F1C6CACB598C9C2903"); // stable through next frame

    PASS();
}

TEST test_lores_40colmix_normal() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    test_type_input("TEXT\rHOME\rLOAD TESTLORES\rLIST\rLIST\rPOKE 7986,127\rRUN\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
#if !CONFORMANT_TRACKS
    ASSERT_SHA("9097A6AE967E4501B40C7CD7EEE115B8C478B345");
#endif

    WAIT_FOR_FB_SHA("B460804F69A416D78462818493933BA2FFEB70C8");

    PASS();
}

TEST test_lores_40colmix_inverse() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    test_type_input("TEXT\rINVERSE\rHOME\rLOAD TESTLORES\rLIST\rLIST\rPOKE 7986,127\rRUN\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA_OLD("5256E8B96CB04F48324B587ECCCF8A435077B5DE");

    WAIT_FOR_FB_SHA("C2ADD78885B65F7D2FA84F999B06CB32D25EF8A0");
    ASSERT_SHA     ("C2ADD78885B65F7D2FA84F999B06CB32D25EF8A0"); // stable through next frame

    PASS();
}

TEST test_lores_80colmix_normal() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    test_type_input("TEXT\rPR#3\rLOAD TESTLORES\rLIST\rLIST\rPOKE 7986,127\rRUN\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA_OLD("9D5D5382B0A18A71DC135CAD51BEA2665ADB5FB2");

    WAIT_FOR_FB_SHA("610E61D466AAE88CF694B3E1029D3D4C28D1D820");
    ASSERT_SHA     ("610E61D466AAE88CF694B3E1029D3D4C28D1D820"); // stable through next frame

    PASS();
}

TEST test_lores_80colmix_inverse() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    test_type_input("TEXT\rINVERSE\rPR#3\rLOAD TESTLORES\rLIST\rLIST\rPOKE 7986,127\rRUN\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA_OLD("7936E87BE1F920AACD43268DB288746528E89959");

    WAIT_FOR_FB_SHA("CBAEE8961F20079BF45CD197878F1111A6E89E26");
    ASSERT_SHA     ("CBAEE8961F20079BF45CD197878F1111A6E89E26"); // stable through next frame

    PASS();
}

// ----------------------------------------------------------------------------
// HIRES
//
// NOTE : These tests assume standard color mode (not b/w or interpolated)
//
#define MOIRE_SHA "1A5DD96B7E3538C2C3625A37653E013E3998F825"

TEST test_hires_with_80col() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    test_type_input("PR#3\rRUN TESTHIRES_2\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA(MOIRE_SHA);

    PASS();
}

TEST test_hires_with_40col() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    test_type_input("RUN TESTHIRES_2\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA(MOIRE_SHA);

    PASS();
}

TEST test_hires_with_40col_page2() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    test_type_input("RUN TESTHIRES_PG2\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    // FIXME TODO ... pump CPU until frame complete a few times ... should check All The SHAs here ...
    display_waitForNextCompleteFramebuffer();
    display_waitForNextCompleteFramebuffer();

    ASSERT_SHA(MOIRE_SHA);

    PASS();
}

TEST test_hires_40colmix_normal() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    test_type_input("POKE 7986,127\rRUN TESTHIRES\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA("37F41F74EB23F8812498F732E6DA34A0EBC4D68A");

    PASS();
}

TEST test_hires_40colmix_inverse() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    test_type_input("INVERSE\rPOKE 7986,127\rRUN TESTHIRES\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA("253D1823F5DAC0300B46B3D49C04CD59CC70076F");

    PASS();
}

TEST test_hires_80colmix_normal() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    test_type_input("PR#3\rLOAD TESTHIRES\rLIST\rLIST\rPOKE 7986,127\rRUN\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    WAIT_FOR_FB_SHA("8D02F9A7CFC7A7E6D836B01862389F55E877E4E6");

    PASS();
}

TEST test_hires_80colmix_inverse() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    test_type_input("INVERSE\rPR#3\rLOAD TESTHIRES\rLIST\rLIST\rPOKE 7986,127\rRUN\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
#if !CONFORMANT_TRACKS
    ASSERT_SHA("FAFBB65013DA3D5173487C3F434C36A7C04DE92E");
#endif

    WAIT_FOR_FB_SHA("30C0329061781FD1BFE214940F9D5EDFA5FA5F08");

    PASS();
}

TEST test_80col_lores() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    test_type_input("RUN TESTLORES80\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA("02257E25170D8E28F607C033B9D623F55641C7BA");

    PASS();
}

TEST test_80col_hires() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    test_type_input("RUN TESTHIRES80_2\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA("B48F940B1566607E2557A50B98C6C87FFF0C05B2");

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA("3BDFD6B009C090789D968AA7F928579EA31CBE3D");

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
    RUN_TESTp(test_boot_video_trace);
    RUN_TESTp(test_video_trace_40col);
    RUN_TESTp(test_video_trace_liltexwin);
#endif

    RUN_TESTp(test_boot_disk);

    // text modes
    RUN_TESTp(test_40col_normal);
    test_do_reboot = false;
    RUN_TESTp(test_40col_normal);
    test_do_reboot = true;

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

    RUN_TEST(test_lores_with_80col);
    test_do_reboot = false;
    RUN_TEST(test_lores_with_80col);
    test_do_reboot = true;

    RUN_TEST(test_lores_with_40col);
    test_do_reboot = false;
    RUN_TEST(test_lores_with_40col);
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
    test_do_reboot = true;

    // hires

    RUN_TESTp(test_hires_with_80col);
    test_do_reboot = false;
    RUN_TESTp(test_hires_with_80col);
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

    RUN_TESTp(test_hires_80colmix_normal);
    test_do_reboot = false;
    RUN_TESTp(test_hires_80colmix_normal);
    test_do_reboot = true;

    RUN_TESTp(test_hires_80colmix_inverse);
    test_do_reboot = false;
    RUN_TESTp(test_hires_80colmix_inverse);
    test_do_reboot = true;

    // double-lo/hi

    RUN_TEST(test_80col_lores);
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

    srandom(time(NULL));

    test_common_init();

    pthread_t p;
    pthread_create(&p, NULL, (void *)&test_thread, (void *)NULL);
    while (!test_thread_running) {
        struct timespec ts = { .tv_sec=0, .tv_nsec=33333333 };
        nanosleep(&ts, NULL);
    }
    pthread_detach(p);
}

