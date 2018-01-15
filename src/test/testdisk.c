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

#define TEST_DISK_EDGE_CASES 1
#define TESTING_DISK "testvm1.dsk.gz"
#define BLANK_DSK "blank.dsk.gz"
#define BLANK_NIB "blank.nib.gz"
#define BLANK_PO  "blank.po.gz"

static bool test_thread_running = false;

extern pthread_mutex_t interface_mutex; // TODO FIXME : raw access to CPU mutex because stepping debugger ...

static void testdisk_setup(void *arg) {
    test_common_setup();
    apple_ii_64k[0][MIXSWITCH_ADDR] = 0x00;
    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    apple_ii_64k[0][TESTOUT_ADDR] = 0x00;
    run_args.joy_button0 = 0xff; // OpenApple
    test_setup_boot_disk(TESTING_DISK, 1);
    if (test_do_reboot) {
        cpu65_interrupt(ResetSig);
    }
}

static void testdisk_teardown(void *arg) {
}

// ----------------------------------------------------------------------------
// Disk TESTS ...

#if CONFORMANT_TRACKS
#define EXPECTED_DISK_TRACE_FILE_SIZE 141348
#define EXPECTED_DISK_TRACE_SHA "8E2415BB7F0A113BFE048FFE0C076AD4B377D22E"
#else
#define EXPECTED_DISK_TRACE_FILE_SIZE 134290
#define EXPECTED_DISK_TRACE_SHA "FA47CC59F0CC7E5B1E938FD54A3BD8DB6C930593"
#endif
TEST test_boot_disk_bytes() {
    srandom(0);

    const char *homedir = HOMEDIR;
    char *disk = NULL;
    ASPRINTF(&disk, "%s/a2_read_disk_test.txt", homedir);
    if (disk) {
        unlink(disk);
        disk6_traceBegin(disk, NULL);
    }

    BOOT_TO_DOS();

    disk6_traceEnd();
    disk6_eject(0);

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(disk, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == EXPECTED_DISK_TRACE_FILE_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = MALLOC(EXPECTED_DISK_TRACE_FILE_SIZE);
        if (fread(buf, 1, EXPECTED_DISK_TRACE_FILE_SIZE, fp) != EXPECTED_DISK_TRACE_FILE_SIZE) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, EXPECTED_DISK_TRACE_FILE_SIZE, md);
        FREE(buf);

        sha1_to_str(md, mdstr0);
        ASSERT(strcmp(mdstr0, EXPECTED_DISK_TRACE_SHA) == 0);
    } while(0);

    unlink(disk);
    FREE(disk);

    PASS();
}

#define EXPECTED_DISK_TRACE_NIB_FILE_SIZE 147366
#define EXPECTED_DISK_TRACE_NIB_SHA "CE61D709A344778AB8A8DACED5A38D0D40F1E645"
TEST test_boot_disk_bytes_nib() {
    test_setup_boot_disk(BLANK_NIB, 0);
    srandom(0);

    const char *homedir = HOMEDIR;
    char *disk = NULL;
    ASPRINTF(&disk, "%s/a2_read_disk_test_nib.txt", homedir);
    if (disk) {
        unlink(disk);
        disk6_traceBegin(disk, NULL);
    }

    BOOT_TO_DOS();

    disk6_traceEnd();
    disk6_eject(0);

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(disk, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == EXPECTED_DISK_TRACE_NIB_FILE_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = MALLOC(EXPECTED_DISK_TRACE_NIB_FILE_SIZE);
        if (fread(buf, 1, EXPECTED_DISK_TRACE_NIB_FILE_SIZE, fp) != EXPECTED_DISK_TRACE_NIB_FILE_SIZE) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, EXPECTED_DISK_TRACE_NIB_FILE_SIZE, md);
        FREE(buf);

        sha1_to_str(md, mdstr0);
        ASSERT(strcmp(mdstr0, EXPECTED_DISK_TRACE_NIB_SHA) == 0);
    } while(0);

    unlink(disk);
    FREE(disk);

    PASS();
}

#if CONFORMANT_TRACKS
#define EXPECTED_DISK_TRACE_PO_FILE_SIZE 141348
#define EXPECTED_DISK_TRACE_PO_SHA "41C382A0A508F9A7935532ECFB7A1B6D53956A8D"
#else
#define EXPECTED_DISK_TRACE_PO_FILE_SIZE 134290
#define EXPECTED_DISK_TRACE_PO_SHA "E85D7B357B02942772F46332953E59CAB67D85CD"
#endif
TEST test_boot_disk_bytes_po() {
    test_setup_boot_disk(BLANK_PO, 0);
    srandom(0);

    const char *homedir = HOMEDIR;
    char *disk = NULL;
    ASPRINTF(&disk, "%s/a2_read_disk_test_po.txt", homedir);
    if (disk) {
        unlink(disk);
        disk6_traceBegin(disk, NULL);
    }

    BOOT_TO_DOS();

    disk6_traceEnd();
    disk6_eject(0);

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(disk, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == EXPECTED_DISK_TRACE_PO_FILE_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = MALLOC(EXPECTED_DISK_TRACE_PO_FILE_SIZE);
        if (fread(buf, 1, EXPECTED_DISK_TRACE_PO_FILE_SIZE, fp) != EXPECTED_DISK_TRACE_PO_FILE_SIZE) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, EXPECTED_DISK_TRACE_PO_FILE_SIZE, md);
        FREE(buf);

        sha1_to_str(md, mdstr0);
        ASSERT(strcmp(mdstr0, EXPECTED_DISK_TRACE_PO_SHA) == 0);
    } while(0);

    unlink(disk);
    FREE(disk);

    PASS();
}

TEST test_boot_disk() {
    BOOT_TO_DOS();
    PASS();
}

#define ASM_INIT() \
    test_type_input( \
            "CALL-151\r" \
            "!\r" \
            "1E00: NOP\r" \
            )

