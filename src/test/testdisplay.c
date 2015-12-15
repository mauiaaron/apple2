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
    joy_button0 = 0xff; // OpenApple
    if (test_do_reboot) {
        cpu65_interrupt(ResetSig);
    }
}

static void testdisplay_teardown(void *arg) {
}

// ----------------------------------------------------------------------------
// Various Display Tests ...

TEST test_boot_disk() {
    test_setup_boot_disk("testdisplay1.nib.gz", 1);

    BOOT_TO_DOS();

    PASS();
}

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
    ASSERT_SHA("7B642FF04DE03142A2CE1062C28A4D92E492EDDC");

    PASS();
}

TEST test_lores_with_40col() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    test_type_input("TEXT\rHOME\rLOAD TESTLORES\rLIST\rLIST\rRUN\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA("D7DC78F5718B4CF8716614E79ADABCAB919FCE5D");

    PASS();
}

TEST test_lores_40colmix_normal() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    test_type_input("TEXT\rHOME\rLOAD TESTLORES\rLIST\rLIST\rPOKE 7986,127\rRUN\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA("9097A6AE967E4501B40C7CD7EEE115B8C478B345");

    PASS();
}

TEST test_lores_40colmix_inverse() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    test_type_input("TEXT\rINVERSE\rHOME\rLOAD TESTLORES\rLIST\rLIST\rPOKE 7986,127\rRUN\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA("5256E8B96CB04F48324B587ECCCF8A435077B5DE");

    PASS();
}

TEST test_lores_80colmix_normal() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    test_type_input("TEXT\rPR#3\rLOAD TESTLORES\rLIST\rLIST\rPOKE 7986,127\rRUN\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA("9D5D5382B0A18A71DC135CAD51BEA2665ADB5FB2");

    PASS();
}

TEST test_lores_80colmix_inverse() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    test_type_input("TEXT\rINVERSE\rPR#3\rLOAD TESTLORES\rLIST\rLIST\rPOKE 7986,127\rRUN\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA("7936E87BE1F920AACD43268DB288746528E89959");

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
    ASSERT_SHA("032BD68899749265EB2934A76A35D7068642824B");

    PASS();
}

TEST test_hires_80colmix_inverse() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    test_type_input("INVERSE\rPR#3\rLOAD TESTHIRES\rLIST\rLIST\rPOKE 7986,127\rRUN\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA("FAFBB65013DA3D5173487C3F434C36A7C04DE92E");

    PASS();
}

TEST test_80col_lores() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    test_type_input("RUN TESTLORES80\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA("18C69AEA1510485839F9724AFB54F74C4991FCCB");

    PASS();
}

TEST test_80col_hires() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    test_type_input("RUN TESTHIRES80_2\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA("126363F103F3A24BB2EB2AEC5B4972F46F7CA24B");

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA("91F63831B6D1145446F0DBACE8EBD284B5690D81");

    PASS();
}

// ----------------------------------------------------------------------------
// Test Suite

GREATEST_SUITE(test_suite_display) {
    pthread_mutex_lock(&interface_mutex);

    GREATEST_SET_SETUP_CB(testdisplay_setup, NULL);
    GREATEST_SET_TEARDOWN_CB(testdisplay_teardown, NULL);
    GREATEST_SET_BREAKPOINT_CB(test_breakpoint, NULL);

    // TESTS --------------------------
    test_thread_running = true;

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

static void *test_thread(void *dummyptr) {
    int argc = test_argc;
    char **argv = test_argv;
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(test_suite_display);
    GREATEST_MAIN_END();
    return NULL;
}

void test_display(int argc, char **argv) {
    test_argc = argc;
    test_argv = argv;

    srandom(time(NULL));

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
    test_display(argc, argv);
}
#endif
