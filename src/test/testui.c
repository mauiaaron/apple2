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
#include <sys/mman.h>

#define BLANK_DSK "blank.dsk.gz"

static bool test_thread_running = false;

extern pthread_mutex_t interface_mutex; // TODO FIXME : raw access to CPU mutex because stepping debugger ...

static void testui_setup(void *arg) {
    test_common_setup();
    apple_ii_64k[0][MIXSWITCH_ADDR] = 0x00;
    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    if (test_do_reboot) {
        joy_button0 = 0xff; // OpenApple
        cpu65_interrupt(ResetSig);
    }
}

static void testui_teardown(void *arg) {
}

// ----------------------------------------------------------------------------
// Various Tests ...

static int _assert_blank_boot(void) {
    // Disk6 ...
    ASSERT(disk6.motor_off == 1);
    ASSERT(disk6.drive == 0);
    ASSERT(disk6.ddrw == 0);
    ASSERT(disk6.disk_byte == 0x0);
    extern int stepper_phases;
    ASSERT(stepper_phases == 0x0);
    ASSERT(disk6.disk[0].is_protected);
    const char *file_name = strrchr(disk6.disk[0].file_name, '/');
    ASSERT(strcmp(file_name, "/blank.dsk") == 0);
    ASSERT(disk6.disk[0].phase == 36);
    ASSERT(disk6.disk[0].run_byte == 1130);
    ASSERT(disk6.disk[0].fd > 0);
    ASSERT(disk6.disk[0].mmap_image != 0);
    ASSERT(disk6.disk[0].mmap_image != MAP_FAILED);
    ASSERT(disk6.disk[0].whole_len == 143360);
    ASSERT(disk6.disk[0].whole_image != NULL);
    ASSERT(disk6.disk[0].track_width == 6040);
    ASSERT(!disk6.disk[0].nibblized);
    ASSERT(!disk6.disk[0].track_dirty);
    extern int skew_table_6_do[16];
    ASSERT(disk6.disk[0].skew_table == skew_table_6_do);
    
    // VM ...
    ASSERT(softswitches  == 0x000140d1);
    ASSERT_SHA_BIN("97AADDDF5D20B793C4558A8928227F0B52565A98", apple_ii_64k[0], /*len:*/sizeof(apple_ii_64k));
    ASSERT_SHA_BIN("2C82E33E964936187CA1DABF71AE6148916BD131", language_card[0], /*len:*/sizeof(language_card));
    ASSERT_SHA_BIN("36F1699537024EC6017A22641FF0EC277AFFD49D", language_banks[0], /*len:*/sizeof(language_banks));
    ASSERT(base_ramrd    == apple_ii_64k[0]);
    ASSERT(base_ramwrt   == apple_ii_64k[0]);
    ASSERT(base_textrd   == apple_ii_64k[0]);
    ASSERT(base_textwrt  == apple_ii_64k[0]);
    ASSERT(base_hgrrd    == apple_ii_64k[0]);
    ASSERT(base_hgrwrt   == apple_ii_64k[0]);
    ASSERT(base_stackzp  == apple_ii_64k[0]);
    ASSERT(base_c3rom    == apple_ii_64k[1]);
    ASSERT(base_cxrom    == apple_ii_64k[0]);
    ASSERT(base_d000_rd  == apple_ii_64k[0]);
    ASSERT(base_d000_wrt == language_banks[0] - 0xD000);
    ASSERT(base_e000_rd  == apple_ii_64k[0]);
    ASSERT(base_e000_wrt == language_card[0] - 0xE000);
    
    // CPU ...
    ASSERT(cpu65_pc      == 0xE783);
    ASSERT(cpu65_ea      == 0x1F33);
    ASSERT(cpu65_a       == 0xFF);
    ASSERT(cpu65_f       == 0x37);
    ASSERT(cpu65_x       == 0xFF);
    ASSERT(cpu65_y       == 0x00);
    ASSERT(cpu65_sp      == 0xF6);
    
    PASS();
}

#define EXPECTED_A2VM_FILE_SIZE 164064

TEST test_save_state_1() {
    test_setup_boot_disk(BLANK_DSK, 1);

    BOOT_TO_DOS();

    _assert_blank_boot();
    
    char *savFile = NULL;
    ASPRINTF(&savFile, "%s/emulator-test.state", HOMEDIR);

    bool ret = emulator_saveState(savFile);
    ASSERT(ret);

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(savFile, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == EXPECTED_A2VM_FILE_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = MALLOC(EXPECTED_A2VM_FILE_SIZE);
        if (fread(buf, 1, EXPECTED_A2VM_FILE_SIZE, fp) != EXPECTED_A2VM_FILE_SIZE) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, EXPECTED_A2VM_FILE_SIZE, md);
        FREE(buf);

        sha1_to_str(md, mdstr0);
        ASSERT(strcasecmp(mdstr0, "0f70ed3ed7e018025ca51dcf5ca063553ee4403f") == 0);
    } while(0);

    FREE(savFile);
    PASS();
}

TEST test_load_state_1() {

    char *savFile = NULL;
    ASPRINTF(&savFile, "%s/emulator-test.state", HOMEDIR);

    bool ret = emulator_loadState(savFile);
    ASSERT(ret);

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    ASSERT_SHA(BOOT_SCREEN);

    _assert_blank_boot();

    unlink(savFile);
    FREE(savFile);

    PASS();
}

TEST test_load_A2VM_1() {

    // test A2VM original format

////#include "test/a2vm-sample.h"

#warning save the binary data to state file, then load and assert various stuff-n-things

    FAIL();
}

// ----------------------------------------------------------------------------
// Test Suite

GREATEST_SUITE(test_suite_ui) {
    pthread_mutex_lock(&interface_mutex);

    test_thread_running = true;
    
    GREATEST_SET_SETUP_CB(testui_setup, NULL);
    GREATEST_SET_TEARDOWN_CB(testui_teardown, NULL);
    GREATEST_SET_BREAKPOINT_CB(test_breakpoint, NULL);

    // TESTS --------------------------

    RUN_TESTp(test_save_state_1);
    RUN_TESTp(test_load_state_1);

    RUN_TESTp(test_load_A2VM_1); // legacy A2VM ...

#if INTERFACE_TOUCH
#   warning TODO : touch joystick(s), keyboard, mouse, menu
#endif

#if INTERFACE_CLASSIC
#   warning TODO : menus, stuff-n-things
#endif

    // ...
    disk6_eject(0);
    pthread_mutex_unlock(&interface_mutex);
}

SUITE(test_suite_ui);
GREATEST_MAIN_DEFS();

static char **test_argv = NULL;
static int test_argc = 0;

static int _test_thread(void) {
    int argc = test_argc;
    char **argv = test_argv;
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(test_suite_ui);
    GREATEST_MAIN_END();
}

static void *test_thread(void *dummyptr) {
    _test_thread();
    return NULL;
}

void test_ui(int _argc, char **_argv) {
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

