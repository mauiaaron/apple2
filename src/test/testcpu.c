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

#define fC C_Flag_6502  // [C]arry
#define fZ Z_Flag_6502  // [Z]ero
#define fI I_Flag_6502  // [I]nterrupt
#define fD D_Flag_6502  // [D]ecimal
#define fB B_Flag_6502  // [B]reak
#define fX X_Flag_6502  // [X]tra (reserved)...
#define fV V_Flag_6502  // o[V]erflow
#define fN N_Flag_6502  // [N]egative

#define TEST_LOC 0x1f00

#define RW_NONE 0x0
#define RW_READ 0x1
#define RW_WRITE 0x2

#define MSG_FLAGS0 "A:%02X op %02X = %02X : %s (emul2ix = %02X : %s)"

#define HEADER0() \
    uint8_t result = 0x0; \
    uint8_t flags = 0x0; \
    char buf0[MSG_SIZE]; \
    char buf1[MSG_SIZE]; \
    char msgbuf[MSG_SIZE];

#define VERIFY_FLAGS() \
    flags_to_string(flags, buf0); \
    flags_to_string(cpu65_current.f, buf1); \
    snprintf(msgbuf, MSG_SIZE, MSG_FLAGS0, regA, val, result, buf0, cpu65_current.a, buf1); \
    ASSERTm(msgbuf, cpu65_current.f == flags);

static void testcpu_setup(void *arg) {

    //reinitialize();

    cpu65_cycles_to_execute = 1;

    cpu65_current.pc = TEST_LOC;
    cpu65_current.a = 0x0;
    cpu65_current.x = 0x0;
    cpu65_current.y = 0x0;
    cpu65_current.f = 0x0;
    cpu65_current.sp = 0xff;

    cpu65_debug.ea = 0xffff;
    cpu65_debug.d = 0xff;
    cpu65_debug.rw = 0xff;
    cpu65_debug.opcode = 0xff;
    cpu65_debug.opcycles = 0xff;

    // clear ZP & stack memory
    memset(apple_ii_64k, 0x0, 0x200);

    // clear prog memory and absolute addressing test locations
    memset(((void*)apple_ii_64k)+TEST_LOC, 0x0, 0x300);
}

static void testcpu_teardown(void *arg) {
    // ...
}

static void testcpu_breakpoint(void *arg) {
    fprintf(GREATEST_STDOUT, "set breakpoint on testcpu_breakpoint to check for problems...\n");
}

static void testcpu_set_opcode3(uint8_t op, uint8_t val, uint8_t arg1) {
    apple_ii_64k[0][TEST_LOC+0] = op;
    apple_ii_64k[0][TEST_LOC+1] = val;
    apple_ii_64k[0][TEST_LOC+2] = arg1;
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
    bool is_neg = (a < 0);
    bool is_negb = (b < 0);

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

    if ( is_neg && is_negb ) {
        if (a < res) {
            *flags |= fV;
        }
    }
    if ( !is_neg && !is_negb ) {
        if (a > res) {
            *flags |= fV;
        }
    }

    *result = (uint8_t)(res & 0xff);
}

TEST test_ADC_imm(uint8_t regA, uint8_t val, bool decimal, bool carry) {
    HEADER0();

    flags |= decimal ? (fD) : 0x00;
    flags |= carry   ? (fC) : 0x00;

    if (decimal && check_skip_illegal_bcd(regA, val)) {
        // NOTE : FIXME TODO skip undocumented/illegal BCD
        SKIPm("Z");
    }

    logic_ADC(regA, val, &result, &flags);

    testcpu_set_opcode2(0x69, val);

    cpu65_current.a  = regA;
    cpu65_current.x  = 0x03;
    cpu65_current.y  = 0x04;
    cpu65_current.sp = 0x80;
    cpu65_current.f |= decimal ? (fD) : 0x00;
    cpu65_current.f |= carry   ? (fC) : 0x00;

    cpu65_run();

    ASSERT(cpu65_current.pc      == TEST_LOC+2);
    ASSERT(cpu65_current.x       == 0x03);
    ASSERT(cpu65_current.y       == 0x04);
    ASSERT(cpu65_current.sp      == 0x80);

    snprintf(msgbuf, MSG_SIZE, MSG_FLAGS0, regA, val, result, buf0, cpu65_current.a, buf1);
    ASSERTm(msgbuf, cpu65_current.a == result); 
    VERIFY_FLAGS();

    ASSERT(cpu65_debug.ea        == TEST_LOC+1);
    ASSERT(cpu65_debug.d         == 0xff);
    ASSERT(cpu65_debug.rw        == RW_READ);
    ASSERT(cpu65_debug.opcode    == 0x69);
    ASSERT(cpu65_debug.opcycles  == (decimal ? 3 : 2));

    PASS();
}