#define ASM_TRIGGER_WATCHPT() \
    test_type_input( \
            " LDA #FF\r" \
            " STA $1F33\r" \
            )

#define ASM_TEST_DISK_READ_NULL() \
    test_type_input( \
            " LDA $C0E9\r" /* drive on */ \
            " LDA $C0EB\r" /* switch to drive 2 */ \
            " LDA #00\r" \
            " LDA $C0EC\r" /* read byte */ \
            " STA $1F43\r" /* save to testout addr */ \
            )

#define ASM_DONE() test_type_input("\r")

#define ASM_GO() test_type_input("1E00G\r")

TEST test_read_null_bytes() {
    BOOT_TO_DOS();

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    apple_ii_64k[0][TESTOUT_ADDR] = 0xAA;

    ASM_INIT();
    ASM_TEST_DISK_READ_NULL();
    ASM_TRIGGER_WATCHPT();
    ASM_DONE();
    ASM_GO();

    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR]    == 0xFF);

    PASS();
}

#define SAVE_SHA1 "B721C61BD10E28F9B833C5F661AA60C73B2D9F74"
TEST test_savehello_dsk() {

    test_setup_boot_disk(BLANK_DSK, 0);
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR]    == 0x00);

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    test_type_input("SAVE HELLO\r");
    test_type_input("POKE7987,255:REM TRIGGER DEBUGGER\r");

    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA(SAVE_SHA1);

    REBOOT_TO_DOS();
    c_debugger_go();
    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA(BOOT_SCREEN);

    disk6_eject(0);

    PASS();
}

TEST test_savehello_nib() {

    test_setup_boot_disk(BLANK_NIB, 0);
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    test_type_input("SAVE HELLO\r");
    test_type_input("POKE7987,255:REM TRIGGER DEBUGGER\r");

    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA(SAVE_SHA1);

    REBOOT_TO_DOS();
    c_debugger_go();
    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA(BOOT_SCREEN);

    disk6_eject(0);

    PASS();
}

TEST test_savehello_po() {

    test_setup_boot_disk(BLANK_PO, 0);
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    test_type_input("SAVE HELLO\r");
    test_type_input("POKE7987,255:REM TRIGGER DEBUGGER\r");

    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA(SAVE_SHA1);

    REBOOT_TO_DOS();
    c_debugger_go();
    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA(BOOT_SCREEN);

    disk6_eject(0);

    PASS();
}

#if CONFORMANT_TRACKS
#   define EXPECTED_DISKWRITE_TRACE_DSK_FILE_SIZE 85915
#   define EXPECTED_DISKWRITE_TRACE_DSK_SHA "05A9043B09605546F2BCFD31CB2E48C779227D95"
#else
#   define EXPECTED_DISKWRITE_TRACE_DSK_FILE_SIZE 85916
#   define EXPECTED_DISKWRITE_TRACE_DSK_SHA "06E446C69C4C3522BDEF146B56B0414CA945A588"
#endif
#define EXPECTED_BSAVE_DSK_SHA "4DC3AEB266692EB5F8C757F36963F8CCC8056AE4"
TEST test_disk_bytes_savehello_dsk() {
    test_setup_boot_disk(BLANK_DSK, 0);
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR]    == 0x00);

    srandom(0);
    const char *homedir = HOMEDIR;
    char *disk = NULL;
    ASPRINTF(&disk, "%s/a2_write_disk_test_dsk.txt", homedir);
    if (disk) {
        unlink(disk);
        disk6_traceBegin(NULL, disk);
    }

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    test_type_input("SAVE HELLO\r");
    test_type_input("POKE7987,255:REM TRIGGER DEBUGGER\r");

    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA(SAVE_SHA1);

    disk6_traceEnd();

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(disk, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == EXPECTED_DISKWRITE_TRACE_DSK_FILE_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = MALLOC(EXPECTED_DISKWRITE_TRACE_DSK_FILE_SIZE);
        if (fread(buf, 1, EXPECTED_DISKWRITE_TRACE_DSK_FILE_SIZE, fp) != EXPECTED_DISKWRITE_TRACE_DSK_FILE_SIZE) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, EXPECTED_DISKWRITE_TRACE_DSK_FILE_SIZE, md);
        FREE(buf);

        sha1_to_str(md, mdstr0);
        ASSERT(strcmp(mdstr0, EXPECTED_DISKWRITE_TRACE_DSK_SHA) == 0);
    } while(0);

    unlink(disk);
    FREE(disk);

    REBOOT_TO_DOS();
    c_debugger_go();
    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA(BOOT_SCREEN);

    disk6_eject(0);

    // Now verify actual disk bytes written to disk
    test_setup_boot_disk(BLANK_DSK, /*readonly:*/0); // !readonly forces gunzip()ping file so we can read raw data ...

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(disk6.disk[0].file_name, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == DSK_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = MALLOC(DSK_SIZE);
        if (fread(buf, 1, DSK_SIZE, fp) != DSK_SIZE) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, DSK_SIZE, md);
        FREE(buf);

        sha1_to_str(md, mdstr0);
        ASSERT(strcmp(mdstr0, EXPECTED_BSAVE_DSK_SHA) == 0);
    } while(0);

    disk6_eject(0);

    PASS();
}

