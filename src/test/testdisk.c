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

#ifdef ANDROID
#   define HOMEDIR data_dir
#else
#   define HOMEDIR getenv("HOME")
#endif

#define ABUSIVE_TESTS 0
#define FINICKY_TESTS 0

#define TESTING_DISK "testvm1.dsk.gz"
#define BLANK_DSK "blank.dsk.gz"
#define BLANK_NIB "blank.nib.gz"
#define BLANK_PO  "blank.po.gz"
#define REBOOT_TO_DOS() \
    do { \
        apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00; \
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

#define EXPECTED_DISK_TRACE_PO_FILE_SIZE 141350
#define EXPECTED_DISK_TRACE_PO_SHA "6C2170D3AA82F87DD34E177309808199BDCCB018"
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
    c_eject_6(0);

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
    c_eject_6(0);

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
    c_eject_6(0);

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
    c_eject_6(0);

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
    c_eject_6(0);

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
    c_eject_6(0);

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
    c_eject_6(0);

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

TEST test_boot_disk() {
    BOOT_TO_DOS();
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
    TYPE_TRIGGER_WATCHPT();

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
    TYPE_TRIGGER_WATCHPT();

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
    TYPE_TRIGGER_WATCHPT();

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

#define EXPECTED_DISKWRITE_TRACE_DSK_FILE_SIZE 63675
#define EXPECTED_DISKWRITE_TRACE_DSK_SHA "CFA1C3AB2CA4F245D291DFC8C277773C5275946C"
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
    TYPE_TRIGGER_WATCHPT();

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

    PASS();
}

#define EXPECTED_DISKWRITE_TRACE_NIB_FILE_SIZE 2409
#define EXPECTED_DISKWRITE_TRACE_NIB_SHA "332EA76D8BCE45ACA6F805B978E6A3327386ABD6"
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
    TYPE_TRIGGER_WATCHPT();

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

    PASS();
}

#define EXPECTED_DISKWRITE_TRACE_PO_FILE_SIZE 63675
#define EXPECTED_DISKWRITE_TRACE_PO_SHA "CFA1C3AB2CA4F245D291DFC8C277773C5275946C"
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
    TYPE_TRIGGER_WATCHPT();

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
TEST test_outofspace_dsk() {
    test_setup_boot_disk(BLANK_DSK, 0);
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR]    == 0x00);

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    EAT_UP_DISK_SPACE();
    TYPE_TRIGGER_WATCHPT();

    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA(NOSPACE_SHA1);

    REBOOT_TO_DOS();
    c_debugger_go();
    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA(BOOT_SCREEN);

    c_eject_6(0);

    PASS();
}

TEST test_outofspace_nib() {
    test_setup_boot_disk(BLANK_NIB, 0);
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR]    == 0x00);

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    EAT_UP_DISK_SPACE();
    TYPE_TRIGGER_WATCHPT();

    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA(NOSPACE_SHA1);

    REBOOT_TO_DOS();
    c_debugger_go();
    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA(BOOT_SCREEN);

    c_eject_6(0);

    PASS();
}

TEST test_outofspace_po() {
    test_setup_boot_disk(BLANK_PO, 0);
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR]    == 0x00);

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    EAT_UP_DISK_SPACE();
    TYPE_TRIGGER_WATCHPT();

    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA(NOSPACE_SHA1);

    REBOOT_TO_DOS();
    c_debugger_go();
    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT_SHA(BOOT_SCREEN);

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
    TYPE_TRIGGER_WATCHPT();

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
    TYPE_TRIGGER_WATCHPT();

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
    TYPE_TRIGGER_WATCHPT();

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

// ----------------------------------------------------------------------------
// Test Suite

static int begin_video = -1;

GREATEST_SUITE(test_suite_disk) {
    GREATEST_SET_SETUP_CB(testdisk_setup, NULL);
    GREATEST_SET_TEARDOWN_CB(testdisk_teardown, NULL);
    GREATEST_SET_BREAKPOINT_CB(test_breakpoint, NULL);

    // TESTS --------------------------
    begin_video=!is_headless;

    RUN_TESTp(test_boot_disk_bytes);
    RUN_TESTp(test_boot_disk_bytes_nib);
    RUN_TESTp(test_boot_disk_bytes_po);

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

    RUN_TESTp(test_boot_disk);

    RUN_TESTp(test_savehello_dsk);
    RUN_TESTp(test_savehello_nib);
    RUN_TESTp(test_savehello_po);

    RUN_TESTp(test_disk_bytes_savehello_dsk);
    RUN_TESTp(test_disk_bytes_savehello_nib);
    RUN_TESTp(test_disk_bytes_savehello_po);

#ifndef ANDROID
    c_debugger_set_timeout(60);
#endif

    RUN_TESTp(test_outofspace_dsk);
    RUN_TESTp(test_outofspace_nib);
    RUN_TESTp(test_outofspace_po);

    RUN_TESTp(test_inithello_dsk);
    RUN_TESTp(test_inithello_nib);
    RUN_TESTp(test_inithello_po);

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
    //pthread_join(p, NULL);
}

#if !defined(__APPLE__)
int main(int argc, char **argv) {
    test_disk(argc, argv);
}
#endif