TEST test_ADC_zpage(uint8_t regA, uint8_t val, uint8_t arg0) {
    HEADER0();

    logic_ADC(regA, val, &result, &flags);

    testcpu_set_opcode2(0x65, arg0);

    apple_ii_64k[0][arg0] = val;

    cpu65_current.a  = regA;
    cpu65_current.x  = 0x03;
    cpu65_current.y  = 0x04;
    cpu65_current.sp = 0x80;

    cpu65_run();

    ASSERT(cpu65_current.pc      == TEST_LOC+2);
    ASSERT(cpu65_current.x       == 0x03);
    ASSERT(cpu65_current.y       == 0x04);
    ASSERT(cpu65_current.sp      == 0x80);

    ASSERT(cpu65_current.a == result); 
    VERIFY_FLAGS();

    ASSERT(cpu65_debug.ea        == arg0);
    ASSERT(cpu65_debug.d         == 0xff);
    ASSERT(cpu65_debug.rw        == RW_READ);
    ASSERT(cpu65_debug.opcode    == 0x65);
    ASSERT(cpu65_debug.opcycles  == (3));

    PASS();
}

TEST test_ADC_zpage_x(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t regX) {
    HEADER0();

    logic_ADC(regA, val, &result, &flags);

    uint8_t idx = arg0 + regX;

    testcpu_set_opcode2(0x75, arg0);

    apple_ii_64k[0][idx] = val;

    cpu65_current.a  = regA;
    cpu65_current.x  = regX;
    cpu65_current.y  = 0x05;
    cpu65_current.sp = 0x81;

    cpu65_run();

    ASSERT(cpu65_current.pc      == TEST_LOC+2);
    ASSERT(cpu65_current.x       == regX);
    ASSERT(cpu65_current.y       == 0x05);
    ASSERT(cpu65_current.sp      == 0x81);

    ASSERT(cpu65_current.a == result); 
    VERIFY_FLAGS();

    ASSERT(cpu65_debug.ea        == idx);
    ASSERT(cpu65_debug.d         == 0xff);
    ASSERT(cpu65_debug.rw        == RW_READ);
    ASSERT(cpu65_debug.opcode    == 0x75);
    ASSERT(cpu65_debug.opcycles  == (4));

    PASS();
}

TEST test_ADC_abs(uint8_t regA, uint8_t val, uint8_t lobyte, uint8_t hibyte) {
    HEADER0();

    logic_ADC(regA, val, &result, &flags);

    testcpu_set_opcode3(0x6d, lobyte, hibyte);

    uint16_t addrs = lobyte | (hibyte<<8);
    apple_ii_64k[0][addrs] = val;

    cpu65_current.a  = regA;
    cpu65_current.x  = 0xf4;
    cpu65_current.y  = 0x05;
    cpu65_current.sp = 0x81;

    cpu65_run();

    ASSERT(cpu65_current.pc      == TEST_LOC+3);
    ASSERT(cpu65_current.x       == 0xf4);
    ASSERT(cpu65_current.y       == 0x05);
    ASSERT(cpu65_current.sp      == 0x81);

    ASSERT(cpu65_current.a == result); 
    VERIFY_FLAGS();

    ASSERT(cpu65_debug.ea        == addrs);
    ASSERT(cpu65_debug.d         == 0xff);
    ASSERT(cpu65_debug.rw        == RW_READ);
    ASSERT(cpu65_debug.opcode    == 0x6d);
    ASSERT(cpu65_debug.opcycles  == (4));

    PASS();
}

TEST test_ADC_abs_x(uint8_t regA, uint8_t val, uint8_t regX, uint8_t lobyte, uint8_t hibyte) {
    HEADER0();

    logic_ADC(regA, val, &result, &flags);

    testcpu_set_opcode3(0x7d, lobyte, hibyte);

    uint8_t cycle_count = 0;
    uint16_t addrs = lobyte | (hibyte<<8);
    addrs = addrs + regX;
    if ((uint8_t)((addrs>>8)&0xff) != (uint8_t)hibyte) {
        ++cycle_count;
    }
    apple_ii_64k[0][addrs] = val;

    cpu65_current.a  = regA;
    cpu65_current.x  = regX;
    cpu65_current.y  = 0x05;
    cpu65_current.sp = 0x81;

    cpu65_run();

    ASSERT(cpu65_current.pc      == TEST_LOC+3);
    ASSERT(cpu65_current.x       == regX);
    ASSERT(cpu65_current.y       == 0x05);
    ASSERT(cpu65_current.sp      == 0x81);

    ASSERT(cpu65_current.a == result); 
    VERIFY_FLAGS();

    ASSERT(cpu65_debug.ea        == addrs);
    ASSERT(cpu65_debug.d         == 0xff);
    ASSERT(cpu65_debug.rw        == RW_READ);
    ASSERT(cpu65_debug.opcode    == 0x7d);

    cycle_count += 4;
    ASSERT(cpu65_debug.opcycles == cycle_count);

    PASS();
}

