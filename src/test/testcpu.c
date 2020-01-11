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

//
// Tests for virtual 65c02 CPU (opcodes and addressing modes)
//

#include "testcommon.h"

#define MSG_SIZE 256

#define fC C_Flag_6502  // [C]arry
#define fZ Z_Flag_6502  // [Z]ero
#define fI I_Flag_6502  // [I]nterrupt
#define fD D_Flag_6502  // [D]ecimal
#define fB B_Flag_6502  // [B]reak
#define fX X_Flag_6502  // [X]tra (reserved)...
#define fV V_Flag_6502  // o[V]erflow
#define fN N_Flag_6502  // [N]egative

#define TEST_LOC 0x1f82
#define TEST_LOC_LO 0x82

#define RW_NONE 0x0
#define RW_READ 0x1
#define RW_WRITE 0x2

#define MSG_FLAGS0 "A:%02X op %02X = %02X : %s (emul2ix = %02X : %s)"

#define VERIFY_FLAGS() \
    do { \
        char msgbuf0[MSG_SIZE]; \
        char bufA[MSG_SIZE]; \
        char bufB[MSG_SIZE]; \
        flags_to_string(flags, bufA); \
        flags_to_string(run_args.cpu65_f, bufB); \
        snprintf(msgbuf0, MSG_SIZE, MSG_FLAGS0, regA, val, result, bufA, run_args.cpu65_a, bufB); \
        ASSERTm(msgbuf0, run_args.cpu65_f == flags); \
    } while (0)

static void testcpu_setup(void *arg) {

    cpu65_uninterrupt(0xff);
    run_args.cpu65_cycle_count = 0;
    run_args.cpu65_cycles_to_execute = 1;

    run_args.cpu65_pc = TEST_LOC;
    run_args.cpu65_a = 0x0;
    run_args.cpu65_x = 0x0;
    run_args.cpu65_y = 0x0;
    run_args.cpu65_f = 0x0;
    run_args.cpu65_sp = 0xff;

    run_args.cpu65_ea = 0xffff;
    run_args.cpu65_d = 0xff;
    run_args.cpu65_rw = 0xff;
    run_args.cpu65_opcode = 0xff;
    run_args.cpu65_opcycles = 0xff;

    // clear ZP & stack memory
    memset(apple_ii_64k, 0x0, 0x200);

    // clear prog memory and absolute addressing test locations
    memset(((void*)apple_ii_64k)+TEST_LOC, 0x0, 0x300);
}

static void testcpu_teardown(void *arg) {
    // ...
}

static void testcpu_set_opcodeX(uint8_t op, uint8_t val, uint8_t arg1, uint16_t addrs) {
    run_args.cpu65_pc = addrs;
    apple_ii_64k[0][addrs] = op;
    apple_ii_64k[0][(uint16_t)(addrs+1)] = val;
    apple_ii_64k[0][(uint16_t)(addrs+2)] = arg1;
}

static void testcpu_set_opcode3(uint8_t op, uint8_t val, uint8_t arg1) {
    testcpu_set_opcodeX(op, val, arg1, TEST_LOC);
}

static void testcpu_set_opcode2(uint8_t op, uint8_t val) {
    uint8_t arg1 = (uint8_t)random();
    testcpu_set_opcode3(op, val, arg1);
}

static void testcpu_set_opcode1(uint8_t op) {
    uint8_t val = (uint8_t)random();
    uint8_t arg1 = (uint8_t)random();
    testcpu_set_opcode3(op, val, arg1);
}

static bool check_skip_illegal_bcd(uint8_t bcd0, uint8_t bcd1) {
    if (bcd0 >= 0xA0) {
        return true;
    }
    if ((bcd0 & 0x0f) >= 0x0A) {
        return true;
    }

    if (bcd1 >= 0xA0) {
        return true;
    }
    if ((bcd1 & 0x0f) >= 0x0A) {
        return true;
    }

    return false;
}

static void flags_to_string(uint8_t flags, char *buf) {
    snprintf(buf, MSG_SIZE, "%c%c%c%c%c%c%c%c",
        (flags & N_Flag_6502) ? 'N' : '-',
        (flags & V_Flag_6502) ? 'V' : '-',
        (flags & X_Flag_6502) ? 'X' : '-',
        (flags & B_Flag_6502) ? 'B' : '-',
        (flags & D_Flag_6502) ? 'D' : '-',
        (flags & I_Flag_6502) ? 'I' : '-',
        (flags & Z_Flag_6502) ? 'Z' : '-',
        (flags & C_Flag_6502) ? 'C' : '-' );
}

// ----------------------------------------------------------------------------
// ADC instructions

static void logic_ADC_dec(/*uint8_t*/int _a, /*uint8_t*/int _b, uint8_t *result, uint8_t *flags) {

    // componentize
    uint8_t x_lo = 0x0;
    uint8_t x_hi = 0x0;

    uint8_t a_lo = (((uint8_t)_a) & 0x0f);
    uint8_t b_lo = (((uint8_t)_b) & 0x0f);

    uint8_t a_hi = (((uint8_t)_a) & 0xf0)>>4;
    uint8_t b_hi = (((uint8_t)_b) & 0xf0)>>4;

    uint8_t carry = (*flags & fC) ? 1 : 0;
    *flags &= ~ fC;

    // BCD add
    x_lo = a_lo + b_lo + carry;
    carry = 0;
    if (x_lo > 9) {
        x_lo += 6;
        carry = (x_lo>>4); // +1 or +2
        x_lo &= 0x0f;
    }
    x_hi = a_hi + b_hi + carry;
    if (x_hi > 9) {
        *flags |= fC;
        x_hi += 6;
    }

    // merge result
    x_hi <<= 4;
    *result = x_hi | x_lo;

    // flags
    if (*result == 0x00) {
        *flags |= fZ;
    }

    if (*result & 0x80) {
        *flags |= fN;
    }
}

static void logic_ADC(/*uint8_t*/int _a, /*uint8_t*/int _b, uint8_t *result, uint8_t *flags) {

    if (*flags & fD) {
        logic_ADC_dec(_a, _b, result, flags);
        return;
    }

    int8_t a = (int8_t)_a;
    int8_t b = (int8_t)_b;
    bool signA = a>>7;
    bool signB = b>>7;

    int8_t carry = (*flags & fC) ? 1 : 0;
    *flags &= ~fC;

    int8_t res = a + b + carry;

    if ((res & 0xff) == 0x0) {
        *flags |= fZ;
    }
    if (res & 0x80) {
        *flags |= fN;
    }

    int32_t res32 = (uint8_t)a+(uint8_t)b+(uint8_t)carry;
    if (res32 & 0x00000100) {
        *flags |= fC;
    }

    if (signA == signB) {
        uint8_t signResult = (res&0xff)>>7;
        if (signA != signResult) {
            *flags |= fV;
        }
    }

    *result = (uint8_t)(res & 0xff);
}

TEST test_ADC_imm(uint8_t regA, uint8_t val, bool decimal, bool carry) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    flags |= decimal ? (fD) : 0x00;
    flags |= carry   ? (fC) : 0x00;

    if (decimal && check_skip_illegal_bcd(regA, val)) {
        // NOTE : FIXME TODO skip undocumented/illegal BCD
        SKIPm("Z");
    }

    logic_ADC(regA, val, &result, &flags);

    testcpu_set_opcode2(0x69, val);

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f |= decimal ? (fD) : 0x00;
    run_args.cpu65_f |= carry   ? (fC) : 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_x       == 0x03);
    ASSERT(run_args.cpu65_y       == 0x04);
    ASSERT(run_args.cpu65_sp      == 0x80);

    do {
        char msgbuf[MSG_SIZE];
        char buf0[MSG_SIZE];
        char buf1[MSG_SIZE];
        snprintf(msgbuf, MSG_SIZE, MSG_FLAGS0, regA, val, result, buf0, run_args.cpu65_a, buf1);
        ASSERTm(msgbuf, run_args.cpu65_a == result);
        VERIFY_FLAGS();
    } while (0);

    ASSERT(run_args.cpu65_ea        == TEST_LOC+1);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0x69);
    ASSERT(run_args.cpu65_opcycles  == (decimal ? 3 : 2));

    PASS();
}

TEST test_ADC_zpage(uint8_t regA, uint8_t val, uint8_t arg0) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_ADC(regA, val, &result, &flags);

    testcpu_set_opcode2(0x65, arg0);

    apple_ii_64k[0][arg0] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x80;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_x       == 0x03);
    ASSERT(run_args.cpu65_y       == 0x04);
    ASSERT(run_args.cpu65_sp      == 0x80);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == arg0);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0x65);
    ASSERT(run_args.cpu65_opcycles  == (3));

    PASS();
}

TEST test_ADC_zpage_x(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t regX) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_ADC(regA, val, &result, &flags);

    uint8_t idx = arg0 + regX;

    testcpu_set_opcode2(0x75, arg0);

    apple_ii_64k[0][idx] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_x       == regX);
    ASSERT(run_args.cpu65_y       == 0x05);
    ASSERT(run_args.cpu65_sp      == 0x81);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == idx);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0x75);
    ASSERT(run_args.cpu65_opcycles  == (4));

    PASS();
}

TEST test_ADC_abs(uint8_t regA, uint8_t val, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_ADC(regA, val, &result, &flags);

    testcpu_set_opcode3(0x6d, lobyte, hibyte);

    uint16_t addrs = lobyte | (hibyte<<8);
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0xf4;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+3);
    ASSERT(run_args.cpu65_x       == 0xf4);
    ASSERT(run_args.cpu65_y       == 0x05);
    ASSERT(run_args.cpu65_sp      == 0x81);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0x6d);
    ASSERT(run_args.cpu65_opcycles  == (4));

    PASS();
}

TEST test_ADC_abs_x(uint8_t regA, uint8_t val, uint8_t regX, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_ADC(regA, val, &result, &flags);

    testcpu_set_opcode3(0x7d, lobyte, hibyte);

    uint8_t cycle_count = 0;
    uint16_t addrs = lobyte | (hibyte<<8);
    addrs = addrs + regX;
    if ((uint8_t)((addrs>>8)&0xff) != (uint8_t)hibyte) {
        ++cycle_count;
    }
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+3);
    ASSERT(run_args.cpu65_x       == regX);
    ASSERT(run_args.cpu65_y       == 0x05);
    ASSERT(run_args.cpu65_sp      == 0x81);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0x7d);

    cycle_count += 4;
    ASSERT(run_args.cpu65_opcycles == cycle_count);

    PASS();
}

TEST test_ADC_abs_y(uint8_t regA, uint8_t val, uint8_t regY, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_ADC(regA, val, &result, &flags);

    testcpu_set_opcode3(0x79, lobyte, hibyte);

    uint8_t cycle_count = 0;
    uint16_t addrs = lobyte | (hibyte<<8);
    addrs = addrs + regY;
    if ((uint8_t)((addrs>>8)&0xff) != (uint8_t)hibyte) {
        ++cycle_count;
    }
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x02;
    run_args.cpu65_y  = regY;
    run_args.cpu65_sp = 0x81;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+3);
    ASSERT(run_args.cpu65_x       == 0x02);
    ASSERT(run_args.cpu65_y       == regY);
    ASSERT(run_args.cpu65_sp      == 0x81);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0x79);

    cycle_count += 4;
    ASSERT(run_args.cpu65_opcycles == cycle_count);

    PASS();
}

TEST test_ADC_ind_x(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t regX, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_ADC(regA, val, &result, &flags);

    testcpu_set_opcode2(0x61, arg0);

    uint8_t idx_lo = arg0 + regX;
    uint8_t idx_hi = idx_lo+1;
    uint16_t addrs = lobyte | (hibyte<<8);

    apple_ii_64k[0][idx_lo] = lobyte;
    apple_ii_64k[0][idx_hi] = hibyte;
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x15;
    run_args.cpu65_sp = 0x81;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_x       == regX);
    ASSERT(run_args.cpu65_y       == 0x15);
    ASSERT(run_args.cpu65_sp      == 0x81);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0x61);

    ASSERT(run_args.cpu65_opcycles  == (6));

    PASS();
}

TEST test_ADC_ind_y(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t regY, uint8_t val_zp0, uint8_t val_zp1) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_ADC(regA, val, &result, &flags);

    testcpu_set_opcode2(0x71, arg0);

    uint8_t idx0 = arg0;
    uint8_t idx1 = arg0+1;

    apple_ii_64k[0][idx0] = val_zp0;
    apple_ii_64k[0][idx1] = val_zp1;

    uint8_t cycle_count = 0;
    uint16_t addrs = val_zp0 | (val_zp1<<8);
    addrs += (uint8_t)regY;
    if ((uint8_t)((addrs>>8)&0xff) != (uint8_t)val_zp1) {
        ++cycle_count;
    }

    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x84;
    run_args.cpu65_y  = regY;
    run_args.cpu65_sp = 0x81;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_x       == 0x84);
    ASSERT(run_args.cpu65_y       == regY);
    ASSERT(run_args.cpu65_sp      == 0x81);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0x71);
    cycle_count += 5;
    ASSERT(run_args.cpu65_opcycles == cycle_count);

    PASS();
}

TEST test_ADC_ind_zpage(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_ADC(regA, val, &result, &flags);

    testcpu_set_opcode2(0x72, arg0);

    uint8_t idx0 = arg0;
    uint8_t idx1 = arg0+1;

    apple_ii_64k[0][idx0] = lobyte;
    apple_ii_64k[0][idx1] = hibyte;

    uint16_t addrs = lobyte | (hibyte<<8);
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x14;
    run_args.cpu65_y  = 0x85;
    run_args.cpu65_sp = 0x81;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_x       == 0x14);
    ASSERT(run_args.cpu65_y       == 0x85);
    ASSERT(run_args.cpu65_sp      == 0x81);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0x72);
    ASSERT(run_args.cpu65_opcycles  == (5));

    PASS();
}

// ----------------------------------------------------------------------------
// AND instructions

static void logic_AND(/*uint8_t*/int _a, /*uint8_t*/int _b, uint8_t *result, uint8_t *flags) {
    uint8_t a = (uint8_t)_a;
    uint8_t b = (uint8_t)_b;

    uint8_t res = a & b;
    if ((res & 0xff) == 0x0) {
        *flags |= fZ;
    }
    if (res & 0x80) {
        *flags |= fN;
    }

    *result = res;
}

TEST test_AND_imm(uint8_t regA, uint8_t val) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_AND(regA, val, &result, &flags);

    testcpu_set_opcode2(0x29, val);

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_x       == 0x03);
    ASSERT(run_args.cpu65_y       == 0x04);
    ASSERT(run_args.cpu65_sp      == 0x80);

    do {
        char msgbuf[MSG_SIZE];
        char buf0[MSG_SIZE];
        char buf1[MSG_SIZE];
        snprintf(msgbuf, MSG_SIZE, MSG_FLAGS0, regA, val, result, buf0, run_args.cpu65_a, buf1);
        ASSERTm(msgbuf, run_args.cpu65_a == result);
        VERIFY_FLAGS();
    } while (0);

    ASSERT(run_args.cpu65_ea        == TEST_LOC+1);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0x29);
    ASSERT(run_args.cpu65_opcycles  == (2));

    PASS();
}

TEST test_AND_zpage(uint8_t regA, uint8_t val, uint8_t arg0) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    logic_AND(regA, val, &result, &flags);

    testcpu_set_opcode2(0x25, arg0);

    apple_ii_64k[0][arg0] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_x       == 0x03);
    ASSERT(run_args.cpu65_y       == 0x04);
    ASSERT(run_args.cpu65_sp      == 0x80);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == arg0);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0x25);
    ASSERT(run_args.cpu65_opcycles  == (3));

    PASS();
}

TEST test_AND_zpage_x(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t regX) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    logic_AND(regA, val, &result, &flags);

    testcpu_set_opcode2(0x35, arg0);

    uint8_t idx = arg0+regX;

    apple_ii_64k[0][idx] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_x       == regX);
    ASSERT(run_args.cpu65_y       == 0x05);
    ASSERT(run_args.cpu65_sp      == 0x81);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == idx);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0x35);
    ASSERT(run_args.cpu65_opcycles  == (4));

    PASS();
}

TEST test_AND_abs(uint8_t regA, uint8_t val, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    logic_AND(regA, val, &result, &flags);

    testcpu_set_opcode3(0x2d, lobyte, hibyte);

    uint16_t addrs = lobyte | (hibyte<<8);
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0xf4;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+3);
    ASSERT(run_args.cpu65_x       == 0xf4);
    ASSERT(run_args.cpu65_y       == 0x05);
    ASSERT(run_args.cpu65_sp      == 0x81);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0x2d);
    ASSERT(run_args.cpu65_opcycles  == (4));

    PASS();
}

TEST test_AND_abs_x(uint8_t regA, uint8_t val, uint8_t regX, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    logic_AND(regA, val, &result, &flags);

    testcpu_set_opcode3(0x3d, lobyte, hibyte);

    uint8_t cycle_count = 4;
    uint16_t addrs = lobyte | (hibyte<<8);
    addrs = addrs + regX;
    if ((uint8_t)((addrs>>8)&0xff) != (uint8_t)hibyte) {
        ++cycle_count;
    }
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+3);
    ASSERT(run_args.cpu65_x       == regX);
    ASSERT(run_args.cpu65_y       == 0x05);
    ASSERT(run_args.cpu65_sp      == 0x81);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0x3d);
    ASSERT(run_args.cpu65_opcycles  == cycle_count);

    PASS();
}

TEST test_AND_abs_y(uint8_t regA, uint8_t val, uint8_t regY, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    logic_AND(regA, val, &result, &flags);

    testcpu_set_opcode3(0x39, lobyte, hibyte);

    uint8_t cycle_count = 4;
    uint16_t addrs = lobyte | (hibyte<<8);
    addrs = addrs + regY;
    if ((uint8_t)((addrs>>8)&0xff) != (uint8_t)hibyte) {
        ++cycle_count;
    }
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x02;
    run_args.cpu65_y  = regY;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+3);
    ASSERT(run_args.cpu65_x       == 0x02);
    ASSERT(run_args.cpu65_y       == regY);
    ASSERT(run_args.cpu65_sp      == 0x81);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0x39);
    ASSERT(run_args.cpu65_opcycles == cycle_count);

    PASS();
}

TEST test_AND_ind_x(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t regX, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    logic_AND(regA, val, &result, &flags);

    testcpu_set_opcode2(0x21, arg0);

    uint8_t idx_lo = arg0 + regX;
    uint8_t idx_hi = idx_lo+1;
    uint16_t addrs = lobyte | (hibyte<<8);

    apple_ii_64k[0][idx_lo] = lobyte;
    apple_ii_64k[0][idx_hi] = hibyte;
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x15;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_x       == regX);
    ASSERT(run_args.cpu65_y       == 0x15);
    ASSERT(run_args.cpu65_sp      == 0x81);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0x21);

    ASSERT(run_args.cpu65_opcycles  == (6));

    PASS();
}

TEST test_AND_ind_y(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t regY, uint8_t val_zp0, uint8_t val_zp1) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    logic_AND(regA, val, &result, &flags);

    testcpu_set_opcode2(0x31, arg0);

    uint8_t idx0 = arg0;
    uint8_t idx1 = arg0+1;

    apple_ii_64k[0][idx0] = val_zp0;
    apple_ii_64k[0][idx1] = val_zp1;

    uint8_t cycle_count = 5;
    uint16_t addrs = val_zp0 | (val_zp1<<8);
    addrs += (uint8_t)regY;
    if ((uint8_t)((addrs>>8)&0xff) != (uint8_t)val_zp1) {
        ++cycle_count;
    }

    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x84;
    run_args.cpu65_y  = regY;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_x       == 0x84);
    ASSERT(run_args.cpu65_y       == regY);
    ASSERT(run_args.cpu65_sp      == 0x81);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0x31);
    ASSERT(run_args.cpu65_opcycles  == cycle_count);

    PASS();
}

TEST test_AND_ind_zpage(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    logic_AND(regA, val, &result, &flags);

    testcpu_set_opcode2(0x32, arg0);

    uint8_t idx0 = arg0;
    uint8_t idx1 = arg0+1;

    apple_ii_64k[0][idx0] = lobyte;
    apple_ii_64k[0][idx1] = hibyte;

    uint16_t addrs = lobyte | (hibyte<<8);
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x14;
    run_args.cpu65_y  = 0x85;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_x       == 0x14);
    ASSERT(run_args.cpu65_y       == 0x85);
    ASSERT(run_args.cpu65_sp      == 0x81);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0x32);
    ASSERT(run_args.cpu65_opcycles  == (5));

    PASS();
}

// ----------------------------------------------------------------------------
// ASL instructions

static void logic_ASL(/*uint8_t*/int _a, uint8_t *result, uint8_t *flags) {
    uint8_t a = (uint8_t)_a;

    *flags |= (a & 0x80) ? fC : 0;
    uint8_t res = a<<1;

    if ((res & 0xff) == 0x0) {
        *flags |= fZ;
    }
    if (res & 0x80) {
        *flags |= fN;
    }

    *result = res;
}

TEST test_ASL_acc(uint8_t regA) {
    uint8_t val = 0xff;
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_ASL(regA, &result, &flags);

    testcpu_set_opcode1(0x0a);

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+1);
    ASSERT(run_args.cpu65_x       == 0x03);
    ASSERT(run_args.cpu65_y       == 0x04);
    ASSERT(run_args.cpu65_sp      == 0x80);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == TEST_LOC);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == 0);
    ASSERT(run_args.cpu65_opcode    == 0x0a);
    ASSERT(run_args.cpu65_opcycles  == (2));

    PASS();
}

TEST test_ASL_zpage(uint8_t regA, uint8_t val, uint8_t arg0) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    logic_ASL(val, &result, &flags);

    testcpu_set_opcode2(0x06, arg0);

    apple_ii_64k[0][arg0] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(apple_ii_64k[0][arg0] == result);

    ASSERT(run_args.cpu65_a  == regA);
    ASSERT(run_args.cpu65_pc == TEST_LOC+2);
    ASSERT(run_args.cpu65_x  == 0x03);
    ASSERT(run_args.cpu65_y  == 0x04);
    ASSERT(run_args.cpu65_sp == 0x80);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == arg0);
    ASSERT(run_args.cpu65_d         == result);
    ASSERT(run_args.cpu65_rw        == (RW_READ|RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0x06);
    ASSERT(run_args.cpu65_opcycles  == (5));

    PASS();
}

TEST test_ASL_zpage_x(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t regX) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    logic_ASL(val, &result, &flags);

    testcpu_set_opcode2(0x16, arg0);

    uint8_t idx = arg0+regX;

    apple_ii_64k[0][idx] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(apple_ii_64k[0][idx] == result);

    ASSERT(run_args.cpu65_a   == regA);
    ASSERT(run_args.cpu65_pc  == TEST_LOC+2);
    ASSERT(run_args.cpu65_x   == regX);
    ASSERT(run_args.cpu65_y   == 0x05);
    ASSERT(run_args.cpu65_sp  == 0x81);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == idx);
    ASSERT(run_args.cpu65_d         == result);
    ASSERT(run_args.cpu65_rw        == (RW_READ|RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0x16);
    ASSERT(run_args.cpu65_opcycles  == (6));

    PASS();
}

TEST test_ASL_abs(uint8_t regA, uint8_t val, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    logic_ASL(val, &result, &flags);

    testcpu_set_opcode3(0x0e, lobyte, hibyte);

    uint16_t addrs = lobyte | (hibyte<<8);
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0xf4;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(apple_ii_64k[0][addrs] == result);

    ASSERT(run_args.cpu65_a  == regA);
    ASSERT(run_args.cpu65_pc == TEST_LOC+3);
    ASSERT(run_args.cpu65_x  == 0xf4);
    ASSERT(run_args.cpu65_y  == 0x05);
    ASSERT(run_args.cpu65_sp == 0x81);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == result);
    ASSERT(run_args.cpu65_rw        == (RW_READ|RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0x0e);
    ASSERT(run_args.cpu65_opcycles  == (6));

    PASS();
}

TEST test_ASL_abs_x(uint8_t regA, uint8_t val, uint8_t regX, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    logic_ASL(val, &result, &flags);

    testcpu_set_opcode3(0x1e, lobyte, hibyte);

    uint8_t cycle_count = 6;
    uint16_t addrs = lobyte | (hibyte<<8);
    addrs = addrs + regX;
    if ((uint8_t)((addrs>>8)&0xff) != (uint8_t)hibyte) {
        ++cycle_count;
    }
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(apple_ii_64k[0][addrs] == result);

    ASSERT(run_args.cpu65_a  == regA);
    ASSERT(run_args.cpu65_pc == TEST_LOC+3);
    ASSERT(run_args.cpu65_x  == regX);
    ASSERT(run_args.cpu65_y  == 0x05);
    ASSERT(run_args.cpu65_sp == 0x81);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == result);
    ASSERT(run_args.cpu65_rw        == (RW_READ|RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0x1e);
    ASSERT(run_args.cpu65_opcycles  == cycle_count);

    PASS();
}

