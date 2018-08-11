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
static color_mode_t test_color_mode = COLOR_MODE_COLOR;

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
    prefs_setLongValue(PREF_DOMAIN_VIDEO, PREF_COLOR_MODE, test_color_mode);
    prefs_sync(PREF_DOMAIN_VIDEO);
}

static void testdisplay_teardown(void *arg) {
    test_color_mode = COLOR_MODE_COLOR;
    test_do_reboot = true;
}

// ----------------------------------------------------------------------------
// Various Display Tests ...

#if VIDEO_TRACING

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

#   define EXPECTED_TRACE_40COL_FILE_SIZ 698230
#   define EXPECTED_TRACE_40COL_FILE_SHA "2B8C050F84776A78F73A7AE803A474820E11B3C1"
TEST test_video_trace_40col(void) {

    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    test_type_input("CATALOG\r\rPOKE7987,255:REM TRIGGER DEBUGGER\r");
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

TEST test_lores_with_80col() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    test_type_input("TEXT\rPR#3\rRUN TESTLORES\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    if (test_color_mode == COLOR_MODE_BW) {
        WAIT_FOR_FB_SHA("DED166782E9C529B6D7DB2EEBFF5877AD49C4C1F");
    } else {
#if !CONFORMANT_TRACKS
        ASSERT_SHA("7B642FF04DE03142A2CE1062C28A4D92E492EDDC");
#endif
        WAIT_FOR_FB_SHA("09C732B37F9E482C8CBE38DA97C99EE640EB8913");
    }

    PASS();
}

TEST test_lores_with_40col() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    test_type_input("TEXT\rHOME\rLOAD TESTLORES\rLIST\rLIST\rRUN\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    if (test_color_mode == COLOR_MODE_BW) {
        WAIT_FOR_FB_SHA("270B44B639E062B1DC7C2EB2E561051130F5F790");
    } else {
        ASSERT_SHA_OLD("D7DC78F5718B4CF8716614E79ADABCAB919FCE5D");
        WAIT_FOR_FB_SHA("36287232F7FD4574AA5E05F1C6CACB598C9C2903");
        ASSERT_SHA     ("36287232F7FD4574AA5E05F1C6CACB598C9C2903"); // stable through next frame
    }

    PASS();
}

TEST test_lores_with_40col_2() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    test_type_input("RUNTESTLORES_2\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    if (test_color_mode == COLOR_MODE_BW) {
        WAIT_FOR_FB_SHA("A3D5F5DF7A2DF15DDDF7E82F83B756827CD142D3");
    } else {
        WAIT_FOR_FB_SHA("D7CC29D2030230258FAFF3772C8F9AD2B318D190");
    }

    PASS();
}

TEST test_lores_40colmix_normal() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    test_type_input("TEXT\rHOME\rLOAD TESTLORES\rLIST\rLIST\rPOKE 7986,127\rRUN\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    if (test_color_mode == COLOR_MODE_BW) {
        WAIT_FOR_FB_SHA("373799861AFCA845826C27571D2FFF7F1CB69BD6");
    } else {
#if !CONFORMANT_TRACKS
        ASSERT_SHA("9097A6AE967E4501B40C7CD7EEE115B8C478B345");
#endif
        WAIT_FOR_FB_SHA("B460804F69A416D78462818493933BA2FFEB70C8");
    }

    PASS();
}

TEST test_lores_40colmix_inverse() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    test_type_input("TEXT\rINVERSE\rHOME\rLOAD TESTLORES\rLIST\rLIST\rPOKE 7986,127\rRUN\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    if (test_color_mode == COLOR_MODE_BW) {
        WAIT_FOR_FB_SHA("F9F4757BB751AD47D975D45DC75B3C93C9F2C6C8");
    } else {
        ASSERT_SHA_OLD("5256E8B96CB04F48324B587ECCCF8A435077B5DE");
        WAIT_FOR_FB_SHA("C2ADD78885B65F7D2FA84F999B06CB32D25EF8A0");
        ASSERT_SHA     ("C2ADD78885B65F7D2FA84F999B06CB32D25EF8A0"); // stable through next frame
    }

    PASS();
}

TEST test_lores_80colmix_normal() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    test_type_input("TEXT\rPR#3\rLOAD TESTLORES\rLIST\rLIST\rPOKE 7986,127\rRUN\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    if (test_color_mode == COLOR_MODE_BW) {
        WAIT_FOR_FB_SHA("DD2A3A05EA38652A86D144FFB5BD98CB24A82FF6");
    } else {
        ASSERT_SHA_OLD("9D5D5382B0A18A71DC135CAD51BEA2665ADB5FB2");
        WAIT_FOR_FB_SHA("610E61D466AAE88CF694B3E1029D3D4C28D1D820");
        ASSERT_SHA     ("610E61D466AAE88CF694B3E1029D3D4C28D1D820"); // stable through next frame
    }

    PASS();
}

