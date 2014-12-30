/*
 * Apple // emulator for *nix
 *
 * This software package is subject to the GNU General Public License
 * version 2 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * THERE ARE NO WARRANTIES WHATSOEVER.
 *
 */

#include "testcommon.h"

#define RESET_INPUT() test_common_setup()

#define TESTING_DISK "testvm1.dsk.gz"
#define BLANK_DSK "blank.dsk.gz"
#define BLANK_NIB "blank.nib.gz"
#define REBOOT_TO_DOS() \
    do { \
        apple_ii_64k[0][TESTOUT_ADDR] = 0x00; \
        joy_button0 = 0xff; \
        cpu65_interrupt(ResetSig); \
    } while (0)

#define TYPE_TRIGGER_WATCHPT() \
    test_type_input("POKE7987,255:REM TRIGGER DEBUGGER\r")

static void testdisk_setup(void *arg) {
    RESET_INPUT();
    apple_ii_64k[0][MIXSWITCH_ADDR] = 0x00;
    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    apple_ii_64k[0][TESTOUT_ADDR] = 0x00;
    joy_button0 = 0xff; // OpenApple
    test_setup_boot_disk(TESTING_DISK, 1);
    if (test_do_reboot) {
        cpu65_interrupt(ResetSig);
    }
}

static void testdisk_teardown(void *arg) {
}

// ----------------------------------------------------------------------------
// Disk TESTS ...

#define EXPECTED_DISK_TRACE_FILE_SIZE 141350
#define EXPECTED_DISK_TRACE_SHA "471EB3D01917B1C6EF9F13C5C7BC1ACE4E74C851"
TEST test_boot_disk_bytes() {
    char *homedir = getenv("HOME");
    char *disk = NULL;
    asprintf(&disk, "%s/a2_read_disk_test.txt", homedir);
    if (disk) {
        unlink(disk);
        c_begin_disk_trace_6(disk, NULL);
    }

    BOOT_TO_DOS();

    c_end_disk_trace_6();
    c_eject_6(0);

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(disk, "r");
        char *buf = malloc(EXPECTED_DISK_TRACE_FILE_SIZE);
        if (fread(buf, 1, EXPECTED_DISK_TRACE_FILE_SIZE, fp) != EXPECTED_DISK_TRACE_FILE_SIZE) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, EXPECTED_DISK_TRACE_FILE_SIZE, md);
        FREE(buf);

        sha1_to_str(md, mdstr);
        ASSERT(strcmp(mdstr, EXPECTED_DISK_TRACE_SHA) == 0);
    } while(0);

    unlink(disk);
    FREE(disk);

    PASS();
}

// This test is majorly abusive ... it creates an ~800MB file in $HOME
// ... but if it's correct, you're fairly assured the cpu/vm is working =)
#if ABUSIVE_TESTS
#define EXPECTED_CPU_TRACE_FILE_SIZE 809430487
TEST test_boot_disk_cputrace() {
    char *homedir = getenv("HOME");
    char *output = NULL;
    asprintf(&output, "%s/a2_cputrace.txt", homedir);
    if (output) {
        unlink(output);
        cpu65_trace_begin(output);
    }

    BOOT_TO_DOS();

    cpu65_trace_end();
    c_eject_6(0);

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(output, "r");
        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == EXPECTED_CPU_TRACE_FILE_SIZE);
        fseek(fp, 0, SEEK_SET);
        char *buf = malloc(EXPECTED_CPU_TRACE_FILE_SIZE);
        if (fread(buf, 1, EXPECTED_CPU_TRACE_FILE_SIZE, fp) != EXPECTED_CPU_TRACE_FILE_SIZE) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, EXPECTED_CPU_TRACE_FILE_SIZE, md);
        FREE(buf);

#if 0
        // this is no longer a stable value to check due to random return values from disk VM routines
        sha1_to_str(md, mdstr);
        ASSERT(strcmp(mdstr, EXPECTED_CPU_TRACE_SHA) == 0);
