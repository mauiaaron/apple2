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
#include "common.h"

#define TEST_LOC 0x1f00

#define RW_NONE 0x0
#define RW_READ 0x1
#define RW_WRITE 0x2

static void testcpu_setup(void *arg) {

    //reinitialize();

    cpu65_cycles_to_execute = 1;

    cpu65_current.pc = 0x1f00;
    cpu65_current.a = 0x0;
    cpu65_current.f = 0x0;
    cpu65_current.x = 0x0;
    cpu65_current.y = 0x0;
    cpu65_current.sp = 0xff;

    cpu65_debug.ea = 0xffff;
    cpu65_debug.d = 0xff;
    cpu65_debug.rw = 0xff;
    cpu65_debug.opcode = 0xff;
    cpu65_debug.opcycles = 0xff;

    // clear stack memory
    for (unsigned int i=0x100; i<0x200; i++) {
        apple_ii_64k[0][i] = 0x0;
    }

    // clear prog memory
    for (unsigned int i=TEST_LOC; i<TEST_LOC+0x100; i++) {
        apple_ii_64k[0][i] = 0x0;
    }
}

static void testcpu_teardown(void *arg) {
    // ...
}

static void testcpu_set_opcode(uint8_t op, uint8_t arg0, uint8_t arg1) {
    apple_ii_64k[0][TEST_LOC+0] = op;
    apple_ii_64k[0][TEST_LOC+1] = arg0;
    apple_ii_64k[0][TEST_LOC+2] = arg1;
}

// ----------------------------------------------------------------------------
// ADC instructions

TEST test_ADC_imm(bool decimal, uint8_t argA, uint8_t arg0, uint8_t result, uint8_t flags) {
    testcpu_set_opcode(0x69, arg0, 0x00);

    char buf0[MSG_SIZE];
    char buf1[MSG_SIZE];
    char msgbuf[MSG_SIZE];

    cpu65_current.a = argA;
    cpu65_current.x = 0x03;
    cpu65_current.y = 0x04;
    cpu65_current.f = decimal ? (D_Flag_6502) : 0x00;
    cpu65_current.sp = 0x80;

    cpu65_run();

    ASSERT(cpu65_current.pc      == 0x1f02);
    ASSERT(cpu65_current.x       == 0x03);
    ASSERT(cpu65_current.y       == 0x04);
    ASSERT(cpu65_current.sp      == 0x80);

    snprintf(msgbuf, MSG_SIZE, "ADC %02X + %02X = %02X (is %02X)", argA, arg0, result, cpu65_current.a);
    ASSERTm(msgbuf, cpu65_current.a == result);

    flags_to_string(flags, &buf0[0]);
    flags_to_string(cpu65_current.f, &buf1[0]);
    snprintf(msgbuf, MSG_SIZE, "ADC result flags = %s (is %s)", buf0, buf1);
    ASSERTm(strdup(msgbuf), cpu65_current.f == flags);

    ASSERT(cpu65_debug.ea        == 0x1f01);
    ASSERT(cpu65_debug.d         == 0xff);
    ASSERT(cpu65_debug.rw        == RW_READ);
    ASSERT(cpu65_debug.opcode    == 0x69);
    ASSERT(cpu65_debug.opcycles  == decimal ? 3 : 2);

    PASS();
}

TEST test_ADC_zpage() {
    PASS();
}

TEST test_ADC_zpage_x() {
    PASS();
}

TEST test_ADC_abs() {
    PASS();
}

TEST test_ADC_abs_x() {
    PASS();
}

TEST test_ADC_abs_y() {
    PASS();
}

TEST test_ADC_ind_x() {
    PASS();
}

TEST test_ADC_ind_y() {
    PASS();
}

// 65c02 : 0x72
TEST test_ADC_ind_zpage() {
    PASS();
}

// ----------------------------------------------------------------------------
// BRK operand (and IRQ handling)

TEST test_BRK() {
    testcpu_set_opcode(0x0, 0x0, 0x0);

    ASSERT(apple_ii_64k[0][0x1ff] != 0x1f);
    ASSERT(apple_ii_64k[0][0x1fe] != 0x01);

    cpu65_current.a = 0x02;
    cpu65_current.x = 0x03;
    cpu65_current.y = 0x04;

    cpu65_run();

    ASSERT(cpu65_current.pc     == 0xc3fa);
    ASSERT(cpu65_current.a      == 0x02);
    ASSERT(cpu65_current.x      == 0x03);
    ASSERT(cpu65_current.y      == 0x04);
    ASSERT(cpu65_current.f      == (B_Flag_6502|X_Flag_6502|I_Flag_6502));
    ASSERT(cpu65_current.sp     == 0xfc);

    ASSERT(apple_ii_64k[0][0x1ff] == 0x1f);
    ASSERT(apple_ii_64k[0][0x1fe] == 0x02);
    ASSERT(apple_ii_64k[0][0x1fd] == cpu65_flags_encode[B_Flag|X_Flag]);

    ASSERT(cpu65_debug.ea       == 0xfffe);
    ASSERT(cpu65_debug.d        == 0xff);
    ASSERT(cpu65_debug.rw       == RW_NONE);
    ASSERT(cpu65_debug.opcode   == 0x0);
    ASSERT(cpu65_debug.opcycles == 7);

    PASS();
}

