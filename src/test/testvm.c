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

static bool test_thread_running = false;

extern pthread_mutex_t interface_mutex; // TODO FIXME : raw access to CPU mutex because stepping debugger ...

static void testvm_setup(void *arg) {
    test_common_setup();
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

// ----------------------------------------------------------------------------
// VM TESTS ...

TEST test_boot_disk() {
    test_setup_boot_disk("testvm1.nib.gz", 1);

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

    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR]    == 0x00);

    test_type_input("RUN TESTRND\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 249);

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    test_common_setup();
    test_type_input("RUN\r");
    c_debugger_go();
    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 26);

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    test_common_setup();
    test_type_input("RUN\r");
    c_debugger_go();
    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 4);

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    test_common_setup();
    test_type_input("RUN\r");
    c_debugger_go();
    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 199);

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    test_common_setup();
    test_type_input("RUN\r");
    c_debugger_go();
    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 141);

    PASS();
}

#if 0
#error this is an unstable test due to VBL refactoring ...
TEST test_read_random2() {
#ifdef __APPLE__
#warning "ignoring random test on Darwin..."
#else

    srandom(0); // force a known sequence
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR]    == 0x00);

    test_type_input("RUN TESTRND2\r");
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 103);

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    test_common_setup();
    test_type_input("RUN\r");
    c_debugger_go();
    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 198);

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    test_common_setup();
    test_type_input("RUN\r");
    c_debugger_go();
    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 105);

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    test_common_setup();
    test_type_input("RUN\r");
    c_debugger_go();
    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 115);

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x0;
    test_common_setup();
    test_type_input("RUN\r");
    c_debugger_go();
    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 81);
#endif

    PASS();
}
#endif

// ----------------------------------------------------------------------------
// Softswitch tests

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

#define ASM_80STORE_OFF() \
    test_type_input(" STA $C000\r")

#define ASM_80STORE_ON() \
    test_type_input(" STA $C001\r")

#define ASM_CHECK_80STORE() \
    test_type_input( \
            " LDA $C018\r" \
            " STA $1F43\r" \
            )

#define ASM_RAMRD_OFF() \
    test_type_input(" STA $C002\r")

#define ASM_RAMRD_MAIN() ASM_RAMRD_OFF()

#define ASM_RAMRD_ON() \
    test_type_input(" STA $C003\r")

#define ASM_RAMRD_AUX() ASM_RAMRD_ON()

#define ASM_CHECK_RAMRD() \
    test_type_input( \
            " LDA $C013\r" \
            " STA $1F43\r" \
            )

#define ASM_RAMWRT_OFF() \
    test_type_input(" STA $C004\r")

#define ASM_RAMWRT_MAIN() ASM_RAMWRT_OFF()

#define ASM_RAMWRT_ON() \
    test_type_input(" STA $C005\r")

#define ASM_RAMWRT_AUX() ASM_RAMWRT_ON()

#define ASM_CHECK_RAMWRT() \
    test_type_input( \
            " LDA $C014\r" \
            " STA $1F43\r" \
            )

#define ASM_CXROM_INTERNAL() \
    test_type_input(" STA $C007\r")

#define ASM_CXROM_PERIPHERAL() \
    test_type_input(" STA $C006\r")

#define ASM_CHECK_CXROM() \
    test_type_input( \
            " LDA $C015\r" \
            " STA $1F43\r" \
            )

#define ASM_ALTZP_OFF() \
    test_type_input(" STA $C008\r")

#define ASM_ALTZP_MAIN() ASM_ALTZP_OFF()

#define ASM_ALTZP_ON() \
    test_type_input(" STA $C009\r")

#define ASM_ALTZP_AUX() ASM_ALTZP_ON()

#define ASM_CHECK_ALTZP() \
    test_type_input( \
            " LDA $C016\r" \
            " STA $1F43\r" \
            )

#define ASM_C3ROM_INTERNAL() \
    test_type_input(" STA $C00A\r")

#define ASM_C3ROM_PERIPHERAL() \
    test_type_input(" STA $C00B\r")

#define ASM_CHECK_C3ROM() \
    test_type_input( \
            " LDA $C017\r" \
            " STA $1F43\r" \
            )

#define ASM_80COL_OFF() \
    test_type_input(" STA $C00C\r")

#define ASM_80COL_ON() \
    test_type_input(" STA $C00D\r")

#define ASM_CHECK_80COL() \
    test_type_input( \
            " LDA $C01F\r" \
            " STA $1F43\r" \
            )

#define ASM_ALTCHAR_OFF() \
    test_type_input(" STA $C00E\r")

#define ASM_ALTCHAR_ON() \
    test_type_input(" STA $C00F\r")

#define ASM_CHECK_ALTCHAR() \
    test_type_input( \
            " LDA $C01E\r" \
            " STA $1F43\r" \
            )

#define TYPE_TEXT_OFF() \
    test_type_input("POKE49232,0:REM C050 TEXT OFF\r")

#define TYPE_TEXT_ON() \
    test_type_input("POKE49233,0:REM C051 TEXT ON\r")

#define TYPE_CHECK_TEXT() \
    test_type_input( \
            "A=PEEK(49178):REM C01A CHECK TEXT\r" \
            "POKE8003,A:REM C01A->1F43\r" /* TESTOUT_ADDR */ \
            )

#define TYPE_MIXED_OFF() \
    test_type_input("POKE49234,0:REM C052 MIXED OFF\r")

#define TYPE_MIXED_ON() \
    test_type_input("POKE49235,0:REM C053 MIXED ON\r")

#define TYPE_CHECK_MIXED() \
    test_type_input( \
            "A=PEEK(49179):REM C01B CHECK MIXED\r" \
            "POKE8003,A:REM C019->1F43\r" /* TESTOUT_ADDR */ \
            )

#define ASM_PAGE2_OFF() \
    test_type_input(" STA $C054\r")

#define ASM_PAGE2_ON() \
    test_type_input(" STA $C055\r")

#define TYPE_PAGE2_OFF() \
    test_type_input("POKE49236,0:REM C054 PAGE2 OFF\r")

#define TYPE_PAGE2_ON() \
    test_type_input("POKE49237,0:REM C055 PAGE2 ON\r")

#define TYPE_CHECK_PAGE2() \
    test_type_input( \
            "A=PEEK(49180):REM C01C CHECK PAGE2\r" \
            "POKE8003,A:REM ->1F43\r" /* TESTOUT_ADDR */ \
            )

#define ASM_HIRES_OFF() \
    test_type_input(" STA $C056\r")

#define ASM_HIRES_ON() \
    test_type_input(" STA $C057\r")

#define TYPE_HIRES_OFF() \
    test_type_input("POKE49238,0:REM C056 HIRES OFF\r")

#define TYPE_HIRES_ON() \
    test_type_input("POKE49239,0:REM C057 HIRES ON\r")

#define TYPE_CHECK_HIRES() \
    test_type_input( \
            "A=PEEK(49181):REM C01D CHECK HIRES\r" \
            "POKE8003,A:REM ->1F43\r" /* TESTOUT_ADDR */ \
            )

#define ASM_DHIRES_ON() \
    test_type_input(" STA $C05E\r")

#define ASM_DHIRES_OFF() \
    test_type_input(" STA $C05F\r")

#define ASM_CHECK_DHIRES() \
    test_type_input( \
            " LDA $C07F\r" \
            " STA $1F43\r" \
            )

#define ASM_IOUDIS_ON() \
    test_type_input(" STA $C07E\r")

#define ASM_IOUDIS_OFF() \
    test_type_input(" STA $C07F\r")

#define ASM_CHECK_IOUDIS() \
    test_type_input( \
            " LDA $C07E\r" \
            " STA $1F43\r" \
            )

#define ASM_IIE_C080() \
    test_type_input(" STA $C080\r")

#define ASM_IIE_C081() \
    test_type_input(" STA $C081\r")

#define ASM_LC_C082() \
    test_type_input(" STA $C082\r")

#define TYPE_BANK2_ON() \
    test_type_input("POKE49282,0:REM C082 BANK2 on\r")

#define ASM_IIE_C083() \
    test_type_input(" STA $C083\r")

#define ASM_IIE_C084() ASM_IIE_C080()

#define ASM_IIE_C085() ASM_IIE_C081()

#define ASM_LC_C086() ASM_LC_C082()

#define ASM_IIE_C087() ASM_IIE_C083()

#define ASM_IIE_C088() \
    test_type_input(" STA $C088\r")

#define ASM_IIE_C089() \
    test_type_input(" STA $C089\r")

#define ASM_LC_C08A() \
    test_type_input(" STA $C08A\r")

#define TYPE_BANK2_OFF() \
    test_type_input("POKE49290,0:REM C08A BANK2 off\r")

#define TYPE_CHECK_BANK2() \
    test_type_input( \
            "A=PEEK(49169):REM C011 CHECK BANK2\r" \
            "POKE8003,A:REM ->1F43\r" /* TESTOUT_ADDR */ \
            )

#define ASM_IIE_C08B() \
    test_type_input(" STA $C08B\r")

#define ASM_IIE_C08C() ASM_IIE_C088()

#define ASM_IIE_C08D() ASM_IIE_C089()

#define ASM_IIE_C08E() ASM_LC_C08A()

#define ASM_IIE_C08F() ASM_IIE_C08B()

#define ASM_LCROM_BANK1() \
    test_type_input(" STA $C08A\r")

#define ASM_LCRW_BANK1() \
    ASM_LCROM_BANK1(); \
    test_type_input( \
            " STA $C08B\r" \
            " STA $C08B\r" \
            )

#define ASM_LCRD_BANK1() \
    ASM_LCROM_BANK1(); \
    test_type_input(" STA $C088\r");

#define ASM_LCWRT_BANK1() \
    ASM_LCROM_BANK1(); \
    test_type_input( \
            " STA $C089\r" \
            " STA $C089\r" \
            )

#define ASM_LCRAM_ON() \
    test_type_input(" STA $C088\r")

#define ASM_LCRAM_OFF() \
    test_type_input(" STA $C08A\r")

#define ASM_CHECK_LCRAM() \
    test_type_input( \
            " LDA $C012\r" \
            " STA $1F43\r" \
            )

TEST test_PAGE2_on(bool flag_80store, bool flag_hires) {
    BOOT_TO_DOS();

    // setup for testing ...

    TYPE_PAGE2_OFF();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    if (flag_80store) {
        TYPE_80STORE_ON();
    } else {
        TYPE_80STORE_OFF();
    }

    if (flag_hires) {
        TYPE_HIRES_ON();
    } else {
        TYPE_HIRES_OFF();
    }

    test_type_input("POKE7987,255:REM TRIGGER DEBUGGER\r");

    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(!(softswitches & SS_PAGE2));
    ASSERT( (flag_80store ? (softswitches & SS_80STORE) : !(softswitches & SS_80STORE)) );
    ASSERT( (flag_hires   ? (softswitches & SS_HIRES)   : !(softswitches & SS_HIRES)) );

    uint32_t switch_save = softswitches;

    // run actual test ...

    test_common_setup();
    TYPE_PAGE2_ON();
    test_type_input("POKE7987,255:REM TRIGGER DEBUGGER\r");

    uint8_t *save_base_textrd = base_textrd;
    uint8_t *save_base_textwrt = base_textwrt;
    uint8_t *save_base_hgrrd = base_hgrrd;
    uint8_t *save_base_hgrwrt = base_hgrwrt;

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT((softswitches & SS_PAGE2));
    ASSERT( (flag_80store ? (softswitches & SS_80STORE) : !(softswitches & SS_80STORE)) );
    ASSERT( (flag_hires   ? (softswitches & SS_HIRES)   : !(softswitches & SS_HIRES)) );

    switch_save = switch_save | SS_PAGE2;

    if (flag_80store) {
        ASSERT(video__current_page == 0);
        switch_save = switch_save & ~SS_SCREEN;
        switch_save = switch_save | SS_80STORE;
        switch_save = switch_save | (SS_TEXTRD|SS_TEXTWRT);
        ASSERT((void *)base_textrd  == (void *)(apple_ii_64k[1]));
        ASSERT((void *)base_textwrt == (void *)(apple_ii_64k[1]));
        if (flag_hires) {
            switch_save = switch_save | (SS_HGRRD|SS_HGRWRT);
            ASSERT((void *)base_hgrrd  == (void *)(apple_ii_64k[1]));
            ASSERT((void *)base_hgrwrt == (void *)(apple_ii_64k[1]));
        } else {
            ASSERT(base_hgrrd  == save_base_hgrrd);  // unchanged
            ASSERT(base_hgrwrt == save_base_hgrwrt); // unchanged
        }
    } else {
        switch_save = switch_save | SS_SCREEN;
        switch_save = switch_save & ~SS_80STORE;
        ASSERT(video__current_page = 1);
        ASSERT(base_textrd  == save_base_textrd);  // unchanged
        ASSERT(base_textwrt == save_base_textwrt); // unchanged
        ASSERT(base_hgrrd  == save_base_hgrrd);    // unchanged
        ASSERT(base_hgrwrt == save_base_hgrwrt);   // unchanged
    }

    ASSERT((softswitches ^ switch_save) == 0);

    PASS();
}

