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

#ifdef HAVE_OPENSSL
#include <openssl/sha.h>
#else
#error "these tests require OpenSSL libraries (SHA)"
#endif

static bool test_do_reboot = true;

extern unsigned char joy_button0;

static void testvm_setup(void *arg) {
    RESET_INPUT();
    apple_ii_64k[0][MIXSWITCH_ADDR] = 0x00;
    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    apple_ii_64k[0][TESTOUT_ADDR] = 0x00;
    joy_button0 = 0xff; // OpenApple
    if (test_do_reboot) {
        cpu65_interrupt(ResetSig);
    }
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
// VM TESTS ...

TEST test_boot_disk() {
    char *disk = strdup("./disks/testvm1.dsk.gz");
    if (c_new_diskette_6(0, disk, 0)) {
        int len = strlen(disk);
        disk[len-3] = '\0';
        ASSERT(!c_new_diskette_6(0, disk, 0));
    }
    free(disk);

    BOOT_TO_DOS();

    PASS();
}

TEST test_read_keyboard() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR]    == 0x00);

    test_type_input("RUN TESTGETKEY\rZ");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 'Z');

    PASS();
}

TEST test_clear_keyboard() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR]    == 0x00);

    test_type_input("RUN TESTCLEARKEY\rZA");
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

    test_type_input("RUN TESTRND\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 0xFF);

    PASS();
}

// ----------------------------------------------------------------------------
// Softswitch tests

#define ASM_INIT() \
    test_type_input( \
            "CALL-151\r" \
            "!\r" \
            "1E00: NOP\r" \
            )

#define ASM_INIT() \
    test_type_input( \
            "CALL-151\r" \
            "!\r" \
            "1E00: NOP\r" \
            )

#define ASM_BEGIN() test_type_input("!\r")

#define ASM_TRIGGER_WATCHPT() \
    test_type_input( \
            " LDA #FF\r" \
            " STA $1F33\r" \
            )

#define ASM_XFER_TEST_TO_AUXMEM() \
    test_type_input( \
            "!\r" \
            "1E80: NOP\r" \
            " LDA #$00\r" \
            " STA $3C\r" \
            " LDA #$1E\r" \
            " STA $3D\r" \
            " LDA #$FF\r" \
            " STA $3E\r" \
            " LDA #$1E\r" \
            " STA $3F\r" \
            " LDA #$00\r" \
            " STA $42\r" \
            " LDA #$1E\r" \
            " STA $43\r" \
            " SEC\r"                  /* MAIN 1E00..1EFF -> AUX 1E00 */ \
            " JSR $C311\r"            /* call AUXMOVE */ \
            " RTS\r" \
            "\r" \
            "1E80G\r" \
            )

#define ASM_DONE() test_type_input("\r")

#define ASM_GO() test_type_input("1E00G\r")

#define TYPE_80STORE_OFF() \
    test_type_input("POKE49152,0:REM C000 80STORE OFF\r")

#define TYPE_80STORE_ON() \
    test_type_input("POKE49153,0:REM C001 80STORE ON\r")

#define ASM_RAMRD_OFF() \
    test_type_input(" STA $C002\r")

#define ASM_RAMRD_ON() \
    test_type_input(" STA $C003\r")

#define ASM_RAMWRT_OFF() \
    test_type_input(" STA $C004\r")

#define ASM_RAMWRT_ON() \
    test_type_input(" STA $C005\r")

#define TYPE_TEXT_OFF() \
    test_type_input("POKE49232,0:REM C050 TEXT OFF\r")

#define TYPE_TEXT_ON() \
    test_type_input("POKE49233,0:REM C051 TEXT ON\r")

#define TYPE_MIXED_OFF() \
    test_type_input("POKE49234,0:REM C052 MIXED OFF\r")

#define TYPE_MIXED_ON() \
    test_type_input("POKE49235,0:REM C053 MIXED ON\r")

#define TYPE_PAGE2_OFF() \
    test_type_input("POKE49236,0:REM C054 PAGE2 OFF\r")

#define TYPE_PAGE2_ON() \
    test_type_input("POKE49237,0:REM C055 PAGE2 ON\r")

#define ASM_HIRES_OFF() \
    test_type_input(" STA $C056\r")

#define ASM_HIRES_ON() \
    test_type_input(" STA $C057\r")

#define TYPE_HIRES_OFF() \
    test_type_input("POKE49238,0:REM C056 HIRES OFF\r")

#define TYPE_HIRES_ON() \
    test_type_input("POKE49239,0:REM C057 HIRES ON\r")

#define TYPE_TRIGGER_WATCHPT() \
    test_type_input("POKE7987,255:REM TRIGGER DEBUGGER\r")

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