#define EXPECTED_DISKWRITE_TRACE_NIB_FILE_SIZE 2409
#define EXPECTED_DISKWRITE_TRACE_NIB_SHA "332EA76D8BCE45ACA6F805B978E6A3327386ABD6"
#define EXPECTED_BSAVE_NIB_SHA "D894560D1061008FFA9F0BC08B163AC7086A7C0E"
TEST test_disk_bytes_savehello_nib() {
    test_setup_boot_disk(BLANK_NIB, 0);
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR]    == 0x00);

    srandom(0);
    const char *homedir = HOMEDIR;
    char *disk = NULL;
    ASPRINTF(&disk, "%s/a2_write_disk_test_nib.txt", homedir);
    if (disk) {
        unlink(disk);
        disk6_traceBegin(NULL, disk);
    }

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    test_type_input("SAVE HELLO\r");
    test_type_input("POKE7987,255:REM TRIGGER DEBUGGER\r");

    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA(SAVE_SHA1);

    disk6_traceEnd();

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(disk, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == EXPECTED_DISKWRITE_TRACE_NIB_FILE_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = MALLOC(EXPECTED_DISKWRITE_TRACE_NIB_FILE_SIZE);
        if (fread(buf, 1, EXPECTED_DISKWRITE_TRACE_NIB_FILE_SIZE, fp) != EXPECTED_DISKWRITE_TRACE_NIB_FILE_SIZE) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, EXPECTED_DISKWRITE_TRACE_NIB_FILE_SIZE, md);
        FREE(buf);

        sha1_to_str(md, mdstr0);
        ASSERT(strcmp(mdstr0, EXPECTED_DISKWRITE_TRACE_NIB_SHA) == 0);
    } while(0);

    unlink(disk);
    FREE(disk);

    REBOOT_TO_DOS();
    c_debugger_go();
    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA(BOOT_SCREEN);

    disk6_eject(0);

    // Now verify actual disk bytes written to disk
    test_setup_boot_disk(BLANK_NIB, /*readonly:*/0); // !readonly forces gunzip()ping file so we can read raw data ...

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(disk6.disk[0].file_name, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == NIB_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = MALLOC(NIB_SIZE);
        if (fread(buf, 1, NIB_SIZE, fp) != NIB_SIZE) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, NIB_SIZE, md);
        FREE(buf);

        sha1_to_str(md, mdstr0);
        ASSERT(strcmp(mdstr0, EXPECTED_BSAVE_NIB_SHA) == 0);
    } while(0);

    disk6_eject(0);

    PASS();
}

#define EXPECTED_DISKWRITE_TRACE_PO_FILE_SIZE EXPECTED_DISKWRITE_TRACE_DSK_FILE_SIZE
#define EXPECTED_DISKWRITE_TRACE_PO_SHA EXPECTED_DISKWRITE_TRACE_DSK_SHA
#define EXPECTED_BSAVE_PO_SHA "9B47A4B92F64ACEB2B82B3B870C78E93780F18F3"
TEST test_disk_bytes_savehello_po() {
    test_setup_boot_disk(BLANK_PO, 0);
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR]    == 0x00);

    srandom(0);
    const char *homedir = HOMEDIR;
    char *disk = NULL;
    ASPRINTF(&disk, "%s/a2_write_disk_test_po.txt", homedir);
    if (disk) {
        unlink(disk);
        disk6_traceBegin(NULL, disk);
    }

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    test_type_input("SAVE HELLO\r");
    test_type_input("POKE7987,255:REM TRIGGER DEBUGGER\r");

    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA(SAVE_SHA1);

    disk6_traceEnd();

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(disk, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == EXPECTED_DISKWRITE_TRACE_PO_FILE_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = MALLOC(EXPECTED_DISKWRITE_TRACE_PO_FILE_SIZE);
        if (fread(buf, 1, EXPECTED_DISKWRITE_TRACE_PO_FILE_SIZE, fp) != EXPECTED_DISKWRITE_TRACE_PO_FILE_SIZE) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, EXPECTED_DISKWRITE_TRACE_PO_FILE_SIZE, md);
        FREE(buf);

        sha1_to_str(md, mdstr0);
        ASSERT(strcmp(mdstr0, EXPECTED_DISKWRITE_TRACE_PO_SHA) == 0);
    } while(0);

    unlink(disk);
    FREE(disk);

    REBOOT_TO_DOS();
    c_debugger_go();
    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA(BOOT_SCREEN);

    disk6_eject(0);

    // Now verify actual disk bytes written to disk
    test_setup_boot_disk(BLANK_PO, /*readonly:*/0); // !readonly forces gunzip()ping file so we can read raw data ...

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(disk6.disk[0].file_name, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == DSK_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = MALLOC(DSK_SIZE);
        if (fread(buf, 1, DSK_SIZE, fp) != DSK_SIZE) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, DSK_SIZE, md);
        FREE(buf);

        sha1_to_str(md, mdstr0);
        ASSERT(strcmp(mdstr0, EXPECTED_BSAVE_PO_SHA) == 0);
    } while(0);

    disk6_eject(0);

    PASS();
}

#define EAT_UP_DISK_SPACE() \
    do { \
        test_type_input("CALL-151\r"); \
        test_type_input("BSAVEJUNK0,A$2000,L$4000\r"); \
        test_type_input("BSAVEJUNK1,A$2000,L$4000\r"); \
        test_type_input("BSAVEJUNK2,A$2000,L$4000\r"); \
        test_type_input("BSAVEJUNK3,A$2000,L$4000\r"); \
        test_type_input("BSAVEJUNK4,A$2000,L$4000\r"); \
        test_type_input("BSAVEJUNK5,A$2000,L$4000\r"); \
        test_type_input("BSAVEJUNK6,A$2000,L$4000\r"); \
        test_type_input("BSAVEJUNK7,A$2000,L$4000\r"); \
    } while (0)