TEST test_PAGE2_off(bool flag_80store, bool flag_hires) {
    BOOT_TO_DOS();

    // setup for testing ...

    TYPE_PAGE2_ON();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    if (flag_80store) {
        TYPE_80STORE_ON();
    } else {
        TYPE_80STORE_OFF();
    }

    if (flag_hires) {
        TYPE_HIRES_ON();
    } else {
        TYPE_HIRES_OFF();
    }

    test_type_input("POKE7987,255:REM TRIGGER DEBUGGER\r");

    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT( (softswitches & SS_PAGE2));
    ASSERT( (flag_80store ? (softswitches & SS_80STORE) : !(softswitches & SS_80STORE)) );
    ASSERT( (flag_hires   ? (softswitches & SS_HIRES)   : !(softswitches & SS_HIRES)) );

    uint32_t switch_save = softswitches;

    // run actual test ...

    test_common_setup();
    TYPE_PAGE2_OFF();
    test_type_input("POKE7987,255:REM TRIGGER DEBUGGER\r");

    uint8_t *save_base_textrd = base_textrd;
    uint8_t *save_base_textwrt = base_textwrt;
    uint8_t *save_base_hgrrd = base_hgrrd;
    uint8_t *save_base_hgrwrt = base_hgrwrt;

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT( (flag_80store ? (softswitches & SS_80STORE) : !(softswitches & SS_80STORE)) );
    ASSERT( (flag_hires   ? (softswitches & SS_HIRES)   : !(softswitches & SS_HIRES)) );

    ASSERT(video__current_page == 0);

    switch_save = switch_save & ~SS_SCREEN;
    switch_save = switch_save & ~SS_PAGE2;
    if (flag_80store) {
        switch_save = switch_save & ~(SS_TEXTRD|SS_TEXTWRT);
        ASSERT((void *)base_textrd  == (void *)(apple_ii_64k));
        ASSERT((void *)base_textwrt == (void *)(apple_ii_64k));
        if (flag_hires) {
            switch_save = switch_save & ~(SS_HGRRD|SS_HGRWRT);
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

    ASSERT((softswitches ^ switch_save) == 0);

    PASS();
}

TEST test_check_PAGE2(bool flag_page2) {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    test_common_setup();

    if (flag_page2) {
        TYPE_PAGE2_ON();
    } else {
        TYPE_PAGE2_OFF();
    }

    TYPE_CHECK_PAGE2();
    test_type_input("POKE7987,255:REM TRIGGER DEBUGGER\r");

    apple_ii_64k[0][TESTOUT_ADDR] = 0x96;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    if (flag_page2) {
        ASSERT((softswitches & SS_PAGE2));
        ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 0x80);
    } else {
        ASSERT(!(softswitches & SS_PAGE2));
        ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 0x00);
    }

    PASS();
}

TEST test_TEXT_on() {
    BOOT_TO_DOS();

    // setup for testing ...

    TYPE_TEXT_OFF();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    test_type_input("POKE7987,255:REM TRIGGER DEBUGGER\r");

    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(!(softswitches & SS_TEXT));

    uint32_t switch_save = softswitches;

    // run actual test ...

    test_common_setup();
    TYPE_TEXT_ON();
    test_type_input("POKE7987,255:REM TRIGGER DEBUGGER\r");

    uint8_t *save_base_textrd = base_textrd;
    uint8_t *save_base_textwrt = base_textwrt;
    uint8_t *save_base_hgrrd = base_hgrrd;
    uint8_t *save_base_hgrwrt = base_hgrwrt;
    int save_current_page = video__current_page;

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    ASSERT((softswitches & SS_TEXT));
    ASSERT(video__current_page == save_current_page);

    switch_save = (switch_save | SS_TEXT);

    ASSERT(base_textrd  == save_base_textrd);  // unchanged
    ASSERT(base_textwrt == save_base_textwrt); // unchanged
    ASSERT(base_hgrrd  == save_base_hgrrd);    // unchanged
    ASSERT(base_hgrwrt == save_base_hgrwrt);   // unchanged

    ASSERT((softswitches ^ switch_save) == 0);

    PASS();
}

TEST test_TEXT_off() {
    BOOT_TO_DOS();

    // setup for testing ...

    TYPE_TEXT_ON();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    test_type_input("POKE7987,255:REM TRIGGER DEBUGGER\r");

    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT((softswitches & SS_TEXT));

    uint32_t switch_save = softswitches;

    // run actual test ...

    test_common_setup();
    TYPE_TEXT_OFF();
    test_type_input("POKE7987,255:REM TRIGGER DEBUGGER\r");

    uint8_t *save_base_textrd = base_textrd;
    uint8_t *save_base_textwrt = base_textwrt;
    uint8_t *save_base_hgrrd = base_hgrrd;
    uint8_t *save_base_hgrwrt = base_hgrwrt;
    int save_current_page = video__current_page;

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    ASSERT(!(softswitches & SS_TEXT));
    ASSERT(video__current_page == save_current_page);

    switch_save = (switch_save & ~SS_TEXT);

    ASSERT(base_textrd  == save_base_textrd);  // unchanged
    ASSERT(base_textwrt == save_base_textwrt); // unchanged
    ASSERT(base_hgrrd  == save_base_hgrrd);    // unchanged
    ASSERT(base_hgrwrt == save_base_hgrwrt);   // unchanged

    ASSERT((softswitches ^ switch_save) == 0);

    PASS();
}

TEST test_check_TEXT(bool flag_text) {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    test_common_setup();

    if (flag_text) {
        TYPE_TEXT_ON();
    } else {
        TYPE_TEXT_OFF();
    }

    TYPE_CHECK_TEXT();
    test_type_input("POKE7987,255:REM TRIGGER DEBUGGER\r");

    apple_ii_64k[0][TESTOUT_ADDR] = 0x96;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    if (flag_text) {
        ASSERT((softswitches & SS_TEXT));
        ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 0x80);
    } else {
        ASSERT(!(softswitches & SS_TEXT));
        ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 0x00);
    }

    PASS();
}

TEST test_MIXED_on() {
    BOOT_TO_DOS();

    // setup for testing ...

    TYPE_MIXED_OFF();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    test_type_input("POKE7987,255:REM TRIGGER DEBUGGER\r");

    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(!(softswitches & SS_MIXED));

    uint32_t switch_save = softswitches;

    // run actual test ...

    test_common_setup();
    TYPE_MIXED_ON();
    test_type_input("POKE7987,255:REM TRIGGER DEBUGGER\r");

    uint8_t *save_base_textrd = base_textrd;
    uint8_t *save_base_textwrt = base_textwrt;
    uint8_t *save_base_hgrrd = base_hgrrd;
    uint8_t *save_base_hgrwrt = base_hgrwrt;
    int save_current_page = video__current_page;

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    ASSERT((softswitches & SS_MIXED));
    ASSERT(video__current_page == save_current_page);

    switch_save = (switch_save | SS_MIXED);

    ASSERT(base_textrd  == save_base_textrd);  // unchanged
    ASSERT(base_textwrt == save_base_textwrt); // unchanged
    ASSERT(base_hgrrd  == save_base_hgrrd);    // unchanged
    ASSERT(base_hgrwrt == save_base_hgrwrt);   // unchanged

    ASSERT((softswitches ^ switch_save) == 0);

    PASS();
}

TEST test_MIXED_off() {
    BOOT_TO_DOS();

    // setup for testing ...

    TYPE_MIXED_ON();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    test_type_input("POKE7987,255:REM TRIGGER DEBUGGER\r");

    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT((softswitches & SS_MIXED));

    uint32_t switch_save = softswitches;

    // run actual test ...

    test_common_setup();
    TYPE_MIXED_OFF();
    test_type_input("POKE7987,255:REM TRIGGER DEBUGGER\r");

    uint8_t *save_base_textrd = base_textrd;
    uint8_t *save_base_textwrt = base_textwrt;
    uint8_t *save_base_hgrrd = base_hgrrd;
    uint8_t *save_base_hgrwrt = base_hgrwrt;
    int save_current_page = video__current_page;

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    ASSERT(!(softswitches & SS_MIXED));
    ASSERT(video__current_page == save_current_page);

    switch_save = (switch_save & ~SS_MIXED);

    ASSERT(base_textrd  == save_base_textrd);  // unchanged
    ASSERT(base_textwrt == save_base_textwrt); // unchanged
    ASSERT(base_hgrrd  == save_base_hgrrd);    // unchanged
    ASSERT(base_hgrwrt == save_base_hgrwrt);   // unchanged

    ASSERT((softswitches ^ switch_save) == 0);

    PASS();
}

TEST test_check_MIXED(bool flag_mixed) {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    test_common_setup();

    if (flag_mixed) {
        TYPE_MIXED_ON();
    } else {
        TYPE_MIXED_OFF();
    }

    TYPE_CHECK_MIXED();
    test_type_input("POKE7987,255:REM TRIGGER DEBUGGER\r");

    apple_ii_64k[0][TESTOUT_ADDR] = 0x96;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    if (flag_mixed) {
        ASSERT((softswitches & SS_MIXED));
        ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 0x80);
    } else {
        ASSERT(!(softswitches & SS_MIXED));
        ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 0x00);
    }

    PASS();
}

TEST test_HIRES_on(bool flag_80store, bool flag_page2) {
    BOOT_TO_DOS();

    // setup for testing ...

    TYPE_HIRES_OFF();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    if (flag_80store) {
        TYPE_80STORE_ON();
    } else {
        TYPE_80STORE_OFF();
    }

    if (flag_page2) {
        TYPE_PAGE2_ON();
    } else {
        TYPE_PAGE2_OFF();
    }

    test_type_input("POKE7987,255:REM TRIGGER DEBUGGER\r");

    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(!(softswitches & SS_HIRES));
    ASSERT( (flag_80store ? (softswitches & SS_80STORE) : !(softswitches & SS_80STORE)) );
    ASSERT( (flag_page2   ? (softswitches & SS_PAGE2)   : !(softswitches & SS_PAGE2)) );

    uint32_t switch_save = softswitches;

    // run actual test ...

    test_common_setup();
    TYPE_HIRES_ON();
    test_type_input("POKE7987,255:REM TRIGGER DEBUGGER\r");

    uint8_t *save_base_textrd = base_textrd;
    uint8_t *save_base_textwrt = base_textwrt;
    uint8_t *save_base_hgrrd = base_hgrrd;
    uint8_t *save_base_hgrwrt = base_hgrwrt;
    int save_current_page = video__current_page;

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT((softswitches & SS_HIRES));
    ASSERT( (flag_80store ? (softswitches & SS_80STORE) : !(softswitches & SS_80STORE)) );
    ASSERT( (flag_page2   ? (softswitches & SS_PAGE2)   : !(softswitches & SS_PAGE2)) );

    switch_save = switch_save | SS_HIRES;

    if (flag_80store) {
        if (flag_page2) {
            switch_save = switch_save | (SS_HGRRD|SS_HGRWRT);
            ASSERT((void *)base_hgrrd  == (void *)(apple_ii_64k[1]));
            ASSERT((void *)base_hgrwrt == (void *)(apple_ii_64k[1]));
        } else {
            switch_save = switch_save & ~(SS_HGRRD|SS_HGRWRT);
            ASSERT((void *)base_hgrrd  == (void *)(apple_ii_64k[0]));
            ASSERT((void *)base_hgrwrt == (void *)(apple_ii_64k[0]));
        }
    } else {
        ASSERT(base_hgrrd  == save_base_hgrrd);    // unchanged
        ASSERT(base_hgrwrt == save_base_hgrwrt);   // unchanged
    }

    ASSERT(video__current_page == save_current_page);
    ASSERT(base_textrd  == save_base_textrd);  // unchanged
    ASSERT(base_textwrt == save_base_textwrt); // unchanged

    ASSERT((softswitches ^ switch_save) == 0);
    PASS();
}