TEST test_lores_80colmix_inverse() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    test_type_input("TEXT\rINVERSE\rPR#3\rLOAD TESTLORES\rLIST\rLIST\rPOKE 7986,127\rRUN\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    if (test_color_mode == COLOR_MODE_BW) {
        WAIT_FOR_FB_SHA("0D51D6A375820FE36E4D95127F0E7A8F71495F4A");
    } else {
        ASSERT_SHA_OLD("7936E87BE1F920AACD43268DB288746528E89959");
        WAIT_FOR_FB_SHA("CBAEE8961F20079BF45CD197878F1111A6E89E26");
        ASSERT_SHA     ("CBAEE8961F20079BF45CD197878F1111A6E89E26"); // stable through next frame
    }

    PASS();
}

// ----------------------------------------------------------------------------
// HIRES
//

#define MOIRE_SHA "1A5DD96B7E3538C2C3625A37653E013E3998F825"
#define MOIRE_SHA_BW "DCB2BADC290A9E0A1DF0DEC45D3A653A40AF8B6B"

TEST test_hires_with_80col() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    test_type_input("PR#3\rRUN TESTHIRES_2\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    if (test_color_mode == COLOR_MODE_BW) {
        ASSERT_SHA(MOIRE_SHA_BW);
    } else {
        ASSERT_SHA(MOIRE_SHA);
    }

    PASS();
}

TEST test_hires_with_40col() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    test_type_input("RUN TESTHIRES_2\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    if (test_color_mode == COLOR_MODE_BW) {
        ASSERT_SHA(MOIRE_SHA_BW);
    } else {
        ASSERT_SHA(MOIRE_SHA);
    }

    PASS();
}

TEST test_hires_with_40col_page2() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    test_type_input("RUN TESTHIRES_PG2\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    if (test_color_mode == COLOR_MODE_BW) {
        WAIT_FOR_FB_SHA(MOIRE_SHA_BW);
    } else {
        WAIT_FOR_FB_SHA(MOIRE_SHA);
    }

    PASS();
}

TEST test_hires_40colmix_normal() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    test_type_input("POKE 7986,127\rRUN TESTHIRES\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    if (test_color_mode == COLOR_MODE_BW) {
        ASSERT_SHA("9611721C0F70C5F1FE0172534EC15B977CB099D4");
    } else {
        ASSERT_SHA("37F41F74EB23F8812498F732E6DA34A0EBC4D68A");
    }

    PASS();
}

TEST test_hires_40colmix_inverse() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    test_type_input("INVERSE\rPOKE 7986,127\rRUN TESTHIRES\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    if (test_color_mode == COLOR_MODE_BW) {
        ASSERT_SHA("5CFA5789735AD09FAB8CC7B6EE44CE22CF64A70D");
    } else {
        ASSERT_SHA("253D1823F5DAC0300B46B3D49C04CD59CC70076F");
    }

    PASS();
}

TEST test_hires_80colmix_normal() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    test_type_input("PR#3\rLOAD TESTHIRES\rLIST\rLIST\rPOKE 7986,127\rRUN\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    if (test_color_mode == COLOR_MODE_BW) {
        WAIT_FOR_FB_SHA("4069102016E4E6AA860A32C6BAC5E4A6C6A45B72");
    } else {
        WAIT_FOR_FB_SHA("8D02F9A7CFC7A7E6D836B01862389F55E877E4E6");
    }

    PASS();
}

TEST test_hires_80colmix_inverse() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    test_type_input("INVERSE\rPR#3\rLOAD TESTHIRES\rLIST\rLIST\rPOKE 7986,127\rRUN\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    if (test_color_mode == COLOR_MODE_BW) {
        WAIT_FOR_FB_SHA("C3C19FB3258E7A58F81BC3DC51C2AEDFFC836285");
    } else {
#if !CONFORMANT_TRACKS
        ASSERT_SHA("FAFBB65013DA3D5173487C3F434C36A7C04DE92E");
#endif
        WAIT_FOR_FB_SHA("30C0329061781FD1BFE214940F9D5EDFA5FA5F08");
    }

    PASS();
}

// ----------------------------------------------------------------------------
// 80LORES & 80HIRES
//

TEST test_80col_lores() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    test_type_input("RUN TESTLORES80\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    if (test_color_mode == COLOR_MODE_BW) {
        ASSERT_SHA("D8238DC3ACC1A0E191CEC06F505A159993C2EBFA");
    } else {
        ASSERT_SHA("5BFF6721FB90B3A6AF88D9021A013C007C4AF23A");
    }

    PASS();
}

TEST test_80col_lores_2() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    test_type_input("RUN TESTLORES80_2\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    if (test_color_mode == COLOR_MODE_BW) {
        ASSERT_SHA("5B99DE77F81AD8718FCFB735215A37F7B5ED5DE7");
    } else {
        ASSERT_SHA("98BB7C04854594D9E709302EF29905D2A89F1D34");
    }

    PASS();
}