// ----------------------------------------------------------------------------
// Branch instructions

TEST test_BCC(int8_t off, bool flag, uint16_t addrs) {
    uint8_t flags = 0x0;

    run_args.cpu65_pc = addrs;
    flags |= flag ? fC : 0;

    uint8_t cycle_count = 2;
    uint16_t newpc = addrs+2;
    if (!flag) {
        uint16_t prebranch = newpc;
        newpc += off;
        ++cycle_count;
        if ((newpc&0xFF00) != (prebranch&0xFF00)) {
            ++cycle_count;
       }
    }

    apple_ii_64k[0][addrs] = 0x90;
    apple_ii_64k[0][(uint16_t)(addrs+1)] = off;
    apple_ii_64k[0][(uint16_t)(addrs+2)] = (uint8_t)random();

    run_args.cpu65_a  = 0xed;
    run_args.cpu65_x  = 0xde;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = flags;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc == newpc);
    ASSERT(run_args.cpu65_a  == 0xed);
    ASSERT(run_args.cpu65_x  == 0xde);
    ASSERT(run_args.cpu65_y  == 0x05);
    ASSERT(run_args.cpu65_sp == 0x81);
    ASSERT(run_args.cpu65_f  == flags);

    ASSERT(run_args.cpu65_ea        == (uint16_t)(addrs+1));
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_NONE);
    ASSERT(run_args.cpu65_opcode    == 0x90);
    ASSERT(run_args.cpu65_opcycles  == cycle_count);

    PASS();
}

TEST test_BCS(int8_t off, bool flag, uint16_t addrs) {
    uint8_t flags = 0x0;

    run_args.cpu65_pc = addrs;
    flags |= flag ? fC : 0;

    uint8_t cycle_count = 2;
    uint16_t newpc = addrs+2;
    if (flag) {
        uint16_t prebranch = newpc;
        newpc += off;
        ++cycle_count;
        if ((newpc&0xFF00) != (prebranch&0xFF00)) {
            ++cycle_count;
        }
    }

    apple_ii_64k[0][addrs] = 0xB0;
    apple_ii_64k[0][(uint16_t)(addrs+1)] = off;
    apple_ii_64k[0][(uint16_t)(addrs+2)] = (uint8_t)random();

    run_args.cpu65_a  = 0xed;
    run_args.cpu65_x  = 0xde;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = flags;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc == newpc);
    ASSERT(run_args.cpu65_a  == 0xed);
    ASSERT(run_args.cpu65_x  == 0xde);
    ASSERT(run_args.cpu65_y  == 0x05);
    ASSERT(run_args.cpu65_sp == 0x81);
    ASSERT(run_args.cpu65_f  == flags);

    ASSERT(run_args.cpu65_ea        == (uint16_t)(addrs+1));
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_NONE);
    ASSERT(run_args.cpu65_opcode    == 0xB0);
    ASSERT(run_args.cpu65_opcycles  == cycle_count);

    PASS();
}

TEST test_BEQ(int8_t off, bool flag, uint16_t addrs) {
    uint8_t flags = 0x0;

    run_args.cpu65_pc = addrs;
    flags |= flag ? fZ : 0;

    uint8_t cycle_count = 2;
    uint16_t newpc = addrs+2;
    if (flag) {
        uint16_t prebranch = newpc;
        newpc += off;
        ++cycle_count;
        if ((newpc&0xFF00) != (prebranch&0xFF00)) {
            ++cycle_count;
        }
    }

    apple_ii_64k[0][addrs] = 0xF0;
    apple_ii_64k[0][(uint16_t)(addrs+1)] = off;
    apple_ii_64k[0][(uint16_t)(addrs+2)] = (uint8_t)random();

    run_args.cpu65_a  = 0xed;
    run_args.cpu65_x  = 0xde;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = flags;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc == newpc);
    ASSERT(run_args.cpu65_a  == 0xed);
    ASSERT(run_args.cpu65_x  == 0xde);
    ASSERT(run_args.cpu65_y  == 0x05);
    ASSERT(run_args.cpu65_sp == 0x81);
    ASSERT(run_args.cpu65_f  == flags);

    ASSERT(run_args.cpu65_ea        == (uint16_t)(addrs+1));
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_NONE);
    ASSERT(run_args.cpu65_opcode    == 0xF0);
    ASSERT(run_args.cpu65_opcycles  == cycle_count);

    PASS();
}

TEST test_BNE(int8_t off, bool flag, uint16_t addrs) {
    uint8_t flags = 0x0;

    run_args.cpu65_pc = addrs;
    flags |= flag ? fZ : 0;

    uint8_t cycle_count = 2;
    uint16_t newpc = addrs+2;
    if (!flag) {
        uint16_t prebranch = newpc;
        newpc += off;
        ++cycle_count;
        if ((newpc&0xFF00) != (prebranch&0xFF00)) {
            ++cycle_count;
        }
    }

    apple_ii_64k[0][addrs] = 0xD0;
    apple_ii_64k[0][(uint16_t)(addrs+1)] = off;
    apple_ii_64k[0][(uint16_t)(addrs+2)] = (uint8_t)random();

    run_args.cpu65_a  = 0xed;
    run_args.cpu65_x  = 0xde;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = flags;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc == newpc);
    ASSERT(run_args.cpu65_a  == 0xed);
    ASSERT(run_args.cpu65_x  == 0xde);
    ASSERT(run_args.cpu65_y  == 0x05);
    ASSERT(run_args.cpu65_sp == 0x81);
    ASSERT(run_args.cpu65_f  == flags);

    ASSERT(run_args.cpu65_ea        == (uint16_t)(addrs+1));
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_NONE);
    ASSERT(run_args.cpu65_opcode    == 0xD0);
    ASSERT(run_args.cpu65_opcycles  == cycle_count);

    PASS();
}

TEST test_BMI(int8_t off, bool flag, uint16_t addrs) {
    uint8_t flags = 0x0;

    run_args.cpu65_pc = addrs;
    flags |= flag ? fN : 0;

    uint8_t cycle_count = 2;
    uint16_t newpc = addrs+2;
    if (flag) {
        uint16_t prebranch = newpc;
        newpc += off;
        ++cycle_count;
        if ((newpc&0xFF00) != (prebranch&0xFF00)) {
            ++cycle_count;
        }
    }

    apple_ii_64k[0][addrs] = 0x30;
    apple_ii_64k[0][(uint16_t)(addrs+1)] = off;
    apple_ii_64k[0][(uint16_t)(addrs+2)] = (uint8_t)random();

    run_args.cpu65_a  = 0xed;
    run_args.cpu65_x  = 0xde;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = flags;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc == newpc);
    ASSERT(run_args.cpu65_a  == 0xed);
    ASSERT(run_args.cpu65_x  == 0xde);
    ASSERT(run_args.cpu65_y  == 0x05);
    ASSERT(run_args.cpu65_sp == 0x81);
    ASSERT(run_args.cpu65_f  == flags);

    ASSERT(run_args.cpu65_ea        == (uint16_t)(addrs+1));
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_NONE);
    ASSERT(run_args.cpu65_opcode    == 0x30);
    ASSERT(run_args.cpu65_opcycles  == cycle_count);

    PASS();
}

TEST test_BPL(int8_t off, bool flag, uint16_t addrs) {
    uint8_t flags = 0x0;

    run_args.cpu65_pc = addrs;
    flags |= flag ? fN : 0;

    uint8_t cycle_count = 2;
    uint16_t newpc = addrs+2;
    if (!flag) {
        uint16_t prebranch = newpc;
        newpc += off;
        ++cycle_count;
        if ((newpc&0xFF00) != (prebranch&0xFF00)) {
            ++cycle_count;
        }
    }

    apple_ii_64k[0][addrs] = 0x10;
    apple_ii_64k[0][(uint16_t)(addrs+1)] = off;
    apple_ii_64k[0][(uint16_t)(addrs+2)] = (uint8_t)random();

    run_args.cpu65_a  = 0xed;
    run_args.cpu65_x  = 0xde;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = flags;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc == newpc);
    ASSERT(run_args.cpu65_a  == 0xed);
    ASSERT(run_args.cpu65_x  == 0xde);
    ASSERT(run_args.cpu65_y  == 0x05);
    ASSERT(run_args.cpu65_sp == 0x81);
    ASSERT(run_args.cpu65_f  == flags);

    ASSERT(run_args.cpu65_ea        == (uint16_t)(addrs+1));
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_NONE);
    ASSERT(run_args.cpu65_opcode    == 0x10);
    ASSERT(run_args.cpu65_opcycles  == cycle_count);

    PASS();
}

TEST test_BRA(int8_t off, bool flag, uint16_t addrs) {
    uint8_t flags = 0x0;

    run_args.cpu65_pc = addrs;
    flags |= flag ? fN : 0;

    uint8_t cycle_count = 3;
    uint16_t newpc = addrs+2;
    uint16_t prebranch = newpc;
    newpc += off;
    if ((newpc&0xFF00) != (prebranch&0xFF00)) {
        ++cycle_count;
    }

    apple_ii_64k[0][addrs] = 0x80;
    apple_ii_64k[0][(uint16_t)(addrs+1)] = off;
    apple_ii_64k[0][(uint16_t)(addrs+2)] = (uint8_t)random();

    run_args.cpu65_a  = 0xed;
    run_args.cpu65_x  = 0xde;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = flags;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc == newpc);
    ASSERT(run_args.cpu65_a  == 0xed);
    ASSERT(run_args.cpu65_x  == 0xde);
    ASSERT(run_args.cpu65_y  == 0x05);
    ASSERT(run_args.cpu65_sp == 0x81);
    ASSERT(run_args.cpu65_f  == flags);

    ASSERT(run_args.cpu65_ea        == (uint16_t)(addrs+1));
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_NONE);
    ASSERT(run_args.cpu65_opcode    == 0x80);
    ASSERT(run_args.cpu65_opcycles  == cycle_count);

    PASS();
}

TEST test_BVC(int8_t off, bool flag, uint16_t addrs) {
    uint8_t flags = 0x0;

    run_args.cpu65_pc = addrs;
    flags |= flag ? fV : 0;

    uint8_t cycle_count = 2;
    uint16_t newpc = addrs+2;
    if (!flag) {
        uint16_t prebranch = newpc;
        newpc += off;
        ++cycle_count;
        if ((newpc&0xFF00) != (prebranch&0xFF00)) {
            ++cycle_count;
        }
    }

    apple_ii_64k[0][addrs] = 0x50;
    apple_ii_64k[0][(uint16_t)(addrs+1)] = off;
    apple_ii_64k[0][(uint16_t)(addrs+2)] = (uint8_t)random();

    run_args.cpu65_a  = 0xed;
    run_args.cpu65_x  = 0xde;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = flags;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc == newpc);
    ASSERT(run_args.cpu65_a  == 0xed);
    ASSERT(run_args.cpu65_x  == 0xde);
    ASSERT(run_args.cpu65_y  == 0x05);
    ASSERT(run_args.cpu65_sp == 0x81);
    ASSERT(run_args.cpu65_f  == flags);

    ASSERT(run_args.cpu65_ea        == (uint16_t)(addrs+1));
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_NONE);
    ASSERT(run_args.cpu65_opcode    == 0x50);
    ASSERT(run_args.cpu65_opcycles  == cycle_count);

    PASS();
}

TEST test_BVS(int8_t off, bool flag, uint16_t addrs) {
    uint8_t flags = 0x0;

    run_args.cpu65_pc = addrs;
    flags |= flag ? fV : 0;

    uint8_t cycle_count = 2;
    uint16_t newpc = addrs+2;
    if (flag) {
        uint16_t prebranch = newpc;
        newpc += off;
        ++cycle_count;
        if ((newpc&0xFF00) != (prebranch&0xFF00)) {
            ++cycle_count;
        }
    }

    apple_ii_64k[0][addrs] = 0x70;
    apple_ii_64k[0][(uint16_t)(addrs+1)] = off;
    apple_ii_64k[0][(uint16_t)(addrs+2)] = (uint8_t)random();

    run_args.cpu65_a  = 0xed;
    run_args.cpu65_x  = 0xde;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = flags;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc == newpc);
    ASSERT(run_args.cpu65_a  == 0xed);
    ASSERT(run_args.cpu65_x  == 0xde);
    ASSERT(run_args.cpu65_y  == 0x05);
    ASSERT(run_args.cpu65_sp == 0x81);
    ASSERT(run_args.cpu65_f  == flags);

    ASSERT(run_args.cpu65_ea        == (uint16_t)(addrs+1));
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_NONE);
    ASSERT(run_args.cpu65_opcode    == 0x70);
    ASSERT(run_args.cpu65_opcycles  == cycle_count);

    PASS();
}

// ----------------------------------------------------------------------------
// BIT instructions

TEST test_BIT_imm(uint8_t regA, uint8_t val) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    if ((regA & val) == 0x0) {
        flags = fZ;
    }

    testcpu_set_opcode2(0x89, val);

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc == TEST_LOC+2);
    ASSERT(run_args.cpu65_a  == regA);
    ASSERT(run_args.cpu65_x  == 0x03);
    ASSERT(run_args.cpu65_y  == 0x04);
    ASSERT(run_args.cpu65_sp == 0x80);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == TEST_LOC+1);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0x89);
    ASSERT(run_args.cpu65_opcycles  == (2));

    PASS();
}

static void logic_BIT(uint8_t regA, uint8_t val, uint8_t *flags) {
    if ((regA & val) == 0x0) {
        *flags |= fZ;
    }
    if (val & 0x80) {
        *flags |= fN;
    }
    if (val & 0x40) {
        *flags |= fV;
    }
}

TEST test_BIT_zpage(uint8_t regA, uint8_t val, uint8_t arg0) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_BIT(regA, val, &flags);

    testcpu_set_opcode2(0x24, arg0);

    apple_ii_64k[0][arg0] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x80;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == 0x03);
    ASSERT(run_args.cpu65_y       == 0x04);
    ASSERT(run_args.cpu65_sp      == 0x80);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == arg0);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0x24);
    ASSERT(run_args.cpu65_opcycles  == (3));

    PASS();
}

TEST test_BIT_zpage_x(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t regX) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_BIT(regA, val, &flags);

    testcpu_set_opcode2(0x34, arg0);

    uint8_t idx = arg0+regX;

    apple_ii_64k[0][idx] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc == TEST_LOC+2);
    ASSERT(run_args.cpu65_a  == regA);
    ASSERT(run_args.cpu65_x  == regX);
    ASSERT(run_args.cpu65_y  == 0x05);
    ASSERT(run_args.cpu65_sp == 0x81);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == idx);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0x34);
    ASSERT(run_args.cpu65_opcycles  == (4));

    PASS();
}

TEST test_BIT_abs(uint8_t regA, uint8_t val, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_BIT(regA, val, &flags);

    testcpu_set_opcode3(0x2c, lobyte, hibyte);

    uint16_t addrs = lobyte | (hibyte<<8);
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0xf4;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc == TEST_LOC+3);
    ASSERT(run_args.cpu65_a  == regA);
    ASSERT(run_args.cpu65_x  == 0xf4);
    ASSERT(run_args.cpu65_y  == 0x05);
    ASSERT(run_args.cpu65_sp == 0x81);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0x2c);
    ASSERT(run_args.cpu65_opcycles  == (4));

    PASS();
}

TEST test_BIT_abs_x(uint8_t regA, uint8_t val, uint8_t regX, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_BIT(regA, val, &flags);

    testcpu_set_opcode3(0x3c, lobyte, hibyte);

    uint8_t cycle_count = 0;
    uint16_t addrs = lobyte | (hibyte<<8);
    addrs = addrs + regX;
    if ((uint8_t)((addrs>>8)&0xff) != (uint8_t)hibyte) {
        ++cycle_count;
    }
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc == TEST_LOC+3);
    ASSERT(run_args.cpu65_a  == regA);
    ASSERT(run_args.cpu65_x  == regX);
    ASSERT(run_args.cpu65_y  == 0x05);
    ASSERT(run_args.cpu65_sp == 0x81);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0x3c);

    cycle_count += 4;
    ASSERT(run_args.cpu65_opcycles == cycle_count);

    PASS();
}

// ----------------------------------------------------------------------------
// BRK operand (and IRQ handling)

TEST test_BRK(void) {
    testcpu_set_opcode1(0x00);

    ASSERT(apple_ii_64k[0][0x1ff] != 0x1f);
    ASSERT(apple_ii_64k[0][0x1fe] != TEST_LOC_LO+2);

    run_args.cpu65_a = 0x02;
    run_args.cpu65_x = 0x03;
    run_args.cpu65_y = 0x04;
    run_args.cpu65_f = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc     == 0xc3fa);
    ASSERT(run_args.cpu65_a      == 0x02);
    ASSERT(run_args.cpu65_x      == 0x03);
    ASSERT(run_args.cpu65_y      == 0x04);
    ASSERT(run_args.cpu65_f      == (fB|fX|fI));
    ASSERT(run_args.cpu65_sp     == 0xfc);

    ASSERT(apple_ii_64k[0][0x1ff] == 0x1f);
    ASSERT(apple_ii_64k[0][0x1fe] == TEST_LOC_LO+2);
    ASSERT(apple_ii_64k[0][0x1fd] == cpu65_flags_encode[B_Flag|X_Flag]);

    //ASSERT(run_args.cpu65_ea       == 0xfffe); -- EA is managed differently on ARM
    ASSERT(run_args.cpu65_d        == 0xff);
    ASSERT(run_args.cpu65_rw       == RW_NONE);
    ASSERT(run_args.cpu65_opcode   == 0x0);
    ASSERT(run_args.cpu65_opcycles == (7));

    PASS();
}

// FIXME TODO : this tests the Apple //e vm, so it prolly should be moved  machine/memory tests ...
TEST test_IRQ(void) {
    testcpu_set_opcode1(0xea/*NOP*/); // Implementation NOTE: not executed. IRQ is handled and one BIT instruction at C3FA location is executed

    cpu65_interrupt(IRQGeneric);

    // These must match what iie_c3rom_peripheral():
    run_args.softswitches |= SS_C3ROM;
    run_args.base_c3rom = apple_ii_64k[1];

    ASSERT(apple_ii_64k[0][0x1ff] != 0x1f);
    ASSERT(apple_ii_64k[0][0x1fe] != TEST_LOC_LO+1);

    run_args.cpu65_a = 0x02;
    run_args.cpu65_x = 0x03;
    run_args.cpu65_y = 0x04;
    run_args.cpu65_f = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc     == 0xc3fd);
    ASSERT(run_args.cpu65_a      == 0x02);
    ASSERT(run_args.cpu65_x      == 0x03);
    ASSERT(run_args.cpu65_y      == 0x04);
    ASSERT(run_args.cpu65_f      == (fB|fX|fI|fZ)); // Implementation NOTE : Z set by 2nd BIT instruction at C3FA
    ASSERT(run_args.cpu65_sp     == 0xfc);

    ASSERT(apple_ii_64k[0][0x1ff] == 0x1f);
    ASSERT(apple_ii_64k[0][0x1fe] == TEST_LOC_LO);
    ASSERT(apple_ii_64k[0][0x1fd] == cpu65_flags_encode[X_Flag]);

    ASSERT(run_args.cpu65_ea       == 0xc015);
    ASSERT(run_args.cpu65_d        == 0xff);
    ASSERT(run_args.cpu65_rw       == RW_READ);
    ASSERT(run_args.cpu65_opcode   == 0x2c);
    ASSERT(run_args.cpu65_opcycles == 11); // 4 cycles BIT instruction + 7 cycles for IRQ handling
    ASSERT(run_args.cpu65_cycle_count == 11);

    // These must match iie_c3rom_internal():
    run_args.softswitches &= ~SS_C3ROM;
    extern void *iie_read_peripheral_card;
    run_args.base_c3rom = (void *)iie_read_peripheral_card;
    ASSERT(!(run_args.softswitches & SS_CXROM));

    cpu65_uninterrupt(IRQGeneric);

    PASS();
}

// ----------------------------------------------------------------------------
// CLx operands

TEST test_CLC(void) {
    testcpu_set_opcode1(0x18);

    run_args.cpu65_a  = 0x02;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = fC;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc     == TEST_LOC+1);
    ASSERT(run_args.cpu65_a      == 0x02);
    ASSERT(run_args.cpu65_x      == 0x03);
    ASSERT(run_args.cpu65_y      == 0x04);
    ASSERT(run_args.cpu65_sp     == 0x81);
    ASSERT(run_args.cpu65_f      == 0x00);

    ASSERT(run_args.cpu65_ea       == TEST_LOC);
    ASSERT(run_args.cpu65_d        == 0xff);
    ASSERT(run_args.cpu65_rw       == RW_NONE);
    ASSERT(run_args.cpu65_opcode   == 0x18);
    ASSERT(run_args.cpu65_opcycles == (2));

    PASS();
}

TEST test_CLD(void) {
    testcpu_set_opcode1(0xd8);

    run_args.cpu65_a  = 0x02;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = fD;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc     == TEST_LOC+1);
    ASSERT(run_args.cpu65_a      == 0x02);
    ASSERT(run_args.cpu65_x      == 0x03);
    ASSERT(run_args.cpu65_y      == 0x04);
    ASSERT(run_args.cpu65_sp     == 0x81);
    ASSERT(run_args.cpu65_f      == 0x00);

    ASSERT(run_args.cpu65_ea       == TEST_LOC);
    ASSERT(run_args.cpu65_d        == 0xff);
    ASSERT(run_args.cpu65_rw       == RW_NONE);
    ASSERT(run_args.cpu65_opcode   == 0xd8);
    ASSERT(run_args.cpu65_opcycles == (2));

    PASS();
}

TEST test_CLI(void) {
    testcpu_set_opcode1(0x58);

    run_args.cpu65_a  = 0x02;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = fI;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc     == TEST_LOC+1);
    ASSERT(run_args.cpu65_a      == 0x02);
    ASSERT(run_args.cpu65_x      == 0x03);
    ASSERT(run_args.cpu65_y      == 0x04);
    ASSERT(run_args.cpu65_sp     == 0x81);
    ASSERT(run_args.cpu65_f      == 0x00);

    ASSERT(run_args.cpu65_ea       == TEST_LOC);
    ASSERT(run_args.cpu65_d        == 0xff);
    ASSERT(run_args.cpu65_rw       == RW_NONE);
    ASSERT(run_args.cpu65_opcode   == 0x58);
    ASSERT(run_args.cpu65_opcycles == (2));

    PASS();
}

TEST test_CLV(void) {
    testcpu_set_opcode1(0xb8);

    run_args.cpu65_a  = 0x02;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = fV;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc     == TEST_LOC+1);
    ASSERT(run_args.cpu65_a      == 0x02);
    ASSERT(run_args.cpu65_x      == 0x03);
    ASSERT(run_args.cpu65_y      == 0x04);
    ASSERT(run_args.cpu65_sp     == 0x81);
    ASSERT(run_args.cpu65_f      == 0x00);

    ASSERT(run_args.cpu65_ea       == TEST_LOC);
    ASSERT(run_args.cpu65_d        == 0xff);
    ASSERT(run_args.cpu65_rw       == RW_NONE);
    ASSERT(run_args.cpu65_opcode   == 0xb8);
    ASSERT(run_args.cpu65_opcycles == (2));

    PASS();
}

// ----------------------------------------------------------------------------
// CMP instructions

static void logic_CMP(/*uint8_t*/int _a, /*uint8_t*/int _b, uint8_t *flags) {
    uint8_t a = (uint8_t)_a;
    uint8_t b = (uint8_t)_b;

    uint8_t res = a - b;

    if ((res & 0xff) == 0x0) {
        *flags |= fZ;
    }
    if (res & 0x80) {
        *flags |= fN;
    }
    if (b <= a) {
        *flags |= fC;
    }
}

TEST test_CMP_imm(uint8_t regA, uint8_t val) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_CMP(regA, val, &flags);

    testcpu_set_opcode2(0xC9, val);

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == 0x03);
    ASSERT(run_args.cpu65_y       == 0x04);
    ASSERT(run_args.cpu65_sp      == 0x80);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == TEST_LOC+1);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xC9);
    ASSERT(run_args.cpu65_opcycles  == (2));

    PASS();
}

TEST test_CMP_zpage(uint8_t regA, uint8_t val, uint8_t arg0) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_CMP(regA, val, &flags);

    testcpu_set_opcode2(0xc5, arg0);

    apple_ii_64k[0][arg0] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == 0x03);
    ASSERT(run_args.cpu65_y       == 0x04);
    ASSERT(run_args.cpu65_sp      == 0x80);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == arg0);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xc5);
    ASSERT(run_args.cpu65_opcycles  == (3));

    PASS();
}

