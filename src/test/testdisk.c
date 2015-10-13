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

#if CONFORMANT_TRACKS
#define EXPECTED_DISK_TRACE_FILE_SIZE 141350
#define EXPECTED_DISK_TRACE_SHA "471EB3D01917B1C6EF9F13C5C7BC1ACE4E74C851"
#else
#define EXPECTED_DISK_TRACE_FILE_SIZE 134292
#define EXPECTED_DISK_TRACE_SHA "19C10B594055D88862A35A45301B2E37A2E7E9F4"
#endif
TEST test_boot_disk_bytes() {
    srandom(0);

    const char *homedir = HOMEDIR;
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
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(disk, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == EXPECTED_DISK_TRACE_FILE_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = malloc(EXPECTED_DISK_TRACE_FILE_SIZE);
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

#define EXPECTED_DISK_TRACE_NIB_FILE_SIZE 147368
#define EXPECTED_DISK_TRACE_NIB_SHA "DE92EABF6C9747353E9C8A367706D70E520CC2C1"
TEST test_boot_disk_bytes_nib() {
    test_setup_boot_disk(BLANK_NIB, 0);
    srandom(0);

    const char *homedir = HOMEDIR;
    char *disk = NULL;
    asprintf(&disk, "%s/a2_read_disk_test_nib.txt", homedir);
    if (disk) {
        unlink(disk);
        c_begin_disk_trace_6(disk, NULL);
    }

    BOOT_TO_DOS();

    c_end_disk_trace_6();
    c_eject_6(0);

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(disk, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == EXPECTED_DISK_TRACE_NIB_FILE_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = malloc(EXPECTED_DISK_TRACE_NIB_FILE_SIZE);
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
#define EXPECTED_DISK_TRACE_PO_FILE_SIZE 141350
#define EXPECTED_DISK_TRACE_PO_SHA "6C2170D3AA82F87DD34E177309808199BDCCB018"
#else
#define EXPECTED_DISK_TRACE_PO_FILE_SIZE 134292
#define EXPECTED_DISK_TRACE_PO_SHA "9A6DDCB421B369A4BB7ACC5E40D24B0F38F98711"
#endif
TEST test_boot_disk_bytes_po() {
    test_setup_boot_disk(BLANK_PO, 0);
    srandom(0);

    const char *homedir = HOMEDIR;
    char *disk = NULL;
    asprintf(&disk, "%s/a2_read_disk_test_po.txt", homedir);
    if (disk) {
        unlink(disk);
        c_begin_disk_trace_6(disk, NULL);
    }

    BOOT_TO_DOS();

    c_end_disk_trace_6();
    c_eject_6(0);

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(disk, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == EXPECTED_DISK_TRACE_PO_FILE_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = malloc(EXPECTED_DISK_TRACE_PO_FILE_SIZE);
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
    apple_ii_64k[0][TESTOUT_ADDR] = 0x00;

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

    c_eject_6(0);

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

    c_eject_6(0);

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

    c_eject_6(0);

    PASS();
}

#if CONFORMANT_TRACKS
#   define EXPECTED_DISKWRITE_TRACE_DSK_FILE_SIZE 63675
#   define EXPECTED_DISKWRITE_TRACE_DSK_SHA "CFA1C3AB2CA4F245D291DFC8C277773C5275946C"
#else
#   define EXPECTED_DISKWRITE_TRACE_DSK_FILE_SIZE 63676 // orly?
#   define EXPECTED_DISKWRITE_TRACE_DSK_SHA "FA3792F09A5E96B906D3F2362C6701C0DFB2130C"
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
    asprintf(&disk, "%s/a2_write_disk_test_dsk.txt", homedir);
    if (disk) {
        unlink(disk);
        c_begin_disk_trace_6(NULL, disk);
    }

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    test_type_input("SAVE HELLO\r");
    test_type_input("POKE7987,255:REM TRIGGER DEBUGGER\r");

    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA(SAVE_SHA1);

    c_end_disk_trace_6();

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(disk, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == EXPECTED_DISKWRITE_TRACE_DSK_FILE_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = malloc(EXPECTED_DISKWRITE_TRACE_DSK_FILE_SIZE);
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

    c_eject_6(0);

    // Now verify actual disk bytes written to disk
    test_setup_boot_disk(BLANK_DSK, 1);

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(disk6.disk[0].file_name, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == DSK_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = malloc(DSK_SIZE);
        if (fread(buf, 1, DSK_SIZE, fp) != DSK_SIZE) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, DSK_SIZE, md);
        FREE(buf);

        sha1_to_str(md, mdstr0);
        ASSERT(strcmp(mdstr0, EXPECTED_BSAVE_DSK_SHA) == 0);
    } while(0);

    c_eject_6(0);

    PASS();
}

#define EXPECTED_DISKWRITE_TRACE_NIB_FILE_SIZE 2409
#define EXPECTED_DISKWRITE_TRACE_NIB_SHA "332EA76D8BCE45ACA6F805B978E6A3327386ABD6"
#define EXPECTED_BSAVE_NIB_SHA "94193718A6B610AE31B5ABE7058416B321968CA1"
TEST test_disk_bytes_savehello_nib() {
    test_setup_boot_disk(BLANK_NIB, 0);
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR]    == 0x00);

    srandom(0);
    const char *homedir = HOMEDIR;
    char *disk = NULL;
    asprintf(&disk, "%s/a2_write_disk_test_nib.txt", homedir);
    if (disk) {
        unlink(disk);
        c_begin_disk_trace_6(NULL, disk);
    }

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    test_type_input("SAVE HELLO\r");
    test_type_input("POKE7987,255:REM TRIGGER DEBUGGER\r");

    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA(SAVE_SHA1);

    c_end_disk_trace_6();

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(disk, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == EXPECTED_DISKWRITE_TRACE_NIB_FILE_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = malloc(EXPECTED_DISKWRITE_TRACE_NIB_FILE_SIZE);
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

    c_eject_6(0);

    // Now verify actual disk bytes written to disk
    test_setup_boot_disk(BLANK_NIB, 1);

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(disk6.disk[0].file_name, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == NIB_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = malloc(NIB_SIZE);
        if (fread(buf, 1, NIB_SIZE, fp) != NIB_SIZE) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, NIB_SIZE, md);
        FREE(buf);

        sha1_to_str(md, mdstr0);
        ASSERT(strcmp(mdstr0, EXPECTED_BSAVE_NIB_SHA) == 0);
    } while(0);

    c_eject_6(0);

    PASS();
}

#if CONFORMANT_TRACKS
#   define EXPECTED_DISKWRITE_TRACE_PO_FILE_SIZE 63675
#   define EXPECTED_DISKWRITE_TRACE_PO_SHA "CFA1C3AB2CA4F245D291DFC8C277773C5275946C"
#else
#   define EXPECTED_DISKWRITE_TRACE_PO_FILE_SIZE 63676 // orly?
#   define EXPECTED_DISKWRITE_TRACE_PO_SHA "FA3792F09A5E96B906D3F2362C6701C0DFB2130C"
#endif
#define EXPECTED_BSAVE_PO_SHA "9B47A4B92F64ACEB2B82B3B870C78E93780F18F3"
TEST test_disk_bytes_savehello_po() {
    test_setup_boot_disk(BLANK_PO, 0);
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR]    == 0x00);

    srandom(0);
    const char *homedir = HOMEDIR;
    char *disk = NULL;
    asprintf(&disk, "%s/a2_write_disk_test_po.txt", homedir);
    if (disk) {
        unlink(disk);
        c_begin_disk_trace_6(NULL, disk);
    }

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    test_type_input("SAVE HELLO\r");
    test_type_input("POKE7987,255:REM TRIGGER DEBUGGER\r");

    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA(SAVE_SHA1);

    c_end_disk_trace_6();

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(disk, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == EXPECTED_DISKWRITE_TRACE_PO_FILE_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = malloc(EXPECTED_DISKWRITE_TRACE_PO_FILE_SIZE);
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

    c_eject_6(0);

    // Now verify actual disk bytes written to disk
    test_setup_boot_disk(BLANK_PO, 1);

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(disk6.disk[0].file_name, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == DSK_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = malloc(DSK_SIZE);
        if (fread(buf, 1, DSK_SIZE, fp) != DSK_SIZE) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, DSK_SIZE, md);
        FREE(buf);

        sha1_to_str(md, mdstr0);
        ASSERT(strcmp(mdstr0, EXPECTED_BSAVE_PO_SHA) == 0);
    } while(0);

    c_eject_6(0);

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
#define EXPECTED_OOS_DSK_SHA "7A77D1CFD9F02A84E0FE39B7A39FBD4ABF1EB78E"
TEST test_outofspace_dsk() {
    test_setup_boot_disk(BLANK_DSK, 0);
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

    c_eject_6(0);

    // Now verify actual disk bytes written to disk
    test_setup_boot_disk(BLANK_DSK, 1);

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(disk6.disk[0].file_name, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == DSK_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = malloc(DSK_SIZE);
        if (fread(buf, 1, DSK_SIZE, fp) != DSK_SIZE) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, DSK_SIZE, md);
        FREE(buf);

        sha1_to_str(md, mdstr0);
        ASSERT(strcmp(mdstr0, EXPECTED_OOS_DSK_SHA) == 0);
    } while(0);

    c_eject_6(0);

    PASS();
}

