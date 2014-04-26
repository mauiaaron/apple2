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

#define RESET_INPUT() \
    input_counter = 0; \
    input_length = 0; \
    input_str[0] = '\0'

#ifdef HAVE_OPENSSL
#include <openssl/sha.h>
#else
#error "these tests require OpenSSL libraries (SHA)"
#endif

static char input_str[TESTBUF_SZ]; // ASCII
static unsigned int input_length = 0;
static unsigned int input_counter = 0;
static bool test_do_reboot = true;

static struct timespec t0 = { 0 };
static struct timespec ti = { 0 };

extern unsigned char joy_button0;

static void testvm_setup(void *arg) {
    apple_ii_64k[0][MIXSWITCH_ADDR] = 0x00;
    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    apple_ii_64k[0][TESTOUT_ADDR] = 0x00;
    RESET_INPUT();
    joy_button0 = 0xff; // OpenApple
    if (test_do_reboot) {
        cpu65_interrupt(ResetSig);
    }
    clock_gettime(CLOCK_MONOTONIC, &t0);
}

static void testvm_teardown(void *arg) {
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

void testing_video_sync() {

#if !HEADLESS
    if (!is_headless) {
        clock_gettime(CLOCK_MONOTONIC, &ti);
        struct timespec deltat = timespec_diff(t0, ti, NULL);
        if (deltat.tv_sec || (deltat.tv_nsec >= NANOSECONDS/15) ) {
            video_sync(0);
            ti = t0;
        }
    }
#endif

    if (!input_length) {
        input_length = strlen(input_str);
    }

    if (input_counter >= input_length) {
        return;
    }

    uint8_t ch = (uint8_t)input_str[input_counter];
    if (ch == '\n') {
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
// VM TESTS ...

TEST test_boot_disk() {
    char *disk = "./disks/testvm1.dsk.gz";
    ASSERT(!c_new_diskette_6(0, disk, 0));

    BOOT_TO_DOS();

    PASS();
}

TEST test_read_keyboard() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR]    == 0x00);

    strcpy(input_str, "RUN TESTGETKEY\rZ");
    input_length = strlen(input_str);
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 'Z');

    PASS();
}

TEST test_clear_keyboard() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR]    == 0x00);

    strcpy(input_str, "RUN TESTCLEARKEY\rZA");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 'A');

    PASS();
}

TEST test_read_random() {
    SKIPm("random numbers currently b0rken...");

    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR]    == 0x00);

    strcpy(input_str, "RUN TESTRND\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 0xFF);

    PASS();
}

// ----------------------------------------------------------------------------
// Softswitch tests

#define TYPE_PAGE2_OFF() \
    strcat(input_str, "POKE49236,0\r") /* C054 */

#define TYPE_PAGE2_ON() \
    strcat(input_str, "POKE49237,0\r") /* C055 */

#define TYPE_80STORE_OFF() \
    strcat(input_str, "POKE49152,0\r") /* C000 */

#define TYPE_80STORE_ON() \
    strcat(input_str, "POKE49153,0\r") /* C001 */

#define TYPE_HIRES_OFF() \
    strcat(input_str, "POKE49238,0\r") /* C056 */

#define TYPE_HIRES_ON() \
    strcat(input_str, "POKE49239,0\r") /* C057 */

#define TYPE_TRIGGER_WATCHPT() \
    strcat(input_str, "POKE7987,255\r")

TEST test_PAGE2_on(bool ss_80store, bool ss_hires) {
    BOOT_TO_DOS();

    // setup for testing ...

    TYPE_PAGE2_OFF();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    if (ss_80store) {
        TYPE_80STORE_ON();
    } else {
        TYPE_80STORE_OFF();
    }

    if (ss_hires) {
        TYPE_HIRES_ON();
    } else {
        TYPE_HIRES_OFF();
    }

    TYPE_TRIGGER_WATCHPT();

    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(!(softswitches & SS_PAGE2));
    ASSERT( (ss_80store ? (softswitches & SS_80STORE) : !(softswitches & SS_80STORE)) );
    ASSERT( (ss_hires   ? (softswitches & SS_HIRES)   : !(softswitches & SS_HIRES)) );

    uint32_t ss_save = softswitches;

    // run actual test ...

    RESET_INPUT();
    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    TYPE_PAGE2_ON();
    TYPE_TRIGGER_WATCHPT();

    uint8_t *save_base_textrd = base_textrd;
    uint8_t *save_base_textwrt = base_textwrt;
    uint8_t *save_base_hgrrd = base_hgrrd;
    uint8_t *save_base_hgrwrt = base_hgrwrt;

    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT((softswitches & SS_PAGE2));
    ASSERT( (ss_80store ? (softswitches & SS_80STORE) : !(softswitches & SS_80STORE)) );
    ASSERT( (ss_hires   ? (softswitches & SS_HIRES)   : !(softswitches & SS_HIRES)) );

    ss_save = ss_save | SS_PAGE2;

    if (ss_80store) {
        ASSERT(video__current_page == 0);
        ss_save = ss_save | (SS_TEXTRD|SS_TEXTWRT);
        ASSERT((void *)base_textrd  == (void *)(apple_ii_64k[1]));
        ASSERT((void *)base_textwrt == (void *)(apple_ii_64k[1]));
        if (ss_hires) {
            ss_save = ss_save | (SS_HGRRD|SS_HGRWRT);
            ASSERT((void *)base_hgrrd  == (void *)(apple_ii_64k[1]));
            ASSERT((void *)base_hgrwrt == (void *)(apple_ii_64k[1]));
        } else {
            ASSERT(base_hgrrd  == save_base_hgrrd);  // unchanged
            ASSERT(base_hgrwrt == save_base_hgrwrt); // unchanged
        }
    } else {
        ss_save = (ss_save | SS_SCREEN);
        ASSERT(video__current_page = 1);
        ASSERT(base_textrd  == save_base_textrd);  // unchanged
        ASSERT(base_textwrt == save_base_textwrt); // unchanged
        ASSERT(base_hgrrd  == save_base_hgrrd);    // unchanged
        ASSERT(base_hgrwrt == save_base_hgrwrt);   // unchanged
    }

    ASSERT((softswitches == ss_save));

    PASS();
}