TEST test_CMP_zpage_x(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t regX) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_CMP(regA, val, &flags);

    testcpu_set_opcode2(0xd5, arg0);

    uint8_t idx = arg0+regX;

    apple_ii_64k[0][idx] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == regX);
    ASSERT(run_args.cpu65_y       == 0x05);
    ASSERT(run_args.cpu65_sp      == 0x81);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == idx);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xd5);
    ASSERT(run_args.cpu65_opcycles  == (4));

    PASS();
}

TEST test_CMP_abs(uint8_t regA, uint8_t val, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_CMP(regA, val, &flags);

    testcpu_set_opcode3(0xcd, lobyte, hibyte);

    uint16_t addrs = lobyte | (hibyte<<8);
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0xf4;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+3);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == 0xf4);
    ASSERT(run_args.cpu65_y       == 0x05);
    ASSERT(run_args.cpu65_sp      == 0x81);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xcd);
    ASSERT(run_args.cpu65_opcycles  == (4));

    PASS();
}

TEST test_CMP_abs_x(uint8_t regA, uint8_t val, uint8_t regX, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_CMP(regA, val, &flags);

    testcpu_set_opcode3(0xdd, lobyte, hibyte);

    uint8_t cycle_count = 4;
    uint16_t addrs = lobyte | (hibyte<<8);
    addrs = addrs + regX;
    if ((uint8_t)((addrs>>8)&0xff) != (uint8_t)hibyte) {
        ++cycle_count;
    }
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+3);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == regX);
    ASSERT(run_args.cpu65_y       == 0x05);
    ASSERT(run_args.cpu65_sp      == 0x81);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xdd);
    ASSERT(run_args.cpu65_opcycles  == cycle_count);

    PASS();
}

TEST test_CMP_abs_y(uint8_t regA, uint8_t val, uint8_t regY, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_CMP(regA, val, &flags);

    testcpu_set_opcode3(0xd9, lobyte, hibyte);

    uint8_t cycle_count = 4;
    uint16_t addrs = lobyte | (hibyte<<8);
    addrs = addrs + regY;
    if ((uint8_t)((addrs>>8)&0xff) != (uint8_t)hibyte) {
        ++cycle_count;
    }
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x02;
    run_args.cpu65_y  = regY;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+3);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == 0x02);
    ASSERT(run_args.cpu65_y       == regY);
    ASSERT(run_args.cpu65_sp      == 0x81);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xd9);
    ASSERT(run_args.cpu65_opcycles == cycle_count);

    PASS();
}

TEST test_CMP_ind_x(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t regX, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_CMP(regA, val, &flags);

    testcpu_set_opcode2(0xc1, arg0);

    uint8_t idx_lo = arg0 + regX;
    uint8_t idx_hi = idx_lo+1;
    uint16_t addrs = lobyte | (hibyte<<8);

    apple_ii_64k[0][idx_lo] = lobyte;
    apple_ii_64k[0][idx_hi] = hibyte;
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x15;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == regX);
    ASSERT(run_args.cpu65_y       == 0x15);
    ASSERT(run_args.cpu65_sp      == 0x81);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xc1);

    ASSERT(run_args.cpu65_opcycles  == (6));

    PASS();
}

TEST test_CMP_ind_y(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t regY, uint8_t val_zp0, uint8_t val_zp1) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_CMP(regA, val, &flags);

    testcpu_set_opcode2(0xd1, arg0);

    uint8_t idx0 = arg0;
    uint8_t idx1 = arg0+1;

    apple_ii_64k[0][idx0] = val_zp0;
    apple_ii_64k[0][idx1] = val_zp1;

    uint8_t cycle_count = 5;
    uint16_t addrs = val_zp0 | (val_zp1<<8);
    addrs += (uint8_t)regY;
    if ((uint8_t)((addrs>>8)&0xff) != (uint8_t)val_zp1) {
        ++cycle_count;
    }

    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x84;
    run_args.cpu65_y  = regY;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == 0x84);
    ASSERT(run_args.cpu65_y       == regY);
    ASSERT(run_args.cpu65_sp      == 0x81);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xd1);
    ASSERT(run_args.cpu65_opcycles  == cycle_count);

    PASS();
}

// 65c02 : 0xD2
TEST test_CMP_ind_zpage(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_CMP(regA, val, &flags);

    testcpu_set_opcode2(0xd2, arg0);

    uint8_t idx0 = arg0;
    uint8_t idx1 = arg0+1;

    apple_ii_64k[0][idx0] = lobyte;
    apple_ii_64k[0][idx1] = hibyte;

    uint16_t addrs = lobyte | (hibyte<<8);
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x14;
    run_args.cpu65_y  = 0x85;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == 0x14);
    ASSERT(run_args.cpu65_y       == 0x85);
    ASSERT(run_args.cpu65_sp      == 0x81);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xd2);
    ASSERT(run_args.cpu65_opcycles  == (5));

    PASS();
}

// ----------------------------------------------------------------------------
// CPx CPy instructions

TEST test_CPX_imm(uint8_t regX, uint8_t val) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    uint8_t regA = 0xaa;

    logic_CMP(regX, val, &flags);

    testcpu_set_opcode2(0xe0, val);

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == regX);
    ASSERT(run_args.cpu65_y       == 0x04);
    ASSERT(run_args.cpu65_sp      == 0x80);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == TEST_LOC+1);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xe0);
    ASSERT(run_args.cpu65_opcycles  == (2));

    PASS();
}

TEST test_CPX_zpage(uint8_t regX, uint8_t val, uint8_t arg0) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    uint8_t regA = 0x55;

    logic_CMP(regX, val, &flags);

    testcpu_set_opcode2(0xe4, arg0);

    apple_ii_64k[0][arg0] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == regX);
    ASSERT(run_args.cpu65_y       == 0x04);
    ASSERT(run_args.cpu65_sp      == 0x80);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == arg0);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xe4);
    ASSERT(run_args.cpu65_opcycles  == (3));

    PASS();
}

TEST test_CPX_abs(uint8_t regX, uint8_t val, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    uint8_t regA = 0xAA;

    logic_CMP(regX, val, &flags);

    testcpu_set_opcode3(0xec, lobyte, hibyte);

    uint16_t addrs = lobyte | (hibyte<<8);
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+3);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == regX);
    ASSERT(run_args.cpu65_y       == 0x05);
    ASSERT(run_args.cpu65_sp      == 0x81);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xec);
    ASSERT(run_args.cpu65_opcycles  == (4));

    PASS();
}

TEST test_CPY_imm(uint8_t regY, uint8_t val) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    uint8_t regA = 0xaa;

    logic_CMP(regY, val, &flags);

    testcpu_set_opcode2(0xc0, val);

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x66;
    run_args.cpu65_y  = regY;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == 0x66);
    ASSERT(run_args.cpu65_y       == regY);
    ASSERT(run_args.cpu65_sp      == 0x80);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == TEST_LOC+1);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xc0);
    ASSERT(run_args.cpu65_opcycles  == (2));

    PASS();
}

TEST test_CPY_zpage(uint8_t regY, uint8_t val, uint8_t arg0) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    uint8_t regA = 0x55;

    logic_CMP(regY, val, &flags);

    testcpu_set_opcode2(0xc4, arg0);

    apple_ii_64k[0][arg0] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x27;
    run_args.cpu65_y  = regY;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == 0x27);
    ASSERT(run_args.cpu65_y       == regY);
    ASSERT(run_args.cpu65_sp      == 0x80);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == arg0);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xc4);
    ASSERT(run_args.cpu65_opcycles  == (3));

    PASS();
}

TEST test_CPY_abs(uint8_t regY, uint8_t val, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    uint8_t regA = 0xAA;

    logic_CMP(regY, val, &flags);

    testcpu_set_opcode3(0xcc, lobyte, hibyte);

    uint16_t addrs = lobyte | (hibyte<<8);
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x7b;
    run_args.cpu65_y  = regY;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+3);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == 0x7b);
    ASSERT(run_args.cpu65_y       == regY);
    ASSERT(run_args.cpu65_sp      == 0x81);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xcc);
    ASSERT(run_args.cpu65_opcycles  == (4));

    PASS();
}

// ----------------------------------------------------------------------------
// DEA, DEX, DEY instructions

static void logic_DEx(/*uint8_t*/int _a, uint8_t *result, uint8_t *flags) {
    uint8_t a = (uint8_t)_a;

    uint8_t res = a-1;
    if ((res & 0xff) == 0x0) {
        *flags |= fZ;
    }
    if (res & 0x80) {
        *flags |= fN;
    }

    *result = res;
}

TEST test_DEA(uint8_t regA) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    uint8_t val = regA;

    logic_DEx(regA, &result, &flags);

    testcpu_set_opcode1(0x3a);

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+1);
    ASSERT(run_args.cpu65_a       == result);
    ASSERT(run_args.cpu65_x       == 0x03);
    ASSERT(run_args.cpu65_y       == 0x04);
    ASSERT(run_args.cpu65_sp      == 0x80);

    do {
        char msgbuf[MSG_SIZE];
        ASSERTm(msgbuf, run_args.cpu65_a == result);
        VERIFY_FLAGS();
    } while (0);

    ASSERT(run_args.cpu65_ea        == TEST_LOC);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_NONE);
    ASSERT(run_args.cpu65_opcode    == 0x3a);
    ASSERT(run_args.cpu65_opcycles  == (2));

    PASS();
}

TEST test_DEX(uint8_t regX) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    uint8_t regA = 0x12;
    uint8_t val = regX;

    logic_DEx(regX, &result, &flags);

    testcpu_set_opcode1(0xca);

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+1);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == result);
    ASSERT(run_args.cpu65_y       == 0x04);
    ASSERT(run_args.cpu65_sp      == 0x80);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == TEST_LOC);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_NONE);
    ASSERT(run_args.cpu65_opcode    == 0xca);
    ASSERT(run_args.cpu65_opcycles  == (2));

    PASS();
}

TEST test_DEY(uint8_t regY) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    uint8_t regA = 0x12;
    uint8_t val = regY;

    logic_DEx(regY, &result, &flags);

    testcpu_set_opcode1(0x88);

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x13;
    run_args.cpu65_y  = regY;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+1);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == 0x13);
    ASSERT(run_args.cpu65_y       == result);
    ASSERT(run_args.cpu65_sp      == 0x80);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == TEST_LOC);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_NONE);
    ASSERT(run_args.cpu65_opcode    == 0x88);
    ASSERT(run_args.cpu65_opcycles  == (2));

    PASS();
}

// ----------------------------------------------------------------------------
// DEC instructions

TEST test_DEC_zpage(uint8_t regA, uint8_t val, uint8_t arg0) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_DEx(val, &result, &flags);

    testcpu_set_opcode2(0xc6, arg0);

    apple_ii_64k[0][arg0] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == 0x03);
    ASSERT(run_args.cpu65_y       == 0x04);
    ASSERT(run_args.cpu65_sp      == 0x80);

    ASSERT(apple_ii_64k[0][arg0] == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == arg0);
    ASSERT(run_args.cpu65_d         == result);
    ASSERT(run_args.cpu65_rw        == (RW_READ|RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0xc6);
    ASSERT(run_args.cpu65_opcycles  == (5));

    PASS();
}

TEST test_DEC_zpage_x(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t regX) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_DEx(val, &result, &flags);

    testcpu_set_opcode2(0xd6, arg0);

    uint8_t idx = arg0+regX;

    apple_ii_64k[0][idx] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == regX);
    ASSERT(run_args.cpu65_y       == 0x05);
    ASSERT(run_args.cpu65_sp      == 0x81);

    ASSERT(apple_ii_64k[0][idx]  == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == idx);
    ASSERT(run_args.cpu65_d         == result);
    ASSERT(run_args.cpu65_rw        == (RW_READ|RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0xd6);
    ASSERT(run_args.cpu65_opcycles  == (6));

    PASS();
}

TEST test_DEC_abs(uint8_t regA, uint8_t val, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_DEx(val, &result, &flags);

    testcpu_set_opcode3(0xce, lobyte, hibyte);

    uint16_t addrs = lobyte | (hibyte<<8);
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0xf4;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+3);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == 0xf4);
    ASSERT(run_args.cpu65_y       == 0x05);
    ASSERT(run_args.cpu65_sp      == 0x81);

    ASSERT(apple_ii_64k[0][addrs] == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == result);
    ASSERT(run_args.cpu65_rw        == (RW_READ|RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0xce);
    ASSERT(run_args.cpu65_opcycles  == (6));

    PASS();
}

TEST test_DEC_abs_x(uint8_t regA, uint8_t val, uint8_t regX, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_DEx(val, &result, &flags);

    testcpu_set_opcode3(0xde, lobyte, hibyte);

    uint8_t cycle_count = 7;
    uint16_t addrs = lobyte | (hibyte<<8);
    addrs = addrs + regX;
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+3);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == regX);
    ASSERT(run_args.cpu65_y       == 0x05);
    ASSERT(run_args.cpu65_sp      == 0x81);

    ASSERT(apple_ii_64k[0][addrs] == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == result);
    ASSERT(run_args.cpu65_rw        == (RW_READ|RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0xde);
    ASSERT(run_args.cpu65_opcycles  == cycle_count);

    PASS();
}

// ----------------------------------------------------------------------------
// EOR instructions

static void logic_EOR(/*uint8_t*/int _a, /*uint8_t*/int _b, uint8_t *result, uint8_t *flags) {
    uint8_t a = (uint8_t)_a;
    uint8_t b = (uint8_t)_b;

    uint8_t res = a ^ b;
    if ((res & 0xff) == 0x0) {
        *flags |= fZ;
    }
    if (res & 0x80) {
        *flags |= fN;
    }

    *result = res;
}

TEST test_EOR_imm(uint8_t regA, uint8_t val) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_EOR(regA, val, &result, &flags);

    testcpu_set_opcode2(0x49, val);

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_x       == 0x03);
    ASSERT(run_args.cpu65_y       == 0x04);
    ASSERT(run_args.cpu65_sp      == 0x80);

    do {
        char msgbuf[MSG_SIZE];
        char buf0[MSG_SIZE];
        char buf1[MSG_SIZE];
        snprintf(msgbuf, MSG_SIZE, MSG_FLAGS0, regA, val, result, buf0, run_args.cpu65_a, buf1);
        ASSERTm(msgbuf, run_args.cpu65_a == result);
        VERIFY_FLAGS();
    } while (0);

    ASSERT(run_args.cpu65_ea        == TEST_LOC+1);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0x49);
    ASSERT(run_args.cpu65_opcycles  == (2));

    PASS();
}

TEST test_EOR_zpage(uint8_t regA, uint8_t val, uint8_t arg0) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    logic_EOR(regA, val, &result, &flags);

    testcpu_set_opcode2(0x45, arg0);

    apple_ii_64k[0][arg0] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_x       == 0x03);
    ASSERT(run_args.cpu65_y       == 0x04);
    ASSERT(run_args.cpu65_sp      == 0x80);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == arg0);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0x45);
    ASSERT(run_args.cpu65_opcycles  == (3));

    PASS();
}

TEST test_EOR_zpage_x(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t regX) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    logic_EOR(regA, val, &result, &flags);

    testcpu_set_opcode2(0x55, arg0);

    uint8_t idx = arg0+regX;

    apple_ii_64k[0][idx] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_x       == regX);
    ASSERT(run_args.cpu65_y       == 0x05);
    ASSERT(run_args.cpu65_sp      == 0x81);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == idx);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0x55);
    ASSERT(run_args.cpu65_opcycles  == (4));

    PASS();
}

TEST test_EOR_abs(uint8_t regA, uint8_t val, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    logic_EOR(regA, val, &result, &flags);

    testcpu_set_opcode3(0x4d, lobyte, hibyte);

    uint16_t addrs = lobyte | (hibyte<<8);
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0xf4;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+3);
    ASSERT(run_args.cpu65_x       == 0xf4);
    ASSERT(run_args.cpu65_y       == 0x05);
    ASSERT(run_args.cpu65_sp      == 0x81);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0x4d);
    ASSERT(run_args.cpu65_opcycles  == (4));

    PASS();
}

TEST test_EOR_abs_x(uint8_t regA, uint8_t val, uint8_t regX, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    logic_EOR(regA, val, &result, &flags);

    testcpu_set_opcode3(0x5d, lobyte, hibyte);

    uint8_t cycle_count = 4;
    uint16_t addrs = lobyte | (hibyte<<8);
    addrs = addrs + regX;
    if ((uint8_t)((addrs>>8)&0xff) != (uint8_t)hibyte) {
        ++cycle_count;
    }
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+3);
    ASSERT(run_args.cpu65_x       == regX);
    ASSERT(run_args.cpu65_y       == 0x05);
    ASSERT(run_args.cpu65_sp      == 0x81);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0x5d);
    ASSERT(run_args.cpu65_opcycles  == cycle_count);

    PASS();
}

TEST test_EOR_abs_y(uint8_t regA, uint8_t val, uint8_t regY, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    logic_EOR(regA, val, &result, &flags);

    testcpu_set_opcode3(0x59, lobyte, hibyte);

    uint8_t cycle_count = 4;
    uint16_t addrs = lobyte | (hibyte<<8);
    addrs = addrs + regY;
    if ((uint8_t)((addrs>>8)&0xff) != (uint8_t)hibyte) {
        ++cycle_count;
    }
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x02;
    run_args.cpu65_y  = regY;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+3);
    ASSERT(run_args.cpu65_x       == 0x02);
    ASSERT(run_args.cpu65_y       == regY);
    ASSERT(run_args.cpu65_sp      == 0x81);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0x59);
    ASSERT(run_args.cpu65_opcycles == cycle_count);

    PASS();
}

TEST test_EOR_ind_x(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t regX, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    logic_EOR(regA, val, &result, &flags);

    testcpu_set_opcode2(0x41, arg0);

    uint8_t idx_lo = arg0 + regX;
    uint8_t idx_hi = idx_lo+1;
    uint16_t addrs = lobyte | (hibyte<<8);

    apple_ii_64k[0][idx_lo] = lobyte;
    apple_ii_64k[0][idx_hi] = hibyte;
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x15;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_x       == regX);
    ASSERT(run_args.cpu65_y       == 0x15);
    ASSERT(run_args.cpu65_sp      == 0x81);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0x41);

    ASSERT(run_args.cpu65_opcycles  == (6));

    PASS();
}

TEST test_EOR_ind_y(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t regY, uint8_t val_zp0, uint8_t val_zp1) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    logic_EOR(regA, val, &result, &flags);

    testcpu_set_opcode2(0x51, arg0);

    uint8_t idx0 = arg0;
    uint8_t idx1 = arg0+1;

    apple_ii_64k[0][idx0] = val_zp0;
    apple_ii_64k[0][idx1] = val_zp1;

    uint8_t cycle_count = 5;
    uint16_t addrs = val_zp0 | (val_zp1<<8);
    addrs += (uint8_t)regY;
    if ((uint8_t)((addrs>>8)&0xff) != (uint8_t)val_zp1) {
        ++cycle_count;
    }

    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x84;
    run_args.cpu65_y  = regY;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_x       == 0x84);
    ASSERT(run_args.cpu65_y       == regY);
    ASSERT(run_args.cpu65_sp      == 0x81);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0x51);
    ASSERT(run_args.cpu65_opcycles  == cycle_count);

    PASS();
}

// 65c02 : 0x52
TEST test_EOR_ind_zpage(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    logic_EOR(regA, val, &result, &flags);

    testcpu_set_opcode2(0x52, arg0);

    uint8_t idx0 = arg0;
    uint8_t idx1 = arg0+1;

    apple_ii_64k[0][idx0] = lobyte;
    apple_ii_64k[0][idx1] = hibyte;

    uint16_t addrs = lobyte | (hibyte<<8);
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x14;
    run_args.cpu65_y  = 0x85;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_x       == 0x14);
    ASSERT(run_args.cpu65_y       == 0x85);
    ASSERT(run_args.cpu65_sp      == 0x81);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0x52);
    ASSERT(run_args.cpu65_opcycles  == (5));

    PASS();
}

// ----------------------------------------------------------------------------
// INA, INX, INY instructions

static void logic_INx(/*uint8_t*/int _a, uint8_t *result, uint8_t *flags) {
    uint8_t a = (uint8_t)_a;

    uint8_t res = a+1;
    if ((res & 0xff) == 0x0) {
        *flags |= fZ;
    }
    if (res & 0x80) {
        *flags |= fN;
    }

    *result = res;
}

TEST test_INA(uint8_t regA) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    uint8_t val = regA;

    logic_INx(regA, &result, &flags);

    testcpu_set_opcode1(0x1a);

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+1);
    ASSERT(run_args.cpu65_a       == result);
    ASSERT(run_args.cpu65_x       == 0x03);
    ASSERT(run_args.cpu65_y       == 0x04);
    ASSERT(run_args.cpu65_sp      == 0x80);

    do {
        char msgbuf[MSG_SIZE];
        ASSERTm(msgbuf, run_args.cpu65_a == result);
        VERIFY_FLAGS();
    } while (0);

    ASSERT(run_args.cpu65_ea        == TEST_LOC);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_NONE);
    ASSERT(run_args.cpu65_opcode    == 0x1a);
    ASSERT(run_args.cpu65_opcycles  == (2));

    PASS();
}

TEST test_INX(uint8_t regX) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    uint8_t regA = 0x31;
    uint8_t val = regX;

    logic_INx(regX, &result, &flags);

    testcpu_set_opcode1(0xe8);

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+1);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == result);
    ASSERT(run_args.cpu65_y       == 0x04);
    ASSERT(run_args.cpu65_sp      == 0x80);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == TEST_LOC);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_NONE);
    ASSERT(run_args.cpu65_opcode    == 0xe8);
    ASSERT(run_args.cpu65_opcycles  == (2));

    PASS();
}

TEST test_INY(uint8_t regY) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    uint8_t regA = 0x21;
    uint8_t val = regY;

    logic_INx(regY, &result, &flags);

    testcpu_set_opcode1(0xc8);

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x13;
    run_args.cpu65_y  = regY;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+1);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == 0x13);
    ASSERT(run_args.cpu65_y       == result);
    ASSERT(run_args.cpu65_sp      == 0x80);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == TEST_LOC);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_NONE);
    ASSERT(run_args.cpu65_opcode    == 0xc8);
    ASSERT(run_args.cpu65_opcycles  == (2));

    PASS();
}

// ----------------------------------------------------------------------------
// INC instructions

TEST test_INC_zpage(uint8_t regA, uint8_t val, uint8_t arg0) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_INx(val, &result, &flags);

    testcpu_set_opcode2(0xe6, arg0);

    apple_ii_64k[0][arg0] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == 0x03);
    ASSERT(run_args.cpu65_y       == 0x04);
    ASSERT(run_args.cpu65_sp      == 0x80);

    ASSERT(apple_ii_64k[0][arg0] == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == arg0);
    ASSERT(run_args.cpu65_d         == result);
    ASSERT(run_args.cpu65_rw        == (RW_READ|RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0xe6);
    ASSERT(run_args.cpu65_opcycles  == (5));

    PASS();
}

TEST test_INC_zpage_x(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t regX) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_INx(val, &result, &flags);

    testcpu_set_opcode2(0xf6, arg0);

    uint8_t idx = arg0+regX;

    apple_ii_64k[0][idx] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == regX);
    ASSERT(run_args.cpu65_y       == 0x05);
    ASSERT(run_args.cpu65_sp      == 0x81);

    ASSERT(apple_ii_64k[0][idx]  == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == idx);
    ASSERT(run_args.cpu65_d         == result);
    ASSERT(run_args.cpu65_rw        == (RW_READ|RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0xf6);
    ASSERT(run_args.cpu65_opcycles  == (6));

    PASS();
}

TEST test_INC_abs(uint8_t regA, uint8_t val, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_INx(val, &result, &flags);

    testcpu_set_opcode3(0xee, lobyte, hibyte);

    uint16_t addrs = lobyte | (hibyte<<8);
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0xf4;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+3);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == 0xf4);
    ASSERT(run_args.cpu65_y       == 0x05);
    ASSERT(run_args.cpu65_sp      == 0x81);

    ASSERT(apple_ii_64k[0][addrs] == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == result);
    ASSERT(run_args.cpu65_rw        == (RW_READ|RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0xee);
    ASSERT(run_args.cpu65_opcycles  == (6));

    PASS();
}

TEST test_INC_abs_x(uint8_t regA, uint8_t val, uint8_t regX, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_INx(val, &result, &flags);

    testcpu_set_opcode3(0xfe, lobyte, hibyte);

    uint8_t cycle_count = 7;
    uint16_t addrs = lobyte | (hibyte<<8);
    addrs = addrs + regX;
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+3);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == regX);
    ASSERT(run_args.cpu65_y       == 0x05);
    ASSERT(run_args.cpu65_sp      == 0x81);

    ASSERT(apple_ii_64k[0][addrs] == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == result);
    ASSERT(run_args.cpu65_rw        == (RW_READ|RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0xfe);
    ASSERT(run_args.cpu65_opcycles  == cycle_count);

    PASS();
}

// ----------------------------------------------------------------------------
// JMP instructions