TEST test_TEXT_on() {
    BOOT_TO_DOS();

    // setup for testing ...

    TYPE_TEXT_OFF();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    TYPE_TRIGGER_WATCHPT();

    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(!(softswitches & SS_TEXT));

    uint32_t ss_save = softswitches;

    // run actual test ...

    RESET_INPUT();
    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    TYPE_TEXT_ON();
    TYPE_TRIGGER_WATCHPT();

    uint8_t *save_base_textrd = base_textrd;
    uint8_t *save_base_textwrt = base_textwrt;
    uint8_t *save_base_hgrrd = base_hgrrd;
    uint8_t *save_base_hgrwrt = base_hgrwrt;
    int save_current_page = video__current_page;

    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    ASSERT((softswitches & SS_TEXT));
    ASSERT(video__current_page == save_current_page);

    ss_save = (ss_save | SS_TEXT);

    ASSERT(base_textrd  == save_base_textrd);  // unchanged
    ASSERT(base_textwrt == save_base_textwrt); // unchanged
    ASSERT(base_hgrrd  == save_base_hgrrd);    // unchanged
    ASSERT(base_hgrwrt == save_base_hgrwrt);   // unchanged

    ASSERT((softswitches == ss_save));

    PASS();
}

TEST test_TEXT_off() {
    BOOT_TO_DOS();

    // setup for testing ...

    TYPE_TEXT_ON();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    TYPE_TRIGGER_WATCHPT();

    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT((softswitches & SS_TEXT));

    uint32_t ss_save = softswitches;

    // run actual test ...

    RESET_INPUT();
    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    TYPE_TEXT_OFF();
    TYPE_TRIGGER_WATCHPT();

    uint8_t *save_base_textrd = base_textrd;
    uint8_t *save_base_textwrt = base_textwrt;
    uint8_t *save_base_hgrrd = base_hgrrd;
    uint8_t *save_base_hgrwrt = base_hgrwrt;
    int save_current_page = video__current_page;

    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    ASSERT(!(softswitches & SS_TEXT));
    ASSERT(video__current_page == save_current_page);

    ss_save = (ss_save & ~SS_TEXT);

    ASSERT(base_textrd  == save_base_textrd);  // unchanged
    ASSERT(base_textwrt == save_base_textwrt); // unchanged
    ASSERT(base_hgrrd  == save_base_hgrrd);    // unchanged
    ASSERT(base_hgrwrt == save_base_hgrwrt);   // unchanged

    ASSERT((softswitches == ss_save));

    PASS();
}

TEST test_MIXED_on() {
    BOOT_TO_DOS();

    // setup for testing ...

    TYPE_MIXED_OFF();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    TYPE_TRIGGER_WATCHPT();

    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(!(softswitches & SS_MIXED));

    uint32_t ss_save = softswitches;

    // run actual test ...

    RESET_INPUT();
    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    TYPE_MIXED_ON();
    TYPE_TRIGGER_WATCHPT();

    uint8_t *save_base_textrd = base_textrd;
    uint8_t *save_base_textwrt = base_textwrt;
    uint8_t *save_base_hgrrd = base_hgrrd;
    uint8_t *save_base_hgrwrt = base_hgrwrt;
    int save_current_page = video__current_page;

    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    ASSERT((softswitches & SS_MIXED));
    ASSERT(video__current_page == save_current_page);

    ss_save = (ss_save | SS_MIXED);

    ASSERT(base_textrd  == save_base_textrd);  // unchanged
    ASSERT(base_textwrt == save_base_textwrt); // unchanged
    ASSERT(base_hgrrd  == save_base_hgrrd);    // unchanged
    ASSERT(base_hgrwrt == save_base_hgrwrt);   // unchanged

    ASSERT((softswitches == ss_save));

    PASS();
}

TEST test_MIXED_off() {
    BOOT_TO_DOS();

    // setup for testing ...

    TYPE_MIXED_ON();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    TYPE_TRIGGER_WATCHPT();

    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT((softswitches & SS_MIXED));

    uint32_t ss_save = softswitches;

    // run actual test ...

    RESET_INPUT();
    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    TYPE_MIXED_OFF();
    TYPE_TRIGGER_WATCHPT();

    uint8_t *save_base_textrd = base_textrd;
    uint8_t *save_base_textwrt = base_textwrt;
    uint8_t *save_base_hgrrd = base_hgrrd;
    uint8_t *save_base_hgrwrt = base_hgrwrt;
    int save_current_page = video__current_page;

    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    ASSERT(!(softswitches & SS_MIXED));
    ASSERT(video__current_page == save_current_page);

    ss_save = (ss_save & ~SS_MIXED);

    ASSERT(base_textrd  == save_base_textrd);  // unchanged
    ASSERT(base_textwrt == save_base_textwrt); // unchanged
    ASSERT(base_hgrrd  == save_base_hgrrd);    // unchanged
    ASSERT(base_hgrwrt == save_base_hgrwrt);   // unchanged

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

    RUN_TESTp(test_TEXT_on);
    RUN_TESTp(test_TEXT_off);

    RUN_TESTp(test_MIXED_on);
    RUN_TESTp(test_MIXED_off);

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