#define NOSPACE_SHA1 "2EA4D4B9F1C6797E476CD0FE59970CC243263B16"
#define EXPECTED_OOS_DSK_SHA "D5C5A3FFB43F3C55E1C9E4ABD8580322415E9CE0"
#if CONFORMANT_TRACKS
#   define EXPECTED_OOS_DSK_TRACE_FILE_SIZE 4386397
#   define EXPECTED_OOS_DSK_TRACE_SHA "FF60D99539047B76B0B441C5907F0FBE3D0B2FCE"
#else
#   define EXPECTED_OOS_DSK_TRACE_FILE_SIZE 4386354
#   define EXPECTED_OOS_DSK_TRACE_SHA "1304191D985B3D3E528FB462D2CF3677584CD2C3"
#endif
TEST test_outofspace_dsk() {
    test_setup_boot_disk(BLANK_DSK, 0);
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR]    == 0x00);

    srandom(0);
    const char *homedir = HOMEDIR;
    char *disk = NULL;
    ASPRINTF(&disk, "%s/a2_oos_dsk_test.txt", homedir);
    if (disk) {
        unlink(disk);
        disk6_traceBegin(NULL, disk);
    }

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    EAT_UP_DISK_SPACE();
    test_type_input("POKE7987,255:REM TRIGGER DEBUGGER\r");

    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA(NOSPACE_SHA1);

    disk6_traceEnd();

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(disk, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == EXPECTED_OOS_DSK_TRACE_FILE_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = MALLOC(EXPECTED_OOS_DSK_TRACE_FILE_SIZE);
        if (fread(buf, 1, EXPECTED_OOS_DSK_TRACE_FILE_SIZE, fp) != EXPECTED_OOS_DSK_TRACE_FILE_SIZE) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, EXPECTED_OOS_DSK_TRACE_FILE_SIZE, md);
        FREE(buf);

        sha1_to_str(md, mdstr0);
        ASSERT(strcmp(mdstr0, EXPECTED_OOS_DSK_TRACE_SHA) == 0);
    } while(0);

    unlink(disk);
    FREE(disk);

    REBOOT_TO_DOS();
    c_debugger_go();
    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA(BOOT_SCREEN);

    disk6_eject(0);

    // Now verify actual disk bytes written to disk
    test_setup_boot_disk(BLANK_DSK, /*readonly:*/0); // !readonly forces gunzip()ping file so we can read raw data ...

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(disk6.disk[0].file_name, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == DSK_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = MALLOC(DSK_SIZE);
        if (fread(buf, 1, DSK_SIZE, fp) != DSK_SIZE) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, DSK_SIZE, md);
        FREE(buf);

        sha1_to_str(md, mdstr0);
        ASSERT(strcmp(mdstr0, EXPECTED_OOS_DSK_SHA) == 0);
    } while(0);

    disk6_eject(0);

    PASS();
}

#define EXPECTED_OOS_NIB_SHA "D1C404E55811C47CF105D08D536EBBEEC7AA7F51"
TEST test_outofspace_nib() {
    test_setup_boot_disk(BLANK_NIB, 0);
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR]    == 0x00);

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    EAT_UP_DISK_SPACE();
    test_type_input("POKE7987,255:REM TRIGGER DEBUGGER\r");

    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA(NOSPACE_SHA1);

    REBOOT_TO_DOS();
    c_debugger_go();
    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA(BOOT_SCREEN);

    disk6_eject(0);

    // Now verify actual disk bytes written to disk
    test_setup_boot_disk(BLANK_NIB, /*readonly:*/0); // !readonly forces gunzip()ping file so we can read raw data ...

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(disk6.disk[0].file_name, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == NIB_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = MALLOC(NIB_SIZE);
        if (fread(buf, 1, NIB_SIZE, fp) != NIB_SIZE) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, NIB_SIZE, md);
        FREE(buf);

        sha1_to_str(md, mdstr0);
        ASSERT(strcmp(mdstr0, EXPECTED_OOS_NIB_SHA) == 0);
    } while(0);

    disk6_eject(0);

    PASS();
}

#define EXPECTED_OOS_PO_SHA "77F0013B9686A26877FD83ECA7E86E55CA3FFC7E"
TEST test_outofspace_po() {
    test_setup_boot_disk(BLANK_PO, 0);
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR]    == 0x00);

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    EAT_UP_DISK_SPACE();
    test_type_input("POKE7987,255:REM TRIGGER DEBUGGER\r");

    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA(NOSPACE_SHA1);

    REBOOT_TO_DOS();
    c_debugger_go();
    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA(BOOT_SCREEN);

    disk6_eject(0);

    // Now verify actual disk bytes written to disk
    test_setup_boot_disk(BLANK_PO, /*readonly:*/0); // !readonly forces gunzip()ping file so we can read raw data ...

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(disk6.disk[0].file_name, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == DSK_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = MALLOC(DSK_SIZE);
        if (fread(buf, 1, DSK_SIZE, fp) != DSK_SIZE) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, DSK_SIZE, md);
        FREE(buf);

        sha1_to_str(md, mdstr0);
        ASSERT(strcmp(mdstr0, EXPECTED_OOS_PO_SHA) == 0);
    } while(0);

    disk6_eject(0);

    PASS();
}

#define JUNK_MEM_SHA1 "10F26F44A736EE68E04C94C416E085E2319B3F9F"
#define JUNK_MEM_END_SHA1 "C09B5F8668F061AACEA5765943BD4D743061F701"
#if CONFORMANT_TRACKS
#   define EXPECTED_BLOAD_TRACE_DSK_FILE_SIZE 1595253
#   define EXPECTED_BLOAD_TRACE_DSK_SHA "BF8719EE6E4814556957603068C47CC3F78E352C"
#else
#   define EXPECTED_BLOAD_TRACE_DSK_FILE_SIZE 1512000
#   define EXPECTED_BLOAD_TRACE_DSK_SHA "F5BE62CEFA89B6B09C0F257D34AD4E0868DA0B4C"
#endif