#endif
    } while(0);

    unlink(output);
    FREE(output);

    PASS();
}
#endif

#define EXPECTED_VM_TRACE_FILE_SIZE 2830810
TEST test_boot_disk_vmtrace() {
    char *homedir = getenv("HOME");
    char *disk = NULL;
    asprintf(&disk, "%s/a2_vmtrace.txt", homedir);
    if (disk) {
        unlink(disk);
        vm_trace_begin(disk);
    }

    BOOT_TO_DOS();

    vm_trace_end();
    c_eject_6(0);

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(disk, "r");
        char *buf = malloc(EXPECTED_VM_TRACE_FILE_SIZE);
        if (fread(buf, 1, EXPECTED_VM_TRACE_FILE_SIZE, fp) != EXPECTED_VM_TRACE_FILE_SIZE) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, EXPECTED_VM_TRACE_FILE_SIZE, md);
        FREE(buf);

#if 0
        // this is no longer a stable value to check due to random return values from disk VM routines
        sha1_to_str(md, mdstr);
        ASSERT(strcmp(mdstr, EXPECTED_VM_TRACE_SHA) == 0);
#endif
    } while(0);

    unlink(disk);
    FREE(disk);

    PASS();
}

TEST test_boot_disk() {
    BOOT_TO_DOS();
    PASS();
}

TEST test_inithello_dsk() {

    test_setup_boot_disk(BLANK_DSK, 0);
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR]    == 0x00);

    test_type_input("INIT HELLO\r");
    TYPE_TRIGGER_WATCHPT();

    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA("10F15B516E4CF2FC5B1712951A6F9C3D90BF595C");

    REBOOT_TO_DOS();
    c_eject_6(0);

    PASS();
}

TEST test_inithello_nib() {

    test_setup_boot_disk(BLANK_NIB, 0);
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    test_type_input("INIT HELLO\r");
    TYPE_TRIGGER_WATCHPT();

    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA("10F15B516E4CF2FC5B1712951A6F9C3D90BF595C");

    REBOOT_TO_DOS();
    c_eject_6(0);

    PASS();
}

// ----------------------------------------------------------------------------
// Test Suite

static int begin_video = -1;

GREATEST_SUITE(test_suite_disk) {
    GREATEST_SET_SETUP_CB(testdisk_setup, NULL);
    GREATEST_SET_TEARDOWN_CB(testdisk_teardown, NULL);

    // TESTS --------------------------
    begin_video=!is_headless;

    RUN_TESTp(test_boot_disk_bytes);
#if ABUSIVE_TESTS
    RUN_TESTp(test_boot_disk_cputrace);
#endif
    RUN_TESTp(test_boot_disk_vmtrace);
    RUN_TESTp(test_boot_disk);

    RUN_TESTp(test_inithello_dsk);
    RUN_TESTp(test_inithello_nib);

    // ...
    c_eject_6(0);
    pthread_mutex_unlock(&interface_mutex);
}

SUITE(test_suite_disk);
GREATEST_MAIN_DEFS();

static char **test_argv = NULL;
static int test_argc = 0;

static int _test_disk(void) {
    int argc = test_argc;
    char **argv = test_argv;
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(test_suite_disk);
    GREATEST_MAIN_END();
}

static void *test_thread(void *dummyptr) {
    _test_disk();
    return NULL;
}

void test_disk(int argc, char **argv) {
    test_argc = argc;
    test_argv = argv;

    c_read_rand(0x0);
    srandom(0); // force a known sequence

    pthread_mutex_lock(&interface_mutex);

    test_common_init(/*cputhread*/true);

    pthread_t p;
    pthread_create(&p, NULL, (void *)&test_thread, (void *)NULL);

    while (begin_video < 0) {
        struct timespec ts = { .tv_sec=0, .tv_nsec=33333333 };
        nanosleep(&ts, NULL);
    }
    if (begin_video) {
        video_main_loop();
    }
    pthread_join(p, NULL);
}

#if !defined(__APPLE__)
int main(int argc, char **argv) {
    test_disk(argc, argv);
}
#endif