TEST test_PAGE2_off(bool ss_80store, bool ss_hires) {
    BOOT_TO_DOS();

    // setup for testing ...

    TYPE_PAGE2_ON();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    if (ss_80store) {
        TYPE_80STORE_ON();
    } else {
        TYPE_80STORE_OFF();
    }

    if (ss_hires) {
        TYPE_HIRES_ON();
    } else {
        TYPE_HIRES_OFF();
    }

    TYPE_TRIGGER_WATCHPT();

    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT( (softswitches & SS_PAGE2));
    ASSERT( (ss_80store ? (softswitches & SS_80STORE) : !(softswitches & SS_80STORE)) );
    ASSERT( (ss_hires   ? (softswitches & SS_HIRES)   : !(softswitches & SS_HIRES)) );

    uint32_t ss_save = softswitches;

    // run actual test ...

    RESET_INPUT();
    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    TYPE_PAGE2_OFF();
    TYPE_TRIGGER_WATCHPT();

    uint8_t *save_base_textrd = base_textrd;
    uint8_t *save_base_textwrt = base_textwrt;
    uint8_t *save_base_hgrrd = base_hgrrd;
    uint8_t *save_base_hgrwrt = base_hgrwrt;

    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT( (ss_80store ? (softswitches & SS_80STORE) : !(softswitches & SS_80STORE)) );
    ASSERT( (ss_hires   ? (softswitches & SS_HIRES)   : !(softswitches & SS_HIRES)) );

    ASSERT(!(softswitches & (SS_PAGE2|SS_SCREEN)));
    ASSERT(video__current_page == 0);

    ss_save = (ss_save & ~(SS_PAGE2|SS_SCREEN));
    if (ss_80store) {
        ss_save = ss_save & ~(SS_TEXTRD|SS_TEXTWRT);
        ASSERT((void *)base_textrd  == (void *)(apple_ii_64k));
        ASSERT((void *)base_textwrt == (void *)(apple_ii_64k));
        if (ss_hires) {
            ss_save = ss_save & ~(SS_HGRRD|SS_HGRWRT);
            ASSERT((void *)base_hgrrd  == (void *)(apple_ii_64k));
            ASSERT((void *)base_hgrwrt == (void *)(apple_ii_64k));
        } else {
            ASSERT(base_hgrrd  == save_base_hgrrd);    // unchanged
            ASSERT(base_hgrwrt == save_base_hgrwrt);   // unchanged
        }
    } else {
        ASSERT(base_textrd  == save_base_textrd);  // unchanged
        ASSERT(base_textwrt == save_base_textwrt); // unchanged
        ASSERT(base_hgrrd  == save_base_hgrrd);    // unchanged
        ASSERT(base_hgrwrt == save_base_hgrwrt);   // unchanged
    }

    ASSERT((softswitches == ss_save));

    PASS();
}

// ----------------------------------------------------------------------------
// Test Suite

extern void cpu_thread(void *dummyptr);

GREATEST_SUITE(test_suite_vm) {

    GREATEST_SET_SETUP_CB(testvm_setup, NULL);
    GREATEST_SET_TEARDOWN_CB(testvm_teardown, NULL);

    c_read_random();
    srandom(0); // force a known sequence

    test_common_init(/*cputhread*/true);

    pthread_mutex_lock(&interface_mutex);

    // TESTS --------------------------

    RUN_TESTp(test_boot_disk);

    RUN_TESTp(test_read_keyboard);

    RUN_TESTp(test_clear_keyboard);

    RUN_TESTp(test_read_random);

    RUN_TESTp(test_PAGE2_on,  /*80STORE*/0, /*HIRES*/0);
    RUN_TESTp(test_PAGE2_on,  /*80STORE*/0, /*HIRES*/1);
    RUN_TESTp(test_PAGE2_on,  /*80STORE*/1, /*HIRES*/0);
    RUN_TESTp(test_PAGE2_on,  /*80STORE*/1, /*HIRES*/1);

    RUN_TESTp(test_PAGE2_off, /*80STORE*/0, /*HIRES*/0);
    RUN_TESTp(test_PAGE2_off, /*80STORE*/0, /*HIRES*/1);
    RUN_TESTp(test_PAGE2_off, /*80STORE*/1, /*HIRES*/0);
    RUN_TESTp(test_PAGE2_off, /*80STORE*/1, /*HIRES*/1);

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