TEST test_bload_trace_dsk() {
    test_setup_boot_disk(BLANK_DSK, 0);
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR]    == 0x00);

    srandom(0);
    const char *homedir = HOMEDIR;
    char *disk = NULL;
    ASPRINTF(&disk, "%s/a2_bload_trace_test_dsk.txt", homedir);
    if (disk) {
        unlink(disk);
        disk6_traceBegin(disk, NULL);
    }

    test_type_input("CALL-151\r");

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    test_type_input("2000<7000.BFFFM\r");
    test_type_input("BLOADJUNK0,A$2000\r");
    test_type_input("1F33:FF\r");
    c_debugger_go();
    ASSERT_SHA_MEM(JUNK_MEM_SHA1, 0x2000, 0x4000);

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    test_common_setup();
    test_type_input("2000<7000.BFFFM\r");
    test_type_input("BLOADJUNK1,A$2000\r");
    test_type_input("1F33:FF\r");
    c_debugger_go();
    ASSERT_SHA_MEM(JUNK_MEM_SHA1, 0x2000, 0x4000);

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    test_common_setup();
    test_type_input("2000<7000.BFFFM\r");
    test_type_input("BLOADJUNK2,A$2000\r");
    test_type_input("1F33:FF\r");
    c_debugger_go();
    ASSERT_SHA_MEM(JUNK_MEM_SHA1, 0x2000, 0x4000);

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    test_common_setup();
    test_type_input("2000<7000.BFFFM\r");
    test_type_input("BLOADJUNK3,A$2000\r");
    test_type_input("1F33:FF\r");
    c_debugger_go();
    ASSERT_SHA_MEM(JUNK_MEM_SHA1, 0x2000, 0x4000);

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    test_common_setup();
    test_type_input("2000<7000.BFFFM\r");
    test_type_input("BLOADJUNK4,A$2000\r");
    test_type_input("1F33:FF\r");
    c_debugger_go();
    ASSERT_SHA_MEM(JUNK_MEM_SHA1, 0x2000, 0x4000);

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    test_common_setup();
    test_type_input("2000<7000.BFFFM\r");
    test_type_input("BLOADJUNK5,A$2000\r");
    test_type_input("1F33:FF\r");
    c_debugger_go();
    ASSERT_SHA_MEM(JUNK_MEM_SHA1, 0x2000, 0x4000);

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    test_common_setup();
    test_type_input("2000<7000.BFFFM\r");
    test_type_input("BLOADJUNK6,A$2000\r");
    test_type_input("1F33:FF\r");
    c_debugger_go();
    ASSERT_SHA_MEM(JUNK_MEM_SHA1, 0x2000, 0x4000);

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    test_common_setup();
    test_type_input("2000<7000.BFFFM\r");
    test_type_input("BLOADJUNK7,A$2000\r");
    test_type_input("1F33:FF\r");
    c_debugger_go();
    ASSERT_SHA_MEM(JUNK_MEM_END_SHA1, 0x2000, 0x4000);

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    disk6_traceEnd();

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(disk, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == EXPECTED_BLOAD_TRACE_DSK_FILE_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = MALLOC(EXPECTED_BLOAD_TRACE_DSK_FILE_SIZE);
        if (fread(buf, 1, EXPECTED_BLOAD_TRACE_DSK_FILE_SIZE, fp) != EXPECTED_BLOAD_TRACE_DSK_FILE_SIZE) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, EXPECTED_BLOAD_TRACE_DSK_FILE_SIZE, md);
        FREE(buf);

        sha1_to_str(md, mdstr0);
        ASSERT(strcmp(mdstr0, EXPECTED_BLOAD_TRACE_DSK_SHA) == 0);
    } while(0);

    unlink(disk);
    FREE(disk);

    disk6_eject(0);

    PASS();
}

#define EXPECTED_BLOAD_TRACE_NIB_FILE_SIZE 1664090
#define EXPECTED_BLOAD_TRACE_NIB_SHA "7BDB0624888762D1DA2879E8437B316FEE5D39D6"

TEST test_bload_trace_nib() {
    test_setup_boot_disk(BLANK_NIB, 0);
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR]    == 0x00);

    srandom(0);
    const char *homedir = HOMEDIR;
    char *disk = NULL;
    ASPRINTF(&disk, "%s/a2_bload_trace_test_nib.txt", homedir);
    if (disk) {
        unlink(disk);
        disk6_traceBegin(disk, NULL);
    }

    test_type_input("CALL-151\r");

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    test_type_input("2000<7000.BFFFM\r");
    test_type_input("BLOADJUNK0,A$2000\r");
    test_type_input("1F33:FF\r");
    c_debugger_go();
    ASSERT_SHA_MEM(JUNK_MEM_SHA1, 0x2000, 0x4000);

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    test_common_setup();
    test_type_input("2000<7000.BFFFM\r");
    test_type_input("BLOADJUNK1,A$2000\r");
    test_type_input("1F33:FF\r");
    c_debugger_go();
    ASSERT_SHA_MEM(JUNK_MEM_SHA1, 0x2000, 0x4000);

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    test_common_setup();
    test_type_input("2000<7000.BFFFM\r");
    test_type_input("BLOADJUNK2,A$2000\r");
    test_type_input("1F33:FF\r");
    c_debugger_go();
    ASSERT_SHA_MEM(JUNK_MEM_SHA1, 0x2000, 0x4000);

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    test_common_setup();
    test_type_input("2000<7000.BFFFM\r");
    test_type_input("BLOADJUNK3,A$2000\r");
    test_type_input("1F33:FF\r");
    c_debugger_go();
    ASSERT_SHA_MEM(JUNK_MEM_SHA1, 0x2000, 0x4000);

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    test_common_setup();
    test_type_input("2000<7000.BFFFM\r");
    test_type_input("BLOADJUNK4,A$2000\r");
    test_type_input("1F33:FF\r");
    c_debugger_go();
    ASSERT_SHA_MEM(JUNK_MEM_SHA1, 0x2000, 0x4000);

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    test_common_setup();
    test_type_input("2000<7000.BFFFM\r");
    test_type_input("BLOADJUNK5,A$2000\r");
    test_type_input("1F33:FF\r");
    c_debugger_go();
    ASSERT_SHA_MEM(JUNK_MEM_SHA1, 0x2000, 0x4000);

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    test_common_setup();
    test_type_input("2000<7000.BFFFM\r");
    test_type_input("BLOADJUNK6,A$2000\r");
    test_type_input("1F33:FF\r");
    c_debugger_go();
    ASSERT_SHA_MEM(JUNK_MEM_SHA1, 0x2000, 0x4000);

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    test_common_setup();
    test_type_input("2000<7000.BFFFM\r");
    test_type_input("BLOADJUNK7,A$2000\r");
    test_type_input("1F33:FF\r");
    c_debugger_go();
    ASSERT_SHA_MEM(JUNK_MEM_END_SHA1, 0x2000, 0x4000);

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    disk6_traceEnd();

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(disk, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == EXPECTED_BLOAD_TRACE_NIB_FILE_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = MALLOC(EXPECTED_BLOAD_TRACE_NIB_FILE_SIZE);
        if (fread(buf, 1, EXPECTED_BLOAD_TRACE_NIB_FILE_SIZE, fp) != EXPECTED_BLOAD_TRACE_NIB_FILE_SIZE) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, EXPECTED_BLOAD_TRACE_NIB_FILE_SIZE, md);
        FREE(buf);

        sha1_to_str(md, mdstr0);
        ASSERT(strcmp(mdstr0, EXPECTED_BLOAD_TRACE_NIB_SHA) == 0);
    } while(0);

    unlink(disk);
    FREE(disk);

    disk6_eject(0);

    PASS();
}