TEST test_JMP_abs(uint8_t lobyte, uint8_t hibyte, uint16_t insAddrs) {

    testcpu_set_opcodeX(0x4c, lobyte, hibyte, insAddrs);

    uint8_t regA = (uint8_t)random();
    uint8_t regX = (uint8_t)random();
    uint8_t regY = (uint8_t)random();
    uint8_t f    = (uint8_t)random();
    uint8_t sp   = (uint8_t)random();

    uint16_t addrs = (hibyte<<8) | lobyte;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = regY;
    run_args.cpu65_sp = sp;
    run_args.cpu65_f  = f;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc     == addrs);
    ASSERT(run_args.cpu65_a      == regA);
    ASSERT(run_args.cpu65_x      == regX);
    ASSERT(run_args.cpu65_y      == regY);
    ASSERT(run_args.cpu65_f      == f);
    ASSERT(run_args.cpu65_sp     == sp);

    ASSERT(run_args.cpu65_ea       == addrs);
    ASSERT(run_args.cpu65_d        == 0xff);
    ASSERT(run_args.cpu65_rw       == RW_NONE);
    ASSERT(run_args.cpu65_opcode   == 0x4c);
    ASSERT(run_args.cpu65_opcycles == (3));

    PASS();
}

TEST test_JMP_ind(uint8_t _lobyte, uint8_t _hibyte, uint16_t insAddrs) {

    testcpu_set_opcodeX(0x6c, _lobyte, _hibyte, insAddrs);

    uint8_t regA = (uint8_t)random();
    uint8_t regX = (uint8_t)random();
    uint8_t regY = (uint8_t)random();
    uint8_t f    = (uint8_t)random();
    uint8_t sp   = (uint8_t)random();

    uint16_t _addrs = (_hibyte<<8) | _lobyte;

    if ((_addrs >= 0xbfff) && (_addrs < 0xd000)) {
        // HACK FIXME TODO NOTE : for now don't test slot memory ...
        PASS();
    }

    uint8_t lo = apple_ii_64k[0][_addrs];
    ++_addrs;
    if (_lobyte == 0xff) {
        _addrs -= 0x100;
    }
    uint8_t hi = apple_ii_64k[0][_addrs];
    uint16_t addr = (hi<<8) | lo;
    if (_lobyte == 0xff) {
        _addrs += 0x100;
    }
    --_addrs;

#if 0
    // Interesting memory will be HI ROM ... enable this to sanity-check
    if (addr != 0) {
        fprintf(stderr, "%04x -> (%04x)\n", _addrs, addr);
    }
#endif

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = regY;
    run_args.cpu65_sp = sp;
    run_args.cpu65_f  = f;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc     == addr);
    ASSERT(run_args.cpu65_a      == regA);
    ASSERT(run_args.cpu65_x      == regX);
    ASSERT(run_args.cpu65_y      == regY);
    ASSERT(run_args.cpu65_f      == f);
    ASSERT(run_args.cpu65_sp     == sp);

    //ASSERT(run_args.cpu65_ea       == _addrs); -- EA is managed differently on ARM
    ASSERT(run_args.cpu65_d        == 0xff);
    ASSERT(run_args.cpu65_rw       == RW_NONE);
    ASSERT(run_args.cpu65_opcode   == 0x6c);
    ASSERT(run_args.cpu65_opcycles == (6));

    PASS();
}

// 65c02 : 0x7C
TEST test_JMP_abs_ind_x(uint8_t _lobyte, uint8_t _hibyte, uint16_t insAddrs, uint8_t _regX) {

    testcpu_set_opcodeX(0x7c, _lobyte, _hibyte, insAddrs);

    uint8_t regA = (uint8_t)random();
    uint8_t regX = _regX;
    uint8_t regY = (uint8_t)random();
    uint8_t f    = (uint8_t)random();
    uint8_t sp   = (uint8_t)random();

    uint16_t _addrs = (_hibyte<<8) | _lobyte;
    _addrs += regX;

    if ((_addrs >= 0xbfff) && (_addrs < 0xd000)) {
        // HACK FIXME TODO NOTE : for now don't test slot memory ...
        PASS();
    }

    uint8_t lo = apple_ii_64k[0][_addrs];
    ++_addrs;
    uint8_t hi = apple_ii_64k[0][_addrs];
    uint16_t addr = (hi<<8) | lo;
    --_addrs;

#if 0
    // Interesting memory will be HI ROM ... enable this to sanity-check
    if (addr != 0) {
        fprintf(stderr, "%04x -> (%04x)\n", _addrs, addr);
    }
#endif

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = regY;
    run_args.cpu65_sp = sp;
    run_args.cpu65_f  = f;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc     == addr);
    ASSERT(run_args.cpu65_a      == regA);
    ASSERT(run_args.cpu65_x      == regX);
    ASSERT(run_args.cpu65_y      == regY);
    ASSERT(run_args.cpu65_f      == f);
    ASSERT(run_args.cpu65_sp     == sp);

    //ASSERT(run_args.cpu65_ea       == _addrs); -- EA is managed differently on ARM
    ASSERT(run_args.cpu65_d        == 0xff);
    ASSERT(run_args.cpu65_rw       == RW_NONE);
    ASSERT(run_args.cpu65_opcode   == 0x7c);
    ASSERT(run_args.cpu65_opcycles == (6));

    PASS();
}

// ----------------------------------------------------------------------------
// JSR operand

TEST test_JSR_abs(uint8_t lobyte, uint8_t hibyte, uint16_t insAddrs) {

    testcpu_set_opcodeX(0x20, lobyte, hibyte, insAddrs);

    uint8_t regA = (uint8_t)random();
    uint8_t regX = (uint8_t)random();
    uint8_t regY = (uint8_t)random();
    uint8_t f    = (uint8_t)random();

    uint16_t addrs = (hibyte<<8) | lobyte;

    insAddrs += 2;
    uint8_t hi_ret = (insAddrs >> 8) & 0xff;
    uint8_t lo_ret = insAddrs & 0xff;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = regY;
    run_args.cpu65_sp = 0xff;
    run_args.cpu65_f  = f;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc     == addrs);
    ASSERT(run_args.cpu65_a      == regA);
    ASSERT(run_args.cpu65_x      == regX);
    ASSERT(run_args.cpu65_y      == regY);
    ASSERT(run_args.cpu65_f      == f);
    ASSERT(run_args.cpu65_sp     == 0xfd);

    ASSERT(apple_ii_64k[0][0x1ff] == hi_ret);
    ASSERT(apple_ii_64k[0][0x1fe] == lo_ret);

    ASSERT(run_args.cpu65_ea       == addrs);
    ASSERT(run_args.cpu65_d        == 0xff);
    ASSERT(run_args.cpu65_rw       == RW_NONE);
    ASSERT(run_args.cpu65_opcode   == 0x20);
    ASSERT(run_args.cpu65_opcycles == (6));

    PASS();
}

// ----------------------------------------------------------------------------
// LDA instructions

static void logic_LDx(/*uint8_t*/int _b, uint8_t *flags) {
    uint8_t b = (uint8_t)_b;

    if ((b & 0xff) == 0x0) {
        *flags |= fZ;
    }
    if (b & 0x80) {
        *flags |= fN;
    }
}

TEST test_LDA_imm(uint8_t regA, uint8_t val) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_LDx(val, &flags);

    testcpu_set_opcode2(0xa9, val);

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_a       == val);
    ASSERT(run_args.cpu65_x       == 0x03);
    ASSERT(run_args.cpu65_y       == 0x04);
    ASSERT(run_args.cpu65_sp      == 0x80);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == TEST_LOC+1);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xa9);
    ASSERT(run_args.cpu65_opcycles  == (2));

    PASS();
}

TEST test_LDA_zpage(uint8_t regA, uint8_t val, uint8_t arg0) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_LDx(val, &flags);

    testcpu_set_opcode2(0xa5, arg0);

    apple_ii_64k[0][arg0] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_a       == val);
    ASSERT(run_args.cpu65_x       == 0x03);
    ASSERT(run_args.cpu65_y       == 0x04);
    ASSERT(run_args.cpu65_sp      == 0x80);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == arg0);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xa5);
    ASSERT(run_args.cpu65_opcycles  == (3));

    PASS();
}

TEST test_LDA_zpage_x(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t regX) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_LDx(val, &flags);

    testcpu_set_opcode2(0xb5, arg0);

    uint8_t idx = arg0+regX;

    apple_ii_64k[0][idx] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_a       == val);
    ASSERT(run_args.cpu65_x       == regX);
    ASSERT(run_args.cpu65_y       == 0x05);
    ASSERT(run_args.cpu65_sp      == 0x81);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == idx);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xb5);
    ASSERT(run_args.cpu65_opcycles  == (4));

    PASS();
}

TEST test_LDA_abs(uint8_t regA, uint8_t val, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_LDx(val, &flags);

    testcpu_set_opcode3(0xad, lobyte, hibyte);

    uint16_t addrs = lobyte | (hibyte<<8);
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0xf4;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+3);
    ASSERT(run_args.cpu65_a       == val);
    ASSERT(run_args.cpu65_x       == 0xf4);
    ASSERT(run_args.cpu65_y       == 0x05);
    ASSERT(run_args.cpu65_sp      == 0x81);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xad);
    ASSERT(run_args.cpu65_opcycles  == (4));

    PASS();
}

TEST test_LDA_abs_x(uint8_t regA, uint8_t val, uint8_t regX, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_LDx(val, &flags);

    testcpu_set_opcode3(0xbd, lobyte, hibyte);

    uint8_t cycle_count = 4;
    uint16_t addrs = lobyte | (hibyte<<8);
    addrs = addrs + regX;
    if ((uint8_t)((addrs>>8)&0xff) != (uint8_t)hibyte) {
        ++cycle_count;
    }
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+3);
    ASSERT(run_args.cpu65_a       == val);
    ASSERT(run_args.cpu65_x       == regX);
    ASSERT(run_args.cpu65_y       == 0x05);
    ASSERT(run_args.cpu65_sp      == 0x81);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xbd);
    ASSERT(run_args.cpu65_opcycles  == cycle_count);

    PASS();
}

TEST test_LDA_abs_y(uint8_t regA, uint8_t val, uint8_t regY, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_LDx(val, &flags);

    testcpu_set_opcode3(0xb9, lobyte, hibyte);

    uint8_t cycle_count = 4;
    uint16_t addrs = lobyte | (hibyte<<8);
    addrs = addrs + regY;
    if ((uint8_t)((addrs>>8)&0xff) != (uint8_t)hibyte) {
        ++cycle_count;
    }
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x02;
    run_args.cpu65_y  = regY;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+3);
    ASSERT(run_args.cpu65_a       == val);
    ASSERT(run_args.cpu65_x       == 0x02);
    ASSERT(run_args.cpu65_y       == regY);
    ASSERT(run_args.cpu65_sp      == 0x81);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xb9);
    ASSERT(run_args.cpu65_opcycles == cycle_count);

    PASS();
}

TEST test_LDA_ind_x(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t regX, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_LDx(val, &flags);

    testcpu_set_opcode2(0xa1, arg0);

    uint8_t idx_lo = arg0 + regX;
    uint8_t idx_hi = idx_lo+1;
    uint16_t addrs = lobyte | (hibyte<<8);

    apple_ii_64k[0][idx_lo] = lobyte;
    apple_ii_64k[0][idx_hi] = hibyte;
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x15;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_a       == val);
    ASSERT(run_args.cpu65_x       == regX);
    ASSERT(run_args.cpu65_y       == 0x15);
    ASSERT(run_args.cpu65_sp      == 0x81);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xa1);

    ASSERT(run_args.cpu65_opcycles  == (6));

    PASS();
}

TEST test_LDA_ind_y(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t regY, uint8_t val_zp0, uint8_t val_zp1) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_LDx(val, &flags);

    testcpu_set_opcode2(0xb1, arg0);

    uint8_t idx0 = arg0;
    uint8_t idx1 = arg0+1;

    apple_ii_64k[0][idx0] = val_zp0;
    apple_ii_64k[0][idx1] = val_zp1;

    uint8_t cycle_count = 5;
    uint16_t addrs = val_zp0 | (val_zp1<<8);
    addrs += (uint8_t)regY;
    if ((uint8_t)((addrs>>8)&0xff) != (uint8_t)val_zp1) {
        ++cycle_count;
    }

    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x84;
    run_args.cpu65_y  = regY;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_a       == val);
    ASSERT(run_args.cpu65_x       == 0x84);
    ASSERT(run_args.cpu65_y       == regY);
    ASSERT(run_args.cpu65_sp      == 0x81);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xb1);
    ASSERT(run_args.cpu65_opcycles  == cycle_count);

    PASS();
}

// 65c02 : 0xB2
TEST test_LDA_ind_zpage(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_LDx(val, &flags);

    testcpu_set_opcode2(0xb2, arg0);

    uint8_t idx0 = arg0;
    uint8_t idx1 = arg0+1;

    apple_ii_64k[0][idx0] = lobyte;
    apple_ii_64k[0][idx1] = hibyte;

    uint16_t addrs = lobyte | (hibyte<<8);
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x14;
    run_args.cpu65_y  = 0x85;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_a       == val);
    ASSERT(run_args.cpu65_x       == 0x14);
    ASSERT(run_args.cpu65_y       == 0x85);
    ASSERT(run_args.cpu65_sp      == 0x81);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xb2);
    ASSERT(run_args.cpu65_opcycles  == (5));

    PASS();
}

// ----------------------------------------------------------------------------
// LDX LDY instructions

TEST test_LDX_imm(uint8_t regX, uint8_t val) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    uint8_t regA = 0xaa;

    logic_LDx(val, &flags);

    testcpu_set_opcode2(0xa2, val);

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == val);
    ASSERT(run_args.cpu65_y       == 0x04);
    ASSERT(run_args.cpu65_sp      == 0x80);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == TEST_LOC+1);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xa2);
    ASSERT(run_args.cpu65_opcycles  == (2));

    PASS();
}

TEST test_LDX_zpage(uint8_t regX, uint8_t val, uint8_t arg0) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    uint8_t regA = 0x55;

    logic_LDx(val, &flags);

    testcpu_set_opcode2(0xa6, arg0);

    apple_ii_64k[0][arg0] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == val);
    ASSERT(run_args.cpu65_y       == 0x04);
    ASSERT(run_args.cpu65_sp      == 0x80);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == arg0);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xa6);
    ASSERT(run_args.cpu65_opcycles  == (3));

    PASS();
}

TEST test_LDX_zpage_y(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t regY) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_LDx(val, &flags);

    testcpu_set_opcode2(0xb6, arg0);

    uint8_t idx = arg0+regY;

    apple_ii_64k[0][idx] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x3e;
    run_args.cpu65_y  = regY;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == val);
    ASSERT(run_args.cpu65_y       == regY);
    ASSERT(run_args.cpu65_sp      == 0x81);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == idx);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xb6);
    ASSERT(run_args.cpu65_opcycles  == (4));

    PASS();
}

TEST test_LDX_abs(uint8_t regX, uint8_t val, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    uint8_t regA = 0xab;

    logic_LDx(val, &flags);

    testcpu_set_opcode3(0xae, lobyte, hibyte);

    uint16_t addrs = lobyte | (hibyte<<8);
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+3);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == val);
    ASSERT(run_args.cpu65_y       == 0x05);
    ASSERT(run_args.cpu65_sp      == 0x81);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xae);
    ASSERT(run_args.cpu65_opcycles  == (4));

    PASS();
}

TEST test_LDX_abs_y(uint8_t regX, uint8_t val, uint8_t regY, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    uint8_t regA = 0xba;

    logic_LDx(val, &flags);

    testcpu_set_opcode3(0xbe, lobyte, hibyte);

    uint8_t cycle_count = 4;
    uint16_t addrs = lobyte | (hibyte<<8);
    addrs = addrs + regY;
    if ((uint8_t)((addrs>>8)&0xff) != (uint8_t)hibyte) {
        ++cycle_count;
    }
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = regY;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+3);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == val);
    ASSERT(run_args.cpu65_y       == regY);
    ASSERT(run_args.cpu65_sp      == 0x81);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xbe);
    ASSERT(run_args.cpu65_opcycles  == cycle_count);

    PASS();
}

TEST test_LDY_imm(uint8_t regY, uint8_t val) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    uint8_t regA = 0xaa;

    logic_LDx(val, &flags);

    testcpu_set_opcode2(0xa0, val);

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x18;
    run_args.cpu65_y  = regY;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == 0x18);
    ASSERT(run_args.cpu65_y       == val);
    ASSERT(run_args.cpu65_sp      == 0x80);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == TEST_LOC+1);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xa0);
    ASSERT(run_args.cpu65_opcycles  == (2));

    PASS();
}

TEST test_LDY_zpage(uint8_t regY, uint8_t val, uint8_t arg0) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    uint8_t regA = 0x55;

    logic_LDx(val, &flags);

    testcpu_set_opcode2(0xa4, arg0);

    apple_ii_64k[0][arg0] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x4e;
    run_args.cpu65_y  = regY;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == 0x4e);
    ASSERT(run_args.cpu65_y       == val);
    ASSERT(run_args.cpu65_sp      == 0x80);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == arg0);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xa4);
    ASSERT(run_args.cpu65_opcycles  == (3));

    PASS();
}

TEST test_LDY_zpage_x(uint8_t regY, uint8_t val, uint8_t arg0, uint8_t regX) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    uint8_t regA = 0xa9;

    logic_LDx(val, &flags);

    testcpu_set_opcode2(0xb4, arg0);

    uint8_t idx = arg0+regX;

    apple_ii_64k[0][idx] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = regY;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == regX);
    ASSERT(run_args.cpu65_y       == val);
    ASSERT(run_args.cpu65_sp      == 0x81);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == idx);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xb4);
    ASSERT(run_args.cpu65_opcycles  == (4));

    PASS();
}

TEST test_LDY_abs(uint8_t regY, uint8_t val, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    uint8_t regA = 0xab;

    logic_LDx(val, &flags);

    testcpu_set_opcode3(0xac, lobyte, hibyte);

    uint16_t addrs = lobyte | (hibyte<<8);
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x1a;
    run_args.cpu65_y  = regY;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+3);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == 0x1a);
    ASSERT(run_args.cpu65_y       == val);
    ASSERT(run_args.cpu65_sp      == 0x81);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xac);
    ASSERT(run_args.cpu65_opcycles  == (4));

    PASS();
}

TEST test_LDY_abs_x(uint8_t regY, uint8_t val, uint8_t regX, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    uint8_t regA = 0x5a;

    logic_LDx(val, &flags);

    testcpu_set_opcode3(0xbc, lobyte, hibyte);

    uint8_t cycle_count = 4;
    uint16_t addrs = lobyte | (hibyte<<8);
    addrs = addrs + regX;
    if ((uint8_t)((addrs>>8)&0xff) != (uint8_t)hibyte) {
        ++cycle_count;
    }
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = regY;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+3);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == regX);
    ASSERT(run_args.cpu65_y       == val);
    ASSERT(run_args.cpu65_sp      == 0x81);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xbc);
    ASSERT(run_args.cpu65_opcycles  == cycle_count);

    PASS();
}

// ----------------------------------------------------------------------------
// LSR instructions

static void logic_LSR(/*uint8_t*/int _a, uint8_t *result, uint8_t *flags) {
    uint8_t a = (uint8_t)_a;

    *flags |= (a & 0x01) ? fC : 0;

    uint8_t res = a>>1;

    if ((res & 0xff) == 0x0) {
        *flags |= fZ;
    }

    *flags &= ~fN;

    *result = res;
}

TEST test_LSR_acc(uint8_t regA) {
    uint8_t val = 0xff;
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_LSR(regA, &result, &flags);

    testcpu_set_opcode1(0x4a);

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+1);
    ASSERT(run_args.cpu65_x       == 0x03);
    ASSERT(run_args.cpu65_y       == 0x04);
    ASSERT(run_args.cpu65_sp      == 0x80);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == TEST_LOC);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_NONE);
    ASSERT(run_args.cpu65_opcode    == 0x4a);
    ASSERT(run_args.cpu65_opcycles  == (2));

    PASS();
}

TEST test_LSR_zpage(uint8_t regA, uint8_t val, uint8_t arg0) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_LSR(val, &result, &flags);

    testcpu_set_opcode2(0x46, arg0);

    apple_ii_64k[0][arg0] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(apple_ii_64k[0][arg0] == result);

    ASSERT(run_args.cpu65_a  == regA);
    ASSERT(run_args.cpu65_pc == TEST_LOC+2);
    ASSERT(run_args.cpu65_x  == 0x03);
    ASSERT(run_args.cpu65_y  == 0x04);
    ASSERT(run_args.cpu65_sp == 0x80);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == arg0);
    ASSERT(run_args.cpu65_d         == result);
    ASSERT(run_args.cpu65_rw        == (RW_READ|RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0x46);
    ASSERT(run_args.cpu65_opcycles  == (5));

    PASS();
}

TEST test_LSR_zpage_x(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t regX) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_LSR(val, &result, &flags);

    testcpu_set_opcode2(0x56, arg0);

    uint8_t idx = arg0+regX;

    apple_ii_64k[0][idx] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(apple_ii_64k[0][idx] == result);

    ASSERT(run_args.cpu65_a   == regA);
    ASSERT(run_args.cpu65_pc  == TEST_LOC+2);
    ASSERT(run_args.cpu65_x   == regX);
    ASSERT(run_args.cpu65_y   == 0x05);
    ASSERT(run_args.cpu65_sp  == 0x81);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == idx);
    ASSERT(run_args.cpu65_d         == result);
    ASSERT(run_args.cpu65_rw        == (RW_READ|RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0x56);
    ASSERT(run_args.cpu65_opcycles  == (6));

    PASS();
}

TEST test_LSR_abs(uint8_t regA, uint8_t val, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_LSR(val, &result, &flags);

    testcpu_set_opcode3(0x4e, lobyte, hibyte);

    uint16_t addrs = lobyte | (hibyte<<8);
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0xf4;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(apple_ii_64k[0][addrs] == result);

    ASSERT(run_args.cpu65_a  == regA);
    ASSERT(run_args.cpu65_pc == TEST_LOC+3);
    ASSERT(run_args.cpu65_x  == 0xf4);
    ASSERT(run_args.cpu65_y  == 0x05);
    ASSERT(run_args.cpu65_sp == 0x81);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == result);
    ASSERT(run_args.cpu65_rw        == (RW_READ|RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0x4e);
    ASSERT(run_args.cpu65_opcycles  == (6));

    PASS();
}

TEST test_LSR_abs_x(uint8_t regA, uint8_t val, uint8_t regX, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_LSR(val, &result, &flags);

    testcpu_set_opcode3(0x5e, lobyte, hibyte);

    uint8_t cycle_count = 6;
    uint16_t addrs = lobyte | (hibyte<<8);
    addrs = addrs + regX;
    if ((uint8_t)((addrs>>8)&0xff) != (uint8_t)hibyte) {
        ++cycle_count;
    }
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(apple_ii_64k[0][addrs] == result);

    ASSERT(run_args.cpu65_a  == regA);
    ASSERT(run_args.cpu65_pc == TEST_LOC+3);
    ASSERT(run_args.cpu65_x  == regX);
    ASSERT(run_args.cpu65_y  == 0x05);
    ASSERT(run_args.cpu65_sp == 0x81);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == result);
    ASSERT(run_args.cpu65_rw        == (RW_READ|RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0x5e);
    ASSERT(run_args.cpu65_opcycles  == cycle_count);

    PASS();
}

// ----------------------------------------------------------------------------
// NOP operand

TEST test_NOP(void) {
    testcpu_set_opcode1(0xea);

    run_args.cpu65_a  = 0x02;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = 0x55;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc     == TEST_LOC+1);
    ASSERT(run_args.cpu65_a      == 0x02);
    ASSERT(run_args.cpu65_x      == 0x03);
    ASSERT(run_args.cpu65_y      == 0x04);
    ASSERT(run_args.cpu65_f      == 0x55);
    ASSERT(run_args.cpu65_sp     == 0x80);

    ASSERT(run_args.cpu65_ea       == TEST_LOC);
    ASSERT(run_args.cpu65_d        == 0xff);
    ASSERT(run_args.cpu65_rw       == RW_NONE);
    ASSERT(run_args.cpu65_opcode   == 0xea);
    ASSERT(run_args.cpu65_opcycles == (2));

    PASS();
}

// ----------------------------------------------------------------------------
// ORA instructions

static void logic_ORA(/*uint8_t*/int _a, /*uint8_t*/int _b, uint8_t *result, uint8_t *flags) {
    uint8_t a = (uint8_t)_a;
    uint8_t b = (uint8_t)_b;

    uint8_t res = a | b;
    if ((res & 0xff) == 0x0) {
        *flags |= fZ;
    }
    if (res & 0x80) {
        *flags |= fN;
    }

    *result = res;
}