TEST test_HIRES_off(bool flag_ramrd, bool flag_ramwrt) {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    ASSERT(apple_ii_64k[1][WATCHPOINT_ADDR] != TEST_FINISHED);

    /* setup */
    ASM_INIT();
    ASM_HIRES_ON();
    if (flag_ramrd) {
        ASM_RAMRD_ON();
    } else {
        ASM_RAMRD_OFF();
    }
    if (flag_ramwrt) {
        ASM_RAMWRT_ON();
    } else {
        ASM_RAMWRT_OFF();
    }
    ASM_TRIGGER_WATCHPT();
    /* test */
    ASM_HIRES_OFF();
    ASM_TRIGGER_WATCHPT();
    ASM_DONE();

    if (flag_ramrd) {
        // sets up to copy the test script into auxram (because execution will continue there when RAMRD on)
        ASM_XFER_TEST_TO_AUXMEM();
    }

    ASM_GO();
    c_debugger_go();

    if (flag_ramwrt) {
        ASSERT(apple_ii_64k[1][WATCHPOINT_ADDR] == TEST_FINISHED);
    } else {
        ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    }
    ASSERT((softswitches & SS_HIRES));
    ASSERT( (flag_ramrd  ? (softswitches & SS_RAMRD)  : !(softswitches & SS_RAMRD)) );
    ASSERT( (flag_ramwrt ? (softswitches & SS_RAMWRT) : !(softswitches & SS_RAMWRT)) );

    uint32_t switch_save = softswitches;
    uint8_t *save_base_textrd = base_textrd;
    uint8_t *save_base_textwrt = base_textwrt;
    uint8_t *save_base_hgrrd = base_hgrrd;
    uint8_t *save_base_hgrwrt = base_hgrwrt;
    int save_current_page = video__current_page;

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    apple_ii_64k[1][WATCHPOINT_ADDR] = 0x00;
    c_debugger_go();

    if (flag_ramwrt) {
        ASSERT(apple_ii_64k[1][WATCHPOINT_ADDR] == TEST_FINISHED);
    } else {
        ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    }
    ASSERT(!(softswitches & SS_HIRES));
    ASSERT( (flag_ramrd  ? (softswitches & SS_RAMRD)  : !(softswitches & SS_RAMRD)) );
    ASSERT( (flag_ramwrt ? (softswitches & SS_RAMWRT) : !(softswitches & SS_RAMWRT)) );

    switch_save = switch_save & ~(SS_HIRES|SS_HGRRD|SS_HGRWRT);

    if (flag_ramrd) {
        ASSERT((void *)base_hgrrd  == (void *)(apple_ii_64k[1]));
        switch_save = switch_save | SS_HGRRD;
    } else {
        ASSERT((void *)base_hgrrd  == (void *)(apple_ii_64k[0]));
    }

    if (flag_ramwrt) {
        ASSERT((void *)base_hgrwrt == (void *)(apple_ii_64k[1]));
        switch_save = switch_save | SS_HGRWRT;
    } else {
        ASSERT((void *)base_hgrwrt == (void *)(apple_ii_64k[0]));
    }

    ASSERT(video__current_page == save_current_page);
    ASSERT(base_textrd  == save_base_textrd);  // unchanged
    ASSERT(base_textwrt == save_base_textwrt); // unchanged

    ASSERT((softswitches ^ switch_save) == 0);
    PASS();
}

TEST test_check_HIRES(bool flag_hires) {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    test_common_setup();

    if (flag_hires) {
        TYPE_HIRES_ON();
    } else {
        TYPE_HIRES_OFF();
    }

    TYPE_CHECK_HIRES();
    test_type_input("POKE7987,255:REM TRIGGER DEBUGGER\r");

    apple_ii_64k[0][TESTOUT_ADDR] = 0x96;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    if (flag_hires) {
        ASSERT((softswitches & SS_HIRES));
        ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 0x80);
    } else {
        ASSERT(!(softswitches & SS_HIRES));
        ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 0x00);
    }

    PASS();
}

TEST test_iie_c080(bool flag_altzp) {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    ASM_INIT();
    if (flag_altzp) {
        ASM_ALTZP_ON();
    } else {
        ASM_ALTZP_OFF();
    }
    ASM_TRIGGER_WATCHPT();
    ASM_IIE_C080();
    ASM_TRIGGER_WATCHPT();
    ASM_DONE();

    ASM_GO();
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(flag_altzp ? (softswitches & SS_ALTZP) : !(softswitches & SS_ALTZP) );

    uint32_t switch_save = softswitches;
    uint8_t *save_base_textrd = base_textrd;
    uint8_t *save_base_textwrt = base_textwrt;
    uint8_t *save_base_hgrrd = base_hgrrd;
    uint8_t *save_base_hgrwrt = base_hgrwrt;
    int save_current_page = video__current_page;

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    switch_save = switch_save | (SS_LCRAM|SS_BANK2);
    switch_save = switch_save & ~(SS_LCSEC|SS_LCWRT);

    ASSERT(video__current_page == save_current_page);

    ASSERT(base_textrd  == save_base_textrd);
    ASSERT(base_textwrt == save_base_textwrt);
    ASSERT(base_hgrrd  == save_base_hgrrd);
    ASSERT(base_hgrwrt == save_base_hgrwrt);

    if (flag_altzp) {
        ASSERT((base_d000_rd == language_banks[0]-0xB000));
        ASSERT((base_e000_rd == language_card[0]-0xC000));
    } else {
        ASSERT((base_d000_rd == language_banks[0]-0xD000));
        ASSERT((base_e000_rd == language_card[0]-0xE000));
    }
    ASSERT((base_d000_wrt == 0));
    ASSERT((base_e000_wrt == 0));

    ASSERT((softswitches ^ switch_save) == 0);

    PASS();
}

TEST test_iie_c081(bool flag_altzp, bool flag_lcsec) {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    ASM_INIT();
    if (flag_altzp) {
        ASM_ALTZP_ON();
    } else {
        ASM_ALTZP_OFF();
    }

    if (!flag_lcsec) {
        ASM_IIE_C080(); // HACK NOTE : turns off SS_LCSEC
    } else {
        ASSERT((softswitches & SS_LCSEC));
    }

    ASM_TRIGGER_WATCHPT();
    ASM_IIE_C081();
    ASM_TRIGGER_WATCHPT();
    ASM_DONE();

    ASM_GO();
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(flag_altzp ? (softswitches & SS_ALTZP) : !(softswitches & SS_ALTZP) );
    ASSERT(flag_lcsec ? (softswitches & SS_LCSEC) : !(softswitches & SS_LCSEC) );

    uint32_t switch_save = softswitches;
    uint8_t *save_base_textrd = base_textrd;
    uint8_t *save_base_textwrt = base_textwrt;
    uint8_t *save_base_hgrrd = base_hgrrd;
    uint8_t *save_base_hgrwrt = base_hgrwrt;
    uint8_t *save_base_d000_wrt = base_d000_wrt;
    uint8_t *save_base_e000_wrt = base_e000_wrt;
    int save_current_page = video__current_page;

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    switch_save = switch_save | (SS_LCSEC|SS_BANK2);
    switch_save = switch_save & ~SS_LCRAM;

    ASSERT(video__current_page == save_current_page);
    ASSERT(base_textrd  == save_base_textrd);
    ASSERT(base_textwrt == save_base_textwrt);
    ASSERT(base_hgrrd  == save_base_hgrrd);
    ASSERT(base_hgrwrt == save_base_hgrwrt);

    if (flag_lcsec) {
        switch_save = switch_save | SS_LCWRT;
        if (flag_altzp) {
            ASSERT((base_d000_wrt == language_banks[0]-0xB000));
            ASSERT((base_e000_wrt == language_card[0]-0xC000));
        } else {
            ASSERT((base_d000_wrt == language_banks[0]-0xD000));
            ASSERT((base_e000_wrt == language_card[0]-0xE000));
        }
    } else {
        ASSERT((base_d000_wrt == save_base_d000_wrt));
        ASSERT((base_e000_wrt == save_base_e000_wrt));
    }

    ASSERT((base_d000_rd == apple_ii_64k[0]));
    ASSERT((base_e000_rd == apple_ii_64k[0]));

    ASSERT((softswitches ^ switch_save) == 0);

    PASS();
}

TEST test_lc_c082() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    uint32_t switch_save = softswitches;
    uint8_t *save_base_textrd = base_textrd;
    uint8_t *save_base_textwrt = base_textwrt;
    uint8_t *save_base_hgrrd = base_hgrrd;
    uint8_t *save_base_hgrwrt = base_hgrwrt;
    uint8_t *save_base_d000_wrt = base_d000_wrt;
    uint8_t *save_base_e000_wrt = base_e000_wrt;
    int save_current_page = video__current_page;

    ASM_INIT();
    ASM_LC_C082();
    ASM_TRIGGER_WATCHPT();
    ASM_DONE();
    ASM_GO();
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    switch_save = switch_save | SS_BANK2;
    switch_save = switch_save & ~(SS_LCRAM|SS_LCWRT|SS_LCSEC);

    ASSERT(video__current_page == save_current_page);
    ASSERT(base_textrd   == save_base_textrd);
    ASSERT(base_textwrt  == save_base_textwrt);
    ASSERT(base_hgrrd    == save_base_hgrrd);
    ASSERT(base_hgrwrt   == save_base_hgrwrt);
    ASSERT(base_d000_rd  == apple_ii_64k[0]);
    ASSERT(base_e000_rd  == apple_ii_64k[0]);
    ASSERT(base_d000_wrt == 0);
    ASSERT(base_e000_wrt == 0);

    ASSERT((softswitches ^ switch_save) == 0);

    PASS();
}

TEST test_iie_c083(bool flag_altzp, bool flag_lcsec) {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    ASM_INIT();
    if (flag_altzp) {
        ASM_ALTZP_ON();
    } else {
        ASM_ALTZP_OFF();
    }

    if (!flag_lcsec) {
        ASM_IIE_C080(); // HACK NOTE : turns off SS_LCSEC
    } else {
        ASSERT((softswitches & SS_LCSEC));
    }

    ASM_TRIGGER_WATCHPT();
    ASM_IIE_C083();
    ASM_TRIGGER_WATCHPT();
    ASM_DONE();

    ASM_GO();
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(flag_altzp ? (softswitches & SS_ALTZP) : !(softswitches & SS_ALTZP) );
    ASSERT(flag_lcsec ? (softswitches & SS_LCSEC) : !(softswitches & SS_LCSEC) );

    uint32_t switch_save = softswitches;
    uint8_t *save_base_textrd = base_textrd;
    uint8_t *save_base_textwrt = base_textwrt;
    uint8_t *save_base_hgrrd = base_hgrrd;
    uint8_t *save_base_hgrwrt = base_hgrwrt;
    uint8_t *save_base_d000_wrt = base_d000_wrt;
    uint8_t *save_base_e000_wrt = base_e000_wrt;
    int save_current_page = video__current_page;

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    switch_save = switch_save | (SS_LCSEC|SS_LCRAM|SS_BANK2);

    ASSERT(video__current_page == save_current_page);
    ASSERT(base_textrd  == save_base_textrd);
    ASSERT(base_textwrt == save_base_textwrt);
    ASSERT(base_hgrrd  == save_base_hgrrd);
    ASSERT(base_hgrwrt == save_base_hgrwrt);

    if (flag_lcsec) {
        switch_save = switch_save | SS_LCWRT;
    }

    if (flag_altzp) {
        ASSERT((base_d000_rd == language_banks[0]-0xB000));
        ASSERT((base_e000_rd  == language_card[0]-0xC000));
        if (flag_lcsec) {
            ASSERT((base_d000_wrt == language_banks[0]-0xB000));
            ASSERT((base_e000_wrt == language_card[0]-0xC000));
        } else {
            ASSERT((base_d000_wrt == save_base_d000_wrt));
            ASSERT((base_e000_wrt == save_base_e000_wrt));
        }
    } else {
        ASSERT((base_d000_rd  == language_banks[0]-0xD000));
        ASSERT((base_e000_rd  == language_card[0]-0xE000));
        ASSERT((base_d000_wrt == save_base_d000_wrt));
        ASSERT((base_e000_wrt == save_base_e000_wrt));
    }

    ASSERT((softswitches ^ switch_save) == 0);

    PASS();
}

TEST test_iie_c088(bool flag_altzp) {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    ASM_INIT();
    if (flag_altzp) {
        ASM_ALTZP_ON();
    } else {
        ASM_ALTZP_OFF();
    }

    ASM_TRIGGER_WATCHPT();
    ASM_IIE_C088();
    ASM_TRIGGER_WATCHPT();
    ASM_DONE();

    ASM_GO();
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(flag_altzp ? (softswitches & SS_ALTZP) : !(softswitches & SS_ALTZP) );

    uint32_t switch_save = softswitches;
    uint8_t *save_base_textrd = base_textrd;
    uint8_t *save_base_textwrt = base_textwrt;
    uint8_t *save_base_hgrrd = base_hgrrd;
    uint8_t *save_base_hgrwrt = base_hgrwrt;
    int save_current_page = video__current_page;

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    switch_save = switch_save | SS_LCRAM;
    switch_save = switch_save & ~(SS_LCWRT|SS_LCSEC|SS_BANK2);

    ASSERT(video__current_page == save_current_page);
    ASSERT(base_textrd  == save_base_textrd);
    ASSERT(base_textwrt == save_base_textwrt);
    ASSERT(base_hgrrd  == save_base_hgrrd);
    ASSERT(base_hgrwrt == save_base_hgrwrt);

    if (flag_altzp) {
        ASSERT((base_d000_rd  == language_banks[0]-0xA000));
        ASSERT((base_e000_rd  == language_card[0]-0xC000));
    } else {
        ASSERT((base_d000_rd  == language_banks[0]-0xC000));
        ASSERT((base_e000_rd  == language_card[0]-0xE000));
    }
    ASSERT((base_d000_wrt == 0));
    ASSERT((base_e000_wrt == 0));

    ASSERT((softswitches ^ switch_save) == 0);

    PASS();
}