TEST test_ADC_abs_y(uint8_t regA, uint8_t val, uint8_t regY, uint8_t lobyte, uint8_t hibyte) {
    HEADER0();

    logic_ADC(regA, val, &result, &flags);

    testcpu_set_opcode3(0x79, lobyte, hibyte);

    uint8_t cycle_count = 0;
    uint16_t addrs = lobyte | (hibyte<<8);
    addrs = addrs + regY;
    if ((uint8_t)((addrs>>8)&0xff) != (uint8_t)hibyte) {
        ++cycle_count;
    }
    apple_ii_64k[0][addrs] = val;

    cpu65_current.a  = regA;
    cpu65_current.x  = 0x02;
    cpu65_current.y  = regY;
    cpu65_current.sp = 0x81;

    cpu65_run();

    ASSERT(cpu65_current.pc      == TEST_LOC+3);
    ASSERT(cpu65_current.x       == 0x02);
    ASSERT(cpu65_current.y       == regY);
    ASSERT(cpu65_current.sp      == 0x81);

    ASSERT(cpu65_current.a == result); 
    VERIFY_FLAGS();

    ASSERT(cpu65_debug.ea        == addrs);
    ASSERT(cpu65_debug.d         == 0xff);
    ASSERT(cpu65_debug.rw        == RW_READ);
    ASSERT(cpu65_debug.opcode    == 0x79);

    cycle_count += 4;
    ASSERT(cpu65_debug.opcycles == cycle_count);

    PASS();
}

TEST test_ADC_ind_x(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t regX, uint8_t lobyte, uint8_t hibyte) {
    HEADER0();

    logic_ADC(regA, val, &result, &flags);

    testcpu_set_opcode2(0x61, arg0);

    uint8_t idx_lo = arg0 + regX;
    uint8_t idx_hi = idx_lo+1;
    uint16_t addrs = lobyte | (hibyte<<8);

    apple_ii_64k[0][idx_lo] = lobyte;
    apple_ii_64k[0][idx_hi] = hibyte;
    apple_ii_64k[0][addrs] = val;

    cpu65_current.a  = regA;
    cpu65_current.x  = regX;
    cpu65_current.y  = 0x15;
    cpu65_current.sp = 0x81;

    cpu65_run();

    ASSERT(cpu65_current.pc      == TEST_LOC+2);
    ASSERT(cpu65_current.x       == regX);
    ASSERT(cpu65_current.y       == 0x15);
    ASSERT(cpu65_current.sp      == 0x81);

    ASSERT(cpu65_current.a == result); 
    VERIFY_FLAGS();

    ASSERT(cpu65_debug.ea        == addrs);
    ASSERT(cpu65_debug.d         == 0xff);
    ASSERT(cpu65_debug.rw        == RW_READ);
    ASSERT(cpu65_debug.opcode    == 0x61);

    ASSERT(cpu65_debug.opcycles  == (6));

    PASS();
}

TEST test_ADC_ind_y(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t regY, uint8_t val_zp0, uint8_t val_zp1) {
    HEADER0();

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

    cpu65_current.a  = regA;
    cpu65_current.x  = 0x84;
    cpu65_current.y  = regY;
    cpu65_current.sp = 0x81;

    cpu65_run();

    ASSERT(cpu65_current.pc      == TEST_LOC+2);
    ASSERT(cpu65_current.x       == 0x84);
    ASSERT(cpu65_current.y       == regY);
    ASSERT(cpu65_current.sp      == 0x81);

    ASSERT(cpu65_current.a == result); 
    VERIFY_FLAGS();

    ASSERT(cpu65_debug.ea        == addrs);
    ASSERT(cpu65_debug.d         == 0xff);
    ASSERT(cpu65_debug.rw        == RW_READ);
    ASSERT(cpu65_debug.opcode    == 0x71);
    cycle_count += 5;
    ASSERT(cpu65_debug.opcycles == cycle_count);

    PASS();
}

TEST test_ADC_ind_zpage(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t lobyte, uint8_t hibyte) {
    HEADER0();

    logic_ADC(regA, val, &result, &flags);

    testcpu_set_opcode2(0x72, arg0);

    uint8_t idx0 = arg0;
    uint8_t idx1 = arg0+1;

    apple_ii_64k[0][idx0] = lobyte;
    apple_ii_64k[0][idx1] = hibyte;

    uint16_t addrs = lobyte | (hibyte<<8);
    apple_ii_64k[0][addrs] = val;

    cpu65_current.a  = regA;
    cpu65_current.x  = 0x14;
    cpu65_current.y  = 0x85;
    cpu65_current.sp = 0x81;

    cpu65_run();

    ASSERT(cpu65_current.pc      == TEST_LOC+2);
    ASSERT(cpu65_current.x       == 0x14);
    ASSERT(cpu65_current.y       == 0x85);
    ASSERT(cpu65_current.sp      == 0x81);

    ASSERT(cpu65_current.a == result); 
    VERIFY_FLAGS();

    ASSERT(cpu65_debug.ea        == addrs);
    ASSERT(cpu65_debug.d         == 0xff);
    ASSERT(cpu65_debug.rw        == RW_READ);
    ASSERT(cpu65_debug.opcode    == 0x72);
    ASSERT(cpu65_debug.opcycles  == (5));

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
    HEADER0();

    logic_AND(regA, val, &result, &flags);

    testcpu_set_opcode2(0x29, val);

    cpu65_current.a  = regA;
    cpu65_current.x  = 0x03;
    cpu65_current.y  = 0x04;
    cpu65_current.sp = 0x80;
    cpu65_current.f  = 0x00;

    cpu65_run();

    ASSERT(cpu65_current.pc      == TEST_LOC+2);
    ASSERT(cpu65_current.x       == 0x03);
    ASSERT(cpu65_current.y       == 0x04);
    ASSERT(cpu65_current.sp      == 0x80);

    snprintf(msgbuf, MSG_SIZE, MSG_FLAGS0, regA, val, result, buf0, cpu65_current.a, buf1);
    ASSERTm(msgbuf, cpu65_current.a == result); 
    VERIFY_FLAGS();

    ASSERT(cpu65_debug.ea        == TEST_LOC+1);
    ASSERT(cpu65_debug.d         == 0xff);
    ASSERT(cpu65_debug.rw        == RW_READ);
    ASSERT(cpu65_debug.opcode    == 0x29);
    ASSERT(cpu65_debug.opcycles  == (2));

    PASS();
}