#define EXPECTED_OOS_NIB_SHA "07CEA71514751338996B8EAC9212E5D77813F66A"
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

    c_eject_6(0);

    // Now verify actual disk bytes written to disk
    test_setup_boot_disk(BLANK_NIB, 1);

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(disk6.disk[0].file_name, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == NIB_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = malloc(NIB_SIZE);
        if (fread(buf, 1, NIB_SIZE, fp) != NIB_SIZE) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, NIB_SIZE, md);
        FREE(buf);

        sha1_to_str(md, mdstr0);
        ASSERT(strcmp(mdstr0, EXPECTED_OOS_NIB_SHA) == 0);
    } while(0);

    c_eject_6(0);

    PASS();
}

#define EXPECTED_OOS_PO_SHA "32AF41E922E0186590C651DC501B725EC2C520FE"
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

    c_eject_6(0);

    // Now verify actual disk bytes written to disk
    test_setup_boot_disk(BLANK_PO, 1);

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(disk6.disk[0].file_name, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == DSK_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = malloc(DSK_SIZE);
        if (fread(buf, 1, DSK_SIZE, fp) != DSK_SIZE) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, DSK_SIZE, md);
        FREE(buf);

        sha1_to_str(md, mdstr0);
        ASSERT(strcmp(mdstr0, EXPECTED_OOS_PO_SHA) == 0);
    } while(0);

    c_eject_6(0);

    PASS();
}