TEST test_iie_c089(bool flag_altzp, bool flag_lcsec) {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    ASM_INIT();
    if (flag_altzp) {
        ASM_ALTZP_ON();
    } else {
        ASM_ALTZP_OFF();
    }

    if (!flag_lcsec) {
        ASM_IIE_C080(); // HACK NOTE : turns off SS_LCSEC
    } else {
        ASSERT((softswitches & SS_LCSEC));
    }

    ASM_TRIGGER_WATCHPT();
    ASM_IIE_C089();
    ASM_TRIGGER_WATCHPT();
    ASM_DONE();

    ASM_GO();
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(flag_altzp ? (softswitches & SS_ALTZP) : !(softswitches & SS_ALTZP) );
    ASSERT(flag_lcsec ? (softswitches & SS_LCSEC) : !(softswitches & SS_LCSEC) );

    uint32_t switch_save = softswitches;
    uint8_t *save_base_textrd = base_textrd;
    uint8_t *save_base_textwrt = base_textwrt;
    uint8_t *save_base_hgrrd = base_hgrrd;
    uint8_t *save_base_hgrwrt = base_hgrwrt;
    uint8_t *save_base_d000_wrt = base_d000_wrt;
    uint8_t *save_base_e000_wrt = base_e000_wrt;
    int save_current_page = video__current_page;

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    switch_save = switch_save | SS_LCSEC;
    switch_save = switch_save & ~(SS_LCRAM|SS_BANK2);

    ASSERT(video__current_page == save_current_page);
    ASSERT(base_textrd  == save_base_textrd);
    ASSERT(base_textwrt == save_base_textwrt);
    ASSERT(base_hgrrd  == save_base_hgrrd);
    ASSERT(base_hgrwrt == save_base_hgrwrt);

    if (flag_lcsec) {
        switch_save = switch_save | SS_LCWRT;
        if (flag_altzp) {
            ASSERT((base_d000_wrt == language_banks[0]-0xA000));
            ASSERT((base_e000_wrt == language_card[0]-0xC000));
        } else {
            ASSERT((base_d000_wrt == language_banks[0]-0xC000));
            ASSERT((base_e000_wrt == language_card[0]-0xE000));
        }
    } else {
        ASSERT((base_d000_wrt == save_base_d000_wrt));
        ASSERT((base_e000_wrt == save_base_e000_wrt));
    }

    ASSERT((base_d000_rd  == apple_ii_64k[0]));
    ASSERT((base_e000_rd  == apple_ii_64k[0]));

    ASSERT((softswitches ^ switch_save) == 0);

    PASS();
}

TEST test_lc_c08a() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    uint32_t switch_save = softswitches;
    uint8_t *save_base_textrd = base_textrd;
    uint8_t *save_base_textwrt = base_textwrt;
    uint8_t *save_base_hgrrd = base_hgrrd;
    uint8_t *save_base_hgrwrt = base_hgrwrt;
    uint8_t *save_base_d000_wrt = base_d000_wrt;
    uint8_t *save_base_e000_wrt = base_e000_wrt;
    int save_current_page = video__current_page;

    ASM_INIT();
    ASM_LC_C08A();
    ASM_TRIGGER_WATCHPT();
    ASM_DONE();
    ASM_GO();
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    switch_save = switch_save & ~(SS_LCRAM|SS_LCWRT|SS_LCSEC|SS_BANK2);

    ASSERT(video__current_page == save_current_page);
    ASSERT(base_textrd   == save_base_textrd);
    ASSERT(base_textwrt  == save_base_textwrt);
    ASSERT(base_hgrrd    == save_base_hgrrd);
    ASSERT(base_hgrwrt   == save_base_hgrwrt);
    ASSERT(base_d000_rd  == apple_ii_64k[0]);
    ASSERT(base_e000_rd  == apple_ii_64k[0]);
    ASSERT(base_d000_wrt == 0);
    ASSERT(base_e000_wrt == 0);

    ASSERT((softswitches ^ switch_save) == 0);

    PASS();
}

TEST test_iie_c08b(bool flag_altzp, bool flag_lcsec) {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    ASM_INIT();
    if (flag_altzp) {
        ASM_ALTZP_ON();
    } else {
        ASM_ALTZP_OFF();
    }

    if (!flag_lcsec) {
        ASM_IIE_C080(); // HACK NOTE : turns off SS_LCSEC
    } else {
        ASSERT((softswitches & SS_LCSEC));
    }

    ASM_TRIGGER_WATCHPT();
    ASM_IIE_C08B();
    ASM_TRIGGER_WATCHPT();
    ASM_DONE();

    ASM_GO();
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(flag_altzp ? (softswitches & SS_ALTZP) : !(softswitches & SS_ALTZP) );
    ASSERT(flag_lcsec ? (softswitches & SS_LCSEC) : !(softswitches & SS_LCSEC) );

    uint32_t switch_save = softswitches;
    uint8_t *save_base_textrd = base_textrd;
    uint8_t *save_base_textwrt = base_textwrt;
    uint8_t *save_base_hgrrd = base_hgrrd;
    uint8_t *save_base_hgrwrt = base_hgrwrt;
    uint8_t *save_base_d000_wrt = base_d000_wrt;
    uint8_t *save_base_e000_wrt = base_e000_wrt;
    int save_current_page = video__current_page;

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    switch_save = switch_save | (SS_LCSEC|SS_LCRAM);
    switch_save = switch_save & ~SS_BANK2;

    ASSERT(video__current_page == save_current_page);
    ASSERT(base_textrd  == save_base_textrd);
    ASSERT(base_textwrt == save_base_textwrt);
    ASSERT(base_hgrrd  == save_base_hgrrd);
    ASSERT(base_hgrwrt == save_base_hgrwrt);

    if (flag_lcsec) {
        switch_save = switch_save | SS_LCWRT;
    } else {
        ASSERT((base_d000_wrt == save_base_d000_wrt));
        ASSERT((base_e000_wrt == save_base_e000_wrt));
    }

    if (flag_altzp) {
        ASSERT((base_d000_rd == language_banks[0]-0xA000));
        ASSERT((base_e000_rd  == language_card[0]-0xC000));
        if (flag_lcsec) {
            ASSERT((base_d000_wrt == language_banks[0]-0xA000));
            ASSERT((base_e000_wrt == language_card[0]-0xC000));
        }
    } else {
        ASSERT((base_d000_rd  == language_banks[0]-0xC000));
        ASSERT((base_e000_rd  == language_card[0]-0xE000));
        if (flag_lcsec) {
            ASSERT((base_d000_wrt == language_banks[0]-0xC000));
            ASSERT((base_e000_wrt == language_card[0]-0xE000));
        }
    }

    ASSERT((softswitches ^ switch_save) == 0);

    PASS();
}

TEST test_check_BANK2(bool flag_bank2) {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    test_common_setup();

    if (flag_bank2) {
        TYPE_BANK2_ON();
    } else {
        TYPE_BANK2_OFF();
    }

    TYPE_CHECK_BANK2();
    test_type_input("POKE7987,255:REM TRIGGER DEBUGGER\r");

    apple_ii_64k[0][TESTOUT_ADDR] = 0x96;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    if (flag_bank2) {
        ASSERT((softswitches & SS_BANK2));
        ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 0x80);
    } else {
        ASSERT(!(softswitches & SS_BANK2));
        ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 0x00);
    }

    PASS();
}

TEST test_check_LCRAM(bool flag_lcram) {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    ASM_INIT();

    if (flag_lcram) {
        ASM_LCRAM_ON();
    } else {
        ASSERT(!(softswitches & SS_LCRAM));
    }

    ASM_CHECK_LCRAM();
    ASM_TRIGGER_WATCHPT();
    ASM_DONE();

    ASM_GO();

    apple_ii_64k[0][TESTOUT_ADDR] = 0x96;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    if (flag_lcram) {
        ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 0x80);
    } else {
        ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 0x00);
    }

    PASS();
}

// ----------------------------------------------------------------------------
// Test miscellaneous //e VM functions

TEST test_80store_on(bool flag_hires, bool flag_page2) {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    ASM_INIT();

    ASM_80STORE_OFF();

    if (flag_hires) {
        ASM_HIRES_ON();
    } else {
        ASM_HIRES_OFF();
    }

    if (flag_page2) {
        ASM_PAGE2_ON();
    } else {
        ASM_PAGE2_OFF();
    }

    ASM_TRIGGER_WATCHPT();
    ASM_80STORE_ON();
    ASM_TRIGGER_WATCHPT();
    ASM_DONE();

    ASM_GO();
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(!(softswitches & SS_80STORE));
    ASSERT(flag_hires  ? (softswitches & SS_HIRES)  : !(softswitches & SS_HIRES) );
    ASSERT(flag_page2  ? (softswitches & SS_PAGE2)  : !(softswitches & SS_PAGE2) );

    uint32_t switch_save = softswitches;
    uint8_t *save_base_textrd = base_textrd;
    uint8_t *save_base_textwrt = base_textwrt;
    uint8_t *save_base_hgrrd = base_hgrrd;
    uint8_t *save_base_hgrwrt = base_hgrwrt;
    int save_current_page = video__current_page;

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT((softswitches & SS_80STORE));
    ASSERT(flag_hires  ? (softswitches & SS_HIRES)  : !(softswitches & SS_HIRES) );
    ASSERT(flag_page2  ? (softswitches & SS_PAGE2)  : !(softswitches & SS_PAGE2) );

    switch_save = switch_save | SS_80STORE;
    switch_save = switch_save & ~SS_SCREEN;

    if (flag_page2) {
        switch_save = switch_save | (SS_TEXTRD|SS_TEXTWRT);
        ASSERT(base_textrd  == apple_ii_64k[1]);
        ASSERT(base_textwrt == apple_ii_64k[1]);
        if (flag_hires) {
            switch_save = switch_save | (SS_HGRRD|SS_HGRWRT);
            ASSERT(base_hgrrd  == apple_ii_64k[1]);
            ASSERT(base_hgrwrt == apple_ii_64k[1]);
        } else {
            ASSERT(base_hgrrd  == save_base_hgrrd);
            ASSERT(base_hgrwrt == save_base_hgrwrt);
        }
    } else {
        switch_save = switch_save & ~(SS_TEXTRD|SS_TEXTWRT);
        ASSERT(base_textrd  == apple_ii_64k[0]);
        ASSERT(base_textwrt == apple_ii_64k[0]);
        if (flag_hires) {
            switch_save = switch_save & ~(SS_HGRRD|SS_HGRWRT);
            ASSERT(base_hgrrd  == apple_ii_64k[0]);
            ASSERT(base_hgrwrt == apple_ii_64k[0]);
        } else {
            ASSERT(base_hgrrd  == save_base_hgrrd);
            ASSERT(base_hgrwrt == save_base_hgrwrt);
        }
    }

    ASSERT(video__current_page == 0);
    ASSERT((softswitches ^ switch_save) == 0);

    PASS();
}