TEST test_AND_zpage(uint8_t regA, uint8_t val, uint8_t arg0) {
    HEADER0();
    logic_AND(regA, val, &result, &flags);

    testcpu_set_opcode2(0x25, arg0);

    apple_ii_64k[0][arg0] = val;

    cpu65_current.a  = regA;
    cpu65_current.x  = 0x03;
    cpu65_current.y  = 0x04;
    cpu65_current.sp = 0x80;
    cpu65_current.f  = 0x00;

    cpu65_run();

    ASSERT(cpu65_current.pc      == TEST_LOC+2);
    ASSERT(cpu65_current.x       == 0x03);
    ASSERT(cpu65_current.y       == 0x04);
    ASSERT(cpu65_current.sp      == 0x80);

    ASSERT(cpu65_current.a == result); 
    VERIFY_FLAGS();

    ASSERT(cpu65_debug.ea        == arg0);
    ASSERT(cpu65_debug.d         == 0xff);
    ASSERT(cpu65_debug.rw        == RW_READ);
    ASSERT(cpu65_debug.opcode    == 0x25);
    ASSERT(cpu65_debug.opcycles  == (3));

    PASS();
}

TEST test_AND_zpage_x(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t regX) {
    HEADER0();
    logic_AND(regA, val, &result, &flags);

    testcpu_set_opcode2(0x35, arg0);

    uint8_t idx = arg0+regX;

    apple_ii_64k[0][idx] = val;

    cpu65_current.a  = regA;
    cpu65_current.x  = regX;
    cpu65_current.y  = 0x05;
    cpu65_current.sp = 0x81;
    cpu65_current.f  = 0x00;

    cpu65_run();

    ASSERT(cpu65_current.pc      == TEST_LOC+2);
    ASSERT(cpu65_current.x       == regX);
    ASSERT(cpu65_current.y       == 0x05);
    ASSERT(cpu65_current.sp      == 0x81);

    ASSERT(cpu65_current.a == result); 
    VERIFY_FLAGS();

    ASSERT(cpu65_debug.ea        == idx);
    ASSERT(cpu65_debug.d         == 0xff);
    ASSERT(cpu65_debug.rw        == RW_READ);
    ASSERT(cpu65_debug.opcode    == 0x35);
    ASSERT(cpu65_debug.opcycles  == (4));

    PASS();
}

TEST test_AND_abs(uint8_t regA, uint8_t val, uint8_t lobyte, uint8_t hibyte) {
    HEADER0();
    logic_AND(regA, val, &result, &flags);

    testcpu_set_opcode3(0x2d, lobyte, hibyte);

    uint16_t addrs = lobyte | (hibyte<<8);
    apple_ii_64k[0][addrs] = val;

    cpu65_current.a  = regA;
    cpu65_current.x  = 0xf4;
    cpu65_current.y  = 0x05;
    cpu65_current.sp = 0x81;
    cpu65_current.f  = 0x00;

    cpu65_run();

    ASSERT(cpu65_current.pc      == TEST_LOC+3);
    ASSERT(cpu65_current.x       == 0xf4);
    ASSERT(cpu65_current.y       == 0x05);
    ASSERT(cpu65_current.sp      == 0x81);

    ASSERT(cpu65_current.a == result); 
    VERIFY_FLAGS();

    ASSERT(cpu65_debug.ea        == addrs);
    ASSERT(cpu65_debug.d         == 0xff);
    ASSERT(cpu65_debug.rw        == RW_READ);
    ASSERT(cpu65_debug.opcode    == 0x2d);
    ASSERT(cpu65_debug.opcycles  == (4));

    PASS();
}

TEST test_AND_abs_x(uint8_t regA, uint8_t val, uint8_t regX, uint8_t lobyte, uint8_t hibyte) {
    HEADER0();
    logic_AND(regA, val, &result, &flags);

    testcpu_set_opcode3(0x3d, lobyte, hibyte);

    uint8_t cycle_count = 4;
    uint16_t addrs = lobyte | (hibyte<<8);
    addrs = addrs + regX;
    if ((uint8_t)((addrs>>8)&0xff) != (uint8_t)hibyte) {
        ++cycle_count;
    }
    apple_ii_64k[0][addrs] = val;

    cpu65_current.a  = regA;
    cpu65_current.x  = regX;
    cpu65_current.y  = 0x05;
    cpu65_current.sp = 0x81;
    cpu65_current.f  = 0x00;

    cpu65_run();

    ASSERT(cpu65_current.pc      == TEST_LOC+3);
    ASSERT(cpu65_current.x       == regX);
    ASSERT(cpu65_current.y       == 0x05);
    ASSERT(cpu65_current.sp      == 0x81);

    ASSERT(cpu65_current.a == result); 
    VERIFY_FLAGS();

    ASSERT(cpu65_debug.ea        == addrs);
    ASSERT(cpu65_debug.d         == 0xff);
    ASSERT(cpu65_debug.rw        == RW_READ);
    ASSERT(cpu65_debug.opcode    == 0x3d);
    ASSERT(cpu65_debug.opcycles  == cycle_count);

    PASS();
}