TEST test_ORA_imm(uint8_t regA, uint8_t val) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_ORA(regA, val, &result, &flags);

    testcpu_set_opcode2(0x09, val);

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_x       == 0x03);
    ASSERT(run_args.cpu65_y       == 0x04);
    ASSERT(run_args.cpu65_sp      == 0x80);

    do {
        char msgbuf[MSG_SIZE];
        char buf0[MSG_SIZE];
        char buf1[MSG_SIZE];
        snprintf(msgbuf, MSG_SIZE, MSG_FLAGS0, regA, val, result, buf0, run_args.cpu65_a, buf1);
        ASSERTm(msgbuf, run_args.cpu65_a == result);
        VERIFY_FLAGS();
    } while (0);

    ASSERT(run_args.cpu65_ea        == TEST_LOC+1);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0x09);
    ASSERT(run_args.cpu65_opcycles  == (2));

    PASS();
}

TEST test_ORA_zpage(uint8_t regA, uint8_t val, uint8_t arg0) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    logic_ORA(regA, val, &result, &flags);

    testcpu_set_opcode2(0x05, arg0);

    apple_ii_64k[0][arg0] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_x       == 0x03);
    ASSERT(run_args.cpu65_y       == 0x04);
    ASSERT(run_args.cpu65_sp      == 0x80);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == arg0);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0x05);
    ASSERT(run_args.cpu65_opcycles  == (3));

    PASS();
}

TEST test_ORA_zpage_x(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t regX) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    logic_ORA(regA, val, &result, &flags);

    testcpu_set_opcode2(0x15, arg0);

    uint8_t idx = arg0+regX;

    apple_ii_64k[0][idx] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_x       == regX);
    ASSERT(run_args.cpu65_y       == 0x05);
    ASSERT(run_args.cpu65_sp      == 0x81);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == idx);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0x15);
    ASSERT(run_args.cpu65_opcycles  == (4));

    PASS();
}

TEST test_ORA_abs(uint8_t regA, uint8_t val, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    logic_ORA(regA, val, &result, &flags);

    testcpu_set_opcode3(0x0d, lobyte, hibyte);

    uint16_t addrs = lobyte | (hibyte<<8);
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0xf4;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+3);
    ASSERT(run_args.cpu65_x       == 0xf4);
    ASSERT(run_args.cpu65_y       == 0x05);
    ASSERT(run_args.cpu65_sp      == 0x81);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0x0d);
    ASSERT(run_args.cpu65_opcycles  == (4));

    PASS();
}

TEST test_ORA_abs_x(uint8_t regA, uint8_t val, uint8_t regX, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    logic_ORA(regA, val, &result, &flags);

    testcpu_set_opcode3(0x1d, lobyte, hibyte);

    uint8_t cycle_count = 4;
    uint16_t addrs = lobyte | (hibyte<<8);
    addrs = addrs + regX;
    if ((uint8_t)((addrs>>8)&0xff) != (uint8_t)hibyte) {
        ++cycle_count;
    }
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+3);
    ASSERT(run_args.cpu65_x       == regX);
    ASSERT(run_args.cpu65_y       == 0x05);
    ASSERT(run_args.cpu65_sp      == 0x81);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0x1d);
    ASSERT(run_args.cpu65_opcycles  == cycle_count);

    PASS();
}

TEST test_ORA_abs_y(uint8_t regA, uint8_t val, uint8_t regY, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    logic_ORA(regA, val, &result, &flags);

    testcpu_set_opcode3(0x19, lobyte, hibyte);

    uint8_t cycle_count = 4;
    uint16_t addrs = lobyte | (hibyte<<8);
    addrs = addrs + regY;
    if ((uint8_t)((addrs>>8)&0xff) != (uint8_t)hibyte) {
        ++cycle_count;
    }
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x02;
    run_args.cpu65_y  = regY;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+3);
    ASSERT(run_args.cpu65_x       == 0x02);
    ASSERT(run_args.cpu65_y       == regY);
    ASSERT(run_args.cpu65_sp      == 0x81);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0x19);
    ASSERT(run_args.cpu65_opcycles == cycle_count);

    PASS();
}

TEST test_ORA_ind_x(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t regX, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    logic_ORA(regA, val, &result, &flags);

    testcpu_set_opcode2(0x01, arg0);

    uint8_t idx_lo = arg0 + regX;
    uint8_t idx_hi = idx_lo+1;
    uint16_t addrs = lobyte | (hibyte<<8);

    apple_ii_64k[0][idx_lo] = lobyte;
    apple_ii_64k[0][idx_hi] = hibyte;
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x15;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_x       == regX);
    ASSERT(run_args.cpu65_y       == 0x15);
    ASSERT(run_args.cpu65_sp      == 0x81);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0x01);

    ASSERT(run_args.cpu65_opcycles  == (6));

    PASS();
}

TEST test_ORA_ind_y(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t regY, uint8_t val_zp0, uint8_t val_zp1) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    logic_ORA(regA, val, &result, &flags);

    testcpu_set_opcode2(0x11, arg0);

    uint8_t idx0 = arg0;
    uint8_t idx1 = arg0+1;

    apple_ii_64k[0][idx0] = val_zp0;
    apple_ii_64k[0][idx1] = val_zp1;

    uint8_t cycle_count = 5;
    uint16_t addrs = val_zp0 | (val_zp1<<8);
    addrs += (uint8_t)regY;
    if ((uint8_t)((addrs>>8)&0xff) != (uint8_t)val_zp1) {
        ++cycle_count;
    }

    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x84;
    run_args.cpu65_y  = regY;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_x       == 0x84);
    ASSERT(run_args.cpu65_y       == regY);
    ASSERT(run_args.cpu65_sp      == 0x81);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0x11);
    ASSERT(run_args.cpu65_opcycles  == cycle_count);

    PASS();
}

// 65c02 : 0x12
TEST test_ORA_ind_zpage(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    logic_ORA(regA, val, &result, &flags);

    testcpu_set_opcode2(0x12, arg0);

    uint8_t idx0 = arg0;
    uint8_t idx1 = arg0+1;

    apple_ii_64k[0][idx0] = lobyte;
    apple_ii_64k[0][idx1] = hibyte;

    uint16_t addrs = lobyte | (hibyte<<8);
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x14;
    run_args.cpu65_y  = 0x85;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_x       == 0x14);
    ASSERT(run_args.cpu65_y       == 0x85);
    ASSERT(run_args.cpu65_sp      == 0x81);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0x12);
    ASSERT(run_args.cpu65_opcycles  == (5));

    PASS();
}

// ----------------------------------------------------------------------------
// PHx instructions

TEST test_PHA(void) {
    testcpu_set_opcode1(0x48);

    uint8_t regA = (uint8_t)random();
    apple_ii_64k[0][0x1ff] = ~regA;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_f  = 0x55;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc     == TEST_LOC+1);
    ASSERT(run_args.cpu65_a      == regA);
    ASSERT(run_args.cpu65_x      == 0x03);
    ASSERT(run_args.cpu65_y      == 0x04);
    ASSERT(run_args.cpu65_f      == 0x55);
    ASSERT(run_args.cpu65_sp     == 0xfe);

    ASSERT(apple_ii_64k[0][0x1ff] == regA);

    ASSERT(run_args.cpu65_ea       == TEST_LOC);
    ASSERT(run_args.cpu65_d        == 0xff);
    ASSERT(run_args.cpu65_rw       == RW_NONE);
    ASSERT(run_args.cpu65_opcode   == 0x48);
    ASSERT(run_args.cpu65_opcycles == (3));

    PASS();
}

static int test_PLP(uint8_t flags);
TEST test_PHP(uint8_t flags) {
    testcpu_set_opcode1(0x08);

    run_args.cpu65_a  = 0x02;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_f  = flags;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc     == TEST_LOC+1);
    ASSERT(run_args.cpu65_a      == 0x02);
    ASSERT(run_args.cpu65_x      == 0x03);
    ASSERT(run_args.cpu65_y      == 0x04);
    ASSERT(run_args.cpu65_f      == flags);
    ASSERT(run_args.cpu65_sp     == 0xfe);

    ASSERT(apple_ii_64k[0][0x1ff] == flags);

    ASSERT(run_args.cpu65_ea       == TEST_LOC);
    ASSERT(run_args.cpu65_d        == 0xff);
    ASSERT(run_args.cpu65_rw       == RW_NONE);
    ASSERT(run_args.cpu65_opcode   == 0x08);
    ASSERT(run_args.cpu65_opcycles == (3));

    run_args.cpu65_pc = TEST_LOC;
    test_PLP(flags);

    PASS();
}

TEST test_PHX(void) {
    testcpu_set_opcode1(0xda);

    uint8_t regX = (uint8_t)random();

    run_args.cpu65_a  = 0x03;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_f  = 0x53;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc     == TEST_LOC+1);
    ASSERT(run_args.cpu65_a      == 0x03);
    ASSERT(run_args.cpu65_x      == regX);
    ASSERT(run_args.cpu65_y      == 0x05);
    ASSERT(run_args.cpu65_f      == 0x53);
    ASSERT(run_args.cpu65_sp     == 0xfe);

    ASSERT(apple_ii_64k[0][0x1ff] == regX);

    ASSERT(run_args.cpu65_ea       == TEST_LOC);
    ASSERT(run_args.cpu65_d        == 0xff);
    ASSERT(run_args.cpu65_rw       == RW_NONE);
    ASSERT(run_args.cpu65_opcode   == 0xda);
    ASSERT(run_args.cpu65_opcycles == (3));

    PASS();
}

TEST test_PHY(void) {
    testcpu_set_opcode1(0x5a);

    uint8_t regY = (uint8_t)random();

    run_args.cpu65_a  = 0x03;
    run_args.cpu65_x  = 0x50;
    run_args.cpu65_y  = regY;
    run_args.cpu65_f  = 0x53;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc     == TEST_LOC+1);
    ASSERT(run_args.cpu65_a      == 0x03);
    ASSERT(run_args.cpu65_x      == 0x50);
    ASSERT(run_args.cpu65_y      == regY);
    ASSERT(run_args.cpu65_f      == 0x53);
    ASSERT(run_args.cpu65_sp     == 0xfe);

    ASSERT(apple_ii_64k[0][0x1ff] == regY);

    ASSERT(run_args.cpu65_ea       == TEST_LOC);
    ASSERT(run_args.cpu65_d        == 0xff);
    ASSERT(run_args.cpu65_rw       == RW_NONE);
    ASSERT(run_args.cpu65_opcode   == 0x5a);
    ASSERT(run_args.cpu65_opcycles == (3));

    PASS();
}

// ----------------------------------------------------------------------------
// PLx instructions

static void logic_PLx(/*uint8_t*/int _a, uint8_t *flags) {
    uint8_t a = (uint8_t)_a;

    uint8_t res = a | a;
    if ((res & 0xff) == 0x0) {
        *flags |= fZ;
    }
    if (res & 0x80) {
        *flags |= fN;
    }
}

TEST test_PLA(uint8_t regA) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    uint8_t val = regA;
    uint8_t sp = 0x80;

    logic_PLx(regA, &flags);

    testcpu_set_opcode1(0x68);
    apple_ii_64k[0][0x101+sp] = regA;

    run_args.cpu65_a  = 0x00;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_f  = 0x00;
    run_args.cpu65_sp = sp;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc     == TEST_LOC+1);
    ASSERT(run_args.cpu65_a      == regA);
    ASSERT(run_args.cpu65_x      == 0x03);
    ASSERT(run_args.cpu65_y      == 0x04);
    ASSERT(run_args.cpu65_sp     == sp+1);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea       == TEST_LOC);
    ASSERT(run_args.cpu65_d        == 0xff);
    ASSERT(run_args.cpu65_rw       == RW_NONE);
    ASSERT(run_args.cpu65_opcode   == 0x68);
    ASSERT(run_args.cpu65_opcycles == (4));

    PASS();
}

TEST test_PLP(uint8_t flags) {
    uint8_t sp = 0x80;

    ASSERT(run_args.cpu65_pc     == TEST_LOC);

    testcpu_set_opcode1(0x28);
    apple_ii_64k[0][0x101+sp] = flags;

    run_args.cpu65_a  = 0x02;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_f  = ~flags;
    run_args.cpu65_sp = sp;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc     == TEST_LOC+1);
    ASSERT(run_args.cpu65_a      == 0x02);
    ASSERT(run_args.cpu65_x      == 0x03);
    ASSERT(run_args.cpu65_y      == 0x04);
    ASSERT(run_args.cpu65_sp     == sp+1);
    ASSERT(run_args.cpu65_f      == (flags | fB | fX));

    ASSERT(run_args.cpu65_ea       == TEST_LOC);
    ASSERT(run_args.cpu65_d        == 0xff);
    ASSERT(run_args.cpu65_rw       == RW_NONE);
    ASSERT(run_args.cpu65_opcode   == 0x28);
    ASSERT(run_args.cpu65_opcycles == (4));

    PASS();
}

TEST test_PLX(uint8_t regX) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    uint8_t regA = 0x00;
    uint8_t val = 0x00;
    uint8_t sp = 0x80;

    logic_PLx(regX, &flags);

    testcpu_set_opcode1(0xfa);
    apple_ii_64k[0][0x101+sp] = regX;

    run_args.cpu65_a  = 0x43;
    run_args.cpu65_x  = 0x00;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_f  = 0x00;
    run_args.cpu65_sp = sp;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc     == TEST_LOC+1);
    ASSERT(run_args.cpu65_a      == 0x43);
    ASSERT(run_args.cpu65_x      == regX);
    ASSERT(run_args.cpu65_y      == 0x04);
    ASSERT(run_args.cpu65_sp     == sp+1);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea       == TEST_LOC);
    ASSERT(run_args.cpu65_d        == 0xff);
    ASSERT(run_args.cpu65_rw       == RW_NONE);
    ASSERT(run_args.cpu65_opcode   == 0xfa);
    ASSERT(run_args.cpu65_opcycles == (4));

    PASS();
}

TEST test_PLY(uint8_t regY) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    uint8_t regA = 0x00;
    uint8_t val = 0x00;
    uint8_t sp = 0x80;

    logic_PLx(regY, &flags);

    testcpu_set_opcode1(0x7a);
    apple_ii_64k[0][0x101+sp] = regY;

    run_args.cpu65_a  = 0x43;
    run_args.cpu65_x  = 0x34;
    run_args.cpu65_y  = 0x00;
    run_args.cpu65_f  = 0x00;
    run_args.cpu65_sp = sp;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc     == TEST_LOC+1);
    ASSERT(run_args.cpu65_a      == 0x43);
    ASSERT(run_args.cpu65_x      == 0x34);
    ASSERT(run_args.cpu65_y      == regY);
    ASSERT(run_args.cpu65_sp     == sp+1);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea       == TEST_LOC);
    ASSERT(run_args.cpu65_d        == 0xff);
    ASSERT(run_args.cpu65_rw       == RW_NONE);
    ASSERT(run_args.cpu65_opcode   == 0x7a);
    ASSERT(run_args.cpu65_opcycles == (4));

    PASS();
}

// ----------------------------------------------------------------------------
// ROL instructions

static void logic_ROL(/*uint8_t*/int _a, uint8_t *result, uint8_t *flags) {
    uint8_t a = (uint8_t)_a;

    uint16_t res = a<<1;

    if (*flags & fC) {
        res |= 0x1;
    }

    if (res & 0x100) {
        *flags |= fC;
    } else {
        *flags &= ~fC;
    }

    if ((res & 0xff) == 0x0) {
        *flags |= fZ;
    }

    if (res & 0x80) {
        *flags |= fN;
    }

    *result = (res & 0xff);
}

TEST test_ROL_acc(uint8_t regA, bool carry) {
    uint8_t val = 0xff;
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    flags |= carry ? (fC) : 0x00;

    logic_ROL(regA, &result, &flags);

    testcpu_set_opcode1(0x2a);

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = carry ? (fC) : 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+1);
    ASSERT(run_args.cpu65_x       == 0x03);
    ASSERT(run_args.cpu65_y       == 0x04);
    ASSERT(run_args.cpu65_sp      == 0x80);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == TEST_LOC);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == 0);
    ASSERT(run_args.cpu65_opcode    == 0x2a);
    ASSERT(run_args.cpu65_opcycles  == (2));

    PASS();
}

TEST test_ROL_zpage(bool regA, uint8_t val, uint8_t arg0, bool carry) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    flags |= carry ? (fC) : 0x00;

    logic_ROL(val, &result, &flags);

    testcpu_set_opcode2(0x26, arg0);

    apple_ii_64k[0][arg0] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = carry ? (fC) : 0x00;

    cpu65_run(&run_args);

    ASSERT(apple_ii_64k[0][arg0] == result);

    ASSERT(run_args.cpu65_a  == regA);
    ASSERT(run_args.cpu65_pc == TEST_LOC+2);
    ASSERT(run_args.cpu65_x  == 0x03);
    ASSERT(run_args.cpu65_y  == 0x04);
    ASSERT(run_args.cpu65_sp == 0x80);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == arg0);
    ASSERT(run_args.cpu65_d         == result);
    ASSERT(run_args.cpu65_rw        == (RW_READ|RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0x26);
    ASSERT(run_args.cpu65_opcycles  == (5));

    PASS();
}

TEST test_ROL_zpage_x(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t regX, bool carry) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    flags |= carry ? (fC) : 0x00;

    logic_ROL(val, &result, &flags);

    testcpu_set_opcode2(0x36, arg0);

    uint8_t idx = arg0+regX;

    apple_ii_64k[0][idx] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = carry ? (fC) : 0x00;

    cpu65_run(&run_args);

    ASSERT(apple_ii_64k[0][idx] == result);

    ASSERT(run_args.cpu65_a   == regA);
    ASSERT(run_args.cpu65_pc  == TEST_LOC+2);
    ASSERT(run_args.cpu65_x   == regX);
    ASSERT(run_args.cpu65_y   == 0x05);
    ASSERT(run_args.cpu65_sp  == 0x81);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == idx);
    ASSERT(run_args.cpu65_d         == result);
    ASSERT(run_args.cpu65_rw        == (RW_READ|RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0x36);
    ASSERT(run_args.cpu65_opcycles  == (6));

    PASS();
}

TEST test_ROL_abs(uint8_t regA, uint8_t val, uint8_t lobyte, uint8_t hibyte, bool carry) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    flags |= carry ? (fC) : 0x00;

    logic_ROL(val, &result, &flags);

    testcpu_set_opcode3(0x2e, lobyte, hibyte);

    uint16_t addrs = lobyte | (hibyte<<8);
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0xf4;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = carry ? (fC) : 0x00;

    cpu65_run(&run_args);

    ASSERT(apple_ii_64k[0][addrs] == result);

    ASSERT(run_args.cpu65_a  == regA);
    ASSERT(run_args.cpu65_pc == TEST_LOC+3);
    ASSERT(run_args.cpu65_x  == 0xf4);
    ASSERT(run_args.cpu65_y  == 0x05);
    ASSERT(run_args.cpu65_sp == 0x81);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == result);
    ASSERT(run_args.cpu65_rw        == (RW_READ|RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0x2e);
    ASSERT(run_args.cpu65_opcycles  == (6));

    PASS();
}

TEST test_ROL_abs_x(uint8_t regA, uint8_t val, uint8_t regX, uint8_t lobyte, uint8_t hibyte, bool carry) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    flags |= carry ? (fC) : 0x00;

    logic_ROL(val, &result, &flags);

    testcpu_set_opcode3(0x3e, lobyte, hibyte);

    uint8_t cycle_count = 6;
    uint16_t addrs = lobyte | (hibyte<<8);
    addrs = addrs + regX;
    if ((uint8_t)((addrs>>8)&0xff) != (uint8_t)hibyte) {
        ++cycle_count;
    }
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = carry ? (fC) : 0x00;

    cpu65_run(&run_args);

    ASSERT(apple_ii_64k[0][addrs] == result);

    ASSERT(run_args.cpu65_a  == regA);
    ASSERT(run_args.cpu65_pc == TEST_LOC+3);
    ASSERT(run_args.cpu65_x  == regX);
    ASSERT(run_args.cpu65_y  == 0x05);
    ASSERT(run_args.cpu65_sp == 0x81);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == result);
    ASSERT(run_args.cpu65_rw        == (RW_READ|RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0x3e);
    ASSERT(run_args.cpu65_opcycles  == cycle_count);

    PASS();
}

// ----------------------------------------------------------------------------
// ROR instructions

static void logic_ROR(/*uint8_t*/int _a, uint8_t *result, uint8_t *flags) {
    uint8_t a = (uint8_t)_a;

    bool carry = (a & 0x01);

    uint8_t res = a>>1;

    if (*flags & fC) {
        res |= 0x80;
    }

    if (carry) {
        *flags |= fC;
    } else {
        *flags &= ~fC;
    }

    if ((res & 0xff) == 0x0) {
        *flags |= fZ;
    }

    if (res & 0x80) {
        *flags |= fN;
    }

    *result = (res & 0xff);
}

TEST test_ROR_acc(uint8_t regA, bool carry) {
    uint8_t val = 0xff;
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    flags |= carry ? (fC) : 0x00;

    logic_ROR(regA, &result, &flags);

    testcpu_set_opcode1(0x6a);

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = carry ? (fC) : 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+1);
    ASSERT(run_args.cpu65_x       == 0x03);
    ASSERT(run_args.cpu65_y       == 0x04);
    ASSERT(run_args.cpu65_sp      == 0x80);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == TEST_LOC);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == 0);
    ASSERT(run_args.cpu65_opcode    == 0x6a);
    ASSERT(run_args.cpu65_opcycles  == (2));

    PASS();
}

TEST test_ROR_zpage(bool regA, uint8_t val, uint8_t arg0, bool carry) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    flags |= carry ? (fC) : 0x00;

    logic_ROR(val, &result, &flags);

    testcpu_set_opcode2(0x66, arg0);

    apple_ii_64k[0][arg0] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = carry ? (fC) : 0x00;

    cpu65_run(&run_args);

    ASSERT(apple_ii_64k[0][arg0] == result);

    ASSERT(run_args.cpu65_a  == regA);
    ASSERT(run_args.cpu65_pc == TEST_LOC+2);
    ASSERT(run_args.cpu65_x  == 0x03);
    ASSERT(run_args.cpu65_y  == 0x04);
    ASSERT(run_args.cpu65_sp == 0x80);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == arg0);
    ASSERT(run_args.cpu65_d         == result);
    ASSERT(run_args.cpu65_rw        == (RW_READ|RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0x66);
    ASSERT(run_args.cpu65_opcycles  == (5));

    PASS();
}

TEST test_ROR_zpage_x(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t regX, bool carry) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    flags |= carry ? (fC) : 0x00;

    logic_ROR(val, &result, &flags);

    testcpu_set_opcode2(0x76, arg0);

    uint8_t idx = arg0+regX;

    apple_ii_64k[0][idx] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = carry ? (fC) : 0x00;

    cpu65_run(&run_args);

    ASSERT(apple_ii_64k[0][idx] == result);

    ASSERT(run_args.cpu65_a   == regA);
    ASSERT(run_args.cpu65_pc  == TEST_LOC+2);
    ASSERT(run_args.cpu65_x   == regX);
    ASSERT(run_args.cpu65_y   == 0x05);
    ASSERT(run_args.cpu65_sp  == 0x81);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == idx);
    ASSERT(run_args.cpu65_d         == result);
    ASSERT(run_args.cpu65_rw        == (RW_READ|RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0x76);
    ASSERT(run_args.cpu65_opcycles  == (6));

    PASS();
}

TEST test_ROR_abs(uint8_t regA, uint8_t val, uint8_t lobyte, uint8_t hibyte, bool carry) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    flags |= carry ? (fC) : 0x00;

    logic_ROR(val, &result, &flags);

    testcpu_set_opcode3(0x6e, lobyte, hibyte);

    uint16_t addrs = lobyte | (hibyte<<8);
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0xf4;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = carry ? (fC) : 0x00;

    cpu65_run(&run_args);

    ASSERT(apple_ii_64k[0][addrs] == result);

    ASSERT(run_args.cpu65_a  == regA);
    ASSERT(run_args.cpu65_pc == TEST_LOC+3);
    ASSERT(run_args.cpu65_x  == 0xf4);
    ASSERT(run_args.cpu65_y  == 0x05);
    ASSERT(run_args.cpu65_sp == 0x81);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == result);
    ASSERT(run_args.cpu65_rw        == (RW_READ|RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0x6e);
    ASSERT(run_args.cpu65_opcycles  == (6));

    PASS();
}