TEST test_IRQ() {
#warning TODO : test IRQ handling ...
    // NOTE : not an opcode
    PASS();
}

// ----------------------------------------------------------------------------
// NOP operand

TEST test_NOP() {
    testcpu_set_opcode(0xea, 0x0, 0x0);

    cpu65_current.a = 0x02;
    cpu65_current.x = 0x03;
    cpu65_current.y = 0x04;
    cpu65_current.f = 0x05;
    cpu65_current.sp = 0x80;

    cpu65_run();

    ASSERT(cpu65_current.pc     == 0x1f01);
    ASSERT(cpu65_current.a      == 0x02);
    ASSERT(cpu65_current.x      == 0x03);
    ASSERT(cpu65_current.y      == 0x04);
    ASSERT(cpu65_current.f      == 0x05);
    ASSERT(cpu65_current.sp     == 0x80);

    ASSERT(cpu65_debug.ea       == 0x1f00);
    ASSERT(cpu65_debug.d        == 0xff);
    ASSERT(cpu65_debug.rw       == RW_NONE);
    ASSERT(cpu65_debug.opcode   == 0xea);
    ASSERT(cpu65_debug.opcycles == 2);

    PASS();
}

// ----------------------------------------------------------------------------

GREATEST_SUITE(test_suite_cpu) {

    GREATEST_SET_SETUP_CB(testcpu_setup, NULL);
    GREATEST_SET_TEARDOWN_CB(testcpu_teardown, NULL);

    load_settings();
    sound_volume = 0;
    do_logging = false;// silence regular emulator logging

    c_initialize_firsttime();

    RUN_TESTp(test_ADC_imm, /*decimal*/false, /*argA*/0x02, /*arg0*/0x1a, /*result*/0x1c, /*flags*/0x0);
    RUN_TESTp(test_ADC_imm, /*decimal*/false, /*argA*/0x02, /*arg0*/0xdc, /*result*/0xde, /*flags*/(N_Flag_6502));
    RUN_TESTp(test_ADC_imm, /*decimal*/false, /*argA*/0x01, /*arg0*/0xff, /*result*/0x00, /*flags*/(C_Flag_6502|Z_Flag_6502));
    RUN_TESTp(test_ADC_imm, /*decimal*/false, /*argA*/0xff, /*arg0*/0xff, /*result*/0xfe, /*flags*/(C_Flag_6502|N_Flag_6502));
    RUN_TESTp(test_ADC_imm, /*decimal*/false, /*argA*/0x00, /*arg0*/0x00, /*result*/0x00, /*flags*/(Z_Flag_6502));
    RUN_TESTp(test_ADC_imm, /*decimal*/false, /*argA*/0x02, /*arg0*/0x7f, /*result*/0x81, /*flags*/(N_Flag_6502|V_Flag_6502));
    RUN_TESTp(test_ADC_imm, /*decimal*/false, /*argA*/0x80, /*arg0*/0x80, /*result*/0x00, /*flags*/(C_Flag_6502|V_Flag_6502|Z_Flag_6502));
    RUN_TESTp(test_ADC_imm, /*decimal*/false, /*argA*/0x81, /*arg0*/0x80, /*result*/0x01, /*flags*/(C_Flag_6502|V_Flag_6502));

    RUN_TESTp(test_ADC_imm, /*decimal*/ true, /*argA*/0x17, /*arg0*/0x19, /*result*/0x36, /*flags*/(D_Flag_6502));
    RUN_TESTp(test_ADC_imm, /*decimal*/ true, /*argA*/0x99, /*arg0*/0x02, /*result*/0x01, /*flags*/(D_Flag_6502|C_Flag_6502));
    RUN_TESTp(test_ADC_imm, /*decimal*/ true, /*argA*/0x99, /*arg0*/0x01, /*result*/0x00, /*flags*/(D_Flag_6502|C_Flag_6502|Z_Flag_6502));
    RUN_TESTp(test_ADC_imm, /*decimal*/ true, /*argA*/0xA0, /*arg0*/0x01, /*result*/0x01, /*flags*/(D_Flag_6502|C_Flag_6502));
    RUN_TESTp(test_ADC_imm, /*decimal*/ true, /*argA*/0xAA, /*arg0*/0x01, /*result*/0x11, /*flags*/(D_Flag_6502|C_Flag_6502));
    RUN_TESTp(test_ADC_imm, /*decimal*/ true, /*argA*/0xBA, /*arg0*/0x01, /*result*/0x21, /*flags*/(D_Flag_6502|C_Flag_6502));


    RUN_TEST(test_BRK);
    RUN_TEST(test_IRQ);

    RUN_TEST(test_NOP);
}

