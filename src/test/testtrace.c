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
    c_debugger_set_watchpoint(WATCHPOINT_ADDR);
}

static void testtrace_teardown(void *arg) {
}

// ----------------------------------------------------------------------------
// Speaker/timing tests

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
// Mockingboard tracing

#define NSCT_DSK "NSCT.dsk.gz"
#define NSCT_TRACE_SHA "FA2FE78A273405E2B129E46FD13F1CB0C56F8D53"
#define NSCT_SAMPS_SHA "DF9686D4835B0E55480BE25CB0A065C85A6963DE"
#define NSCT_TRACE_TARGET_SIZ (512 * 65536)  // 2^25
#define NSCT_SAMPS_TARGET_SIZ (2048 * 65536) // 2^27
TEST test_mockingboard_1() {
    test_setup_boot_disk(NSCT_DSK, 0);

    const char *homedir = HOMEDIR;

    char *mbTraceFile = NULL;
    ASPRINTF(&mbTraceFile, "%s/a2_mb_nsct", homedir);
    ASSERT(mbTraceFile);
    unlink(mbTraceFile);

    char *mbSampsFile = NULL;
    ASPRINTF(&mbSampsFile, "%s.samp", mbTraceFile);
    ASSERT(mbSampsFile);
    unlink(mbSampsFile);

    // Poll for trace file of particular size
    mb_traceBegin(mbTraceFile); // ".samp" file is automatically created ...
    c_debugger_clear_watchpoints();
    c_debugger_set_timeout(1);
    do {
        c_debugger_go();

        FILE *fpTrace = fopen(mbTraceFile, "r");
        fseek(fpTrace, 0, SEEK_END);
        long minSizeTrace = ftell(fpTrace);

        FILE *fpSamps = fopen(mbSampsFile, "r");
        fseek(fpSamps, 0, SEEK_END);
        long minSizeSamps = ftell(fpSamps);

        if ( (minSizeTrace < NSCT_TRACE_TARGET_SIZ) || (minSizeSamps < NSCT_SAMPS_TARGET_SIZ) ) {
            fclose(fpTrace);
            fclose(fpSamps);
            continue;
        }

        // trace has generated files of sufficient length

        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        mb_traceEnd();
        truncate(mbTraceFile, NSCT_TRACE_TARGET_SIZ);
        truncate(mbSampsFile, NSCT_SAMPS_TARGET_SIZ);

        // verify trace file
        do {
            unsigned char *buf = MALLOC(NSCT_TRACE_TARGET_SIZ);
            fseek(fpTrace, 0, SEEK_SET);
            ASSERT(fread(buf, 1, NSCT_TRACE_TARGET_SIZ, fpTrace) == NSCT_TRACE_TARGET_SIZ);
            fclose(fpTrace); fpTrace = NULL;
            SHA1(buf, NSCT_TRACE_TARGET_SIZ, md);
            FREE(buf);
            sha1_to_str(md, mdstr0);
            ASSERT(strcmp(mdstr0, NSCT_TRACE_SHA) == 0);
        } while (0);

        // verify trace samples file
        do {
            unsigned char *buf = MALLOC(NSCT_SAMPS_TARGET_SIZ);
            fseek(fpSamps, 0, SEEK_SET);
            ASSERT(fread(buf, 1, NSCT_SAMPS_TARGET_SIZ, fpSamps) == NSCT_SAMPS_TARGET_SIZ);
            fclose(fpSamps); fpSamps = NULL;
            SHA1(buf, NSCT_SAMPS_TARGET_SIZ, md);
            FREE(buf);
            sha1_to_str(md, mdstr0);
            ASSERT(strcmp(mdstr0, NSCT_SAMPS_SHA) == 0);
        } while (0);

        break;
    } while (1);
    c_debugger_set_timeout(0);

    unlink(mbTraceFile);
    FREE(mbTraceFile);
    unlink(mbSampsFile);
    FREE(mbSampsFile);

    disk6_eject(0);

    PASS();
}

// ----------------------------------------------------------------------------
// CPU tracing