#define JUNK_MEM_SHA1 "10F26F44A736EE68E04C94C416E085E2319B3F9F"
#define JUNK_MEM_END_SHA1 "C09B5F8668F061AACEA5765943BD4D743061F701"
#if CONFORMANT_TRACKS
#   define EXPECTED_BLOAD_TRACE_DSK_FILE_SIZE 1595253
#   define EXPECTED_BLOAD_TRACE_DSK_SHA "8485095E5ED8CEF5D8A177B78A53E3E6A022366D"
#else
#   define EXPECTED_BLOAD_TRACE_DSK_FILE_SIZE 1512000
#   define EXPECTED_BLOAD_TRACE_DSK_SHA "99B23A0DF5FCAE3ECB494F98ADE938D9FBF9D726"
#endif

TEST test_bload_trace_dsk() {
    test_setup_boot_disk(BLANK_DSK, 0);
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR]    == 0x00);

    srandom(0);
    const char *homedir = HOMEDIR;
    char *disk = NULL;
    asprintf(&disk, "%s/a2_bload_trace_test_dsk.txt", homedir);
    if (disk) {
        unlink(disk);
        c_begin_disk_trace_6(disk, NULL);
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
    c_end_disk_trace_6();

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(disk, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == EXPECTED_BLOAD_TRACE_DSK_FILE_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = malloc(EXPECTED_BLOAD_TRACE_DSK_FILE_SIZE);
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

    c_eject_6(0);

    PASS();
}

#define EXPECTED_BLOAD_TRACE_NIB_FILE_SIZE 1664090
#define EXPECTED_BLOAD_TRACE_NIB_SHA "09C6C94B3B7E0EFE13A4C74F72A899CE68A9524F"

TEST test_bload_trace_nib() {
    test_setup_boot_disk(BLANK_NIB, 0);
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR]    == 0x00);

    srandom(0);
    const char *homedir = HOMEDIR;
    char *disk = NULL;
    asprintf(&disk, "%s/a2_bload_trace_test_nib.txt", homedir);
    if (disk) {
        unlink(disk);
        c_begin_disk_trace_6(disk, NULL);
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
    c_end_disk_trace_6();

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(disk, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == EXPECTED_BLOAD_TRACE_NIB_FILE_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = malloc(EXPECTED_BLOAD_TRACE_NIB_FILE_SIZE);
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

    c_eject_6(0);

    PASS();
}

#if CONFORMANT_TRACKS
#   define EXPECTED_BLOAD_TRACE_PO_FILE_SIZE 1595253
#   define EXPECTED_BLOAD_TRACE_PO_SHA "8485095E5ED8CEF5D8A177B78A53E3E6A022366D"
#else
#   define EXPECTED_BLOAD_TRACE_PO_FILE_SIZE 1512000
#   define EXPECTED_BLOAD_TRACE_PO_SHA "99B23A0DF5FCAE3ECB494F98ADE938D9FBF9D726"
#endif

TEST test_bload_trace_po() {
    test_setup_boot_disk(BLANK_PO, 0);
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR]    == 0x00);

    srandom(0);
    const char *homedir = HOMEDIR;
    char *disk = NULL;
    asprintf(&disk, "%s/a2_bload_trace_test_po.txt", homedir);
    if (disk) {
        unlink(disk);
        c_begin_disk_trace_6(disk, NULL);
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
    c_end_disk_trace_6();

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(disk, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == EXPECTED_BLOAD_TRACE_PO_FILE_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = malloc(EXPECTED_BLOAD_TRACE_PO_FILE_SIZE);
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

    c_eject_6(0);

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

    c_eject_6(0);

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

    c_eject_6(0);

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

    c_eject_6(0);

    PASS();
}

#define EXPECTED_STABILITY_DSK_FILE_SIZE 143360
#define EXPECTED_STABILITY_DSK_SHA "4DC3AEB266692EB5F8C757F36963F8CCC8056AE4"

TEST test_data_stability_dsk() {

    test_setup_boot_disk(BLANK_DSK, 1);

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(disk6.disk[0].file_name, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == EXPECTED_STABILITY_DSK_FILE_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = malloc(EXPECTED_STABILITY_DSK_FILE_SIZE);
        if (fread(buf, 1, EXPECTED_STABILITY_DSK_FILE_SIZE, fp) != EXPECTED_STABILITY_DSK_FILE_SIZE) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, EXPECTED_STABILITY_DSK_FILE_SIZE, md);
        FREE(buf);

        sha1_to_str(md, mdstr0);
        ASSERT(strcmp(mdstr0, EXPECTED_STABILITY_DSK_SHA) == 0);
    } while(0);

    c_eject_6(0);

    PASS();
}