TEST test_ROR_abs_x(uint8_t regA, uint8_t val, uint8_t regX, uint8_t lobyte, uint8_t hibyte, bool carry) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    flags |= carry ? (fC) : 0x00;

    logic_ROR(val, &result, &flags);

    testcpu_set_opcode3(0x7e, lobyte, hibyte);

    uint8_t cycle_count = 6;
    uint16_t addrs = lobyte | (hibyte<<8);
    addrs = addrs + regX;
    if ((uint8_t)((addrs>>8)&0xff) != (uint8_t)hibyte) {
        ++cycle_count;
    }
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = carry ? (fC) : 0x00;

    cpu65_run(&run_args);

    ASSERT(apple_ii_64k[0][addrs] == result);

    ASSERT(run_args.cpu65_a  == regA);
    ASSERT(run_args.cpu65_pc == TEST_LOC+3);
    ASSERT(run_args.cpu65_x  == regX);
    ASSERT(run_args.cpu65_y  == 0x05);
    ASSERT(run_args.cpu65_sp == 0x81);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == result);
    ASSERT(run_args.cpu65_rw        == (RW_READ|RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0x7e);
    ASSERT(run_args.cpu65_opcycles  == cycle_count);

    PASS();
}

// ----------------------------------------------------------------------------
// RTI operand

TEST test_RTI(uint8_t flags) {

    testcpu_set_opcode1(0x40);

    run_args.cpu65_a  = 0x02;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_f  = 0x00;
    run_args.cpu65_sp = 0x80;

    uint8_t lo_ret = (uint8_t)random();
    uint8_t hi_ret = (uint8_t)random();

    apple_ii_64k[0][0x181] = flags;
    apple_ii_64k[0][0x182] = lo_ret;
    apple_ii_64k[0][0x183] = hi_ret;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc     == ((hi_ret<<8)| lo_ret));
    ASSERT(run_args.cpu65_a      == 0x02);
    ASSERT(run_args.cpu65_x      == 0x03);
    ASSERT(run_args.cpu65_y      == 0x04);
    ASSERT(run_args.cpu65_f      == (flags | fB | fX));
    ASSERT(run_args.cpu65_sp     == 0x83);

    ASSERT(run_args.cpu65_ea       == TEST_LOC);
    ASSERT(run_args.cpu65_d        == 0xff);
    ASSERT(run_args.cpu65_rw       == RW_NONE);
    ASSERT(run_args.cpu65_opcode   == 0x40);
    ASSERT(run_args.cpu65_opcycles == (6));

    PASS();
}

// ----------------------------------------------------------------------------
// RTS operand

TEST test_RTS(uint8_t lobyte, uint8_t hibyte, uint16_t insAddrs) {

    testcpu_set_opcodeX(0x60, random(), random(), insAddrs);

    run_args.cpu65_a  = 0x02;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_f  = 0x00;
    run_args.cpu65_sp = 0x80;

    apple_ii_64k[0][0x181] = lobyte;
    apple_ii_64k[0][0x182] = hibyte;

    cpu65_run(&run_args);

    uint16_t newpc = ((hibyte<<8) | lobyte) + 1;
    ASSERT(run_args.cpu65_pc     == newpc);
    ASSERT(run_args.cpu65_a      == 0x02);
    ASSERT(run_args.cpu65_x      == 0x03);
    ASSERT(run_args.cpu65_y      == 0x04);
    ASSERT(run_args.cpu65_f      == 0x00);
    ASSERT(run_args.cpu65_sp     == 0x82);

    ASSERT(run_args.cpu65_ea       == insAddrs);
    ASSERT(run_args.cpu65_d        == 0xff);
    ASSERT(run_args.cpu65_rw       == RW_NONE);
    ASSERT(run_args.cpu65_opcode   == 0x60);
    ASSERT(run_args.cpu65_opcycles == (6));

    PASS();
}

// ----------------------------------------------------------------------------
// SBC instructions

static void logic_SBC_dec(/*uint8_t*/int _a, /*uint8_t*/int _b, uint8_t *result, uint8_t *flags) {

    // componentize
    uint8_t x_lo = 0x0;
    uint8_t x_hi = 0x0;

    uint8_t a_lo = (((uint8_t)_a) & 0x0f);
    uint8_t b_lo = (((uint8_t)_b) & 0x0f);

    uint8_t a_hi = (((uint8_t)_a) & 0xf0)>>4;
    uint8_t b_hi = (((uint8_t)_b) & 0xf0)>>4;

    uint8_t borrow = ((*flags & fC) == 0x0) ? 1 : 0;
    *flags |= fC;

    // BCD subtract
    x_lo = a_lo - b_lo - borrow;
    borrow = 0;
    if (x_lo > 9) {
        borrow = 1;
        x_lo -= 6;
        x_lo &= 0x0f;
    }
    x_hi = a_hi - b_hi - borrow;
    if (x_hi > 9) {
        *flags &= ~fC;
        x_hi -= 6;
    }

    // merge result
    x_hi <<= 4;
    *result = x_hi | x_lo;

    // flags
    if (*result == 0x00) {
        *flags |= fZ;
    }

    if (*result & 0x80) {
        *flags |= fN;
    }
}

static void logic_SBC(/*uint8_t*/int _a, /*uint8_t*/int _b, uint8_t *result, uint8_t *flags) {

    if (*flags & fD) {
        logic_SBC_dec(_a, _b, result, flags);
        return;
    }

    int8_t a = (int8_t)_a;
    int8_t b = (int8_t)_b;
    bool signA = a>>7;
    bool signB = b>>7;

    int8_t borrow = ((*flags & fC) == 0x0) ? 1 : 0;
    *flags |= fC;

    int8_t res = a - b - borrow;
    if ((res & 0xff) == 0x0) {
        *flags |= fZ;
    }
    if (res & 0x80) {
        *flags |= fN;
    }

    uint32_t res32 = (uint8_t)a-(uint8_t)b-(uint8_t)borrow;
    if (res32 & 0x80000000) {
        *flags &= ~fC;
    }

    if (signA != signB) {
        uint8_t signResult = (res&0xff)>>7;
        if (signA != signResult) {
            *flags |= fV;
        }
    }

    *result = (uint8_t)(res & 0xff);
}

TEST test_SBC_imm(uint8_t regA, uint8_t val, bool decimal, bool carry) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    if (decimal && check_skip_illegal_bcd(regA, val)) {
        // NOTE : FIXME TODO skip undocumented/illegal BCD
        SKIPm("Z");
    }

    flags |= decimal ? (fD) : 0x00;
    flags |= carry   ? (fC) : 0x00;

    logic_SBC(regA, val, &result, &flags);

    testcpu_set_opcode2(0xe9, val);

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x33;
    run_args.cpu65_y  = 0x44;
    run_args.cpu65_sp = 0x88;
    run_args.cpu65_f |= decimal ? (fD) : 0x00;
    run_args.cpu65_f |= carry   ? (fC) : 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_x       == 0x33);
    ASSERT(run_args.cpu65_y       == 0x44);
    ASSERT(run_args.cpu65_sp      == 0x88);

    do {
        char msgbuf[MSG_SIZE];
        char buf0[MSG_SIZE];
        char buf1[MSG_SIZE];
        snprintf(msgbuf, MSG_SIZE, MSG_FLAGS0, regA, val, result, buf0, run_args.cpu65_a, buf1);
        ASSERTm(msgbuf, run_args.cpu65_a == result);
        VERIFY_FLAGS();
    } while (0);

    ASSERT(run_args.cpu65_ea        == TEST_LOC+1);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xe9);
    ASSERT(run_args.cpu65_opcycles  == (decimal ? 3 : 2));

    PASS();
}

TEST test_SBC_zpage(uint8_t regA, uint8_t val, uint8_t arg0) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_SBC(regA, val, &result, &flags);

    testcpu_set_opcode2(0xe5, arg0);

    apple_ii_64k[0][arg0] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x80;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_x       == 0x03);
    ASSERT(run_args.cpu65_y       == 0x04);
    ASSERT(run_args.cpu65_sp      == 0x80);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == arg0);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xe5);
    ASSERT(run_args.cpu65_opcycles  == (3));

    PASS();
}

TEST test_SBC_zpage_x(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t regX) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_SBC(regA, val, &result, &flags);

    uint8_t idx = arg0 + regX;

    testcpu_set_opcode2(0xf5, arg0);

    apple_ii_64k[0][idx] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_x       == regX);
    ASSERT(run_args.cpu65_y       == 0x05);
    ASSERT(run_args.cpu65_sp      == 0x81);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == idx);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xf5);
    ASSERT(run_args.cpu65_opcycles  == (4));

    PASS();
}

TEST test_SBC_abs(uint8_t regA, uint8_t val, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_SBC(regA, val, &result, &flags);

    testcpu_set_opcode3(0xed, lobyte, hibyte);

    uint16_t addrs = lobyte | (hibyte<<8);
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0xf4;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+3);
    ASSERT(run_args.cpu65_x       == 0xf4);
    ASSERT(run_args.cpu65_y       == 0x05);
    ASSERT(run_args.cpu65_sp      == 0x81);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xed);
    ASSERT(run_args.cpu65_opcycles  == (4));

    PASS();
}

TEST test_SBC_abs_x(uint8_t regA, uint8_t val, uint8_t regX, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_SBC(regA, val, &result, &flags);

    testcpu_set_opcode3(0xfd, lobyte, hibyte);

    uint8_t cycle_count = 0;
    uint16_t addrs = lobyte | (hibyte<<8);
    addrs = addrs + regX;
    if ((uint8_t)((addrs>>8)&0xff) != (uint8_t)hibyte) {
        ++cycle_count;
    }
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+3);
    ASSERT(run_args.cpu65_x       == regX);
    ASSERT(run_args.cpu65_y       == 0x05);
    ASSERT(run_args.cpu65_sp      == 0x81);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xfd);

    cycle_count += 4;
    ASSERT(run_args.cpu65_opcycles == cycle_count);

    PASS();
}

TEST test_SBC_abs_y(uint8_t regA, uint8_t val, uint8_t regY, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_SBC(regA, val, &result, &flags);

    testcpu_set_opcode3(0xf9, lobyte, hibyte);

    uint8_t cycle_count = 0;
    uint16_t addrs = lobyte | (hibyte<<8);
    addrs = addrs + regY;
    if ((uint8_t)((addrs>>8)&0xff) != (uint8_t)hibyte) {
        ++cycle_count;
    }
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x02;
    run_args.cpu65_y  = regY;
    run_args.cpu65_sp = 0x81;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+3);
    ASSERT(run_args.cpu65_x       == 0x02);
    ASSERT(run_args.cpu65_y       == regY);
    ASSERT(run_args.cpu65_sp      == 0x81);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xf9);

    cycle_count += 4;
    ASSERT(run_args.cpu65_opcycles == cycle_count);

    PASS();
}

TEST test_SBC_ind_x(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t regX, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_SBC(regA, val, &result, &flags);

    testcpu_set_opcode2(0xe1, arg0);

    uint8_t idx_lo = arg0 + regX;
    uint8_t idx_hi = idx_lo+1;
    uint16_t addrs = lobyte | (hibyte<<8);

    apple_ii_64k[0][idx_lo] = lobyte;
    apple_ii_64k[0][idx_hi] = hibyte;
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x15;
    run_args.cpu65_sp = 0x81;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_x       == regX);
    ASSERT(run_args.cpu65_y       == 0x15);
    ASSERT(run_args.cpu65_sp      == 0x81);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xe1);

    ASSERT(run_args.cpu65_opcycles  == (6));

    PASS();
}

TEST test_SBC_ind_y(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t regY, uint8_t val_zp0, uint8_t val_zp1) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_SBC(regA, val, &result, &flags);

    testcpu_set_opcode2(0xf1, arg0);

    uint8_t idx0 = arg0;
    uint8_t idx1 = arg0+1;

    apple_ii_64k[0][idx0] = val_zp0;
    apple_ii_64k[0][idx1] = val_zp1;

    uint8_t cycle_count = 0;
    uint16_t addrs = val_zp0 | (val_zp1<<8);
    addrs += (uint8_t)regY;
    if ((uint8_t)((addrs>>8)&0xff) != (uint8_t)val_zp1) {
        ++cycle_count;
    }

    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x84;
    run_args.cpu65_y  = regY;
    run_args.cpu65_sp = 0x81;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_x       == 0x84);
    ASSERT(run_args.cpu65_y       == regY);
    ASSERT(run_args.cpu65_sp      == 0x81);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xf1);
    cycle_count += 5;
    ASSERT(run_args.cpu65_opcycles == cycle_count);

    PASS();
}

TEST test_SBC_ind_zpage(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_SBC(regA, val, &result, &flags);

    testcpu_set_opcode2(0xf2, arg0);

    uint8_t idx0 = arg0;
    uint8_t idx1 = arg0+1;

    apple_ii_64k[0][idx0] = lobyte;
    apple_ii_64k[0][idx1] = hibyte;

    uint16_t addrs = lobyte | (hibyte<<8);
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x14;
    run_args.cpu65_y  = 0x85;
    run_args.cpu65_sp = 0x81;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_x       == 0x14);
    ASSERT(run_args.cpu65_y       == 0x85);
    ASSERT(run_args.cpu65_sp      == 0x81);

    ASSERT(run_args.cpu65_a == result);
    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_READ);
    ASSERT(run_args.cpu65_opcode    == 0xf2);
    ASSERT(run_args.cpu65_opcycles  == (5));

    PASS();
}

// ----------------------------------------------------------------------------
// SEx operands [sic]

TEST test_SEC(void) {
    testcpu_set_opcode1(0x38);

    run_args.cpu65_a  = 0x02;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc     == TEST_LOC+1);
    ASSERT(run_args.cpu65_a      == 0x02);
    ASSERT(run_args.cpu65_x      == 0x03);
    ASSERT(run_args.cpu65_y      == 0x04);
    ASSERT(run_args.cpu65_sp     == 0x81);
    ASSERT(run_args.cpu65_f      == (fC));

    ASSERT(run_args.cpu65_ea       == TEST_LOC);
    ASSERT(run_args.cpu65_d        == 0xff);
    ASSERT(run_args.cpu65_rw       == RW_NONE);
    ASSERT(run_args.cpu65_opcode   == 0x38);
    ASSERT(run_args.cpu65_opcycles == (2));

    PASS();
}

TEST test_SED(void) {
    testcpu_set_opcode1(0xf8);

    run_args.cpu65_a  = 0x02;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc     == TEST_LOC+1);
    ASSERT(run_args.cpu65_a      == 0x02);
    ASSERT(run_args.cpu65_x      == 0x03);
    ASSERT(run_args.cpu65_y      == 0x04);
    ASSERT(run_args.cpu65_sp     == 0x81);
    ASSERT(run_args.cpu65_f      == (fD));

    ASSERT(run_args.cpu65_ea       == TEST_LOC);
    ASSERT(run_args.cpu65_d        == 0xff);
    ASSERT(run_args.cpu65_rw       == RW_NONE);
    ASSERT(run_args.cpu65_opcode   == 0xf8);
    ASSERT(run_args.cpu65_opcycles == (2));

    PASS();
}

TEST test_SEI(void) {
    testcpu_set_opcode1(0x78);

    run_args.cpu65_a  = 0x02;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc     == TEST_LOC+1);
    ASSERT(run_args.cpu65_a      == 0x02);
    ASSERT(run_args.cpu65_x      == 0x03);
    ASSERT(run_args.cpu65_y      == 0x04);
    ASSERT(run_args.cpu65_sp     == 0x81);
    ASSERT(run_args.cpu65_f      == (fI));

    ASSERT(run_args.cpu65_ea       == TEST_LOC);
    ASSERT(run_args.cpu65_d        == 0xff);
    ASSERT(run_args.cpu65_rw       == RW_NONE);
    ASSERT(run_args.cpu65_opcode   == 0x78);
    ASSERT(run_args.cpu65_opcycles == (2));

    PASS();
}

// ----------------------------------------------------------------------------
// STA instructions

TEST test_STA_zpage(uint8_t regA, uint8_t val, uint8_t arg0) {

    testcpu_set_opcode2(0x85, arg0);

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(apple_ii_64k[0][arg0] == regA);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == 0x03);
    ASSERT(run_args.cpu65_y       == 0x04);
    ASSERT(run_args.cpu65_sp      == 0x80);
    ASSERT(run_args.cpu65_f       == 0x00);

    ASSERT(run_args.cpu65_ea        == arg0);
    ASSERT(run_args.cpu65_d         == regA);
    ASSERT(run_args.cpu65_rw        == (RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0x85);
    ASSERT(run_args.cpu65_opcycles  == (3));

    PASS();
}

TEST test_STA_zpage_x(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t regX) {

    testcpu_set_opcode2(0x95, arg0);

    uint8_t idx = arg0+regX;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(apple_ii_64k[0][idx] == regA);

    ASSERT(run_args.cpu65_a   == regA);
    ASSERT(run_args.cpu65_pc  == TEST_LOC+2);
    ASSERT(run_args.cpu65_x   == regX);
    ASSERT(run_args.cpu65_y   == 0x05);
    ASSERT(run_args.cpu65_sp  == 0x81);
    ASSERT(run_args.cpu65_f   == 0x00);

    ASSERT(run_args.cpu65_ea        == idx);
    ASSERT(run_args.cpu65_d         == regA);
    ASSERT(run_args.cpu65_rw        == (RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0x95);
    ASSERT(run_args.cpu65_opcycles  == (4));

    PASS();
}

TEST test_STA_abs(uint8_t regA, uint8_t val, uint8_t lobyte, uint8_t hibyte) {

    testcpu_set_opcode3(0x8d, lobyte, hibyte);

    uint16_t addrs = lobyte | (hibyte<<8);

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0xf4;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(apple_ii_64k[0][addrs] == regA);

    ASSERT(run_args.cpu65_a  == regA);
    ASSERT(run_args.cpu65_pc == TEST_LOC+3);
    ASSERT(run_args.cpu65_x  == 0xf4);
    ASSERT(run_args.cpu65_y  == 0x05);
    ASSERT(run_args.cpu65_sp == 0x81);
    ASSERT(run_args.cpu65_f  == 0x00);

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == regA);
    ASSERT(run_args.cpu65_rw        == (RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0x8d);
    ASSERT(run_args.cpu65_opcycles  == (4));

    PASS();
}

TEST test_STA_abs_x(uint8_t regA, uint8_t val, uint8_t regX, uint8_t lobyte, uint8_t hibyte) {

    testcpu_set_opcode3(0x9d, lobyte, hibyte);

    uint16_t addrs = lobyte | (hibyte<<8);
    addrs = addrs + regX;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(apple_ii_64k[0][addrs] == regA);

    ASSERT(run_args.cpu65_a  == regA);
    ASSERT(run_args.cpu65_pc == TEST_LOC+3);
    ASSERT(run_args.cpu65_x  == regX);
    ASSERT(run_args.cpu65_y  == 0x05);
    ASSERT(run_args.cpu65_sp == 0x81);
    ASSERT(run_args.cpu65_f  == 0x00);

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == regA);
    ASSERT(run_args.cpu65_rw        == (RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0x9d);
    ASSERT(run_args.cpu65_opcycles  == 5);

    PASS();
}

TEST test_STA_abs_y(uint8_t regA, uint8_t val, uint8_t regY, uint8_t lobyte, uint8_t hibyte) {

    testcpu_set_opcode3(0x99, lobyte, hibyte);

    uint16_t addrs = lobyte | (hibyte<<8);
    addrs = addrs + regY;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x02;
    run_args.cpu65_y  = regY;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(apple_ii_64k[0][addrs] == regA);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+3);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == 0x02);
    ASSERT(run_args.cpu65_y       == regY);
    ASSERT(run_args.cpu65_sp      == 0x81);
    ASSERT(run_args.cpu65_f       == 0x00);

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == regA);
    ASSERT(run_args.cpu65_rw        == RW_WRITE);
    ASSERT(run_args.cpu65_opcode    == 0x99);
    ASSERT(run_args.cpu65_opcycles  == 5);

    PASS();
}

TEST test_STA_ind_x(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t regX, uint8_t lobyte, uint8_t hibyte) {

    testcpu_set_opcode2(0x81, arg0);

    uint8_t idx_lo = arg0 + regX;
    uint8_t idx_hi = idx_lo+1;
    uint16_t addrs = lobyte | (hibyte<<8);

    apple_ii_64k[0][idx_lo] = lobyte;
    apple_ii_64k[0][idx_hi] = hibyte;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x15;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(apple_ii_64k[0][addrs] == regA);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == regX);
    ASSERT(run_args.cpu65_y       == 0x15);
    ASSERT(run_args.cpu65_sp      == 0x81);
    ASSERT(run_args.cpu65_f       == 0x00);

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == regA);
    ASSERT(run_args.cpu65_rw        == (RW_READ|RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0x81);

    ASSERT(run_args.cpu65_opcycles  == (6));

    PASS();
}

TEST test_STA_ind_y(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t regY, uint8_t val_zp0, uint8_t val_zp1) {

    testcpu_set_opcode2(0x91, arg0);

    uint8_t idx0 = arg0;
    uint8_t idx1 = arg0+1;

    apple_ii_64k[0][idx0] = val_zp0;
    apple_ii_64k[0][idx1] = val_zp1;

    uint16_t addrs = val_zp0 | (val_zp1<<8);
    addrs += (uint8_t)regY;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x84;
    run_args.cpu65_y  = regY;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(apple_ii_64k[0][addrs] == regA);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == 0x84);
    ASSERT(run_args.cpu65_y       == regY);
    ASSERT(run_args.cpu65_sp      == 0x81);
    ASSERT(run_args.cpu65_f       == 0x00);

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == regA);
    ASSERT(run_args.cpu65_rw        == (RW_READ|RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0x91);
    ASSERT(run_args.cpu65_opcycles  == 6);

    PASS();
}

// 65c02 : 0x92
TEST test_STA_ind_zpage(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t lobyte, uint8_t hibyte) {

    testcpu_set_opcode2(0x92, arg0);

    uint8_t idx0 = arg0;
    uint8_t idx1 = arg0+1;

    apple_ii_64k[0][idx0] = lobyte;
    apple_ii_64k[0][idx1] = hibyte;

    uint16_t addrs = lobyte | (hibyte<<8);

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x14;
    run_args.cpu65_y  = 0x85;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(apple_ii_64k[0][addrs] == regA);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == 0x14);
    ASSERT(run_args.cpu65_y       == 0x85);
    ASSERT(run_args.cpu65_sp      == 0x81);
    ASSERT(run_args.cpu65_f       == 0x00);

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == regA);
    ASSERT(run_args.cpu65_rw        == (RW_READ|RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0x92);
    ASSERT(run_args.cpu65_opcycles  == (5));

    PASS();
}

// ----------------------------------------------------------------------------
// STx instructions

TEST test_STX_zpage(uint8_t regX, uint8_t val, uint8_t arg0) {

    testcpu_set_opcode2(0x86, arg0);

    run_args.cpu65_a  = 0xe3;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(apple_ii_64k[0][arg0] == regX);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_a       == 0xe3);
    ASSERT(run_args.cpu65_x       == regX);
    ASSERT(run_args.cpu65_y       == 0x04);
    ASSERT(run_args.cpu65_sp      == 0x80);
    ASSERT(run_args.cpu65_f       == 0x00);

    ASSERT(run_args.cpu65_ea        == arg0);
    ASSERT(run_args.cpu65_d         == regX);
    ASSERT(run_args.cpu65_rw        == (RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0x86);
    ASSERT(run_args.cpu65_opcycles  == (3));

    PASS();
}

TEST test_STX_zpage_y(uint8_t regX, uint8_t val, uint8_t arg0, uint8_t regY) {

    testcpu_set_opcode2(0x96, arg0);

    uint8_t idx = arg0+regY;

    run_args.cpu65_a  = 0xcc;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = regY;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(apple_ii_64k[0][idx]  == regX);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_a       == 0xcc);
    ASSERT(run_args.cpu65_x       == regX);
    ASSERT(run_args.cpu65_y       == regY);
    ASSERT(run_args.cpu65_sp      == 0x81);
    ASSERT(run_args.cpu65_f       == 0x00);

    ASSERT(run_args.cpu65_ea        == idx);
    ASSERT(run_args.cpu65_d         == regX);
    ASSERT(run_args.cpu65_rw        == RW_WRITE);
    ASSERT(run_args.cpu65_opcode    == 0x96);
    ASSERT(run_args.cpu65_opcycles  == (4));

    PASS();
}

TEST test_STX_abs(uint8_t regX, uint8_t val, uint8_t lobyte, uint8_t hibyte) {

    testcpu_set_opcode3(0x8e, lobyte, hibyte);

    uint16_t addrs = lobyte | (hibyte<<8);

    run_args.cpu65_a  = 0xf4;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(apple_ii_64k[0][addrs] == regX);

    ASSERT(run_args.cpu65_a  == 0xf4);
    ASSERT(run_args.cpu65_pc == TEST_LOC+3);
    ASSERT(run_args.cpu65_x  == regX);
    ASSERT(run_args.cpu65_y  == 0x05);
    ASSERT(run_args.cpu65_sp == 0x81);
    ASSERT(run_args.cpu65_f  == 0x00);

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == regX);
    ASSERT(run_args.cpu65_rw        == (RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0x8e);
    ASSERT(run_args.cpu65_opcycles  == (4));

    PASS();
}

