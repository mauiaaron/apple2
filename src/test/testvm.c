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

#include "greatest.h"
#include "testcommon.h"

#ifdef HAVE_OPENSSL
#include <openssl/sha.h>
#else
#error "these tests require OpenSSL libraries"
#endif

#define TEST_FINISHED 0xff
#define MIXSWITCH_ADDR 0x1f32   // PEEK(7986)
#define WATCHPOINT_ADDR 0x1f33  // PEEK(7987)

static char *input_str = NULL; // ASCII
static unsigned int input_length = 0;
static unsigned int input_counter = 0;
static bool test_do_reboot = true;

extern unsigned char joy_button0;

static void testvm_setup(void *arg) {
    apple_ii_64k[0][MIXSWITCH_ADDR] = 0x00;
    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    input_counter = 0;
    input_length = 0;
    joy_button0 = 0xff; // OpenApple
    if (test_do_reboot) {
        cpu65_interrupt(ResetSig);
    }
}

static void testvm_teardown(void *arg) {
    if (input_str) {
        free(input_str);
    }
    input_str = NULL;
}

static void testvm_breakpoint(void *arg) {
    fprintf(GREATEST_STDOUT, "set breakpoint on testvm_breakpoint to check for problems...\n");
}

static void sha1_to_str(const uint8_t * const md, char *buf) {
    int i=0;
    for (int j=0; j<SHA_DIGEST_LENGTH; j++, i+=2) {
        sprintf(buf+i, "%02X", md[j]);
    }
    sprintf(buf+i, "%c", '\0');
}

// ----------------------------------------------------------------------------
// test video functions and stubs

void video_sync(int ignored) {

    if (input_counter >= input_length) {
        return;
    }

    uint8_t ch = (uint8_t)input_str[input_counter];
    if (ch == '\n') {
        fprintf(stderr, "converting '\\n' to '\\r' in test input string...");
        ch = '\r';
    }

    if ( (apple_ii_64k[0][0xC000] & 0x80) || (apple_ii_64k[1][0xC000] & 0x80) ) {
        // last character typed not processed by emulator...
        return;
    }

    apple_ii_64k[0][0xC000] = ch | 0x80;
    apple_ii_64k[1][0xC000] = ch | 0x80;

    ++input_counter;
}

// ----------------------------------------------------------------------------
// Stub functions because I've reached diminishing returns with the build system ...
//
// NOTE: You'd think the commandline CFLAGS set specifically for this test program would pass down to the sources in
// subdirectories, but it apparently isn't.  GNU buildsystem bug?  Also see HACK FIXME TODO NOTE in Makefile.am
//

uint8_t c_MB_Read(uint16_t addr) {
    return 0x0;
}

void c_MB_Write(uint16_t addr, uint8_t byte) {
}

uint8_t c_PhasorIO(uint16_t addr) {
    return 0x0;
}

void SpkrToggle() {
}

void c_interface_print(int x, int y, const int cs, const char *s) {
}

// ----------------------------------------------------------------------------
// VM TESTS ...

#define DO_SHA(SHA_STR) \
    uint8_t md[SHA_DIGEST_LENGTH]; \
    char mdstr[(SHA_DIGEST_LENGTH*2)+1]; \
    const uint8_t * const fb = video_current_framebuffer(); \
    SHA1(fb, SCANWIDTH*SCANHEIGHT, md); \
    sha1_to_str(md, mdstr);

#define BOOT_TO_DOS() \
    if (test_do_reboot) { \
        ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED); \
        c_debugger_go(); \
        if (apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED) { \
            DO_SHA(); \
            if (strcmp(mdstr, "F8D6C781E0BB7B3DDBECD69B25E429D845506594") != 0) { \
                FAILm("expected boot framebuffer not found"); \
            } \
        } else { \
            FAILm("boot watchpoint not triggered"); \
        } \
        apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00; \
    }


TEST test_boot_disk() {
    char *disk = "./disks/testvm1.dsk.gz";
    if (c_new_diskette_6(0, disk, 0)) {
        FAILm("Cannot insert test diskette!");
    }

    BOOT_TO_DOS();

    PASS();
}

// ----------------------------------------------------------------------------
// TEXT

TEST test_40col_normal() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    input_str = strdup("RUN TESTNORMALTEXT\r");
    input_length = strlen(input_str);
    c_debugger_go();
    if (apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED) {
        DO_SHA();
        if (strcmp(mdstr, "51E5960115380C64351ED00A2ACAB0EB67970249") == 0) {
            PASS();
        }
    }

    FAILm("test fail");
}

TEST test_80col_normal() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    input_str = strdup("PR#3\rRUN TESTNORMALTEXT\r");
    input_length = strlen(input_str);
    c_debugger_go();
    if (apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED) {
        DO_SHA();
        if (strcmp(mdstr, "ED9CE59F41A51A5ABB1617383A411455677A78E3") == 0) {
            PASS();
        }
    }

    FAILm("test fail");
}

TEST test_40col_inverse() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    input_str = strdup("RUN TESTINVERSETEXT\r");
    input_length = strlen(input_str);
    c_debugger_go();
    if (apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED) {
        DO_SHA();
        if (strcmp(mdstr, "20957B960C3C0DE0ABEE0058A08C0DDA24AB31D8") == 0) {
            PASS();
        }
    }

    FAILm("test fail");
}

TEST test_80col_inverse() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    input_str = strdup("PR#3\rRUN TESTINVERSETEXT\r");
    input_length = strlen(input_str);
    c_debugger_go();
    if (apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED) {
        DO_SHA();
        if (strcmp(mdstr, "037798F4BCF740D0A7CBF7CDF5FC5D1B0B3C77A2") == 0) {
            PASS();
        }
    }

    FAILm("test fail");
}