TEST test_80col_hires() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    test_type_input("RUN TESTHIRES80_2\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    if (test_color_mode == COLOR_MODE_BW) {
        ASSERT_SHA("647F3A377513486121C7609E3F53E97DC6FC456D");
    } else {
        ASSERT_SHA("919EBCBABEA57E932F84E9864B2C35F57F8909B4");
    }

    PASS();
}

// ----------------------------------------------------------------------------
// Test Suite

GREATEST_SUITE(test_suite_display) {
    pthread_mutex_lock(&interface_mutex);

    test_thread_running = true;

    typedef struct parms_t {
        color_mode_t mode;
        bool doReboot;
    } parms_t;
    static parms_t parmsArray[] = {
        { COLOR_MODE_COLOR, true },
        { COLOR_MODE_COLOR, false },
        { COLOR_MODE_BW, true },
        { COLOR_MODE_BW, false },
    };
    unsigned int count = sizeof(parmsArray)/sizeof(parmsArray[0]);

    GREATEST_SET_SETUP_CB(testdisplay_setup, NULL);
    GREATEST_SET_TEARDOWN_CB(testdisplay_teardown, NULL);
    GREATEST_SET_BREAKPOINT_CB(test_breakpoint, NULL);

    // TESTS --------------------------

#if VIDEO_TRACING && CONFORMANT_TRACKS
    RUN_TESTp(test_boot_video_trace);
    RUN_TESTp(test_video_trace_40col);
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

    for (unsigned int i=0; i<count; i++) {
        test_do_reboot = parmsArray[i].doReboot;
        test_color_mode = parmsArray[i].mode;
        RUN_TEST(test_lores_with_80col);
    }

    for (unsigned int i=0; i<count; i++) {
        test_do_reboot = parmsArray[i].doReboot;
        test_color_mode = parmsArray[i].mode;
        RUN_TEST(test_lores_with_40col);
    }

    for (unsigned int i=0; i<count; i++) {
        test_do_reboot = parmsArray[i].doReboot;
        test_color_mode = parmsArray[i].mode;
        RUN_TEST(test_lores_with_40col_2);
    }

    for (unsigned int i=0; i<count; i++) {
        test_do_reboot = parmsArray[i].doReboot;
        test_color_mode = parmsArray[i].mode;
        RUN_TEST(test_lores_40colmix_normal);
    }

    for (unsigned int i=0; i<count; i++) {
        test_do_reboot = parmsArray[i].doReboot;
        test_color_mode = parmsArray[i].mode;
        RUN_TEST(test_lores_40colmix_inverse);
    }

    for (unsigned int i=0; i<count; i++) {
        test_do_reboot = parmsArray[i].doReboot;
        test_color_mode = parmsArray[i].mode;
        RUN_TEST(test_lores_80colmix_normal);
    }

    for (unsigned int i=0; i<count; i++) {
        test_do_reboot = parmsArray[i].doReboot;
        test_color_mode = parmsArray[i].mode;
        RUN_TEST(test_lores_80colmix_inverse);
    }

    // hires

    for (unsigned int i=0; i<count; i++) {
        test_do_reboot = parmsArray[i].doReboot;
        test_color_mode = parmsArray[i].mode;
        RUN_TEST(test_hires_with_80col);
    }

    for (unsigned int i=0; i<count; i++) {
        test_do_reboot = parmsArray[i].doReboot;
        test_color_mode = parmsArray[i].mode;
        RUN_TEST(test_hires_with_40col);
    }

    for (unsigned int i=0; i<count; i++) {
        test_do_reboot = parmsArray[i].doReboot;
        test_color_mode = parmsArray[i].mode;
        RUN_TEST(test_hires_with_40col_page2);
    }

    for (unsigned int i=0; i<count; i++) {
        test_do_reboot = parmsArray[i].doReboot;
        test_color_mode = parmsArray[i].mode;
        RUN_TEST(test_hires_40colmix_normal);
    }

    for (unsigned int i=0; i<count; i++) {
        test_do_reboot = parmsArray[i].doReboot;
        test_color_mode = parmsArray[i].mode;
        RUN_TEST(test_hires_40colmix_inverse);
    }

    for (unsigned int i=0; i<count; i++) {
        test_do_reboot = parmsArray[i].doReboot;
        test_color_mode = parmsArray[i].mode;
        RUN_TEST(test_hires_80colmix_normal);
    }

    for (unsigned int i=0; i<count; i++) {
        test_do_reboot = parmsArray[i].doReboot;
        test_color_mode = parmsArray[i].mode;
        RUN_TEST(test_hires_80colmix_inverse);
    }

    // double-lo/hi

    RUN_TEST(test_80col_lores);
    test_color_mode = COLOR_MODE_BW;
    RUN_TEST(test_80col_lores);

    RUN_TEST(test_80col_lores_2);
    test_color_mode = COLOR_MODE_BW;
    RUN_TEST(test_80col_lores_2);

    RUN_TEST(test_80col_hires);
    test_color_mode = COLOR_MODE_BW;
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