#define EXPECTED_BLOAD_TRACE_PO_FILE_SIZE EXPECTED_BLOAD_TRACE_DSK_FILE_SIZE
#define EXPECTED_BLOAD_TRACE_PO_SHA EXPECTED_BLOAD_TRACE_DSK_SHA

TEST test_bload_trace_po() {
    test_setup_boot_disk(BLANK_PO, 0);
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR]    == 0x00);

    srandom(0);
    const char *homedir = HOMEDIR;
    char *disk = NULL;
    ASPRINTF(&disk, "%s/a2_bload_trace_test_po.txt", homedir);
    if (disk) {
        unlink(disk);
        disk6_traceBegin(disk, NULL);
    }

    test_type_input("CALL-151\r");

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    test_type_input("2000<7000.BFFFM\r");
    test_type_input("BLOADJUNK0,A$2000\r");
    test_type_input("1F33:FF\r");
    c_debugger_go();
    ASSERT_SHA_MEM(JUNK_MEM_SHA1, 0x2000, 0x4000);

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    test_common_setup();
    test_type_input("2000<7000.BFFFM\r");
    test_type_input("BLOADJUNK1,A$2000\r");
    test_type_input("1F33:FF\r");
    c_debugger_go();
    ASSERT_SHA_MEM(JUNK_MEM_SHA1, 0x2000, 0x4000);

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    test_common_setup();
    test_type_input("2000<7000.BFFFM\r");
    test_type_input("BLOADJUNK2,A$2000\r");
    test_type_input("1F33:FF\r");
    c_debugger_go();
    ASSERT_SHA_MEM(JUNK_MEM_SHA1, 0x2000, 0x4000);

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    test_common_setup();
    test_type_input("2000<7000.BFFFM\r");
    test_type_input("BLOADJUNK3,A$2000\r");
    test_type_input("1F33:FF\r");
    c_debugger_go();
    ASSERT_SHA_MEM(JUNK_MEM_SHA1, 0x2000, 0x4000);

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    test_common_setup();
    test_type_input("2000<7000.BFFFM\r");
    test_type_input("BLOADJUNK4,A$2000\r");
    test_type_input("1F33:FF\r");
    c_debugger_go();
    ASSERT_SHA_MEM(JUNK_MEM_SHA1, 0x2000, 0x4000);

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    test_common_setup();
    test_type_input("2000<7000.BFFFM\r");
    test_type_input("BLOADJUNK5,A$2000\r");
    test_type_input("1F33:FF\r");
    c_debugger_go();
    ASSERT_SHA_MEM(JUNK_MEM_SHA1, 0x2000, 0x4000);

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    test_common_setup();
    test_type_input("2000<7000.BFFFM\r");
    test_type_input("BLOADJUNK6,A$2000\r");
    test_type_input("1F33:FF\r");
    c_debugger_go();
    ASSERT_SHA_MEM(JUNK_MEM_SHA1, 0x2000, 0x4000);

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    test_common_setup();
    test_type_input("2000<7000.BFFFM\r");
    test_type_input("BLOADJUNK7,A$2000\r");
    test_type_input("1F33:FF\r");
    c_debugger_go();
    ASSERT_SHA_MEM(JUNK_MEM_END_SHA1, 0x2000, 0x4000);

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    disk6_traceEnd();

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(disk, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == EXPECTED_BLOAD_TRACE_PO_FILE_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = MALLOC(EXPECTED_BLOAD_TRACE_PO_FILE_SIZE);
        if (fread(buf, 1, EXPECTED_BLOAD_TRACE_PO_FILE_SIZE, fp) != EXPECTED_BLOAD_TRACE_PO_FILE_SIZE) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, EXPECTED_BLOAD_TRACE_PO_FILE_SIZE, md);
        FREE(buf);

        sha1_to_str(md, mdstr0);
        ASSERT(strcmp(mdstr0, EXPECTED_BLOAD_TRACE_PO_SHA) == 0);
    } while(0);

    unlink(disk);
    FREE(disk);

    disk6_eject(0);

    PASS();
}

#define INIT_SHA1 "10F15B516E4CF2FC5B1712951A6F9C3D90BF595C"
TEST test_inithello_dsk() {

    test_setup_boot_disk(BLANK_DSK, 0);
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR]    == 0x00);

    test_type_input("INIT HELLO\r");
    test_type_input("POKE7987,255:REM TRIGGER DEBUGGER\r");

    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA(INIT_SHA1);

    REBOOT_TO_DOS();
    c_debugger_go();
    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA(BOOT_SCREEN);

    disk6_eject(0);

    PASS();
}

TEST test_inithello_nib() {

    test_setup_boot_disk(BLANK_NIB, 0);
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    test_type_input("INIT HELLO\r");
    test_type_input("POKE7987,255:REM TRIGGER DEBUGGER\r");

    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA(INIT_SHA1);

    REBOOT_TO_DOS();
    c_debugger_go();
    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA(BOOT_SCREEN);

    disk6_eject(0);

    PASS();
}