// ----------------------------------------------------------------------------
// HIRES
//
// NOTE : These tests assume standard color mode (not b/w or interpolated)
//

TEST test_hires_with_80col() {
    BOOT_TO_DOS();

    SKIPm("hires with 80col appears to be b0rken");

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    input_str = strdup("PR#3\rRUN TESTHIRES_2\r");
    input_length = strlen(input_str);
    c_debugger_go();
    if (apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED) {
        DO_SHA();
        if (strcmp(mdstr, "8EF89A5E0501191847E9907416309B33D4B48713") == 0) {
            PASS();
        }
    }

    FAILm("test fail");
}

TEST test_hires_with_40col() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    input_str = strdup("RUN TESTHIRES_2\r");
    input_length = strlen(input_str);
    c_debugger_go();
    if (apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED) {
        DO_SHA();
        if (strcmp(mdstr, "1A5DD96B7E3538C2C3625A37653E013E3998F825") == 0) {
            PASS();
        }
    }

    FAILm("test fail");
}

TEST test_hires_40colmix_normal() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    input_str = strdup("POKE 7986,127\rRUN TESTHIRES\r");
    input_length = strlen(input_str);
    c_debugger_go();
    if (apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED) {
        DO_SHA();
        if (strcmp(mdstr, "37F41F74EB23F8812498F732E6DA34A0EBC4D68A") == 0) {
            PASS();
        }
    }

    FAILm("test fail");
}

TEST test_hires_40colmix_inverse() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    input_str = strdup("INVERSE\rPOKE 7986,127\rRUN TESTHIRES\r");
    input_length = strlen(input_str);
    c_debugger_go();
    if (apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED) {
        DO_SHA();
        if (strcmp(mdstr, "253D1823F5DAC0300B46B3D49C04CD59CC70076F") == 0) {
            PASS();
        }
    }

    FAILm("test fail");
}

TEST test_hires_80colmix_normal() {
    BOOT_TO_DOS();

    SKIPm("hires with 80col appears to be b0rken");

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    input_str = strdup("PR#3\rLOAD TESTHIRES\rLIST\rLIST\rPOKE 7986,127\rRUN\r");
    input_length = strlen(input_str);
    c_debugger_go();
    if (apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED) {
        DO_SHA();
        if (strcmp(mdstr, "032BD68899749265EB2934A76A35D7068642824B") == 0) {
            PASS();
        }
    }

    FAILm("test fail");
}

TEST test_hires_80colmix_inverse() {
    BOOT_TO_DOS();

    SKIPm("hires with 80col appears to be b0rken");

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    input_str = strdup("PR#3\rLOAD TESTHIRES\rLIST\rLIST\rPOKE 7986,127\rINVERSE\rRUN\r");
    input_length = strlen(input_str);
    c_debugger_go();
    if (apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED) {
        DO_SHA();
        if (strcmp(mdstr, "253D1823F5DAC0300B46B3D49C04CD59CC70076F") == 0) {
            PASS();
        }
    }

    FAILm("test fail");
}

TEST test_80col_lores() {
    BOOT_TO_DOS();

    // this graphic looks wrong compared to the manual ...
    SKIPm("mode needs to be properly implemented");
}

TEST test_80col_hires() {
    BOOT_TO_DOS();

    // double-hires graphics are in-flux ...
    SKIPm("mode needs to be properly implemented");
}

// ----------------------------------------------------------------------------
// Test Suite

extern void cpu_thread(void *dummyptr);

GREATEST_SUITE(test_suite_vm) {

    srandom(time(NULL));

    GREATEST_SET_SETUP_CB(testvm_setup, NULL);
    GREATEST_SET_TEARDOWN_CB(testvm_teardown, NULL);
    GREATEST_SET_BREAKPOINT_CB(testvm_breakpoint, NULL);

    do_logging = false;// silence regular emulator logging
    setenv("APPLE2IXCFG", "nosuchconfigfile", 1);

    load_settings();
    c_initialize_firsttime();

    // kludgey set max CPU speed... 
    cpu_scale_factor = CPU_SCALE_FASTEST;
    cpu_altscale_factor = CPU_SCALE_FASTEST;
    g_bFullSpeed = true;

    caps_lock = true;

    // spin off cpu thread
    pthread_create(&cpu_thread_id, NULL, (void *) &cpu_thread, (void *)NULL);

    pthread_mutex_lock(&interface_mutex);

    c_debugger_set_watchpoint(WATCHPOINT_ADDR);
    c_debugger_set_timeout(5);

    // TESTS --------------------------

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

//    RUN_TEST(test_lores);
//
//    RUN_TEST(test_lores_40colmix_normal);
//
//    RUN_TEST(test_lores_40colmix_inverse);
//
//    RUN_TEST(test_lores_80colmix_normal);
//
//    RUN_TEST(test_lores_80colmix_inverse);

    // hires

    // HACK FIXME TODO : there appears to be bugs with various 80col graphics modes ...

    RUN_TESTp(test_hires_with_80col);
    test_do_reboot = false;
    RUN_TESTp(test_hires_with_80col);
    test_do_reboot = true;

    RUN_TEST(test_hires_with_40col);
    test_do_reboot = false;
    RUN_TEST(test_hires_with_40col);
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
    test_do_reboot = false;
    RUN_TEST(test_80col_lores);
    test_do_reboot = true;

    RUN_TEST(test_80col_hires);
    test_do_reboot = false;
    RUN_TEST(test_80col_hires);
    test_do_reboot = true;

    // ...
    c_eject_6(0);
    pthread_mutex_unlock(&interface_mutex);
}

SUITE(test_suite_vm);
GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(test_suite_vm);
    GREATEST_MAIN_END();
}