TEST test_80store_off(bool flag_ramrd, bool flag_ramwrt, bool flag_page2) {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    ASSERT(apple_ii_64k[1][WATCHPOINT_ADDR] != TEST_FINISHED);

    ASM_INIT();

    ASM_80STORE_ON();

    if (flag_ramrd) {
        ASM_RAMRD_ON();
    } else {
        ASM_RAMRD_OFF();
    }

    if (flag_ramwrt) {
        ASM_RAMWRT_ON();
    } else {
        ASM_RAMWRT_OFF();
    }

    if (flag_page2) {
        ASM_PAGE2_ON();
    } else {
        ASM_PAGE2_OFF();
    }

    ASM_TRIGGER_WATCHPT();
    ASM_80STORE_OFF();
    ASM_TRIGGER_WATCHPT();
    ASM_DONE();

    if (flag_ramrd) {
        // sets up to copy the test script into auxram (because execution will continue there when RAMRD on)
        ASM_XFER_TEST_TO_AUXMEM();
    }

    ASM_GO();
    c_debugger_go();

    if (flag_ramwrt) {
        ASSERT(apple_ii_64k[1][WATCHPOINT_ADDR] == TEST_FINISHED);
    } else {
        ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    }
    ASSERT((softswitches & SS_80STORE));
    ASSERT(flag_ramrd  ? (softswitches & SS_RAMRD)  : !(softswitches & SS_RAMRD) );
    ASSERT(flag_ramwrt ? (softswitches & SS_RAMWRT) : !(softswitches & SS_RAMWRT) );
    ASSERT(flag_page2  ? (softswitches & SS_PAGE2)  : !(softswitches & SS_PAGE2) );

    uint32_t switch_save = softswitches;
    uint8_t *save_base_textrd = base_textrd;
    uint8_t *save_base_textwrt = base_textwrt;
    uint8_t *save_base_hgrrd = base_hgrrd;
    uint8_t *save_base_hgrwrt = base_hgrwrt;
    int save_current_page = video__current_page;

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    apple_ii_64k[1][WATCHPOINT_ADDR] = 0x00;
    c_debugger_go();

    if (flag_ramwrt) {
        ASSERT(apple_ii_64k[1][WATCHPOINT_ADDR] == TEST_FINISHED);
    } else {
        ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    }
    ASSERT(!(softswitches & SS_80STORE));
    ASSERT(flag_ramrd  ? (softswitches & SS_RAMRD)  : !(softswitches & SS_RAMRD) );
    ASSERT(flag_ramwrt ? (softswitches & SS_RAMWRT) : !(softswitches & SS_RAMWRT) );
    ASSERT(flag_page2  ? (softswitches & SS_PAGE2)  : !(softswitches & SS_PAGE2) );

    switch_save = switch_save & ~(SS_80STORE|SS_TEXTRD|SS_TEXTWRT|SS_HGRRD|SS_HGRWRT);

    if (flag_ramrd) {
        switch_save = switch_save | (SS_TEXTRD|SS_HGRRD);
        ASSERT(base_textrd  == apple_ii_64k[1]);
        ASSERT(base_hgrrd   == apple_ii_64k[1]);
    } else {
        ASSERT(base_textrd  == apple_ii_64k[0]);
        ASSERT(base_hgrrd   == apple_ii_64k[0]);
    }

    if (flag_ramwrt) {
        switch_save = switch_save | (SS_TEXTWRT|SS_HGRWRT);
        ASSERT(base_textwrt == apple_ii_64k[1]);
        ASSERT(base_hgrwrt  == apple_ii_64k[1]);
    } else {
        ASSERT(base_textwrt == apple_ii_64k[0]);
        ASSERT(base_hgrwrt  == apple_ii_64k[0]);
    }

    if (flag_page2) {
        switch_save = switch_save | SS_PAGE2;
        switch_save = switch_save | SS_SCREEN;
        switch_save = switch_save & ~SS_80STORE;
        ASSERT(video__current_page == 1);
    } else {
        ASSERT(video__current_page == 0);
    }

    ASSERT((softswitches ^ switch_save) == 0);

    PASS();
}

TEST test_check_80store(bool flag_80store) {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    uint32_t switch_save = softswitches;

    ASM_INIT();

    if (flag_80store) {
        ASM_80STORE_ON();
    } else {
        ASSERT(!(softswitches & SS_80STORE));
    }

    ASM_CHECK_80STORE();
    ASM_TRIGGER_WATCHPT();
    ASM_DONE();
    ASM_GO();

    apple_ii_64k[0][TESTOUT_ADDR] = 0x96;
    c_debugger_go();
    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    if (flag_80store) {
        switch_save = switch_save | SS_80STORE;
        ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 0x80);
    } else {
        ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 0x00);
    }

    ASSERT((softswitches ^ switch_save) == 0);

    PASS();
}

TEST test_ramrd_main(bool flag_80store, bool flag_hires) {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    ASM_INIT();

    ASM_RAMRD_AUX();

    if (flag_80store) {
        ASM_80STORE_ON();
    } else {
        ASM_80STORE_OFF();
    }

    if (flag_hires) {
        ASM_HIRES_ON();
    } else {
        ASM_HIRES_OFF();
    }

    ASM_TRIGGER_WATCHPT();
    ASM_RAMRD_MAIN();
    ASM_TRIGGER_WATCHPT();
    ASM_DONE();

    // sets up to copy the test script into auxram (because execution will continue there when RAMRD on)
    ASM_XFER_TEST_TO_AUXMEM();

    ASM_GO();
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT((softswitches & SS_RAMRD));
    ASSERT(flag_80store ? (softswitches & SS_80STORE) : !(softswitches & SS_80STORE) );
    ASSERT(flag_hires   ? (softswitches & SS_HIRES)   : !(softswitches & SS_HIRES) );

    uint32_t switch_save = softswitches;
    uint8_t *save_base_ramrd = base_ramrd;
    uint8_t *save_base_ramwrt = base_ramwrt;
    uint8_t *save_base_textrd = base_textrd;
    uint8_t *save_base_textwrt = base_textwrt;
    uint8_t *save_base_hgrrd = base_hgrrd;
    uint8_t *save_base_hgrwrt = base_hgrwrt;
    int save_current_page = video__current_page;

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(!(softswitches & SS_RAMRD));
    ASSERT(flag_80store ? (softswitches & SS_80STORE) : !(softswitches & SS_80STORE) );
    ASSERT(flag_hires   ? (softswitches & SS_HIRES)   : !(softswitches & SS_HIRES) );

    switch_save = switch_save & ~SS_RAMRD;

    ASSERT(base_ramrd == apple_ii_64k[0]);
    ASSERT(base_ramwrt == save_base_ramwrt);

    if (flag_80store) {
        if (flag_hires) {
            ASSERT(base_hgrrd == save_base_hgrrd);
        } else {
            switch_save = switch_save & ~SS_HGRRD;
            ASSERT(base_hgrrd == apple_ii_64k[0]);
        }
        ASSERT(base_textrd == save_base_textrd);
    } else {
        switch_save = switch_save & ~(SS_TEXTRD|SS_HGRRD);
        ASSERT(base_textrd  == apple_ii_64k[0]);
        ASSERT(base_hgrrd   == apple_ii_64k[0]);
    }

    ASSERT(base_textwrt == save_base_textwrt);
    ASSERT(base_hgrwrt  == save_base_hgrwrt);
    ASSERT(video__current_page == save_current_page);

    ASSERT((softswitches ^ switch_save) == 0);

    PASS();
}

TEST test_ramrd_aux(bool flag_80store, bool flag_hires) {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    ASM_INIT();

    ASM_RAMRD_MAIN();

    if (flag_80store) {
        ASM_80STORE_ON();
    } else {
        ASM_80STORE_OFF();
    }

    if (flag_hires) {
        ASM_HIRES_ON();
    } else {
        ASM_HIRES_OFF();
    }

    ASM_TRIGGER_WATCHPT();
    ASM_RAMRD_AUX();
    ASM_TRIGGER_WATCHPT();
    ASM_DONE();

    // sets up to copy the test script into auxram (because execution will continue there when RAMRD on)
    ASM_XFER_TEST_TO_AUXMEM();

    ASM_GO();
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(!(softswitches & SS_RAMRD));
    ASSERT(flag_80store ? (softswitches & SS_80STORE) : !(softswitches & SS_80STORE) );
    ASSERT(flag_hires   ? (softswitches & SS_HIRES)   : !(softswitches & SS_HIRES) );

    uint32_t switch_save = softswitches;
    uint8_t *save_base_ramrd = base_ramrd;
    uint8_t *save_base_ramwrt = base_ramwrt;
    uint8_t *save_base_textrd = base_textrd;
    uint8_t *save_base_textwrt = base_textwrt;
    uint8_t *save_base_hgrrd = base_hgrrd;
    uint8_t *save_base_hgrwrt = base_hgrwrt;
    int save_current_page = video__current_page;

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT((softswitches & SS_RAMRD));
    ASSERT(flag_80store ? (softswitches & SS_80STORE) : !(softswitches & SS_80STORE) );
    ASSERT(flag_hires   ? (softswitches & SS_HIRES)   : !(softswitches & SS_HIRES) );

    switch_save = switch_save | SS_RAMRD;

    ASSERT(base_ramrd == apple_ii_64k[1]);
    ASSERT(base_ramwrt == save_base_ramwrt);

    if (flag_80store) {
        if (flag_hires) {
            ASSERT(base_hgrrd == save_base_hgrrd);
        } else {
            switch_save = switch_save | SS_HGRRD;
            ASSERT(base_hgrrd == apple_ii_64k[1]);
        }
        ASSERT(base_textrd == save_base_textrd);
    } else {
        switch_save = switch_save | (SS_TEXTRD|SS_HGRRD);
        ASSERT(base_textrd  == apple_ii_64k[1]);
        ASSERT(base_hgrrd   == apple_ii_64k[1]);
    }

    ASSERT(base_textwrt == save_base_textwrt);
    ASSERT(base_hgrwrt  == save_base_hgrwrt);
    ASSERT(video__current_page == save_current_page);

    ASSERT((softswitches ^ switch_save) == 0);

    PASS();
}

TEST test_check_ramrd(bool flag_ramrd) {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    ASM_INIT();

    if (flag_ramrd) {
        ASM_RAMRD_ON();
    } else {
        ASSERT(!(softswitches & SS_RAMRD));
    }

    ASM_CHECK_RAMRD();
    ASM_TRIGGER_WATCHPT();
    ASM_DONE();

    if (flag_ramrd) {
        // sets up to copy the test script into auxram (because execution will continue there when RAMRD on)
        ASM_XFER_TEST_TO_AUXMEM();
    }

    ASM_GO();

    apple_ii_64k[0][TESTOUT_ADDR] = 0x96;
    c_debugger_go();
    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    if (flag_ramrd) {
        ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 0x80);
    } else {
        ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 0x00);
    }

    PASS();
}

