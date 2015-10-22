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

#define ABUSIVE_TESTS 1
#define FINICKY_TESTS 1

#define TESTING_DISK "testvm1.dsk.gz"
#define BLANK_DSK "blank.dsk.gz"
#define BLANK_NIB "blank.nib.gz"
#define BLANK_PO  "blank.po.gz"

static bool test_thread_running = false;

extern pthread_mutex_t interface_mutex; // TODO FIXME : raw access to CPU mutex because stepping debugger ...

static void testtrace_setup(void *arg) {
    test_common_setup();
    apple_ii_64k[0][MIXSWITCH_ADDR] = 0x00;
    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    apple_ii_64k[0][TESTOUT_ADDR] = 0x00;
    joy_button0 = 0xff; // OpenApple
    test_setup_boot_disk(TESTING_DISK, 1);
    if (test_do_reboot) {
        cpu65_interrupt(ResetSig);
    }
}

static void testtrace_teardown(void *arg) {
}

// ----------------------------------------------------------------------------
// Disk TESTS ...

// This test is majorly abusive ... it creates an ~800MB file in $HOME
// ... but if it's correct, you're fairly assured the cpu/vm is working =)
#if ABUSIVE_TESTS
#define EXPECTED_CPU_TRACE_FILE_SIZE 809430487
#define EXPECTED_CPU_TRACE_SHA "4DB0C2547A0F02450A0E5E663C5BE8EA776C7A41"
TEST test_boot_disk_cputrace() {
    const char *homedir = HOMEDIR;
    char *output = NULL;
    asprintf(&output, "%s/a2_cputrace.txt", homedir);
    if (output) {
        unlink(output);
        cpu65_trace_begin(output);
    }

    srandom(0);
    BOOT_TO_DOS();

    cpu65_trace_end();
    disk6_eject(0);

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(output, "r");
        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == EXPECTED_CPU_TRACE_FILE_SIZE);
        fseek(fp, 0, SEEK_SET);
        unsigned char *buf = malloc(EXPECTED_CPU_TRACE_FILE_SIZE);
        if (fread(buf, 1, EXPECTED_CPU_TRACE_FILE_SIZE, fp) != EXPECTED_CPU_TRACE_FILE_SIZE) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, EXPECTED_CPU_TRACE_FILE_SIZE, md);
        FREE(buf);

        sha1_to_str(md, mdstr0);
        ASSERT(strcmp(mdstr0, EXPECTED_CPU_TRACE_SHA) == 0);
    } while(0);

    unlink(output);
    FREE(output);

    PASS();
}
#endif

#define EXPECTED_CPUTRACE_HELLO_FILE_SIZE 118664420
#define EXPECTED_CPUTRACE_HELLO_SHA "C01DDA6AE63A2FEA0CE8DA3A3B258F96AE8BA79B"
TEST test_cputrace_hello_dsk() {
    test_setup_boot_disk(BLANK_DSK, 0);

    BOOT_TO_DOS();

    const char *homedir = HOMEDIR;
    char *output = NULL;
    asprintf(&output, "%s/a2_cputrace_hello_dsk.txt", homedir);
    if (output) {
        unlink(output);
        cpu65_trace_begin(output);
    }

    srandom(0);
    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    test_type_input("RUN HELLO\r");
    c_debugger_go();

    cpu65_trace_end();
    disk6_eject(0);

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(output, "r");
        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == EXPECTED_CPUTRACE_HELLO_FILE_SIZE);
        fseek(fp, 0, SEEK_SET);
        unsigned char *buf = malloc(EXPECTED_CPUTRACE_HELLO_FILE_SIZE);
        if (fread(buf, 1, EXPECTED_CPUTRACE_HELLO_FILE_SIZE, fp) != EXPECTED_CPUTRACE_HELLO_FILE_SIZE) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, EXPECTED_CPUTRACE_HELLO_FILE_SIZE, md);
        FREE(buf);

        sha1_to_str(md, mdstr0);
        ASSERT(strcmp(mdstr0, EXPECTED_CPUTRACE_HELLO_SHA) == 0);
    } while(0);

    unlink(output);
    FREE(output);

    PASS();
}