// This test is majorly abusive ... it creates an ~900MB file in $HOME
// ... but if it's correct, you're fairly assured the cpu/vm is working =)
#if ABUSIVE_TESTS
#define EXPECTED_CPU_TRACE_FILE_SIZE 889495699
#define EXPECTED_CPU_TRACE_SHA "ADFF915FF3B8F15428DD89580DBE21CDF71B7E39"
TEST test_boot_disk_cputrace() {
    const char *homedir = HOMEDIR;
    char *output = NULL;
    ASPRINTF(&output, "%s/a2_cputrace.txt", homedir);
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
        unsigned char *buf = MALLOC(EXPECTED_CPU_TRACE_FILE_SIZE);
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

#define EXPECTED_CPUTRACE_HELLO_FILE_SIZE 118170437
#define EXPECTED_CPUTRACE_HELLO_SHA "D5BF7650E51404F3358C9C4CBBEBAA191E53AD72"
TEST test_cputrace_hello_dsk() {
    test_setup_boot_disk(BLANK_DSK, 0);

    BOOT_TO_DOS();

    const char *homedir = HOMEDIR;
    char *output = NULL;
    ASPRINTF(&output, "%s/a2_cputrace_hello_dsk.txt", homedir);
    if (output) {
        unlink(output);
        cpu65_trace_begin(output);
    }

    srandom(0);
    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    test_type_input_deterministically("RUN HELLO\r");
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
        unsigned char *buf = MALLOC(EXPECTED_CPUTRACE_HELLO_FILE_SIZE);
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

#define EXPECTED_CPUTRACE_HELLO_NIB_FILE_SIZE 14153921
#define EXPECTED_CPUTRACE_HELLO_NIB_SHA "B09F85206E964487378C5B62EA26626A6E8159F1"
TEST test_cputrace_hello_nib() {
    test_setup_boot_disk(BLANK_NIB, 0);

    BOOT_TO_DOS();

    const char *homedir = HOMEDIR;
    char *output = NULL;
    ASPRINTF(&output, "%s/a2_cputrace_hello_nib.txt", homedir);
    if (output) {
        unlink(output);
        cpu65_trace_begin(output);
    }

    srandom(0);
    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    test_type_input_deterministically("RUN HELLO\r");
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
        unsigned char *buf = MALLOC(EXPECTED_CPUTRACE_HELLO_NIB_FILE_SIZE);
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

#define EXPECTED_CPUTRACE_HELLO_PO_FILE_SIZE  EXPECTED_CPUTRACE_HELLO_FILE_SIZE
#define EXPECTED_CPUTRACE_HELLO_PO_SHA        EXPECTED_CPUTRACE_HELLO_SHA
TEST test_cputrace_hello_po() {
    test_setup_boot_disk(BLANK_PO, 0);

    BOOT_TO_DOS();

    const char *homedir = HOMEDIR;
    char *output = NULL;
    ASPRINTF(&output, "%s/a2_cputrace_hello_po.txt", homedir);
    if (output) {
        unlink(output);
        cpu65_trace_begin(output);
    }

    srandom(0);
    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    test_type_input_deterministically("RUN HELLO\r");
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
        unsigned char *buf = MALLOC(EXPECTED_CPUTRACE_HELLO_PO_FILE_SIZE);
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

// ----------------------------------------------------------------------------
// VM tracing

#define EXPECTED_VM_TRACE_FILE_SIZE 2383449
#define EXPECTED_VM_TRACE_SHA "0B387CBF4342CC24E0B9D2DA37AF517FD75DF467"
TEST test_boot_disk_vmtrace() {
    const char *homedir = HOMEDIR;
    char *disk = NULL;
    ASPRINTF(&disk, "%s/a2_vmtrace.txt", homedir);
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

        unsigned char *buf = MALLOC(EXPECTED_VM_TRACE_FILE_SIZE);
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

#define EXPECTED_VM_TRACE_NIB_FILE_SIZE 2470474
#define EXPECTED_VM_TRACE_NIB_SHA "0256D57E561FE301588B1FC081F04D20AA870789"
TEST test_boot_disk_vmtrace_nib() {
    test_setup_boot_disk(BLANK_NIB, 0);

    const char *homedir = HOMEDIR;
    char *disk = NULL;
    ASPRINTF(&disk, "%s/a2_vmtrace_nib.txt", homedir);
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

        unsigned char *buf = MALLOC(EXPECTED_VM_TRACE_NIB_FILE_SIZE);
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
#define EXPECTED_VM_TRACE_PO_SHA "3AE332028B37DE1DD0F967C095800E28D5DC6DB7"
TEST test_boot_disk_vmtrace_po() {
    test_setup_boot_disk(BLANK_PO, 0);

    const char *homedir = HOMEDIR;
    char *disk = NULL;
    ASPRINTF(&disk, "%s/a2_vmtrace_po.txt", homedir);
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

        unsigned char *buf = MALLOC(EXPECTED_VM_TRACE_PO_FILE_SIZE);
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

    test_thread_running = true;
    
    GREATEST_SET_SETUP_CB(testtrace_setup, NULL);
    GREATEST_SET_TEARDOWN_CB(testtrace_teardown, NULL);
    GREATEST_SET_BREAKPOINT_CB(test_breakpoint, NULL);

    // TESTS --------------------------

    RUN_TESTp(test_timing_overflow);
    RUN_TESTp(test_boot_sound);

    RUN_TESTp(test_mockingboard_1);

#if ABUSIVE_TESTS
    RUN_TESTp(test_boot_disk_cputrace);
#endif

    RUN_TESTp(test_cputrace_hello_dsk);
    RUN_TESTp(test_cputrace_hello_nib);
    RUN_TESTp(test_cputrace_hello_po);
    RUN_TESTp(test_boot_disk_vmtrace);
    RUN_TESTp(test_boot_disk_vmtrace_nib);
    RUN_TESTp(test_boot_disk_vmtrace_po);

    // ...
    disk6_eject(0);
    pthread_mutex_unlock(&interface_mutex);
}

SUITE(test_suite_trace);
GREATEST_MAIN_DEFS();

static char **test_argv = NULL;
static int test_argc = 0;

static int _test_thread(void) {
    int argc = test_argc;
    char **argv = test_argv;
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(test_suite_trace);
    GREATEST_MAIN_END();
}

static void *test_thread(void *dummyptr) {
    _test_thread();
    return NULL;
}

void test_trace(int _argc, char **_argv) {
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