TEST test_ramwrt_main(bool flag_80store, bool flag_hires) {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    ASSERT(apple_ii_64k[1][WATCHPOINT_ADDR] != TEST_FINISHED);

    ASM_INIT();

    ASM_RAMWRT_AUX();

    if (flag_80store) {
        ASM_80STORE_ON();
    } else {
        ASM_80STORE_OFF();
    }

    if (flag_hires) {
        ASM_HIRES_ON();
    } else {
        ASM_HIRES_OFF();
    }

    ASM_TRIGGER_WATCHPT();
    ASM_RAMWRT_MAIN();
    ASM_TRIGGER_WATCHPT();
    ASM_DONE();

    ASM_GO();
    c_debugger_go();

    ASSERT(apple_ii_64k[1][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT((softswitches & SS_RAMWRT));
    ASSERT(flag_80store ? (softswitches & SS_80STORE) : !(softswitches & SS_80STORE) );
    ASSERT(flag_hires   ? (softswitches & SS_HIRES)   : !(softswitches & SS_HIRES) );

    uint32_t switch_save = softswitches;
    uint8_t *save_base_ramrd = base_ramrd;
    uint8_t *save_base_ramwrt = base_ramwrt;
    uint8_t *save_base_textrd = base_textrd;
    uint8_t *save_base_textwrt = base_textwrt;
    uint8_t *save_base_hgrrd = base_hgrrd;
    uint8_t *save_base_hgrwrt = base_hgrwrt;
    int save_current_page = video__current_page;

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    apple_ii_64k[1][WATCHPOINT_ADDR] = 0x00;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(!(softswitches & SS_RAMWRT));
    ASSERT(flag_80store ? (softswitches & SS_80STORE) : !(softswitches & SS_80STORE) );
    ASSERT(flag_hires   ? (softswitches & SS_HIRES)   : !(softswitches & SS_HIRES) );

    switch_save = switch_save & ~SS_RAMWRT;

    ASSERT(base_ramrd == save_base_ramrd);
    ASSERT(base_ramwrt == apple_ii_64k[0]);

    if (flag_80store) {
        if (flag_hires) {
            ASSERT(base_hgrwrt == save_base_hgrwrt);
        } else {
            switch_save = switch_save & ~SS_HGRWRT;
            ASSERT(base_hgrwrt == apple_ii_64k[0]);
        }
        ASSERT(base_textwrt == save_base_textwrt);
    } else {
        switch_save = switch_save & ~(SS_TEXTWRT|SS_HGRWRT);
        ASSERT(base_textwrt == apple_ii_64k[0]);
        ASSERT(base_hgrwrt  == apple_ii_64k[0]);
    }

    ASSERT(base_textrd == save_base_textrd);
    ASSERT(base_hgrrd  == save_base_hgrrd);
    ASSERT(video__current_page == save_current_page);

    ASSERT((softswitches ^ switch_save) == 0);

    PASS();
}

TEST test_ramwrt_aux(bool flag_80store, bool flag_hires) {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    ASSERT(apple_ii_64k[1][WATCHPOINT_ADDR] != TEST_FINISHED);

    ASM_INIT();

    ASM_RAMWRT_MAIN();

    if (flag_80store) {
        ASM_80STORE_ON();
    } else {
        ASM_80STORE_OFF();
    }

    if (flag_hires) {
        ASM_HIRES_ON();
    } else {
        ASM_HIRES_OFF();
    }

    ASM_TRIGGER_WATCHPT();
    ASM_RAMWRT_AUX();
    ASM_TRIGGER_WATCHPT();
    ASM_DONE();

    ASM_GO();
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(!(softswitches & SS_RAMWRT));
    ASSERT(flag_80store ? (softswitches & SS_80STORE) : !(softswitches & SS_80STORE) );
    ASSERT(flag_hires   ? (softswitches & SS_HIRES)   : !(softswitches & SS_HIRES) );

    uint32_t switch_save = softswitches;
    uint8_t *save_base_ramrd = base_ramrd;
    uint8_t *save_base_ramwrt = base_ramwrt;
    uint8_t *save_base_textrd = base_textrd;
    uint8_t *save_base_textwrt = base_textwrt;
    uint8_t *save_base_hgrrd = base_hgrrd;
    uint8_t *save_base_hgrwrt = base_hgrwrt;
    int save_current_page = video__current_page;

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    apple_ii_64k[1][WATCHPOINT_ADDR] = 0x00;
    c_debugger_go();

    ASSERT(apple_ii_64k[1][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT((softswitches & SS_RAMWRT));
    ASSERT(flag_80store ? (softswitches & SS_80STORE) : !(softswitches & SS_80STORE) );
    ASSERT(flag_hires   ? (softswitches & SS_HIRES)   : !(softswitches & SS_HIRES) );

    switch_save = switch_save | SS_RAMWRT;

    ASSERT(base_ramrd == save_base_ramrd);
    ASSERT(base_ramwrt == apple_ii_64k[1]);

    if (flag_80store) {
        if (flag_hires) {
            ASSERT(base_hgrwrt == save_base_hgrwrt);
        } else {
            switch_save = switch_save | SS_HGRWRT;
            ASSERT(base_hgrwrt == apple_ii_64k[1]);
        }
        ASSERT(base_textwrt == save_base_textwrt);
    } else {
        switch_save = switch_save | (SS_TEXTWRT|SS_HGRWRT);
        ASSERT(base_textwrt  == apple_ii_64k[1]);
        ASSERT(base_hgrwrt   == apple_ii_64k[1]);
    }

    ASSERT(base_textrd == save_base_textrd);
    ASSERT(base_hgrrd  == save_base_hgrrd);
    ASSERT(video__current_page == save_current_page);

    ASSERT((softswitches ^ switch_save) == 0);

    PASS();
}

TEST test_check_ramwrt(bool flag_ramwrt) {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);
    ASSERT(apple_ii_64k[1][WATCHPOINT_ADDR] != TEST_FINISHED);

    ASM_INIT();

    if (flag_ramwrt) {
        ASM_RAMWRT_ON();
    } else {
        ASSERT(!(softswitches & SS_RAMWRT));
    }

    ASM_CHECK_RAMWRT();
    ASM_TRIGGER_WATCHPT();
    ASM_DONE();

    ASM_GO();

    apple_ii_64k[0][TESTOUT_ADDR] = 0x96;
    apple_ii_64k[1][TESTOUT_ADDR] = 0x96;
    c_debugger_go();

    if (flag_ramwrt) {
        ASSERT(apple_ii_64k[1][WATCHPOINT_ADDR] == TEST_FINISHED);
        ASSERT(apple_ii_64k[1][TESTOUT_ADDR] == 0x80);
    } else {
        ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
        ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 0x00);
    }

    PASS();
}

#warning WARNING TODO FIXME ... these are poor tests ... really we should be testing small assembly programs that read/write/check banked memory
TEST test_altzp_main(bool flag_lcram, bool flag_lcwrt) {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    ASM_INIT();

    ASM_ALTZP_AUX();

    ASM_LCROM_BANK1();

    if (flag_lcram) {
        if (flag_lcwrt) {
            ASM_LCRW_BANK1();
        } else {
            ASM_LCRD_BANK1();
        }
    } else if (flag_lcwrt) {
        ASM_LCWRT_BANK1();
    }

    ASM_TRIGGER_WATCHPT();
    ASM_ALTZP_MAIN();
    ASM_TRIGGER_WATCHPT();
    ASM_DONE();

    ASM_GO();
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT((softswitches & SS_ALTZP));
    ASSERT(flag_lcram ? (softswitches & SS_LCRAM) : !(softswitches & SS_LCRAM) );
    ASSERT(flag_lcwrt ? (softswitches & SS_LCWRT) : !(softswitches & SS_LCWRT) );

    ASSERT((base_stackzp == apple_ii_64k[1]));

    uint32_t switch_save = softswitches;
    uint8_t *save_base_d000_rd = base_d000_rd;
    uint8_t *save_base_d000_wrt = base_d000_wrt;
    uint8_t *save_base_e000_rd = base_e000_rd;
    uint8_t *save_base_e000_wrt = base_e000_wrt;
    int save_current_page = video__current_page;

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(!(softswitches & SS_ALTZP));
    ASSERT(flag_lcram ? (softswitches & SS_LCRAM) : !(softswitches & SS_LCRAM) );
    ASSERT(flag_lcwrt ? (softswitches & SS_LCWRT) : !(softswitches & SS_LCWRT) );

    ASSERT((base_stackzp == apple_ii_64k[0]));

    switch_save = switch_save & ~SS_ALTZP;

    if (flag_lcram) {
        ASSERT(base_d000_rd  == save_base_d000_rd-0x2000);
        ASSERT(base_e000_rd  == language_card[0]-0xE000);
        if (flag_lcwrt) {
            ASSERT(base_d000_wrt == save_base_d000_wrt-0x2000);
            ASSERT(base_e000_wrt == language_card[0]-0xE000);
        } else {
            ASSERT(base_d000_wrt == save_base_d000_wrt);
            ASSERT(base_e000_wrt == save_base_e000_wrt);
        }
    } else if (flag_lcwrt) {
        ASSERT(base_d000_rd  == save_base_d000_rd);
        ASSERT(base_e000_rd  == save_base_e000_rd);
        ASSERT(base_d000_wrt == save_base_d000_wrt-0x2000);
        ASSERT(base_e000_wrt == language_card[0]-0xE000);
    } else {
        ASSERT(base_d000_rd  == save_base_d000_rd);
        ASSERT(base_d000_wrt == save_base_d000_wrt);
        ASSERT(base_e000_rd  == save_base_e000_rd);
        ASSERT(base_e000_wrt == save_base_e000_wrt);
    }

    ASSERT(video__current_page == save_current_page);

    ASSERT((softswitches ^ switch_save) == 0);

    PASS();
}

#warning WARNING TODO FIXME ... these are poor tests ... really we should be testing small assembly programs that read/write/check banked memory
TEST test_altzp_aux(bool flag_lcram, bool flag_lcwrt) {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    ASM_INIT();

    ASM_ALTZP_MAIN();

    ASM_LCROM_BANK1();

    if (flag_lcram) {
        if (flag_lcwrt) {
            ASM_LCRW_BANK1();
        } else {
            ASM_LCRD_BANK1();
        }
    } else if (flag_lcwrt) {
        ASM_LCWRT_BANK1();
    }

    ASM_TRIGGER_WATCHPT();
    ASM_ALTZP_AUX();
    ASM_TRIGGER_WATCHPT();
    ASM_DONE();

    ASM_GO();
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(!(softswitches & SS_ALTZP));
    ASSERT(flag_lcram ? (softswitches & SS_LCRAM) : !(softswitches & SS_LCRAM) );
    ASSERT(flag_lcwrt ? (softswitches & SS_LCWRT) : !(softswitches & SS_LCWRT) );

    ASSERT((base_stackzp == apple_ii_64k[0]));

    uint32_t switch_save = softswitches;
    uint8_t *save_base_d000_rd = base_d000_rd;
    uint8_t *save_base_d000_wrt = base_d000_wrt;
    uint8_t *save_base_e000_rd = base_e000_rd;
    uint8_t *save_base_e000_wrt = base_e000_wrt;
    int save_current_page = video__current_page;

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT((softswitches & SS_ALTZP));
    ASSERT(flag_lcram ? (softswitches & SS_LCRAM) : !(softswitches & SS_LCRAM) );
    ASSERT(flag_lcwrt ? (softswitches & SS_LCWRT) : !(softswitches & SS_LCWRT) );

    ASSERT((base_stackzp == apple_ii_64k[1]));

    switch_save = switch_save | SS_ALTZP;

    if (flag_lcram) {
        ASSERT(base_d000_rd  == save_base_d000_rd+0x2000);
        ASSERT(base_e000_rd  == language_card[0]-0xC000);
        if (flag_lcwrt) {
            ASSERT(base_d000_wrt == save_base_d000_wrt+0x2000);
            ASSERT(base_e000_wrt == language_card[0]-0xC000);
        } else {
            ASSERT(base_d000_wrt == save_base_d000_wrt);
            ASSERT(base_e000_wrt == save_base_e000_wrt);
        }
    } else if (flag_lcwrt) {
        ASSERT(base_d000_rd  == save_base_d000_rd);
        ASSERT(base_e000_rd  == save_base_e000_rd);
        ASSERT(base_d000_wrt == save_base_d000_wrt+0x2000);
        ASSERT(base_e000_wrt == language_card[0]-0xC000);
    } else {
        ASSERT(base_d000_rd  == save_base_d000_rd);
        ASSERT(base_d000_wrt == save_base_d000_wrt);
        ASSERT(base_e000_rd  == save_base_e000_rd);
        ASSERT(base_e000_wrt == save_base_e000_wrt);
    }

    ASSERT(video__current_page == save_current_page);

    ASSERT((softswitches ^ switch_save) == 0);

    PASS();
}

TEST test_check_altzp(bool flag_altzp) {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    ASM_INIT();

    if (flag_altzp) {
        ASM_ALTZP_ON();
    } else {
        ASSERT(!(softswitches & SS_ALTZP));
    }

    ASM_CHECK_ALTZP();
    ASM_TRIGGER_WATCHPT();
    ASM_DONE();

    ASM_GO();

    apple_ii_64k[0][TESTOUT_ADDR] = 0x96;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    if (flag_altzp) {
        ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 0x80);
    } else {
        ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 0x00);
    }

    PASS();
}

TEST test_80col_on() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    ASM_INIT();
    ASM_80COL_OFF();
    ASM_TRIGGER_WATCHPT();
    ASM_80COL_ON();
    ASM_TRIGGER_WATCHPT();
    ASM_DONE();

    ASM_GO();
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(!(softswitches & SS_80COL));

    uint32_t switch_save = softswitches;
    int save_current_page = video__current_page;

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT((softswitches & SS_80COL));

    switch_save = switch_save | SS_80COL;

    ASSERT(video__current_page == save_current_page);

    ASSERT((softswitches ^ switch_save) == 0);

    PASS();
}

TEST test_80col_off() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    ASM_INIT();
    ASM_80COL_ON();
    ASM_TRIGGER_WATCHPT();
    ASM_80COL_OFF();
    ASM_TRIGGER_WATCHPT();
    ASM_DONE();

    ASM_GO();
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT((softswitches & SS_80COL));

    uint32_t switch_save = softswitches;
    uint8_t *save_base_d000_rd = base_d000_rd;
    uint8_t *save_base_d000_wrt = base_d000_wrt;
    uint8_t *save_base_e000_rd = base_e000_rd;
    uint8_t *save_base_e000_wrt = base_e000_wrt;
    int save_current_page = video__current_page;

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(!(softswitches & SS_80COL));

    switch_save = switch_save & ~SS_80COL;

    ASSERT(video__current_page == save_current_page);

    ASSERT((softswitches ^ switch_save) == 0);

    PASS();
}

TEST test_check_80col(bool flag_80col) {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    ASM_INIT();

    if (flag_80col) {
        ASM_80COL_ON();
    } else {
        ASSERT(!(softswitches & SS_80COL));
    }

    ASM_CHECK_80COL();
    ASM_TRIGGER_WATCHPT();
    ASM_DONE();

    ASM_GO();

    apple_ii_64k[0][TESTOUT_ADDR] = 0x96;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    if (flag_80col) {
        ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 0x80);
    } else {
        ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 0x00);
    }

    PASS();
}

TEST test_altchar_on() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    ASM_INIT();
    ASM_ALTCHAR_OFF();
    ASM_TRIGGER_WATCHPT();
    ASM_ALTCHAR_ON();
    ASM_TRIGGER_WATCHPT();
    ASM_DONE();

    ASM_GO();
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(!(softswitches & SS_ALTCHAR));

    uint32_t switch_save = softswitches;
    int save_current_page = video__current_page;

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT((softswitches & SS_ALTCHAR));

    // TODO : test/verify font?

    switch_save = switch_save | SS_ALTCHAR;

    ASSERT(video__current_page == save_current_page);

    ASSERT((softswitches ^ switch_save) == 0);

    PASS();
}