TEST test_inithello_po() {

    test_setup_boot_disk(BLANK_PO, 0);
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    test_type_input("INIT HELLO\r");
    test_type_input("POKE7987,255:REM TRIGGER DEBUGGER\r");

    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA(INIT_SHA1);

    REBOOT_TO_DOS();
    c_debugger_go();
    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA(BOOT_SCREEN);

    disk6_eject(0);

    PASS();
}

#define EXPECTED_STABILITY_DSK_FILE_SIZE 143360
#define EXPECTED_STABILITY_DSK_SHA "4DC3AEB266692EB5F8C757F36963F8CCC8056AE4"

TEST test_data_stability_dsk() {

    test_setup_boot_disk(BLANK_DSK, /*readonly:*/0); // !readonly forces gunzip()ping file so we can read raw data ...

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(disk6.disk[0].file_name, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == EXPECTED_STABILITY_DSK_FILE_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = MALLOC(EXPECTED_STABILITY_DSK_FILE_SIZE);
        if (fread(buf, 1, EXPECTED_STABILITY_DSK_FILE_SIZE, fp) != EXPECTED_STABILITY_DSK_FILE_SIZE) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, EXPECTED_STABILITY_DSK_FILE_SIZE, md);
        FREE(buf);

        sha1_to_str(md, mdstr0);
        ASSERT(strcmp(mdstr0, EXPECTED_STABILITY_DSK_SHA) == 0);
    } while(0);

    disk6_eject(0);

    PASS();
}

#define EXPECTED_STABILITY_NIB_FILE_SIZE 232960
#define EXPECTED_STABILITY_NIB_SHA "D894560D1061008FFA9F0BC08B163AC7086A7C0E"

TEST test_data_stability_nib() {

    test_setup_boot_disk(BLANK_NIB, /*readonly:*/0); // !readonly forces gunzip()ping file so we can read raw data ...

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(disk6.disk[0].file_name, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == EXPECTED_STABILITY_NIB_FILE_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = MALLOC(EXPECTED_STABILITY_NIB_FILE_SIZE);
        if (fread(buf, 1, EXPECTED_STABILITY_NIB_FILE_SIZE, fp) != EXPECTED_STABILITY_NIB_FILE_SIZE) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, EXPECTED_STABILITY_NIB_FILE_SIZE, md);
        FREE(buf);

        sha1_to_str(md, mdstr0);
        ASSERT(strcmp(mdstr0, EXPECTED_STABILITY_NIB_SHA) == 0);
    } while(0);

    disk6_eject(0);

    PASS();
}

#define EXPECTED_STABILITY_PO_FILE_SIZE 143360
#define EXPECTED_STABILITY_PO_SHA "9B47A4B92F64ACEB2B82B3B870C78E93780F18F3"

TEST test_data_stability_po() {

    test_setup_boot_disk(BLANK_PO, /*readonly:*/0); // !readonly forces gunzip()ping file so we can read raw data ...

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(disk6.disk[0].file_name, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == EXPECTED_STABILITY_PO_FILE_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = MALLOC(EXPECTED_STABILITY_PO_FILE_SIZE);
        if (fread(buf, 1, EXPECTED_STABILITY_PO_FILE_SIZE, fp) != EXPECTED_STABILITY_PO_FILE_SIZE) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, EXPECTED_STABILITY_PO_FILE_SIZE, md);
        FREE(buf);

        sha1_to_str(md, mdstr0);
        ASSERT(strcmp(mdstr0, EXPECTED_STABILITY_PO_SHA) == 0);
    } while(0);

    disk6_eject(0);

    PASS();
}

#define GZBAD_NIB "testgzheader.nib"
#define GZBAD_NIB_LOAD_SHA1 "98EB8D2EF486E5BF888789A6FF9D4E3DEC7902B7"
#define GZBAD_NIB_LOAD_SHA2 "764F580287564B5464BF98BC2026E110F06C9EA4"
static int _test_disk_image_with_gzip_header(int readonly) {

    test_setup_boot_disk(GZBAD_NIB, readonly);

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    c_debugger_go();
    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    test_type_input("CLEAR\r");

    c_debugger_set_timeout(10);
    c_debugger_go();
    c_debugger_set_timeout(0);

    do {
        uint8_t md[SHA_DIGEST_LENGTH];

        uint8_t *fb = CALLOC(1, SCANWIDTH*SCANHEIGHT);
        display_renderStagingFramebuffer(fb);
        SHA1(fb, SCANWIDTH*SCANHEIGHT, md);
        FREE(fb);

        sha1_to_str(md, mdstr);
        bool matches_sha1 = (strcasecmp(mdstr, GZBAD_NIB_LOAD_SHA1) == 0);
        bool matches_sha2 = (strcasecmp(mdstr, GZBAD_NIB_LOAD_SHA2) == 0);
        ASSERT(matches_sha1 || matches_sha2);
    } while(0);

    disk6_eject(0);

    PASS();
}

TEST test_disk_image_with_gzip_header_ro() {
    return _test_disk_image_with_gzip_header(/*readonly:*/1);
}

TEST test_disk_image_with_gzip_header_rw() {
    return _test_disk_image_with_gzip_header(/*readonly:*/0);
}