TEST test_AND_abs_y(uint8_t regA, uint8_t val, uint8_t regY, uint8_t lobyte, uint8_t hibyte) {
    HEADER0();
    logic_AND(regA, val, &result, &flags);

    testcpu_set_opcode3(0x39, lobyte, hibyte);

    uint8_t cycle_count = 4;
    uint16_t addrs = lobyte | (hibyte<<8);
    addrs = addrs + regY;
    if ((uint8_t)((addrs>>8)&0xff) != (uint8_t)hibyte) {
        ++cycle_count;
    }
    apple_ii_64k[0][addrs] = val;

    cpu65_current.a  = regA;
    cpu65_current.x  = 0x02;
    cpu65_current.y  = regY;
    cpu65_current.sp = 0x81;
    cpu65_current.f  = 0x00;

    cpu65_run();

    ASSERT(cpu65_current.pc      == TEST_LOC+3);
    ASSERT(cpu65_current.x       == 0x02);
    ASSERT(cpu65_current.y       == regY);
    ASSERT(cpu65_current.sp      == 0x81);

    ASSERT(cpu65_current.a == result); 
    VERIFY_FLAGS();

    ASSERT(cpu65_debug.ea        == addrs);
    ASSERT(cpu65_debug.d         == 0xff);
    ASSERT(cpu65_debug.rw        == RW_READ);
    ASSERT(cpu65_debug.opcode    == 0x39);
    ASSERT(cpu65_debug.opcycles == cycle_count);

    PASS();
}

TEST test_AND_ind_x(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t regX, uint8_t lobyte, uint8_t hibyte) {
    HEADER0();
    logic_AND(regA, val, &result, &flags);

    testcpu_set_opcode2(0x21, arg0);

    uint8_t idx_lo = arg0 + regX;
    uint8_t idx_hi = idx_lo+1;
    uint16_t addrs = lobyte | (hibyte<<8);

    apple_ii_64k[0][idx_lo] = lobyte;
    apple_ii_64k[0][idx_hi] = hibyte;
    apple_ii_64k[0][addrs] = val;

    cpu65_current.a  = regA;
    cpu65_current.x  = regX;
    cpu65_current.y  = 0x15;
    cpu65_current.sp = 0x81;
    cpu65_current.f  = 0x00;

    cpu65_run();

    ASSERT(cpu65_current.pc      == TEST_LOC+2);
    ASSERT(cpu65_current.x       == regX);
    ASSERT(cpu65_current.y       == 0x15);
    ASSERT(cpu65_current.sp      == 0x81);

    ASSERT(cpu65_current.a == result); 
    VERIFY_FLAGS();

    ASSERT(cpu65_debug.ea        == addrs);
    ASSERT(cpu65_debug.d         == 0xff);
    ASSERT(cpu65_debug.rw        == RW_READ);
    ASSERT(cpu65_debug.opcode    == 0x21);

    ASSERT(cpu65_debug.opcycles  == (6));

    PASS();
}

TEST test_AND_ind_y(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t regY, uint8_t val_zp0, uint8_t val_zp1) {
    HEADER0();
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

    cpu65_current.a  = regA;
    cpu65_current.x  = 0x84;
    cpu65_current.y  = regY;
    cpu65_current.sp = 0x81;
    cpu65_current.f  = 0x00;

    cpu65_run();

    ASSERT(cpu65_current.pc      == TEST_LOC+2);
    ASSERT(cpu65_current.x       == 0x84);
    ASSERT(cpu65_current.y       == regY);
    ASSERT(cpu65_current.sp      == 0x81);

    ASSERT(cpu65_current.a == result); 
    VERIFY_FLAGS();

    ASSERT(cpu65_debug.ea        == addrs);
    ASSERT(cpu65_debug.d         == 0xff);
    ASSERT(cpu65_debug.rw        == RW_READ);
    ASSERT(cpu65_debug.opcode    == 0x31);
    ASSERT(cpu65_debug.opcycles  == cycle_count);

    PASS();
}

