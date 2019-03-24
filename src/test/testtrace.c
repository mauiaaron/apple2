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

#define TESTING_DISK "testvm1.dsk.gz"
#define BLANK_DSK "blank.dsk.gz"
#define BLANK_NIB "blank.nib.gz"
#define BLANK_PO  "blank.po.gz"

#if !CONFORMANT_TRACKS
#error trace testing against baseline should be built with CONFORMANT_TRACKS ... FIXME NOW
#endif

static bool test_thread_running = false;

extern pthread_mutex_t interface_mutex; // TODO FIXME : raw access to CPU mutex because stepping debugger ...

static void testtrace_setup(void *arg) {
    disk6_init(); // ensure better for tracing stability against baseline ...
    test_common_setup();
    apple_ii_64k[0][MIXSWITCH_ADDR] = 0x00;
    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    apple_ii_64k[0][TESTOUT_ADDR] = 0x00;
    run_args.joy_button0 = 0xff; // OpenApple
    test_setup_boot_disk(TESTING_DISK, 1);
    if (test_do_reboot) {
        cpu65_interrupt(ResetSig);
    }
    debugger_setWatchpoint(WATCHPOINT_ADDR);
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
    test_setup_boot_disk(BLANK_DSK, /*readonly:*/1);
    debugger_setTimeout(1);

    ASSERT(!cycles_overflowed);
    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    // force an almost overflow
    testing_getCyclesCount = &testspeaker_getCyclesCount;
    testing_cyclesOverflow = &testspeaker_cyclesOverflow;

    do {
        run_args.emul_reinitialize = 1;
        debugger_go();

        if (cycles_overflowed) {
            break;
        }

        ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    } while (1);

    ASSERT(cycles_overflowed);

    debugger_setTimeout(0);

    debugger_go();
    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

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
#define NSCT_TRACE_SHA "B280BBCDC3C8475E53213D0DC78BD20D0E83D133"
#define NSCT_SAMPS_SHA "A534CFDCD3A99468793E4D50E7F651E27964FD75"
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
    debugger_clearWatchpoints();
    debugger_setTimeout(1);
    do {
        debugger_go();

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
    debugger_setTimeout(0);

    unlink(mbTraceFile);
    FREE(mbTraceFile);
    unlink(mbSampsFile);
    FREE(mbSampsFile);

    disk6_eject(0);

    PASS();
}

// ----------------------------------------------------------------------------
// CPU tracing

// This test is majorly abusive ... it creates an ~1GB file in $HOME
// ... but if it's correct, you're fairly assured the cpu/vm is working =)
#define EXPECTED_CPU_TRACE_FILE_SIZE 1098663679
#define EXPECTED_CPU_TRACE_SHA "BFE90A6B7EAAB23F050874EC29A2E66EE92FE9DD"
TEST test_boot_disk_cputrace() {
    const char *homedir = HOMEDIR;
    char *output = NULL;
    ASPRINTF(&output, "%s/a2_cputrace.txt", homedir);
    if (output) {
        unlink(output);
        cpu65_trace_begin(output);
    }

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
#undef EXPECTED_CPU_TRACE_FILE_SIZE
#undef EXPECTED_CPU_TRACE_SHA

#define EXPECTED_BOOT_SIZ2 555444333
#define EXPECTED_BOOT_SHA2 "E807FDC048868FAC246B15CA0F2D5F7B2A12CA2B"
TEST test_boot_disk_cputrace2() { // Failing now due to difference in IRQ timing against baseline
    test_setup_boot_disk(NSCT_DSK, 0);

    const char *homedir = HOMEDIR;
    char *output = NULL;

    ASPRINTF(&output, "%s/a2_cputrace.txt", homedir);
    if (output) {
        unlink(output);
        cpu65_trace_begin(output);
    }

    // Poll for trace file of particular size
    debugger_clearWatchpoints();
    debugger_setTimeout(1);
    do {
        debugger_go();

        FILE *fpTrace = fopen(output, "r");
        fseek(fpTrace, 0, SEEK_END);
        long minSizeTrace = ftell(fpTrace);

        if (minSizeTrace < EXPECTED_BOOT_SIZ2) {
            fclose(fpTrace);
            continue;
        }

        // trace has generated files of sufficient length

        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        cpu65_trace_end();
        truncate(output, EXPECTED_BOOT_SIZ2);

        // verify trace file
        do {
            unsigned char *buf = MALLOC(EXPECTED_BOOT_SIZ2);
            fseek(fpTrace, 0, SEEK_SET);
            ASSERT(fread(buf, 1, EXPECTED_BOOT_SIZ2, fpTrace) == EXPECTED_BOOT_SIZ2);
            fclose(fpTrace); fpTrace = NULL;
            SHA1(buf, EXPECTED_BOOT_SIZ2, md);
            FREE(buf);
            sha1_to_str(md, mdstr0);
            ASSERT(strcmp(mdstr0, EXPECTED_BOOT_SHA2) == 0);
        } while (0);

        break;

    } while (1);

    debugger_setTimeout(0);

    disk6_eject(0);

    unlink(output);
    FREE(output);

    PASS();
}
#undef EXPECTED_BOOT_SIZ2
#undef EXPECTED_BOOT_SHA2

#define EXPECTED_BOOT_SIZ3 1555666777
#define EXPECTED_BOOT_SHA3 "4e90d33f165a5bedd65588ffe1d5618ba131ed61"
TEST test_boot_disk_cputrace3() {
    test_setup_boot_disk("testdisplay2.dsk.gz", 0); // boots directly into LILTEXWIN

    const char *homedir = HOMEDIR;
    char *output = NULL;

    ASPRINTF(&output, "%s/a2_cputrace.txt", homedir);
    if (output) {
        unlink(output);
        cpu65_trace_begin(output);
    }

    // Poll for trace file of particular size
    debugger_clearWatchpoints();
    debugger_setTimeout(1);
    do {
        debugger_go();

        FILE *fpTrace = fopen(output, "r");
        fseek(fpTrace, 0, SEEK_END);
        long minSizeTrace = ftell(fpTrace);

        if (minSizeTrace < EXPECTED_BOOT_SIZ3) {
            fclose(fpTrace);
            continue;
        }

        // trace has generated files of sufficient length

        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        cpu65_trace_end();
        truncate(output, EXPECTED_BOOT_SIZ3);

        // verify trace file
        do {
            unsigned char *buf = MALLOC(EXPECTED_BOOT_SIZ3);
            fseek(fpTrace, 0, SEEK_SET);
            ASSERT(fread(buf, 1, EXPECTED_BOOT_SIZ3, fpTrace) == EXPECTED_BOOT_SIZ3);
            fclose(fpTrace); fpTrace = NULL;
            SHA1(buf, EXPECTED_BOOT_SIZ3, md);
            FREE(buf);
            sha1_to_str(md, mdstr0);
            ASSERT(strcasecmp(mdstr0, EXPECTED_BOOT_SHA3) == 0);
        } while (0);

        break;

    } while (1);

    debugger_setTimeout(0);

    disk6_eject(0);

    unlink(output);
    FREE(output);

    PASS();
}
#undef EXPECTED_BOOT_SIZ3
#undef EXPECTED_BOOT_SHA3

#define EXPECTED_CPUTRACE_HELLO_FILE_SIZE 146562789
#define EXPECTED_CPUTRACE_HELLO_SHA "A2F1806F4539E84C5047F49038D8BC92349E33AC"
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

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    test_type_input_deterministically("RUN HELLO\r");
    debugger_go();

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

#define EXPECTED_CPUTRACE_HELLO_NIB_FILE_SIZE 17318711
#define EXPECTED_CPUTRACE_HELLO_NIB_SHA "4DD390FCC46A1928D967F286C33ABD18AE2B9EEC"
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

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    test_type_input_deterministically("RUN HELLO\r");
    debugger_go();

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

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    test_type_input_deterministically("RUN HELLO\r");
    debugger_go();

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

#if NULL_AUDIO_RENDERER_IS_FIXED_FOR_THIS_TEST
    RUN_TESTp(test_mockingboard_1);
#endif

    RUN_TESTp(test_boot_disk_cputrace);
#if CPU_TRACING_WITH_IRQ_HANDLING_SAME_AS_BASELINE
    RUN_TESTp(test_boot_disk_cputrace2);
#endif
    RUN_TESTp(test_boot_disk_cputrace3);

    RUN_TESTp(test_cputrace_hello_dsk);
    RUN_TESTp(test_cputrace_hello_nib);
    RUN_TESTp(test_cputrace_hello_po);

#if VM_TRACING_FIXED
    // 2016/10/01 : VM tracing is undergoing upheaval
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