#define GZINVALID_DSK "CorruptedGzipped.dsk.gz"
static int _test_disk_invalid_gzipped(int readonly) {

    int found_disk_image_file = 0;
    {
        char **paths = test_copy_disk_paths(GZINVALID_DSK);

        char **path = &paths[0];
        while (*path) {
            char *diskPath = *path;
            ++path;

            int fd = -1;
            TEMP_FAILURE_RETRY(fd = open(diskPath, readonly ? O_RDONLY : O_RDWR));
            if (fd != -1) {

                ++found_disk_image_file;

                int err = disk6_insert(fd, /*drive:*/0, diskPath, readonly) != NULL;
                TEMP_FAILURE_RETRY(close(fd));
                ASSERT(err);

                // did not actually insert corruped disk image and did not crash
                ASSERT(disk6.disk[0].file_name == NULL);
                ASSERT(disk6.disk[0].fd == -1);

                ASSERT(disk6.disk[0].raw_image_data == (void *)-1/*MAP_FAILED*/);
                ASSERT(disk6.disk[0].whole_len == 0);
                ASSERT(disk6.disk[0].nib_image_data == NULL);
                ASSERT(disk6.disk[0].nibblized == false);
                ASSERT(disk6.disk[0].is_protected == false);
                ASSERT(disk6.disk[0].track_valid == false);
                ASSERT(disk6.disk[0].track_dirty == false);
                ASSERT(disk6.disk[0].skew_table == NULL);
                ASSERT(disk6.disk[0].track_width == 0);
            }
        }

        path = &paths[0];
        while (*path) {
            char *diskPath = *path;
            ++path;
            FREE(diskPath);
        }

        FREE(paths);
    }

    ASSERT(found_disk_image_file > 0);

    PASS();
}

TEST test_disk_invalid_gzipped_ro() {
    return _test_disk_invalid_gzipped(/*readonly:*/1);
}

TEST test_disk_invalid_gzipped_rw() {
    return _test_disk_invalid_gzipped(/*readonly:*/0);
}

#if TEST_DISK_EDGE_CASES
#define DROL_DSK "Drol.dsk.gz"
#define DROL_CRACK_SCREEN_SHA "FD7332529E117F14DA3880BB36FE8E23C3704799"
TEST test_reinsert_edgecase() {
    test_setup_boot_disk(DROL_DSK, 0);

    // TODO FIXME : we need both a timeout and a step-until-framebuffer-is-a-particular-SHA routine ...

    // First verify we hit the crackscreen
    c_debugger_set_timeout(10);
    c_debugger_go();
    ASSERT_SHA(DROL_CRACK_SCREEN_SHA);

    // re-insert
    disk6_eject(0);
    test_setup_boot_disk(DROL_DSK, 0);

    // press key to continue and verify we are at a non-blank screen in a short amount of time
    test_type_input(" ");
    c_debugger_clear_watchpoints();
    c_debugger_go();
    uint8_t md[SHA_DIGEST_LENGTH];

    uint8_t *fb = CALLOC(1, SCANWIDTH*SCANHEIGHT);
    display_renderStagingFramebuffer(fb);
    SHA1(fb, SCANWIDTH*SCANHEIGHT, md);
    FREE(fb);

    sha1_to_str(md, mdstr);
    ASSERT(strcmp(mdstr, BLANK_SCREEN) != 0);

    c_debugger_set_timeout(0);
    disk6_eject(0);

    PASS();
}
#endif

// ----------------------------------------------------------------------------
// Test Suite

GREATEST_SUITE(test_suite_disk) {
    pthread_mutex_lock(&interface_mutex);

    test_thread_running = true;
    
    GREATEST_SET_SETUP_CB(testdisk_setup, NULL);
    GREATEST_SET_TEARDOWN_CB(testdisk_teardown, NULL);
    GREATEST_SET_BREAKPOINT_CB(test_breakpoint, NULL);

    // TESTS --------------------------

    RUN_TESTp(test_boot_disk_bytes);
    RUN_TESTp(test_boot_disk_bytes_nib);
    RUN_TESTp(test_boot_disk_bytes_po);

    RUN_TESTp(test_boot_disk);

    RUN_TESTp(test_read_null_bytes);

    RUN_TESTp(test_savehello_dsk);
    RUN_TESTp(test_savehello_nib);
    RUN_TESTp(test_savehello_po);

    RUN_TESTp(test_disk_bytes_savehello_dsk);
    RUN_TESTp(test_disk_bytes_savehello_nib);
    RUN_TESTp(test_disk_bytes_savehello_po);

    c_debugger_set_timeout(0);

    // test order from here is important ...
    //  * load the disks with a buncha junk files
    //  * verify integrity of the junk files
    //  * inithello and verify boots
    //  * check that the disk images are ultimately unchanged

    RUN_TESTp(test_outofspace_dsk);
    RUN_TESTp(test_bload_trace_dsk);
    RUN_TESTp(test_outofspace_nib);
    RUN_TESTp(test_bload_trace_nib);
    RUN_TESTp(test_outofspace_po);
    RUN_TESTp(test_bload_trace_po);

    RUN_TESTp(test_inithello_dsk);
    RUN_TESTp(test_inithello_nib);
    RUN_TESTp(test_inithello_po);

    RUN_TESTp(test_data_stability_dsk);
    RUN_TESTp(test_data_stability_nib);
    RUN_TESTp(test_data_stability_po);

    RUN_TESTp(test_disk_image_with_gzip_header_ro);
    RUN_TESTp(test_disk_image_with_gzip_header_rw);

    RUN_TESTp(test_disk_invalid_gzipped_ro);
    RUN_TESTp(test_disk_invalid_gzipped_rw);

    // edge-case tests may require testing copyrighted images (which I have in my possession by legally owning the
    // original disk image (yep, I do ;-)
#if TEST_DISK_EDGE_CASES
    RUN_TESTp(test_reinsert_edgecase);
#endif

    // ...
    disk6_eject(0);
    pthread_mutex_unlock(&interface_mutex);
}

SUITE(test_suite_disk);
GREATEST_MAIN_DEFS();

static char **test_argv = NULL;
static int test_argc = 0;

static int _test_thread(void) {
    int argc = test_argc;
    char **argv = test_argv;
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(test_suite_disk);
    GREATEST_MAIN_END();
}

static void *test_thread(void *dummyptr) {
    _test_thread();
    return NULL;
}

void test_disk(int _argc, char **_argv) {
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