TEST test_AND_ind_zpage(uint8_t regA, uint8_t val, uint8_t arg0, uint8_t lobyte, uint8_t hibyte) {
    HEADER0();
    logic_AND(regA, val, &result, &flags);

    testcpu_set_opcode2(0x32, arg0);

    uint8_t idx0 = arg0;
    uint8_t idx1 = arg0+1;

    apple_ii_64k[0][idx0] = lobyte;
    apple_ii_64k[0][idx1] = hibyte;

    uint16_t addrs = lobyte | (hibyte<<8);
    apple_ii_64k[0][addrs] = val;

    cpu65_current.a  = regA;
    cpu65_current.x  = 0x14;
    cpu65_current.y  = 0x85;
    cpu65_current.sp = 0x81;
    cpu65_current.f  = 0x00;

    cpu65_run();

    ASSERT(cpu65_current.pc      == TEST_LOC+2);
    ASSERT(cpu65_current.x       == 0x14);
    ASSERT(cpu65_current.y       == 0x85);
    ASSERT(cpu65_current.sp      == 0x81);

    ASSERT(cpu65_current.a == result); 
    VERIFY_FLAGS();

    ASSERT(cpu65_debug.ea        == addrs);
    ASSERT(cpu65_debug.d         == 0xff);
    ASSERT(cpu65_debug.rw        == RW_READ);
    ASSERT(cpu65_debug.opcode    == 0x32);
    ASSERT(cpu65_debug.opcycles  == (5));

    PASS();
}

// ----------------------------------------------------------------------------
// BRK operand (and IRQ handling)

TEST test_BRK() {
    testcpu_set_opcode1(0x00);

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
    ASSERT(cpu65_current.f      == (fB|fX|fI));
    ASSERT(cpu65_current.sp     == 0xfc);

    ASSERT(apple_ii_64k[0][0x1ff] == 0x1f);
    ASSERT(apple_ii_64k[0][0x1fe] == 0x02);
    ASSERT(apple_ii_64k[0][0x1fd] == cpu65_flags_encode[B_Flag|X_Flag]);

    ASSERT(cpu65_debug.ea       == 0xfffe);
    ASSERT(cpu65_debug.d        == 0xff);
    ASSERT(cpu65_debug.rw       == RW_NONE);
    ASSERT(cpu65_debug.opcode   == 0x0);
    ASSERT(cpu65_debug.opcycles == (7));

    PASS();
}

TEST test_IRQ() {
    // NOTE : not an opcode
    SKIPm("unimplemented for now");
}

// ----------------------------------------------------------------------------
// NOP operand

TEST test_NOP() {
    testcpu_set_opcode1(0xea);

    cpu65_current.a  = 0x02;
    cpu65_current.x  = 0x03;
    cpu65_current.y  = 0x04;
    cpu65_current.sp = 0x80;
    cpu65_current.f  = 0x05;

    cpu65_run();

    ASSERT(cpu65_current.pc     == TEST_LOC+1);
    ASSERT(cpu65_current.a      == 0x02);
    ASSERT(cpu65_current.x      == 0x03);
    ASSERT(cpu65_current.y      == 0x04);
    ASSERT(cpu65_current.f      == 0x05);
    ASSERT(cpu65_current.sp     == 0x80);

    ASSERT(cpu65_debug.ea       == TEST_LOC);
    ASSERT(cpu65_debug.d        == 0xff);
    ASSERT(cpu65_debug.rw       == RW_NONE);
    ASSERT(cpu65_debug.opcode   == 0xea);
    ASSERT(cpu65_debug.opcycles == (2));

    PASS();
}

// ----------------------------------------------------------------------------
// Test Suite

static unsigned int testcounter = 0;

typedef int(*test_func_ptr0)(void);
typedef int(*test_func_ptr)(uint8_t x, ...);

typedef struct test_func_t {
    unsigned int id;
    char *name;
    void *func;
    UT_hash_handle hh;
} test_func_t;
static test_func_t *test_funcs = NULL;