TEST test_STY_zpage(uint8_t regY, uint8_t val, uint8_t arg0) {

    testcpu_set_opcode2(0x84, arg0);

    run_args.cpu65_a  = 0xa8;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = regY;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(apple_ii_64k[0][arg0] == regY);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_a       == 0xa8);
    ASSERT(run_args.cpu65_x       == 0x03);
    ASSERT(run_args.cpu65_y       == regY);
    ASSERT(run_args.cpu65_sp      == 0x80);
    ASSERT(run_args.cpu65_f       == 0x00);

    ASSERT(run_args.cpu65_ea        == arg0);
    ASSERT(run_args.cpu65_d         == regY);
    ASSERT(run_args.cpu65_rw        == (RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0x84);
    ASSERT(run_args.cpu65_opcycles  == (3));

    PASS();
}

TEST test_STY_zpage_x(uint8_t regY, uint8_t val, uint8_t arg0, uint8_t regX) {

    testcpu_set_opcode2(0x94, arg0);

    uint8_t idx = arg0+regX;

    run_args.cpu65_a  = 0x11;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = regY;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(apple_ii_64k[0][idx] == regY);

    ASSERT(run_args.cpu65_a   == 0x11);
    ASSERT(run_args.cpu65_pc  == TEST_LOC+2);
    ASSERT(run_args.cpu65_x   == regX);
    ASSERT(run_args.cpu65_y   == regY);
    ASSERT(run_args.cpu65_sp  == 0x81);
    ASSERT(run_args.cpu65_f   == 0x00);

    ASSERT(run_args.cpu65_ea        == idx);
    ASSERT(run_args.cpu65_d         == regY);
    ASSERT(run_args.cpu65_rw        == (RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0x94);
    ASSERT(run_args.cpu65_opcycles  == (4));

    PASS();
}

TEST test_STY_abs(uint8_t regY, uint8_t val, uint8_t lobyte, uint8_t hibyte) {

    testcpu_set_opcode3(0x8c, lobyte, hibyte);

    uint16_t addrs = lobyte | (hibyte<<8);

    run_args.cpu65_a  = 0x4f;
    run_args.cpu65_x  = 0xf4;
    run_args.cpu65_y  = regY;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(apple_ii_64k[0][addrs] == regY);

    ASSERT(run_args.cpu65_a  == 0x4f);
    ASSERT(run_args.cpu65_pc == TEST_LOC+3);
    ASSERT(run_args.cpu65_x  == 0xf4);
    ASSERT(run_args.cpu65_y  == regY);
    ASSERT(run_args.cpu65_sp == 0x81);
    ASSERT(run_args.cpu65_f  == 0x00);

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == regY);
    ASSERT(run_args.cpu65_rw        == (RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0x8c);
    ASSERT(run_args.cpu65_opcycles  == (4));

    PASS();
}

// 65c02 : 0x64
TEST test_STZ_zpage(uint8_t regA, uint8_t val, uint8_t arg0) {

    testcpu_set_opcode2(0x64, arg0);

    apple_ii_64k[0][arg0] = 0xff;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(apple_ii_64k[0][arg0] == 0x00);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == 0x03);
    ASSERT(run_args.cpu65_y       == 0x04);
    ASSERT(run_args.cpu65_sp      == 0x80);
    ASSERT(run_args.cpu65_f       == 0x00);

    ASSERT(run_args.cpu65_ea        == arg0);
    ASSERT(run_args.cpu65_d         == 0x00);
    ASSERT(run_args.cpu65_rw        == (RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0x64);
    ASSERT(run_args.cpu65_opcycles  == (3));

    PASS();
}

// 65c02 : 0x74
TEST test_STZ_zpage_x(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t regX) {

    testcpu_set_opcode2(0x74, arg0);

    uint8_t idx = arg0+regX;

    apple_ii_64k[0][idx] = 0xff;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(apple_ii_64k[0][idx] == 0x00);

    ASSERT(run_args.cpu65_a   == regA);
    ASSERT(run_args.cpu65_pc  == TEST_LOC+2);
    ASSERT(run_args.cpu65_x   == regX);
    ASSERT(run_args.cpu65_y   == 0x05);
    ASSERT(run_args.cpu65_sp  == 0x81);
    ASSERT(run_args.cpu65_f   == 0x00);

    ASSERT(run_args.cpu65_ea        == idx);
    ASSERT(run_args.cpu65_d         == 0x00);
    ASSERT(run_args.cpu65_rw        == (RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0x74);
    ASSERT(run_args.cpu65_opcycles  == (4));

    PASS();
}

// 65c02 : 0x9C
TEST test_STZ_abs(uint8_t regA, uint8_t val, uint8_t lobyte, uint8_t hibyte) {

    testcpu_set_opcode3(0x9c, lobyte, hibyte);

    uint16_t addrs = lobyte | (hibyte<<8);

    apple_ii_64k[0][addrs] = 0xff;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0xf4;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(apple_ii_64k[0][addrs] == 0x00);

    ASSERT(run_args.cpu65_a  == regA);
    ASSERT(run_args.cpu65_pc == TEST_LOC+3);
    ASSERT(run_args.cpu65_x  == 0xf4);
    ASSERT(run_args.cpu65_y  == 0x05);
    ASSERT(run_args.cpu65_sp == 0x81);
    ASSERT(run_args.cpu65_f  == 0x00);

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0x00);
    ASSERT(run_args.cpu65_rw        == (RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0x9c);
    ASSERT(run_args.cpu65_opcycles  == (4));

    PASS();
}

// 65c02 : 0x9E
TEST test_STZ_abs_x(uint8_t regA, uint8_t val, uint8_t regX, uint8_t lobyte, uint8_t hibyte) {

    testcpu_set_opcode3(0x9e, lobyte, hibyte);

    uint16_t addrs = lobyte | (hibyte<<8);
    addrs = addrs + regX;

    apple_ii_64k[0][addrs] = 0xff;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(apple_ii_64k[0][addrs] == 0x00);

    ASSERT(run_args.cpu65_a  == regA);
    ASSERT(run_args.cpu65_pc == TEST_LOC+3);
    ASSERT(run_args.cpu65_x  == regX);
    ASSERT(run_args.cpu65_y  == 0x05);
    ASSERT(run_args.cpu65_sp == 0x81);
    ASSERT(run_args.cpu65_f  == 0x00);

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == 0x00);
    ASSERT(run_args.cpu65_rw        == (RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0x9e);
    ASSERT(run_args.cpu65_opcycles  == 5);

    PASS();
}

// ----------------------------------------------------------------------------
// TAx, TxA instructions

static void logic_TAx(/*uint8_t*/int _a, uint8_t *flags) {
    uint8_t a = (uint8_t)_a;

    if ((a & 0xff) == 0x0) {
        *flags |= fZ;
    }
    if (a & 0x80) {
        *flags |= fN;
    }
}

TEST test_TAX(uint8_t regA) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    uint8_t val = regA;

    logic_TAx(regA, &flags);
    uint8_t regX = ~regA;

    testcpu_set_opcode1(0xaa);

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+1);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == regA);
    ASSERT(run_args.cpu65_y       == 0x04);
    ASSERT(run_args.cpu65_sp      == 0x80);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == TEST_LOC);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_NONE);
    ASSERT(run_args.cpu65_opcode    == 0xaa);
    ASSERT(run_args.cpu65_opcycles  == (2));

    PASS();
}

TEST test_TAY(uint8_t regA) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    uint8_t val = regA;

    logic_TAx(regA, &flags);
    uint8_t regY = ~regA;

    testcpu_set_opcode1(0xa8);

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x6e;
    run_args.cpu65_y  = regY;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+1);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == 0x6e);
    ASSERT(run_args.cpu65_y       == regA);
    ASSERT(run_args.cpu65_sp      == 0x80);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == TEST_LOC);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_NONE);
    ASSERT(run_args.cpu65_opcode    == 0xa8);
    ASSERT(run_args.cpu65_opcycles  == (2));

    PASS();
}

TEST test_TXA(uint8_t regX) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    uint8_t val = regX;

    logic_TAx(regX, &flags);
    uint8_t regA = ~regX;

    testcpu_set_opcode1(0x8a);

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+1);
    ASSERT(run_args.cpu65_a       == regX);
    ASSERT(run_args.cpu65_x       == regX);
    ASSERT(run_args.cpu65_y       == 0x04);
    ASSERT(run_args.cpu65_sp      == 0x80);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == TEST_LOC);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_NONE);
    ASSERT(run_args.cpu65_opcode    == 0x8a);
    ASSERT(run_args.cpu65_opcycles  == (2));

    PASS();
}

TEST test_TYA(uint8_t regY) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    uint8_t val = regY;

    logic_TAx(regY, &flags);
    uint8_t regA = ~regY;

    testcpu_set_opcode1(0x98);

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0xa4;
    run_args.cpu65_y  = regY;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+1);
    ASSERT(run_args.cpu65_a       == regY);
    ASSERT(run_args.cpu65_x       == 0xa4);
    ASSERT(run_args.cpu65_y       == regY);
    ASSERT(run_args.cpu65_sp      == 0x80);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == TEST_LOC);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_NONE);
    ASSERT(run_args.cpu65_opcode    == 0x98);
    ASSERT(run_args.cpu65_opcycles  == (2));

    PASS();
}

// ----------------------------------------------------------------------------
// TRB & TSB operands

static void logic_TRB(/*uint8_t*/int _a, /*uint8_t*/int _b, uint8_t *result, uint8_t *flags) {
    uint8_t a = (uint8_t)_a;
    uint8_t b = (uint8_t)_b;

    uint8_t res = (~a) & b;
    if ((a & b) == 0x0) {
        *flags |= fZ;
    }

    *result = res;
}

TEST test_TRB_abs(uint8_t regA, uint8_t val, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_TRB(regA, val, &result, &flags);

    testcpu_set_opcode3(0x1c, lobyte, hibyte);

    uint16_t addrs = lobyte | (hibyte<<8);
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0xf4;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(apple_ii_64k[0][addrs] == result);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+3);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == 0xf4);
    ASSERT(run_args.cpu65_y       == 0x05);
    ASSERT(run_args.cpu65_sp      == 0x81);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == result);
    ASSERT(run_args.cpu65_rw        == (RW_READ|RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0x1c);
    ASSERT(run_args.cpu65_opcycles  == (6));

    PASS();
}

// 65c02 : 0x14
TEST test_TRB_zpage(uint8_t regA, uint8_t val, uint8_t arg0) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_TRB(regA, val, &result, &flags);

    testcpu_set_opcode2(0x14, arg0);

    apple_ii_64k[0][arg0] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(apple_ii_64k[0][arg0] == result);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == 0x03);
    ASSERT(run_args.cpu65_y       == 0x04);
    ASSERT(run_args.cpu65_sp      == 0x80);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == arg0);
    ASSERT(run_args.cpu65_d         == result);
    ASSERT(run_args.cpu65_rw        == (RW_READ|RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0x14);
    ASSERT(run_args.cpu65_opcycles  == (5));

    PASS();
}

static void logic_TSB(/*uint8_t*/int _a, /*uint8_t*/int _b, uint8_t *result, uint8_t *flags) {
    uint8_t a = (uint8_t)_a;
    uint8_t b = (uint8_t)_b;

    uint8_t res = a | b;
    if ((a & b) == 0x0) {
        *flags |= fZ;
    }

    *result = res;
}

// 65c02 : 0x0C
TEST test_TSB_abs(uint8_t regA, uint8_t val, uint8_t lobyte, uint8_t hibyte) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_TSB(regA, val, &result, &flags);

    testcpu_set_opcode3(0x0c, lobyte, hibyte);

    uint16_t addrs = lobyte | (hibyte<<8);
    apple_ii_64k[0][addrs] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0xf4;
    run_args.cpu65_y  = 0x05;
    run_args.cpu65_sp = 0x81;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(apple_ii_64k[0][addrs] == result);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+3);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == 0xf4);
    ASSERT(run_args.cpu65_y       == 0x05);
    ASSERT(run_args.cpu65_sp      == 0x81);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == addrs);
    ASSERT(run_args.cpu65_d         == result);
    ASSERT(run_args.cpu65_rw        == (RW_READ|RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0x0c);
    ASSERT(run_args.cpu65_opcycles  == (6));

    PASS();
}

// 65c02 : 0x04
TEST test_TSB_zpage(uint8_t regA, uint8_t val, uint8_t arg0) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;

    logic_TSB(regA, val, &result, &flags);

    testcpu_set_opcode2(0x04, arg0);

    apple_ii_64k[0][arg0] = val;

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = 0x03;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = 0x80;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(apple_ii_64k[0][arg0] == result);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+2);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == 0x03);
    ASSERT(run_args.cpu65_y       == 0x04);
    ASSERT(run_args.cpu65_sp      == 0x80);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == arg0);
    ASSERT(run_args.cpu65_d         == result);
    ASSERT(run_args.cpu65_rw        == (RW_READ|RW_WRITE));
    ASSERT(run_args.cpu65_opcode    == 0x04);
    ASSERT(run_args.cpu65_opcycles  == (5));

    PASS();
}

// ----------------------------------------------------------------------------
// TSX, TXS instructions

static void logic_TSX(/*uint8_t*/int _a, uint8_t *flags) {
    uint8_t a = (uint8_t)_a;

    if ((a & 0xff) == 0x0) {
        *flags |= fZ;
    }
    if (a & 0x80) {
        *flags |= fN;
    }
}

TEST test_TSX(uint8_t sp) {
    uint8_t result = 0x0;
    uint8_t flags = 0x0;
    uint8_t regA = (uint8_t)random();
    uint8_t val = regA;
    uint8_t regX = ~sp;

    logic_TSX(sp, &flags);

    testcpu_set_opcode1(0xba);

    run_args.cpu65_a  = regA;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = sp;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+1);
    ASSERT(run_args.cpu65_a       == regA);
    ASSERT(run_args.cpu65_x       == sp);
    ASSERT(run_args.cpu65_y       == 0x04);
    ASSERT(run_args.cpu65_sp      == sp);

    VERIFY_FLAGS();

    ASSERT(run_args.cpu65_ea        == TEST_LOC);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_NONE);
    ASSERT(run_args.cpu65_opcode    == 0xba);
    ASSERT(run_args.cpu65_opcycles  == (2));

    PASS();
}

TEST test_TXS(uint8_t regX) {
    uint8_t sp = ~regX;

    testcpu_set_opcode1(0x9a);

    run_args.cpu65_a  = 0x22;
    run_args.cpu65_x  = regX;
    run_args.cpu65_y  = 0x04;
    run_args.cpu65_sp = sp;
    run_args.cpu65_f  = 0x00;

    cpu65_run(&run_args);

    ASSERT(run_args.cpu65_pc      == TEST_LOC+1);
    ASSERT(run_args.cpu65_a       == 0x22);
    ASSERT(run_args.cpu65_x       == regX);
    ASSERT(run_args.cpu65_y       == 0x04);
    ASSERT(run_args.cpu65_sp      == regX);
    ASSERT(run_args.cpu65_f       == 0x00);

    ASSERT(run_args.cpu65_ea        == TEST_LOC);
    ASSERT(run_args.cpu65_d         == 0xff);
    ASSERT(run_args.cpu65_rw        == RW_NONE);
    ASSERT(run_args.cpu65_opcode    == 0x9a);
    ASSERT(run_args.cpu65_opcycles  == (2));

    PASS();
}

// ----------------------------------------------------------------------------
// Test Suite

typedef struct test_func_t {
    char *name;
    void (*func)(void);
} test_func_t;