TEST test_altchar_off() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    ASM_INIT();
    ASM_ALTCHAR_ON();
    ASM_TRIGGER_WATCHPT();
    ASM_ALTCHAR_OFF();
    ASM_TRIGGER_WATCHPT();
    ASM_DONE();

    ASM_GO();
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT((softswitches & SS_ALTCHAR));

    uint32_t switch_save = softswitches;
    uint8_t *save_base_d000_rd = base_d000_rd;
    uint8_t *save_base_d000_wrt = base_d000_wrt;
    uint8_t *save_base_e000_rd = base_e000_rd;
    uint8_t *save_base_e000_wrt = base_e000_wrt;
    int save_current_page = video__current_page;

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(!(softswitches & SS_ALTCHAR));

    // TODO : test/verify font?

    switch_save = switch_save & ~SS_ALTCHAR;

    ASSERT(video__current_page == save_current_page);

    ASSERT((softswitches ^ switch_save) == 0);

    PASS();
}

TEST test_check_altchar(bool flag_altchar) {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    ASM_INIT();

    if (flag_altchar) {
        ASM_ALTCHAR_ON();
    } else {
        ASSERT(!(softswitches & SS_ALTCHAR));
    }

    ASM_CHECK_ALTCHAR();
    ASM_TRIGGER_WATCHPT();
    ASM_DONE();

    ASM_GO();

    apple_ii_64k[0][TESTOUT_ADDR] = 0x96;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    if (flag_altchar) {
        ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 0x80);
    } else {
        ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 0x00);
    }

    PASS();
}

TEST test_ioudis_on() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    ASM_INIT();
    ASM_IOUDIS_OFF();
    ASM_TRIGGER_WATCHPT();
    ASM_IOUDIS_ON();
    ASM_TRIGGER_WATCHPT();
    ASM_DONE();

    ASM_GO();
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(!(softswitches & SS_IOUDIS));

    uint32_t switch_save = softswitches;
    int save_current_page = video__current_page;

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT((softswitches & SS_IOUDIS));

    switch_save = switch_save | SS_IOUDIS;

    ASSERT(video__current_page == save_current_page);

    ASSERT((softswitches ^ switch_save) == 0);

    PASS();
}

TEST test_ioudis_off() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    ASM_INIT();
    ASM_IOUDIS_ON();
    ASM_TRIGGER_WATCHPT();
    ASM_IOUDIS_OFF();
    ASM_TRIGGER_WATCHPT();
    ASM_DONE();

    ASM_GO();
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT((softswitches & SS_IOUDIS));

    uint32_t switch_save = softswitches;
    uint8_t *save_base_d000_rd = base_d000_rd;
    uint8_t *save_base_d000_wrt = base_d000_wrt;
    uint8_t *save_base_e000_rd = base_e000_rd;
    uint8_t *save_base_e000_wrt = base_e000_wrt;
    int save_current_page = video__current_page;

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(!(softswitches & SS_IOUDIS));

    switch_save = switch_save & ~SS_IOUDIS;

    ASSERT(video__current_page == save_current_page);

    ASSERT((softswitches ^ switch_save) == 0);

    PASS();
}

TEST test_check_ioudis(bool flag_ioudis) {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    ASM_INIT();

    if (flag_ioudis) {
        ASSERT((softswitches & SS_IOUDIS));
    } else {
        ASM_IOUDIS_OFF();
    }

    ASM_CHECK_IOUDIS();
    ASM_TRIGGER_WATCHPT();
    ASM_DONE();

    ASM_GO();

    apple_ii_64k[0][TESTOUT_ADDR] = 0x96;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    if (flag_ioudis) {
        ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 0x80);
    } else {
        ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 0x00);
    }

    PASS();
}

TEST test_dhires_on(bool flag_ioudis/* FIXME TODO : possibly testing a existing bug? should this other switch matter? */) {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    ASM_INIT();
    if (flag_ioudis) {
        ASSERT((softswitches & SS_IOUDIS));
    } else {
        ASM_IOUDIS_OFF();
    }
    ASM_DHIRES_OFF();
    ASM_TRIGGER_WATCHPT();
    ASM_DHIRES_ON();
    ASM_TRIGGER_WATCHPT();
    ASM_DONE();

    ASM_GO();
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(!(softswitches & SS_DHIRES));

    uint32_t switch_save = softswitches;
    int save_current_page = video__current_page;

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT((softswitches & SS_DHIRES));

    switch_save = switch_save | SS_DHIRES;

    ASSERT(video__current_page == save_current_page);

    ASSERT((softswitches ^ switch_save) == 0);

    PASS();
}

TEST test_dhires_off(bool flag_ioudis/* FIXME TODO : possibly testing a existing bug? should this other switch matter? */) {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    ASM_INIT();
    if (flag_ioudis) {
        ASSERT((softswitches & SS_IOUDIS));
    } else {
        ASM_IOUDIS_OFF();
    }
    ASM_DHIRES_ON();
    ASM_TRIGGER_WATCHPT();
    ASM_DHIRES_OFF();
    ASM_TRIGGER_WATCHPT();
    ASM_DONE();

    ASM_GO();
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT((softswitches & SS_DHIRES));

    uint32_t switch_save = softswitches;
    uint8_t *save_base_d000_rd = base_d000_rd;
    uint8_t *save_base_d000_wrt = base_d000_wrt;
    uint8_t *save_base_e000_rd = base_e000_rd;
    uint8_t *save_base_e000_wrt = base_e000_wrt;
    int save_current_page = video__current_page;

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(!(softswitches & SS_DHIRES));

    switch_save = switch_save & ~SS_DHIRES;

    ASSERT(video__current_page == save_current_page);

    ASSERT((softswitches ^ switch_save) == 0);

    PASS();
}

TEST test_check_dhires(bool flag_dhires, bool flag_ioudis/* FIXME TODO : possibly testing a existing bug? should this other switch matter? */) {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    ASM_INIT();

    if (flag_ioudis) {
        ASSERT((softswitches & SS_IOUDIS));
    } else {
        ASM_IOUDIS_OFF();
    }

    if (flag_dhires) {
        ASM_DHIRES_ON();
    } else {
        ASSERT(!(softswitches & SS_DHIRES));
    }

    ASM_CHECK_DHIRES();
    ASM_TRIGGER_WATCHPT();
    ASM_DONE();

    ASM_GO();

    apple_ii_64k[0][TESTOUT_ADDR] = 0x96;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    if (flag_dhires) {
        ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 0x80);
    } else {
        ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 0x00);
    }

    PASS();
}

#warning WARNING TODO FIXME ... these are poor tests ... really we should be testing small assembly programs that read/write/check banked memory
TEST test_c3rom_internal() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    ASM_INIT();
    ASM_C3ROM_PERIPHERAL();
    ASM_TRIGGER_WATCHPT();
    ASM_C3ROM_INTERNAL();
    ASM_TRIGGER_WATCHPT();
    ASM_DONE();

    ASM_GO();
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(!(softswitches & SS_C3ROM));

    uint32_t switch_save = softswitches;
    uint8_t *save_base_c3rom = base_c3rom;

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT((softswitches & SS_C3ROM));

    switch_save = switch_save | SS_C3ROM;

    ASSERT((base_c3rom == apple_ii_64k[1]));

    ASSERT((softswitches ^ switch_save) == 0);

    PASS();
}

#warning WARNING TODO FIXME ... these are poor tests ... really we should be testing small assembly programs that read/write/check banked memory
TEST test_c3rom_peripheral(bool flag_cxrom) {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    ASM_INIT();

    if (flag_cxrom) {
        ASM_CXROM_INTERNAL();
    } else {
        ASM_CXROM_PERIPHERAL();
    }

    ASM_C3ROM_INTERNAL();
    ASM_TRIGGER_WATCHPT();
    ASM_C3ROM_PERIPHERAL();
    ASM_TRIGGER_WATCHPT();
    ASM_DONE();

    ASM_GO();
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT((softswitches & SS_C3ROM));

    uint32_t switch_save = softswitches;
    uint8_t *save_base_c3rom = base_c3rom;

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(!(softswitches & SS_C3ROM));

    switch_save = switch_save & ~SS_C3ROM;

    if (flag_cxrom) {
        ASSERT((softswitches & SS_CXROM));
        ASSERT((base_c3rom == save_base_c3rom));
    } else {
        ASSERT(!(softswitches & SS_CXROM));
        ASSERT((base_c3rom == apple_ii_64k[0]));
    }

    ASSERT((softswitches ^ switch_save) == 0);

    PASS();
}

TEST test_check_c3rom(bool flag_c3rom) {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    ASM_INIT();

    if (flag_c3rom) {
        ASSERT((softswitches & SS_C3ROM));
    } else {
        ASM_C3ROM_PERIPHERAL();
    }

    ASM_CHECK_C3ROM();
    ASM_TRIGGER_WATCHPT();
    ASM_DONE();

    ASM_GO();

    apple_ii_64k[0][TESTOUT_ADDR] = 0x96;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    if (flag_c3rom) {
        ASSERT((softswitches & SS_C3ROM));
        ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 0x00);
    } else {
        ASSERT(!(softswitches & SS_C3ROM));
        ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 0x80);
    }

    PASS();
}

#warning WARNING TODO FIXME ... these are poor tests ... really we should be testing small assembly programs that read/write/check banked memory
TEST test_cxrom_internal() {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    ASM_INIT();
    ASM_CXROM_PERIPHERAL();
    ASM_TRIGGER_WATCHPT();
    ASM_CXROM_INTERNAL();
    ASM_TRIGGER_WATCHPT();
    ASM_DONE();

    ASM_GO();
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(!(softswitches & SS_CXROM));

    uint32_t switch_save = softswitches;
    uint8_t *save_base_cxrom = base_cxrom;
    uint8_t *save_base_c3rom = base_c3rom;
    uint8_t *save_base_c4rom = base_c4rom;
    uint8_t *save_base_c5rom = base_c5rom;

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT((softswitches & SS_CXROM));

    switch_save = switch_save | SS_CXROM;

    ASSERT((base_cxrom == apple_ii_64k[1]));
    ASSERT((base_c3rom == apple_ii_64k[1]));
    ASSERT((base_c4rom == apple_ii_64k[1]));
    ASSERT((base_c5rom == apple_ii_64k[1]));

    ASSERT((softswitches ^ switch_save) == 0);

    PASS();
}

#warning WARNING TODO FIXME ... these are poor tests ... really we should be testing small assembly programs that read/write/check banked memory
TEST test_cxrom_peripheral(bool flag_c3rom) {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    ASM_INIT();

    if (flag_c3rom) {
        ASM_C3ROM_INTERNAL();
    } else {
        ASM_C3ROM_PERIPHERAL();
    }

    ASM_CXROM_INTERNAL();
    ASM_TRIGGER_WATCHPT();
    ASM_CXROM_PERIPHERAL();
    ASM_TRIGGER_WATCHPT();
    ASM_DONE();

    ASM_GO();
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT((softswitches & SS_CXROM));

    uint32_t switch_save = softswitches;
    uint8_t *save_base_cxrom = base_cxrom;
    uint8_t *save_base_c3rom = base_c3rom;

    apple_ii_64k[0][WATCHPOINT_ADDR] = 0x00;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);
    ASSERT(!(softswitches & SS_CXROM));

    switch_save = switch_save & ~SS_CXROM;

    if (flag_c3rom) {
        ASSERT((softswitches & SS_C3ROM));
        ASSERT((base_c3rom == save_base_c3rom));
    } else {
        ASSERT(!(softswitches & SS_C3ROM));
        ASSERT((base_c3rom == apple_ii_64k[0]));
    }

    ASSERT(base_cxrom == apple_ii_64k[0]);

    // TODO FIXME ... test other peripherals base_xxx here ...

    ASSERT((softswitches ^ switch_save) == 0);

    PASS();
}

TEST test_check_cxrom(bool flag_cxrom) {
    BOOT_TO_DOS();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] != TEST_FINISHED);

    ASM_INIT();

    if (flag_cxrom) {
        ASM_CXROM_INTERNAL();
    } else {
        ASSERT(!(softswitches & SS_CXROM));
    }

    ASM_CHECK_CXROM();
    ASM_TRIGGER_WATCHPT();
    ASM_DONE();

    ASM_GO();

    apple_ii_64k[0][TESTOUT_ADDR] = 0x96;
    c_debugger_go();

    ASSERT(apple_ii_64k[0][WATCHPOINT_ADDR] == TEST_FINISHED);

    if (flag_cxrom) {
        ASSERT((softswitches & SS_CXROM));
        ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 0x80);
    } else {
        ASSERT(!(softswitches & SS_CXROM));
        ASSERT(apple_ii_64k[0][TESTOUT_ADDR] == 0x00);
    }

    PASS();
}