#define EXPECTED_CPUTRACE_HELLO_NIB_FILE_SIZE 14612543
#define EXPECTED_CPUTRACE_HELLO_NIB_SHA "2D494B4302CC6E3753D7AB50B587C03C7E05C93A"
TEST test_cputrace_hello_nib() {
    test_setup_boot_disk(BLANK_NIB, 0);

    BOOT_TO_DOS();

    const char *homedir = HOMEDIR;
    char *output = NULL;
    asprintf(&output, "%s/a2_cputrace_hello_nib.txt", homedir);
    if (output) {
        unlink(output);
        cpu65_trace_begin(output);
    }

    srandom(0);
    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    test_type_input("RUN HELLO\r");
    c_debugger_go();

    cpu65_trace_end();
    disk6_eject(0);

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(output, "r");
        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == EXPECTED_CPUTRACE_HELLO_NIB_FILE_SIZE);
        fseek(fp, 0, SEEK_SET);
        unsigned char *buf = malloc(EXPECTED_CPUTRACE_HELLO_NIB_FILE_SIZE);
        if (fread(buf, 1, EXPECTED_CPUTRACE_HELLO_NIB_FILE_SIZE, fp) != EXPECTED_CPUTRACE_HELLO_NIB_FILE_SIZE) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, EXPECTED_CPUTRACE_HELLO_NIB_FILE_SIZE, md);
        FREE(buf);

        sha1_to_str(md, mdstr0);
        ASSERT(strcmp(mdstr0, EXPECTED_CPUTRACE_HELLO_NIB_SHA) == 0);
    } while(0);

    unlink(output);
    FREE(output);

    PASS();
}

#define EXPECTED_CPUTRACE_HELLO_PO_FILE_SIZE 118680586
#define EXPECTED_CPUTRACE_HELLO_PO_SHA "716D8D515876C138B7F3D8F078F05684C801D707"
TEST test_cputrace_hello_po() {
    test_setup_boot_disk(BLANK_PO, 0);

    BOOT_TO_DOS();

    const char *homedir = HOMEDIR;
    char *output = NULL;
    asprintf(&output, "%s/a2_cputrace_hello_po.txt", homedir);
    if (output) {
        unlink(output);
        cpu65_trace_begin(output);
    }

    srandom(0);
    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    test_type_input("RUN HELLO\r");
    c_debugger_go();

    cpu65_trace_end();
    disk6_eject(0);

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(output, "r");
        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == EXPECTED_CPUTRACE_HELLO_PO_FILE_SIZE);
        fseek(fp, 0, SEEK_SET);
        unsigned char *buf = malloc(EXPECTED_CPUTRACE_HELLO_PO_FILE_SIZE);
        if (fread(buf, 1, EXPECTED_CPUTRACE_HELLO_PO_FILE_SIZE, fp) != EXPECTED_CPUTRACE_HELLO_PO_FILE_SIZE) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, EXPECTED_CPUTRACE_HELLO_PO_FILE_SIZE, md);
        FREE(buf);

        sha1_to_str(md, mdstr0);
        ASSERT(strcmp(mdstr0, EXPECTED_CPUTRACE_HELLO_PO_SHA) == 0);
    } while(0);

    unlink(output);
    FREE(output);

    PASS();
}

#define EXPECTED_VM_TRACE_FILE_SIZE 2830792
#define EXPECTED_VM_TRACE_SHA "E3AA4EBEACF9053D619E115F6AEB454A8939BFB4"
TEST test_boot_disk_vmtrace() {
    const char *homedir = HOMEDIR;
    char *disk = NULL;
    asprintf(&disk, "%s/a2_vmtrace.txt", homedir);
    if (disk) {
        unlink(disk);
        vm_trace_begin(disk);
    }

    srandom(0);
    BOOT_TO_DOS();

    vm_trace_end();
    disk6_eject(0);

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(disk, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == EXPECTED_VM_TRACE_FILE_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = malloc(EXPECTED_VM_TRACE_FILE_SIZE);
        if (fread(buf, 1, EXPECTED_VM_TRACE_FILE_SIZE, fp) != EXPECTED_VM_TRACE_FILE_SIZE) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, EXPECTED_VM_TRACE_FILE_SIZE, md);
        FREE(buf);

        sha1_to_str(md, mdstr0);
        ASSERT(strcmp(mdstr0, EXPECTED_VM_TRACE_SHA) == 0);
    } while(0);

    unlink(disk);
    FREE(disk);

    PASS();
}