#define EXPECTED_STABILITY_NIB_FILE_SIZE 232960
#define EXPECTED_STABILITY_NIB_SHA "94193718A6B610AE31B5ABE7058416B321968CA1"

TEST test_data_stability_nib() {

    test_setup_boot_disk(BLANK_NIB, 0);

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(disk6.disk[0].file_name, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == EXPECTED_STABILITY_NIB_FILE_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = malloc(EXPECTED_STABILITY_NIB_FILE_SIZE);
        if (fread(buf, 1, EXPECTED_STABILITY_NIB_FILE_SIZE, fp) != EXPECTED_STABILITY_NIB_FILE_SIZE) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, EXPECTED_STABILITY_NIB_FILE_SIZE, md);
        FREE(buf);

        sha1_to_str(md, mdstr0);
        ASSERT(strcmp(mdstr0, EXPECTED_STABILITY_NIB_SHA) == 0);
    } while(0);

    c_eject_6(0);

    PASS();
}

#define EXPECTED_STABILITY_PO_FILE_SIZE 143360
#define EXPECTED_STABILITY_PO_SHA "9B47A4B92F64ACEB2B82B3B870C78E93780F18F3"

TEST test_data_stability_po() {

    test_setup_boot_disk(BLANK_PO, 0);

    do {
        uint8_t md[SHA_DIGEST_LENGTH];
        char mdstr0[(SHA_DIGEST_LENGTH*2)+1];

        FILE *fp = fopen(disk6.disk[0].file_name, "r");

        fseek(fp, 0, SEEK_END);
        long expectedSize = ftell(fp);
        ASSERT(expectedSize == EXPECTED_STABILITY_PO_FILE_SIZE);
        fseek(fp, 0, SEEK_SET);

        unsigned char *buf = malloc(EXPECTED_STABILITY_PO_FILE_SIZE);
        if (fread(buf, 1, EXPECTED_STABILITY_PO_FILE_SIZE, fp) != EXPECTED_STABILITY_PO_FILE_SIZE) {
            ASSERT(false);
        }
        fclose(fp); fp = NULL;
        SHA1(buf, EXPECTED_STABILITY_PO_FILE_SIZE, md);
        FREE(buf);

        sha1_to_str(md, mdstr0);
        ASSERT(strcmp(mdstr0, EXPECTED_STABILITY_PO_SHA) == 0);
    } while(0);

    c_eject_6(0);

    PASS();
}

// ----------------------------------------------------------------------------
// Test Suite

GREATEST_SUITE(test_suite_disk) {
    GREATEST_SET_SETUP_CB(testdisk_setup, NULL);
    GREATEST_SET_TEARDOWN_CB(testdisk_teardown, NULL);
    GREATEST_SET_BREAKPOINT_CB(test_breakpoint, NULL);

    // TESTS --------------------------
    test_thread_running = true;

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
    //  * load the disks with a buncha junk fiiles
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

    // ...
    c_eject_6(0);
    pthread_mutex_unlock(&interface_mutex);
}

SUITE(test_suite_disk);
GREATEST_MAIN_DEFS();

static char **test_argv = NULL;
static int test_argc = 0;

static void *test_thread(void *dummyptr) {
    int argc = test_argc;
    char **argv = test_argv;
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(test_suite_disk);
    GREATEST_MAIN_END();
    return NULL;
}

void test_disk(int argc, char **argv) {
    test_argc = argc;
    test_argv = argv;

    pthread_mutex_lock(&interface_mutex);

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
    test_disk(argc, argv);
}
#endif