GREATEST_SUITE(test_suite_cpu) {

    GREATEST_SET_SETUP_CB(testcpu_setup, NULL);
    GREATEST_SET_TEARDOWN_CB(testcpu_teardown, NULL);
    GREATEST_SET_BREAKPOINT_CB(test_breakpoint, NULL);

    test_common_init();
    assert(cpu_thread_id == 0 && "This test is not designed to run with alternate CPU thread");
    extern void reinitialize(void);
    reinitialize();

    run_args.emul_reinitialize = 0;

    // --------------------------------

    fprintf(GREATEST_STDOUT, "\ntest_BRK :\n");
    RUN_TEST(test_BRK);
    fprintf(GREATEST_STDOUT, "\ntest_IRQ :\n");
    RUN_TEST(test_IRQ);
    fprintf(GREATEST_STDOUT, "\ntest_CLC :\n");
    RUN_TEST(test_CLC);
    fprintf(GREATEST_STDOUT, "\ntest_CLD :\n");
    RUN_TEST(test_CLD);
    fprintf(GREATEST_STDOUT, "\ntest_CLI :\n");
    RUN_TEST(test_CLI);
    fprintf(GREATEST_STDOUT, "\ntest_CLV :\n");
    RUN_TEST(test_CLV);
    fprintf(GREATEST_STDOUT, "\ntest_NOP :\n");
    RUN_TEST(test_NOP);
    fprintf(GREATEST_STDOUT, "\ntest_PHA :\n");
    RUN_TEST(test_PHA);
    fprintf(GREATEST_STDOUT, "\ntest_PHX :\n");
    RUN_TEST(test_PHX);
    fprintf(GREATEST_STDOUT, "\ntest_PHY :\n");
    RUN_TEST(test_PHY);
    fprintf(GREATEST_STDOUT, "\ntest_SEC :\n");
    RUN_TEST(test_SEC);
    fprintf(GREATEST_STDOUT, "\ntest_SED :\n");
    RUN_TEST(test_SED);
    fprintf(GREATEST_STDOUT, "\ntest_SEI :\n");
    RUN_TEST(test_SEI);

    // ------------------------------------------------------------------------
    // Branch tests :
    // NOTE : these should be a comprehensive exercise of the branching logic

    greatest_info.flags = GREATEST_FLAG_SILENT_SUCCESS;

    do {
        test_func_t funcs[] = {
            { "test_BCC", (void(*)(void))test_BCC, },
            { "test_BCS", (void(*)(void))test_BCS, },
            { "test_BEQ", (void(*)(void))test_BEQ, },
            { "test_BNE", (void(*)(void))test_BNE, },
            { "test_BMI", (void(*)(void))test_BMI, },
            { "test_BPL", (void(*)(void))test_BPL, },
            { "test_BRA", (void(*)(void))test_BRA, },
            { "test_BVC", (void(*)(void))test_BVC, },
            { "test_BVS", (void(*)(void))test_BVS, },
        };
        const unsigned int count = sizeof(funcs) / sizeof(test_func_t);

        for (unsigned int i=0; i<count; i++) {
            char *name = funcs[i].name;
            int (*func)(int8_t, bool, uint16_t) = (int (*)(int8_t, bool, uint16_t))funcs[i].func;

            fprintf(GREATEST_STDOUT, "\n%s (SILENCED OUTPUT) :\n", name);

            for (uint16_t addrs = 0x1f02; addrs < 0x2000; addrs++) {
                for (uint8_t flag = 0x00; flag < 0x02; flag++) {
                    uint8_t off=0x00;
                    do {
                        RUN_TESTp(func, off, flag, addrs);
                    } while (++off);
                }
            }

            // 16bit branch overflow tests
            for (uint16_t addrs = 0xff00; addrs >= 0xff00 || addrs < 0x00fe; addrs++) {
                for (uint8_t flag = 0x00; flag < 0x02; flag++) {
                    uint8_t off=0x00;
                    do {
                        RUN_TESTp(func, off, flag, addrs);
                    } while (++off);
                }
            }

            // 16bit branch underflow tests
            for (uint16_t addrs = 0x00fe; addrs <= 0x00fe || addrs > 0xff00; addrs--) {
                for (uint8_t flag = 0x00; flag < 0x02; flag++) {
                    uint8_t off=0x00;
                    do {
                        RUN_TESTp(func, off, flag, addrs);
                    } while (++off);
                }
            }

            fprintf(GREATEST_STDOUT, "...OK\n");
        }
    } while (0);

    greatest_info.flags = 0x0;

    // ------------------------------------------------------------------------
    // Immediate addressing mode tests #1 :
    // NOTE : these should be a comprehensive exercise of the instruction logic

    greatest_info.flags = GREATEST_FLAG_SILENT_SUCCESS;

    do {
        test_func_t funcs[] = {
            { "test_ADC_imm", (void(*)(void))test_ADC_imm, },
            { "test_AND_imm", (void(*)(void))test_AND_imm, },
            { "test_BIT_imm", (void(*)(void))test_BIT_imm, },
            { "test_CMP_imm", (void(*)(void))test_CMP_imm, },
            { "test_CPX_imm", (void(*)(void))test_CPX_imm, },
            { "test_CPY_imm", (void(*)(void))test_CPY_imm, },
            { "test_EOR_imm", (void(*)(void))test_EOR_imm, },
            { "test_LDA_imm", (void(*)(void))test_LDA_imm, },
            { "test_LDX_imm", (void(*)(void))test_LDX_imm, },
            { "test_LDY_imm", (void(*)(void))test_LDY_imm, },
            { "test_ORA_imm", (void(*)(void))test_ORA_imm, },
            { "test_SBC_imm", (void(*)(void))test_SBC_imm, },
        };
        const unsigned int count = sizeof(funcs) / sizeof(test_func_t);

        for (unsigned int i=0; i<count; i++) {
            char *name = funcs[i].name;
            int (*func)(uint8_t, uint8_t, bool, bool) = (int (*)(uint8_t, uint8_t, bool, bool))funcs[i].func;

            fprintf(GREATEST_STDOUT, "\n%s (SILENCED OUTPUT) :\n", name);

            // test comprehensive logic in immediate mode (since no addressing to test) ...
            uint8_t regA=0x00;
            do {
                uint8_t val=0x00;
                do {
                    RUN_TESTp(func, regA, val, /*decimal*/false, /*carry*/false);
                    RUN_TESTp(func, regA, val, /*decimal*/ true, /*carry*/false);
                    RUN_TESTp(func, regA, val, /*decimal*/false, /*carry*/true);
                    RUN_TESTp(func, regA, val, /*decimal*/ true, /*carry*/true);
                } while (++val);
            } while (++regA);

            fprintf(GREATEST_STDOUT, "...OK\n");
        }
    } while (0);

    greatest_info.flags = 0x0;

    // ------------------------------------------------------------------------
    // Immediate/absolute addressing mode tests

    greatest_info.flags = GREATEST_FLAG_SILENT_SUCCESS;

    do {
        test_func_t funcs[] = {
            { "test_JMP_abs",       (void(*)(void))test_JMP_abs, },
            { "test_JMP_abs_ind_x", (void(*)(void))test_JMP_abs_ind_x, },
            { "test_JMP_ind",       (void(*)(void))test_JMP_ind, },
            { "test_JSR_abs",       (void(*)(void))test_JSR_abs, },
            { "test_RTS",           (void(*)(void))test_RTS, },
        };
        const unsigned int count = sizeof(funcs) / sizeof(test_func_t);

        for (unsigned int i=0; i<count; i++) {
            char *name = funcs[i].name;
            int (*func)(uint8_t, uint8_t, uint16_t, uint8_t) = (int (*)(uint8_t, uint8_t, uint16_t, uint8_t))funcs[i].func;

            fprintf(GREATEST_STDOUT, "\n%s (SILENCED OUTPUT) :\n", name);

            // test comprehensive logic in immediate mode (since no addressing to test) ...
            uint8_t lobyte=0x00;
            do {
                uint8_t hibyte=0x00;
                do {
                    RUN_TESTp(func, lobyte, hibyte, TEST_LOC, (uint8_t)random());
                } while (++hibyte);
            } while (++lobyte);

            // test 16bit overflow/underflow
            lobyte=0x00;
            do {
                uint8_t regX=0x00;
                do {
                    RUN_TESTp(func, lobyte, (uint8_t)random(), 0xFFFC, regX);
                    RUN_TESTp(func, lobyte, (uint8_t)random(), 0xFFFD, regX);
                    RUN_TESTp(func, lobyte, (uint8_t)random(), 0xFFFE, regX);
                    RUN_TESTp(func, lobyte, (uint8_t)random(), 0xFFFF, regX);
                    RUN_TESTp(func, lobyte, (uint8_t)random(), 0x0000, regX);
                    RUN_TESTp(func, lobyte, (uint8_t)random(), 0x0001, regX);
                    RUN_TESTp(func, lobyte, (uint8_t)random(), 0x0002, regX);
                    RUN_TESTp(func, lobyte, (uint8_t)random(), 0x0003, regX);
                } while (++regX);
            } while (++lobyte);

            fprintf(GREATEST_STDOUT, "...OK\n");
        }

    } while (0);

    greatest_info.flags = 0x0;

    // ------------------------------------------------------------------------
    // Accumulator addressing & PLx tests :
    // NOTE : these should be a comprehensive exercise of the instruction logic

    greatest_info.flags = GREATEST_FLAG_SILENT_SUCCESS;

    do {
        test_func_t funcs[] = {
            { "test_ASL_acc",   (void(*)(void))test_ASL_acc, },
            { "test_DEA",       (void(*)(void))test_DEA, },
            { "test_DEX",       (void(*)(void))test_DEX, },
            { "test_DEY",       (void(*)(void))test_DEY, },
            { "test_INA",       (void(*)(void))test_INA, },
            { "test_INX",       (void(*)(void))test_INX, },
            { "test_INY",       (void(*)(void))test_INY, },
            { "test_LSR_acc",   (void(*)(void))test_LSR_acc, },
            { "test_PHP",       (void(*)(void))test_PHP, },
            { "test_PLA",       (void(*)(void))test_PLA, },
            { "test_PLP",       (void(*)(void))test_PLP, },
            { "test_PLX",       (void(*)(void))test_PLX, },
            { "test_PLY",       (void(*)(void))test_PLY, },
            { "test_ROL_acc",   (void(*)(void))test_ROL_acc, },
            { "test_ROR_acc",   (void(*)(void))test_ROR_acc, },
            { "test_RTI",       (void(*)(void))test_RTI, },
            { "test_TAX",       (void(*)(void))test_TAX, },
            { "test_TAY",       (void(*)(void))test_TAY, },
            { "test_TSX",       (void(*)(void))test_TSX, },
            { "test_TXS",       (void(*)(void))test_TXS, },
            { "test_TXA",       (void(*)(void))test_TXA, },
            { "test_TYA",       (void(*)(void))test_TYA, },
        };
        const unsigned int count = sizeof(funcs) / sizeof(test_func_t);

        for (unsigned int i=0; i<count; i++) {
            char *name = funcs[i].name;
            int (*func)(uint8_t, bool) = (int (*)(uint8_t, bool))funcs[i].func;

            fprintf(GREATEST_STDOUT, "\n%s (SILENCED OUTPUT) :\n", name);

            // test comprehensive logic in immediate mode (since no addressing to test) ...
            uint8_t regA=0x00;
            do {
                RUN_TESTp(func, regA, true);
                RUN_TESTp(func, regA, false);
            } while (++regA);

            fprintf(GREATEST_STDOUT, "...OK\n");
        }
    } while (0);

    greatest_info.flags = 0x0;

    // ------------------------------------------------------------------------
    // Other addressing modes tests :
    // NOTE : unlike immediate-mode addressing tests above, these tests are not designed to be a comprehensive test of
    // instruction logic.  Rather--for clarity--they are designed to comprehensively test the addressing logic,
    // including all edge cases

    // --------------------------------
#ifdef ANDROID
    greatest_info.flags = GREATEST_FLAG_SILENT_SUCCESS;
#endif

    do {
        test_func_t funcs[] = {
            { "test_ADC_zpage", (void(*)(void))test_ADC_zpage, },
            { "test_AND_zpage", (void(*)(void))test_AND_zpage, },
            { "test_ASL_zpage", (void(*)(void))test_ASL_zpage, },
            { "test_BIT_zpage", (void(*)(void))test_BIT_zpage, },
            { "test_CMP_zpage", (void(*)(void))test_CMP_zpage, },
            { "test_CPX_zpage", (void(*)(void))test_CPX_zpage, },
            { "test_CPY_zpage", (void(*)(void))test_CPY_zpage, },
            { "test_DEC_zpage", (void(*)(void))test_DEC_zpage, },
            { "test_EOR_zpage", (void(*)(void))test_EOR_zpage, },
            { "test_INC_zpage", (void(*)(void))test_INC_zpage, },
            { "test_LDA_zpage", (void(*)(void))test_LDA_zpage, },
            { "test_LDX_zpage", (void(*)(void))test_LDX_zpage, },
            { "test_LDY_zpage", (void(*)(void))test_LDY_zpage, },
            { "test_LSR_zpage", (void(*)(void))test_LSR_zpage, },
            { "test_ORA_zpage", (void(*)(void))test_ORA_zpage, },
            { "test_ROL_zpage", (void(*)(void))test_ROL_zpage, },
            { "test_ROR_zpage", (void(*)(void))test_ROR_zpage, },
            { "test_SBC_zpage", (void(*)(void))test_SBC_zpage, },
            { "test_STA_zpage", (void(*)(void))test_STA_zpage, },
            { "test_STX_zpage", (void(*)(void))test_STX_zpage, },
            { "test_STY_zpage", (void(*)(void))test_STY_zpage, },
            { "test_STZ_zpage", (void(*)(void))test_STZ_zpage, },
            { "test_TRB_zpage", (void(*)(void))test_TRB_zpage, },
            { "test_TSB_zpage", (void(*)(void))test_TSB_zpage, },
        };
        const unsigned int count = sizeof(funcs) / sizeof(test_func_t);

        for (unsigned int i=0; i<count; i++) {
            char *name = funcs[i].name;
            int (*func)(uint8_t, uint8_t, uint8_t, bool) = (int (*)(uint8_t, uint8_t, uint8_t, bool))funcs[i].func;

            fprintf(GREATEST_STDOUT, "\n%s :\n", name);

            // test addressing is working ...
            uint8_t arg0 = 0x00;
            do {
                RUN_TESTp(func, /*A*/0x0f, /*val*/0x0f, arg0, /*carry*/true);
                RUN_TESTp(func, /*A*/0x0f, /*val*/0x0f, arg0, /*carry*/false);
                RUN_TESTp(func, /*A*/0x7f, /*val*/0x7f, arg0, /*carry*/true);
                RUN_TESTp(func, /*A*/0x7f, /*val*/0x7f, arg0, /*carry*/false);
                RUN_TESTp(func, /*A*/0xaa, /*val*/0x55, arg0, /*carry*/true);
                RUN_TESTp(func, /*A*/0xaa, /*val*/0x55, arg0, /*carry*/false);
                RUN_TESTp(func, /*A*/0x00, /*val*/0xff, arg0, /*carry*/true);
                RUN_TESTp(func, /*A*/0x00, /*val*/0xff, arg0, /*carry*/false);
                ++arg0;
            } while (arg0);

#ifdef ANDROID
            fprintf(GREATEST_STDOUT, "...OK\n");
#endif
        }
    } while (0);

    // --------------------------------
    greatest_info.flags = GREATEST_FLAG_SILENT_SUCCESS;

    do {
        test_func_t funcs[] = {
            { "test_ADC_zpage_x", (void(*)(void))test_ADC_zpage_x, },
            { "test_AND_zpage_x", (void(*)(void))test_AND_zpage_x, },
            { "test_ASL_zpage_x", (void(*)(void))test_ASL_zpage_x, },
            { "test_BIT_zpage_x", (void(*)(void))test_BIT_zpage_x, },
            { "test_CMP_zpage_x", (void(*)(void))test_CMP_zpage_x, },
            { "test_DEC_zpage_x", (void(*)(void))test_DEC_zpage_x, },
            { "test_EOR_zpage_x", (void(*)(void))test_EOR_zpage_x, },
            { "test_INC_zpage_x", (void(*)(void))test_INC_zpage_x, },
            { "test_LDA_zpage_x", (void(*)(void))test_LDA_zpage_x, },
            { "test_LDX_zpage_y", (void(*)(void))test_LDX_zpage_y, },
            { "test_LDY_zpage_x", (void(*)(void))test_LDY_zpage_x, },
            { "test_LSR_zpage_x", (void(*)(void))test_LSR_zpage_x, },
            { "test_ORA_zpage_x", (void(*)(void))test_ORA_zpage_x, },
            { "test_SBC_zpage_x", (void(*)(void))test_SBC_zpage_x, },
            { "test_ROL_zpage_x", (void(*)(void))test_ROL_zpage_x, },
            { "test_ROR_zpage_x", (void(*)(void))test_ROR_zpage_x, },
            { "test_STA_zpage_x", (void(*)(void))test_STA_zpage_x, },
            { "test_STX_zpage_y", (void(*)(void))test_STX_zpage_y, },
            { "test_STY_zpage_x", (void(*)(void))test_STY_zpage_x, },
            { "test_STZ_zpage_x", (void(*)(void))test_STZ_zpage_x, },
        };
        const unsigned int count = sizeof(funcs) / sizeof(test_func_t);

        for (unsigned int i=0; i<count; i++) {
            char *name = funcs[i].name;
            int (*func)(uint8_t, uint8_t, uint8_t, uint8_t, bool) = (int (*)(uint8_t, uint8_t, uint8_t, uint8_t, bool))funcs[i].func;

            fprintf(GREATEST_STDOUT, "\n%s :\n", name);

            // test addressing is working ...
            uint8_t regX = 0x0;
            do {
                uint8_t arg0 = 0x0;
                do {
                    RUN_TESTp(func, /*A*/0x0f, /*val*/0x0f, arg0, regX, /*carry*/true);
                    RUN_TESTp(func, /*A*/0x0f, /*val*/0x0f, arg0, regX, /*carry*/false);
                    RUN_TESTp(func, /*A*/0x7f, /*val*/0x7f, arg0, regX, /*carry*/true);
                    RUN_TESTp(func, /*A*/0x7f, /*val*/0x7f, arg0, regX, /*carry*/false);
                    RUN_TESTp(func, /*A*/0xaa, /*val*/0x55, arg0, regX, /*carry*/true);
                    RUN_TESTp(func, /*A*/0xaa, /*val*/0x55, arg0, regX, /*carry*/false);
                    RUN_TESTp(func, /*A*/0x00, /*val*/0xff, arg0, regX, /*carry*/true);
                    RUN_TESTp(func, /*A*/0x00, /*val*/0xff, arg0, regX, /*carry*/false);
                } while (++arg0);
            } while (++regX);

            fprintf(GREATEST_STDOUT, "...OK\n");
        }
    } while (0);

    greatest_info.flags = 0x0;

    // --------------------------------
#ifdef ANDROID
    greatest_info.flags = GREATEST_FLAG_SILENT_SUCCESS;
#endif

    do {
        test_func_t funcs[] = {
            { "test_ADC_abs", (void(*)(void))test_ADC_abs, },
            { "test_AND_abs", (void(*)(void))test_AND_abs, },
            { "test_ASL_abs", (void(*)(void))test_ASL_abs, },
            { "test_BIT_abs", (void(*)(void))test_BIT_abs, },
            { "test_CMP_abs", (void(*)(void))test_CMP_abs, },
            { "test_CPX_abs", (void(*)(void))test_CPX_abs, },
            { "test_CPY_abs", (void(*)(void))test_CPY_abs, },
            { "test_DEC_abs", (void(*)(void))test_DEC_abs, },
            { "test_EOR_abs", (void(*)(void))test_EOR_abs, },
            { "test_INC_abs", (void(*)(void))test_INC_abs, },
            { "test_LDA_abs", (void(*)(void))test_LDA_abs, },
            { "test_LDX_abs", (void(*)(void))test_LDX_abs, },
            { "test_LDY_abs", (void(*)(void))test_LDY_abs, },
            { "test_LSR_abs", (void(*)(void))test_LSR_abs, },
            { "test_ORA_abs", (void(*)(void))test_ORA_abs, },
            { "test_ROL_abs", (void(*)(void))test_ROL_abs, },
            { "test_ROR_abs", (void(*)(void))test_ROR_abs, },
            { "test_SBC_abs", (void(*)(void))test_SBC_abs, },
            { "test_STA_abs", (void(*)(void))test_STA_abs, },
            { "test_STX_abs", (void(*)(void))test_STX_abs, },
            { "test_STY_abs", (void(*)(void))test_STY_abs, },
            { "test_STZ_abs", (void(*)(void))test_STZ_abs, },
            { "test_TRB_abs", (void(*)(void))test_TRB_abs, },
            { "test_TSB_abs", (void(*)(void))test_TSB_abs, },
        };
        const unsigned int count = sizeof(funcs) / sizeof(test_func_t);

        for (unsigned int i=0; i<count; i++) {
            char *name = funcs[i].name;
            int (*func)(uint8_t, uint8_t, uint8_t, uint8_t, bool) = (int (*)(uint8_t, uint8_t, uint8_t, uint8_t, bool))funcs[i].func;

            fprintf(GREATEST_STDOUT, "\n%s :\n", name);

            // test addressing is working ...
            for (uint8_t lobyte=0xfd; lobyte>0xf0; lobyte++) {
                uint8_t hibyte = 0x1f;
                RUN_TESTp(func, /*A*/0x0f, /*val*/0x0f, lobyte, hibyte, /*carry*/false);
                RUN_TESTp(func, /*A*/0x0f, /*val*/0x0f, lobyte, hibyte, /*carry*/true);
                RUN_TESTp(func, /*A*/0x7f, /*val*/0x7f, lobyte, hibyte, /*carry*/false);
                RUN_TESTp(func, /*A*/0x7f, /*val*/0x7f, lobyte, hibyte, /*carry*/true);
                RUN_TESTp(func, /*A*/0xaa, /*val*/0x55, lobyte, hibyte, /*carry*/false);
                RUN_TESTp(func, /*A*/0xaa, /*val*/0x55, lobyte, hibyte, /*carry*/true);
                RUN_TESTp(func, /*A*/0x00, /*val*/0xff, lobyte, hibyte, /*carry*/false);
                RUN_TESTp(func, /*A*/0x00, /*val*/0xff, lobyte, hibyte, /*carry*/true);
            }

#ifdef ANDROID
            fprintf(GREATEST_STDOUT, "...OK\n");
#endif
        }
    } while (0);

    // --------------------------------

    do {
        test_func_t funcs[] = {
            { "test_ADC_abs_x", (void(*)(void))test_ADC_abs_x, },
            { "test_AND_abs_x", (void(*)(void))test_AND_abs_x, },
            { "test_ASL_abs_x", (void(*)(void))test_ASL_abs_x, },
            { "test_BIT_abs_x", (void(*)(void))test_BIT_abs_x, },
            { "test_CMP_abs_x", (void(*)(void))test_CMP_abs_x, },
            { "test_DEC_abs_x", (void(*)(void))test_DEC_abs_x, },
            { "test_EOR_abs_x", (void(*)(void))test_EOR_abs_x, },
            { "test_INC_abs_x", (void(*)(void))test_INC_abs_x, },
            { "test_LDA_abs_x", (void(*)(void))test_LDA_abs_x, },
            { "test_LDY_abs_x", (void(*)(void))test_LDY_abs_x, },
            { "test_LSR_abs_x", (void(*)(void))test_LSR_abs_x, },
            { "test_ORA_abs_x", (void(*)(void))test_ORA_abs_x, },
            { "test_ROL_abs_x", (void(*)(void))test_ROL_abs_x, },
            { "test_ROR_abs_x", (void(*)(void))test_ROR_abs_x, },
            { "test_SBC_abs_x", (void(*)(void))test_SBC_abs_x, },
            { "test_STA_abs_x", (void(*)(void))test_STA_abs_x, },
            { "test_STZ_abs_x", (void(*)(void))test_STZ_abs_x, },
        };
        const unsigned int count = sizeof(funcs) / sizeof(test_func_t);

        for (unsigned int i=0; i<count; i++) {
            char *name = funcs[i].name;
            int (*func)(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, bool) = (int (*)(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, bool))funcs[i].func;

            fprintf(GREATEST_STDOUT, "\n%s :\n", name);

            // test that addressing is working ...
            uint8_t hibyte = 0x1f;
            uint8_t lobyte = 0x20;
            for (uint8_t regX=0x50; regX>0x4f; regX+=0x30) {
                RUN_TESTp(func, /*A*/0x0f, /*val*/0x0f, regX, lobyte, hibyte, /*carry*/true);
                RUN_TESTp(func, /*A*/0x0f, /*val*/0x0f, regX, lobyte, hibyte, /*carry*/false);
                RUN_TESTp(func, /*A*/0x7f, /*val*/0x7f, regX, lobyte, hibyte, /*carry*/true);
                RUN_TESTp(func, /*A*/0x7f, /*val*/0x7f, regX, lobyte, hibyte, /*carry*/false);
                RUN_TESTp(func, /*A*/0xaa, /*val*/0x55, regX, lobyte, hibyte, /*carry*/true);
                RUN_TESTp(func, /*A*/0xaa, /*val*/0x55, regX, lobyte, hibyte, /*carry*/false);
                RUN_TESTp(func, /*A*/0x00, /*val*/0xff, regX, lobyte, hibyte, /*carry*/true);
                RUN_TESTp(func, /*A*/0x00, /*val*/0xff, regX, lobyte, hibyte, /*carry*/false);

                RUN_TESTp(func, /*A*/0x24, /*val*/0x42, 0x20, 0xfe,   0xff,   /*carry*/true);  // wrap to zpage
                RUN_TESTp(func, /*A*/0x24, /*val*/0x42, 0x20, 0xfe,   0xff,   /*carry*/false); // wrap to zpage
            }

#ifdef ANDROID
            fprintf(GREATEST_STDOUT, "...OK\n");
#endif
        }
    } while (0);

    // --------------------------------

    do {
        test_func_t funcs[] = {
            { "test_ADC_abs_y", (void(*)(void))test_ADC_abs_y, },
            { "test_AND_abs_y", (void(*)(void))test_AND_abs_y, },
            { "test_CMP_abs_y", (void(*)(void))test_CMP_abs_y, },
            { "test_EOR_abs_y", (void(*)(void))test_EOR_abs_y, },
            { "test_LDA_abs_y", (void(*)(void))test_LDA_abs_y, },
            { "test_LDX_abs_y", (void(*)(void))test_LDX_abs_y, },
            { "test_ORA_abs_y", (void(*)(void))test_ORA_abs_y, },
            { "test_SBC_abs_y", (void(*)(void))test_SBC_abs_y, },
            { "test_STA_abs_y", (void(*)(void))test_STA_abs_y, },
        };
        const unsigned int count = sizeof(funcs) / sizeof(test_func_t);

        for (unsigned int i=0; i<count; i++) {
            char *name = funcs[i].name;
            int (*func)(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t) = (int (*)(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t))funcs[i].func;

            fprintf(GREATEST_STDOUT, "\n%s :\n", name);

            // test addressing is working ...
            uint8_t hibyte = 0x1f;
            uint8_t lobyte = 0x20;
            for (uint8_t regY=0x50; regY>0x4f; regY+=0x30) {
                RUN_TESTp(func, /*A*/0x0f, /*val*/0x0f, regY, lobyte, hibyte);
                RUN_TESTp(func, /*A*/0x7f, /*val*/0x7f, regY, lobyte, hibyte);
                RUN_TESTp(func, /*A*/0xaa, /*val*/0x55, regY, lobyte, hibyte);
                RUN_TESTp(func, /*A*/0x00, /*val*/0xff, regY, lobyte, hibyte);
                RUN_TESTp(func, /*A*/0x24, /*val*/0x42, 0x20, 0xfe,   0xff); // wrap to zpage
            }

#ifdef ANDROID
            fprintf(GREATEST_STDOUT, "...OK\n");
#endif
        }
    } while (0);

    // --------------------------------

    do {
        test_func_t funcs[] = {
            { "test_ADC_ind_x", (void(*)(void))test_ADC_ind_x, },
            { "test_AND_ind_x", (void(*)(void))test_AND_ind_x, },
            { "test_CMP_ind_x", (void(*)(void))test_CMP_ind_x, },
            { "test_EOR_ind_x", (void(*)(void))test_EOR_ind_x, },
            { "test_LDA_ind_x", (void(*)(void))test_LDA_ind_x, },
            { "test_ORA_ind_x", (void(*)(void))test_ORA_ind_x, },
            { "test_SBC_ind_x", (void(*)(void))test_SBC_ind_x, },
            { "test_STA_ind_x", (void(*)(void))test_STA_ind_x, },
        };
        const unsigned int count = sizeof(funcs) / sizeof(test_func_t);

        for (unsigned int i=0; i<count; i++) {
            char *name = funcs[i].name;
            int (*func)(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t) = (int (*)(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t))funcs[i].func;

            fprintf(GREATEST_STDOUT, "\n%s :\n", name);

            // test addressing is working ...
            uint8_t hibyte = 0x1f;
            for (uint8_t lobyte=0xfd; lobyte>0xf0; lobyte++) {
                for (uint8_t regX=0x42; regX>0x3F; regX+=0x40) {
                    RUN_TESTp(func, /*A*/0x0f, /*val*/0x0f, /*arg0*/0x24, regX, lobyte, hibyte);
                    RUN_TESTp(func, /*A*/0x7f, /*val*/0x7f, /*arg0*/0x24, regX, lobyte, hibyte);
                    RUN_TESTp(func, /*A*/0xaa, /*val*/0x55, /*arg0*/0x24, regX, lobyte, hibyte);
                    RUN_TESTp(func, /*A*/0x00, /*val*/0xff, /*arg0*/0x24, regX, lobyte, hibyte);
                }
            }

#ifdef ANDROID
            fprintf(GREATEST_STDOUT, "...OK\n");
#endif
        }
    } while (0);

    // --------------------------------

    do {
        test_func_t funcs[] = {
            { "test_ADC_ind_y", (void(*)(void))test_ADC_ind_y, },
            { "test_AND_ind_y", (void(*)(void))test_AND_ind_y, },
            { "test_CMP_ind_y", (void(*)(void))test_CMP_ind_y, },
            { "test_EOR_ind_y", (void(*)(void))test_EOR_ind_y, },
            { "test_LDA_ind_y", (void(*)(void))test_LDA_ind_y, },
            { "test_ORA_ind_y", (void(*)(void))test_ORA_ind_y, },
            { "test_SBC_ind_y", (void(*)(void))test_SBC_ind_y, },
            { "test_STA_ind_y", (void(*)(void))test_STA_ind_y, },
        };
        const unsigned int count = sizeof(funcs) / sizeof(test_func_t);

        for (unsigned int i=0; i<count; i++) {
            char *name = funcs[i].name;
            int (*func)(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t) = (int (*)(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t))funcs[i].func;

            fprintf(GREATEST_STDOUT, "\n%s :\n", name);

            // test addressing is working ...
            RUN_TESTp(func, /*A*/0x0f, /*val*/0x0f, /*arg0*/0x24, /*regY*/0x10, /*val_zp0*/0x22, /*val_zp1*/0x1f);
            RUN_TESTp(func, /*A*/0x7f, /*val*/0x7f, /*arg0*/0x24, /*regY*/0x80, /*val_zp0*/0x80, /*val_zp1*/0x1f);
            RUN_TESTp(func, /*A*/0xaa, /*val*/0x55, /*arg0*/0x24, /*regY*/0xAA, /*val_zp0*/0xAA, /*val_zp1*/0x1f);
            RUN_TESTp(func, /*A*/0x00, /*val*/0xff, /*arg0*/0x24, /*regY*/0x80, /*val_zp0*/0x90, /*val_zp1*/0xff);

#ifdef ANDROID
            fprintf(GREATEST_STDOUT, "...OK\n");
#endif
        }
    } while (0);

    // --------------------------------

    do {
        test_func_t funcs[] = {
            { "test_ADC_ind_zpage", (void(*)(void))test_ADC_ind_zpage, },
            { "test_AND_ind_zpage", (void(*)(void))test_AND_ind_zpage, },
            { "test_CMP_ind_zpage", (void(*)(void))test_CMP_ind_zpage, },
            { "test_EOR_ind_zpage", (void(*)(void))test_EOR_ind_zpage, },
            { "test_LDA_ind_zpage", (void(*)(void))test_LDA_ind_zpage, },
            { "test_ORA_ind_zpage", (void(*)(void))test_ORA_ind_zpage, },
            { "test_SBC_ind_zpage", (void(*)(void))test_SBC_ind_zpage, },
            { "test_STA_ind_zpage", (void(*)(void))test_STA_ind_zpage, },
        };
        const unsigned int count = sizeof(funcs) / sizeof(test_func_t);

        for (unsigned int i=0; i<count; i++) {
            char *name = funcs[i].name;
            int (*func)(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t) = (int (*)(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t))funcs[i].func;

            fprintf(GREATEST_STDOUT, "\n%s :\n", name);

            // test addressing is working ...
            for (uint8_t lobyte=0xfd; lobyte>0xf0; lobyte++) {
                RUN_TESTp(func, /*A*/0x0f, /*val*/0x0f, /*arg0*/0x00, /*lobyte*/0x33, /*hibyte*/0x1f);
                RUN_TESTp(func, /*A*/0x7f, /*val*/0x7f, /*arg0*/0x7f, /*lobyte*/0x33, /*hibyte*/0x1f);
                RUN_TESTp(func, /*A*/0xaa, /*val*/0x55, /*arg0*/0xAB, /*lobyte*/0x33, /*hibyte*/0x1f);
                RUN_TESTp(func, /*A*/0x00, /*val*/0xff, /*arg0*/0xff, /*lobyte*/0x33, /*hibyte*/0x1f);
            }

#ifdef ANDROID
            fprintf(GREATEST_STDOUT, "...OK\n");
#endif
        }
    } while (0);
}

SUITE(test_suite_cpu);
GREATEST_MAIN_DEFS();

int test_cpu(int argc, char **argv) {
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(test_suite_cpu);
    GREATEST_MAIN_END();
}