#define EXPECTED_VM_TRACE_NIB_FILE_SIZE 2930056
#define EXPECTED_VM_TRACE_NIB_SHA "D60DAE2F3AA4002678457F6D16FC8A25FA14C10E"
TEST test_boot_disk_vmtrace_nib() {
    test_setup_boot_disk(BLANK_NIB, 0);

    const char *homedir = HOMEDIR;
    char *disk = NULL;
    asprintf(&disk, "%s/a2_vmtrace_nib.txt", homedir);
    if (disk) {
        unlink(disk);
        vm_trace_begin(disk);
    }

    srandom(0);
    BOOT_TO_DOS();

    vm_trace_end();
    disk6_eject(0);

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(disk, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == EXPECTED_VM_TRACE_NIB_FILE_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = malloc(EXPECTED_VM_TRACE_NIB_FILE_SIZE);
        if (fread(buf, 1, EXPECTED_VM_TRACE_NIB_FILE_SIZE, fp) != EXPECTED_VM_TRACE_NIB_FILE_SIZE) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, EXPECTED_VM_TRACE_NIB_FILE_SIZE, md);
        FREE(buf);

        sha1_to_str(md, mdstr0);
        ASSERT(strcmp(mdstr0, EXPECTED_VM_TRACE_NIB_SHA) == 0);
    } while(0);

    unlink(disk);
    FREE(disk);

    PASS();
}

#define EXPECTED_VM_TRACE_PO_FILE_SIZE EXPECTED_VM_TRACE_FILE_SIZE
#define EXPECTED_VM_TRACE_PO_SHA "DA200BE91FD8D6D09E551A19ED0445F985898C16"
TEST test_boot_disk_vmtrace_po() {
    test_setup_boot_disk(BLANK_PO, 0);

    const char *homedir = HOMEDIR;
    char *disk = NULL;
    asprintf(&disk, "%s/a2_vmtrace_po.txt", homedir);
    if (disk) {
        unlink(disk);
        vm_trace_begin(disk);
    }

    srandom(0);
    BOOT_TO_DOS();

    vm_trace_end();
    disk6_eject(0);

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(disk, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == EXPECTED_VM_TRACE_PO_FILE_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = malloc(EXPECTED_VM_TRACE_PO_FILE_SIZE);
        if (fread(buf, 1, EXPECTED_VM_TRACE_PO_FILE_SIZE, fp) != EXPECTED_VM_TRACE_PO_FILE_SIZE) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, EXPECTED_VM_TRACE_PO_FILE_SIZE, md);
        FREE(buf);

        sha1_to_str(md, mdstr0);
        ASSERT(strcmp(mdstr0, EXPECTED_VM_TRACE_PO_SHA) == 0);
    } while(0);

    unlink(disk);
    FREE(disk);

    PASS();
}

// ----------------------------------------------------------------------------
// Test Suite

GREATEST_SUITE(test_suite_trace) {
    pthread_mutex_lock(&interface_mutex);

    GREATEST_SET_SETUP_CB(testtrace_setup, NULL);
    GREATEST_SET_TEARDOWN_CB(testtrace_teardown, NULL);
    GREATEST_SET_BREAKPOINT_CB(test_breakpoint, NULL);

    // TESTS --------------------------
    test_thread_running = true;

#if ABUSIVE_TESTS
    RUN_TESTp(test_boot_disk_cputrace);
#endif

#if FINICKY_TESTS
    RUN_TESTp(test_cputrace_hello_dsk);
    RUN_TESTp(test_cputrace_hello_nib);
    RUN_TESTp(test_cputrace_hello_po);
    RUN_TESTp(test_boot_disk_vmtrace);
    RUN_TESTp(test_boot_disk_vmtrace_nib);
    RUN_TESTp(test_boot_disk_vmtrace_po);
#endif

    // ...
    disk6_eject(0);
    pthread_mutex_unlock(&interface_mutex);
}

SUITE(test_suite_trace);
GREATEST_MAIN_DEFS();

static char **test_argv = NULL;
static int test_argc = 0;

static void *test_thread(void *dummyptr) {
    int argc = test_argc;
    char **argv = test_argv;
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(test_suite_trace);
    GREATEST_MAIN_END();
    return NULL;
}

void test_trace(int argc, char **argv) {
    test_argc = argc;
    test_argv = argv;

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
    test_trace(argc, argv);
}
#endif