#define A2_ADD_TEST(TEST) \
    do { \
        test_func_t *test_func = malloc(sizeof(test_func_t)); \
        test_func->id = testcounter; \
        test_func->name = strdup(#TEST); \
        test_func->func = TEST; \
        HASH_ADD_INT(test_funcs, id, test_func); \
        ++testcounter; \
    } while(0);

#define A2_REMOVE_TEST(TEST) \
    do { \
        HASH_DEL(test_funcs, TEST); \
        free(TEST->name); \
        free(TEST); \
    } while(0);

#define A2_RUN_TESTp(TEST, ...) RUN_TESTp( ((test_func_ptr)(TEST)), __VA_ARGS__)

GREATEST_SUITE(test_suite_cpu) {

    GREATEST_SET_SETUP_CB(testcpu_setup, NULL);
    GREATEST_SET_TEARDOWN_CB(testcpu_teardown, NULL);
    GREATEST_SET_BREAKPOINT_CB(testcpu_breakpoint, NULL);

    load_settings();
    sound_volume = 0;
    do_logging = false;// silence regular emulator logging

    c_initialize_firsttime();

    test_func_t *func=NULL, *tmp=NULL;

    // --------------------------------
    A2_ADD_TEST(test_BRK);
    A2_ADD_TEST(test_IRQ);
    A2_ADD_TEST(test_NOP);
    HASH_ITER(hh, test_funcs, func, tmp) {
        fprintf(GREATEST_STDOUT, "\n%s :\n", func->name);
        RUN_TEST(((test_func_ptr0)(func->func)));
        A2_REMOVE_TEST(func);
    }

    // ------------------------------------------------------------------------
    // Immediate addressing mode tests :
    // NOTE : these should be a comprehensive exercise of the instruction logic

    greatest_info.flags = GREATEST_FLAG_SILENT_SUCCESS;
    A2_ADD_TEST(test_ADC_imm);
    A2_ADD_TEST(test_AND_imm);
    HASH_ITER(hh, test_funcs, func, tmp) {
        fprintf(GREATEST_STDOUT, "\n%s (SILENCED OUTPUT) :\n", func->name);

        // test comprehensive logic in immediate mode (since no addressing to test) ...
        uint8_t regA=0x00;
        do {
            uint8_t val=0x00;
            do {
                A2_RUN_TESTp( func->func, regA, val, /*decimal*/false, /*carry*/false);
                A2_RUN_TESTp( func->func, regA, val, /*decimal*/ true, /*carry*/false);
                A2_RUN_TESTp( func->func, regA, val, /*decimal*/false, /*carry*/true);
                A2_RUN_TESTp( func->func, regA, val, /*decimal*/ true, /*carry*/true);
            } while (++val);
        } while (++regA);

        fprintf(GREATEST_STDOUT, "...OK\n");
        A2_REMOVE_TEST(func);
    }
    greatest_info.flags = 0x0;

    // ------------------------------------------------------------------------
    // Other addressing modes tests :
    // NOTE : unlike immediate-mode addressing tests above, these tests are not designed to be a comprehensive test of
    // instruction logic.  Rather--for clarity--they are designed to comprehensively test the addressing logic,
    // including all edge cases 

    // --------------------------------
    // zpage TODO FIXME DESCRIPTION HERE .....................................
    A2_ADD_TEST(test_ADC_zpage);
    A2_ADD_TEST(test_AND_zpage);
    HASH_ITER(hh, test_funcs, func, tmp) {
        fprintf(GREATEST_STDOUT, "\n%s :\n", func->name);

        // test addressing is working ...
        uint8_t arg0 = 0x00;
        do {
            A2_RUN_TESTp( func->func, /*A*/0x0f, /*val*/0x0f, arg0);
            A2_RUN_TESTp( func->func, /*A*/0x7f, /*val*/0x7f, arg0);
            A2_RUN_TESTp( func->func, /*A*/0xaa, /*val*/0x55, arg0);
            A2_RUN_TESTp( func->func, /*A*/0x00, /*val*/0xff, arg0);
            ++arg0;
        } while (arg0);

        A2_REMOVE_TEST(func);
    }

    // --------------------------------
    A2_ADD_TEST(test_ADC_zpage_x);
    A2_ADD_TEST(test_AND_zpage_x);
    HASH_ITER(hh, test_funcs, func, tmp) {
        fprintf(GREATEST_STDOUT, "\n%s :\n", func->name);

        // test addressing is working ...
        for (uint8_t regX=0x42; regX>0x3F; regX+=0x40) {
            A2_RUN_TESTp( func->func, /*A*/0x0f, /*val*/0x0f, /*arg0*/0x24, regX);
            A2_RUN_TESTp( func->func, /*A*/0x7f, /*val*/0x7f, /*arg0*/0x24, regX);
            A2_RUN_TESTp( func->func, /*A*/0xaa, /*val*/0x55, /*arg0*/0x24, regX);
            A2_RUN_TESTp( func->func, /*A*/0x00, /*val*/0xff, /*arg0*/0x24, regX);
        }

        A2_REMOVE_TEST(func);
    }

    // --------------------------------
    A2_ADD_TEST(test_ADC_abs);
    A2_ADD_TEST(test_AND_abs);
    HASH_ITER(hh, test_funcs, func, tmp) {
        fprintf(GREATEST_STDOUT, "\n%s :\n", func->name);

        // test addressing is working ...
        for (uint8_t lobyte=0xfd; lobyte>0xf0; lobyte++) {
            uint8_t hibyte = 0x1f;
            A2_RUN_TESTp( func->func, /*A*/0x0f, /*val*/0x0f, lobyte, hibyte);
            A2_RUN_TESTp( func->func, /*A*/0x7f, /*val*/0x7f, lobyte, hibyte);
            A2_RUN_TESTp( func->func, /*A*/0xaa, /*val*/0x55, lobyte, hibyte);
            A2_RUN_TESTp( func->func, /*A*/0x00, /*val*/0xff, lobyte, hibyte);
        }

        A2_REMOVE_TEST(func);
    }

    // --------------------------------
    A2_ADD_TEST(test_ADC_abs_x);
    A2_ADD_TEST(test_AND_abs_x);
    HASH_ITER(hh, test_funcs, func, tmp) {
        fprintf(GREATEST_STDOUT, "\n%s :\n", func->name);

        // test addressing is working ...
        uint8_t hibyte = 0x1f;
        uint8_t lobyte = 0x20;
        for (uint8_t regX=0x50; regX>0x4f; regX+=0x30) {
            A2_RUN_TESTp( func->func, /*A*/0x0f, /*val*/0x0f, regX, lobyte, hibyte);
            A2_RUN_TESTp( func->func, /*A*/0x7f, /*val*/0x7f, regX, lobyte, hibyte);
            A2_RUN_TESTp( func->func, /*A*/0xaa, /*val*/0x55, regX, lobyte, hibyte);
            A2_RUN_TESTp( func->func, /*A*/0x00, /*val*/0xff, regX, lobyte, hibyte);
            A2_RUN_TESTp( func->func, /*A*/0x24, /*val*/0x42, 0x20, 0xfe,   0xff); // wrap to zpage
        }

        A2_REMOVE_TEST(func);
    }

    // --------------------------------
    A2_ADD_TEST(test_ADC_abs_y);
    A2_ADD_TEST(test_AND_abs_y);
    HASH_ITER(hh, test_funcs, func, tmp) {
        fprintf(GREATEST_STDOUT, "\n%s :\n", func->name);

        // test addressing is working ...
        uint8_t hibyte = 0x1f;
        uint8_t lobyte = 0x20;
        for (uint8_t regY=0x50; regY>0x4f; regY+=0x30) {
            A2_RUN_TESTp( func->func, /*A*/0x0f, /*val*/0x0f, regY, lobyte, hibyte);
            A2_RUN_TESTp( func->func, /*A*/0x7f, /*val*/0x7f, regY, lobyte, hibyte);
            A2_RUN_TESTp( func->func, /*A*/0xaa, /*val*/0x55, regY, lobyte, hibyte);
            A2_RUN_TESTp( func->func, /*A*/0x00, /*val*/0xff, regY, lobyte, hibyte);
            A2_RUN_TESTp( func->func, /*A*/0x24, /*val*/0x42, 0x20, 0xfe,   0xff); // wrap to zpage
        }

        A2_REMOVE_TEST(func);
    }

    // --------------------------------
    A2_ADD_TEST(test_ADC_ind_x);
    A2_ADD_TEST(test_AND_ind_x);
    HASH_ITER(hh, test_funcs, func, tmp) {
        fprintf(GREATEST_STDOUT, "\n%s :\n", func->name);

        // test addressing is working ...
        uint8_t hibyte = 0x1f;
        for (uint8_t lobyte=0xfd; lobyte>0xf0; lobyte++) {
            for (uint8_t regX=0x42; regX>0x3F; regX+=0x40) {
                A2_RUN_TESTp( func->func, /*A*/0x0f, /*val*/0x0f, /*arg0*/0x24, regX, lobyte, hibyte);
                A2_RUN_TESTp( func->func, /*A*/0x7f, /*val*/0x7f, /*arg0*/0x24, regX, lobyte, hibyte);
                A2_RUN_TESTp( func->func, /*A*/0xaa, /*val*/0x55, /*arg0*/0x24, regX, lobyte, hibyte);
                A2_RUN_TESTp( func->func, /*A*/0x00, /*val*/0xff, /*arg0*/0x24, regX, lobyte, hibyte);
            }
        }

        A2_REMOVE_TEST(func);
    }

    // --------------------------------
    A2_ADD_TEST(test_ADC_ind_y);
    A2_ADD_TEST(test_AND_ind_y);
    HASH_ITER(hh, test_funcs, func, tmp) {
        fprintf(GREATEST_STDOUT, "\n%s :\n", func->name);

        // test addressing is working ...
        A2_RUN_TESTp( func->func, /*A*/0x0f, /*val*/0x0f, /*arg0*/0x24, /*regY*/0x10, /*val_zp0*/0x22, /*val_zp1*/0x1f);
        A2_RUN_TESTp( func->func, /*A*/0x7f, /*val*/0x7f, /*arg0*/0x24, /*regY*/0x80, /*val_zp0*/0x80, /*val_zp1*/0x1f);
        A2_RUN_TESTp( func->func, /*A*/0xaa, /*val*/0x55, /*arg0*/0x24, /*regY*/0xAA, /*val_zp0*/0xAA, /*val_zp1*/0x1f);
        A2_RUN_TESTp( func->func, /*A*/0x00, /*val*/0xff, /*arg0*/0x24, /*regY*/0x80, /*val_zp0*/0x90, /*val_zp1*/0xff);

        A2_REMOVE_TEST(func);
    }

    // --------------------------------
    A2_ADD_TEST(test_ADC_ind_zpage);
    A2_ADD_TEST(test_AND_ind_zpage);
    HASH_ITER(hh, test_funcs, func, tmp) {
        fprintf(GREATEST_STDOUT, "\n%s :\n", func->name);

        // test addressing is working ...
        for (uint8_t lobyte=0xfd; lobyte>0xf0; lobyte++) {
            uint8_t hibyte = 0x1f;
            A2_RUN_TESTp( func->func, /*A*/0x0f, /*val*/0x0f, /*arg0*/0x00, /*lobyte*/0x33, /*hibyte*/0x1f);
            A2_RUN_TESTp( func->func, /*A*/0x7f, /*val*/0x7f, /*arg0*/0x7f, /*lobyte*/0x33, /*hibyte*/0x1f);
            A2_RUN_TESTp( func->func, /*A*/0xaa, /*val*/0x55, /*arg0*/0xAB, /*lobyte*/0x33, /*hibyte*/0x1f);
            A2_RUN_TESTp( func->func, /*A*/0x00, /*val*/0xff, /*arg0*/0xff, /*lobyte*/0x33, /*hibyte*/0x1f);
        }

        A2_REMOVE_TEST(func);
    }
}