// ----------------------------------------------------------------------------
// Test Suite

GREATEST_SUITE(test_suite_vm) {
    pthread_mutex_lock(&interface_mutex);

    GREATEST_SET_SETUP_CB(testvm_setup, NULL);
    GREATEST_SET_TEARDOWN_CB(testvm_teardown, NULL);
    GREATEST_SET_BREAKPOINT_CB(test_breakpoint, NULL);

    // TESTS --------------------------
    test_thread_running=true;

    RUN_TESTp(test_boot_disk);

#if defined(ANDROID)
#warning FIXME TODO ... why are these test broken on Android?!
#else
    RUN_TESTp(test_read_keyboard);

    RUN_TESTp(test_clear_keyboard);

    RUN_TESTp(test_read_random);
#if 0
#error this is an unstable test due to VBL refactoring ...
    RUN_TESTp(test_read_random2);
#endif

#endif

    RUN_TESTp(test_PAGE2_on,  /*80STORE*/0, /*HIRES*/0);
    RUN_TESTp(test_PAGE2_on,  /*80STORE*/0, /*HIRES*/1);
    RUN_TESTp(test_PAGE2_on,  /*80STORE*/1, /*HIRES*/0);
    RUN_TESTp(test_PAGE2_on,  /*80STORE*/1, /*HIRES*/1);

    RUN_TESTp(test_PAGE2_off, /*80STORE*/0, /*HIRES*/0);
    RUN_TESTp(test_PAGE2_off, /*80STORE*/0, /*HIRES*/1);
    RUN_TESTp(test_PAGE2_off, /*80STORE*/1, /*HIRES*/0);
    RUN_TESTp(test_PAGE2_off, /*80STORE*/1, /*HIRES*/1);

    RUN_TESTp(test_check_PAGE2, /*PAGE2*/0);
    RUN_TESTp(test_check_PAGE2, /*PAGE2*/1);

    RUN_TESTp(test_TEXT_on);
    RUN_TESTp(test_TEXT_off);
    RUN_TESTp(test_check_TEXT, /*TEXT*/0);
    RUN_TESTp(test_check_TEXT, /*TEXT*/1);

    RUN_TESTp(test_MIXED_on);
    RUN_TESTp(test_MIXED_off);
    RUN_TESTp(test_check_MIXED, /*TEXT*/0);
    RUN_TESTp(test_check_MIXED, /*TEXT*/1);

    RUN_TESTp(test_HIRES_on,  /*80STORE*/0, /*PAGE2*/0);
    RUN_TESTp(test_HIRES_on,  /*80STORE*/0, /*PAGE2*/1);
    RUN_TESTp(test_HIRES_on,  /*80STORE*/1, /*PAGE2*/0);
    RUN_TESTp(test_HIRES_on,  /*80STORE*/1, /*PAGE2*/1);

    RUN_TESTp(test_HIRES_off, /*RAMRD*/0, /*RAMWRT*/0);
    RUN_TESTp(test_HIRES_off, /*RAMRD*/0, /*RAMWRT*/1);
    RUN_TESTp(test_HIRES_off, /*RAMRD*/1, /*RAMWRT*/0);
    RUN_TESTp(test_HIRES_off, /*RAMRD*/1, /*RAMWRT*/1);

    RUN_TESTp(test_check_HIRES, /*HIRES*/0);
    RUN_TESTp(test_check_HIRES, /*HIRES*/1);

    RUN_TESTp(test_iie_c080, /*ALTZP*/0);
    RUN_TESTp(test_iie_c080, /*ALTZP*/1);

    RUN_TESTp(test_iie_c081, /*ALTZP*/0, /*LCSEC*/0);
    RUN_TESTp(test_iie_c081, /*ALTZP*/0, /*LCSEC*/1);
    RUN_TESTp(test_iie_c081, /*ALTZP*/1, /*LCSEC*/0);
    RUN_TESTp(test_iie_c081, /*ALTZP*/1, /*LCSEC*/1);

    RUN_TESTp(test_lc_c082);

    RUN_TESTp(test_iie_c083, /*ALTZP*/0, /*LCSEC*/0);
    RUN_TESTp(test_iie_c083, /*ALTZP*/0, /*LCSEC*/1);
    RUN_TESTp(test_iie_c083, /*ALTZP*/1, /*LCSEC*/0);
    RUN_TESTp(test_iie_c083, /*ALTZP*/1, /*LCSEC*/1);

    RUN_TESTp(test_iie_c088, /*ALTZP*/1);
    RUN_TESTp(test_iie_c088, /*ALTZP*/0);

    RUN_TESTp(test_iie_c089, /*ALTZP*/0, /*LCSEC*/0);
    RUN_TESTp(test_iie_c089, /*ALTZP*/0, /*LCSEC*/1);
    RUN_TESTp(test_iie_c089, /*ALTZP*/1, /*LCSEC*/0);
    RUN_TESTp(test_iie_c089, /*ALTZP*/1, /*LCSEC*/1);

    RUN_TESTp(test_lc_c08a);

    RUN_TESTp(test_iie_c08b, /*ALTZP*/0, /*LCSEC*/0);
    RUN_TESTp(test_iie_c08b, /*ALTZP*/0, /*LCSEC*/1);
    RUN_TESTp(test_iie_c08b, /*ALTZP*/1, /*LCSEC*/0);
    RUN_TESTp(test_iie_c08b, /*ALTZP*/1, /*LCSEC*/1);

    RUN_TESTp(test_check_BANK2, /*ALTZP*/0);
    RUN_TESTp(test_check_BANK2, /*ALTZP*/1);
    RUN_TESTp(test_check_LCRAM, /*LCRAM*/0);
    RUN_TESTp(test_check_LCRAM, /*LCRAM*/1);

    RUN_TESTp(test_80store_on, /*HIRES*/0, /*PAGE2*/0);
    RUN_TESTp(test_80store_on, /*HIRES*/0, /*PAGE2*/1);
    RUN_TESTp(test_80store_on, /*HIRES*/1, /*PAGE2*/0);
    RUN_TESTp(test_80store_on, /*HIRES*/1, /*PAGE2*/1);

    RUN_TESTp(test_80store_off, /*RAMRD*/0, /*RAMWRT*/0, /*PAGE2*/0);
    RUN_TESTp(test_80store_off, /*RAMRD*/0, /*RAMWRT*/0, /*PAGE2*/1);
    RUN_TESTp(test_80store_off, /*RAMRD*/0, /*RAMWRT*/1, /*PAGE2*/0);
    RUN_TESTp(test_80store_off, /*RAMRD*/0, /*RAMWRT*/1, /*PAGE2*/1);
    RUN_TESTp(test_80store_off, /*RAMRD*/1, /*RAMWRT*/0, /*PAGE2*/0);
    RUN_TESTp(test_80store_off, /*RAMRD*/1, /*RAMWRT*/0, /*PAGE2*/1);
    RUN_TESTp(test_80store_off, /*RAMRD*/1, /*RAMWRT*/1, /*PAGE2*/0);
    RUN_TESTp(test_80store_off, /*RAMRD*/1, /*RAMWRT*/1, /*PAGE2*/1);

    RUN_TESTp(test_check_80store, /*80STORE*/0);
    RUN_TESTp(test_check_80store, /*80STORE*/1);

    RUN_TESTp(test_ramrd_main, /*80STORE*/0, /*HIRES*/0);
    RUN_TESTp(test_ramrd_main, /*80STORE*/0, /*HIRES*/1);
    RUN_TESTp(test_ramrd_main, /*80STORE*/1, /*HIRES*/0);
    RUN_TESTp(test_ramrd_main, /*80STORE*/1, /*HIRES*/1);

    RUN_TESTp(test_ramrd_aux, /*80STORE*/0, /*HIRES*/0);
    RUN_TESTp(test_ramrd_aux, /*80STORE*/0, /*HIRES*/1);
    RUN_TESTp(test_ramrd_aux, /*80STORE*/1, /*HIRES*/0);
    RUN_TESTp(test_ramrd_aux, /*80STORE*/1, /*HIRES*/1);

    RUN_TESTp(test_check_ramrd, /*RAMRD*/0);
    RUN_TESTp(test_check_ramrd, /*RAMRD*/1);

    RUN_TESTp(test_ramwrt_main, /*80STORE*/0, /*HIRES*/0);
    RUN_TESTp(test_ramwrt_main, /*80STORE*/0, /*HIRES*/1);
    RUN_TESTp(test_ramwrt_main, /*80STORE*/1, /*HIRES*/0);
    RUN_TESTp(test_ramwrt_main, /*80STORE*/1, /*HIRES*/1);

    RUN_TESTp(test_ramwrt_aux, /*80STORE*/0, /*HIRES*/0);
    RUN_TESTp(test_ramwrt_aux, /*80STORE*/0, /*HIRES*/1);
    RUN_TESTp(test_ramwrt_aux, /*80STORE*/1, /*HIRES*/0);
    RUN_TESTp(test_ramwrt_aux, /*80STORE*/1, /*HIRES*/1);

    RUN_TESTp(test_check_ramwrt, /*RAMWRT*/0);
    RUN_TESTp(test_check_ramwrt, /*RAMWRT*/1);

    RUN_TESTp(test_altzp_main, /*LCRAM*/0, /*LCWRT*/0);
    RUN_TESTp(test_altzp_main, /*LCRAM*/0, /*LCWRT*/1);
    RUN_TESTp(test_altzp_main, /*LCRAM*/1, /*LCWRT*/0);
    RUN_TESTp(test_altzp_main, /*LCRAM*/1, /*LCWRT*/1);

    RUN_TESTp(test_altzp_aux, /*LCRAM*/0, /*LCWRT*/0);
    RUN_TESTp(test_altzp_aux, /*LCRAM*/0, /*LCWRT*/1);
    RUN_TESTp(test_altzp_aux, /*LCRAM*/1, /*LCWRT*/0);
    RUN_TESTp(test_altzp_aux, /*LCRAM*/1, /*LCWRT*/1);

    RUN_TESTp(test_check_altzp, /*ALTZP*/0);
    RUN_TESTp(test_check_altzp, /*ALTZP*/1);

    RUN_TESTp(test_80col_on);
    RUN_TESTp(test_80col_off);
    RUN_TESTp(test_check_80col, /*80COL*/0);
    RUN_TESTp(test_check_80col, /*80COL*/1);

    RUN_TESTp(test_altchar_on);
    RUN_TESTp(test_altchar_off);
    RUN_TESTp(test_check_altchar, /*ALTCHAR*/0);
    RUN_TESTp(test_check_altchar, /*ALTCHAR*/1);

    RUN_TESTp(test_ioudis_on);
    RUN_TESTp(test_ioudis_off);
    RUN_TESTp(test_check_ioudis, /*IOUDIS*/0);
    RUN_TESTp(test_check_ioudis, /*IOUDIS*/1);

    RUN_TESTp(test_dhires_on, /*IOUDIS*/0);
    RUN_TESTp(test_dhires_on, /*IOUDIS*/1);
    RUN_TESTp(test_dhires_off, /*IOUDIS*/0);
    RUN_TESTp(test_dhires_off, /*IOUDIS*/1);
    RUN_TESTp(test_check_dhires, /*DHIRES*/0, /*IOUDIS*/0);
    RUN_TESTp(test_check_dhires, /*DHIRES*/0, /*IOUDIS*/1);
    RUN_TESTp(test_check_dhires, /*DHIRES*/1, /*IOUDIS*/0);
    RUN_TESTp(test_check_dhires, /*DHIRES*/1, /*IOUDIS*/1);

    RUN_TESTp(test_c3rom_internal);
    RUN_TESTp(test_c3rom_peripheral, /*CXROM*/0);
    RUN_TESTp(test_c3rom_peripheral, /*CXROM*/1);
    RUN_TESTp(test_check_c3rom, /*C3ROM*/0);
    RUN_TESTp(test_check_c3rom, /*C3ROM*/1);

    RUN_TESTp(test_cxrom_internal);
    RUN_TESTp(test_cxrom_peripheral, /*C3ROM*/0);
    RUN_TESTp(test_cxrom_peripheral, /*C3ROM*/1);
    RUN_TESTp(test_check_cxrom, /*CXROM*/0);
    RUN_TESTp(test_check_cxrom, /*CXROM*/1);

    // ...
    c_eject_6(0);
    pthread_mutex_unlock(&interface_mutex);
}

SUITE(test_suite_vm);
GREATEST_MAIN_DEFS();

static char **test_argv = NULL;
static int test_argc = 0;

static void *test_thread(void *dummyptr) {
    int argc = test_argc;
    char **argv = test_argv;
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(test_suite_vm);
    GREATEST_MAIN_END();
    return NULL;
}

void test_vm(int argc, char **argv) {
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
    test_vm(argc, argv);
}
#endif
