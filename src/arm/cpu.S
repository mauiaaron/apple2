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

/*
 * 65c02 CPU opcodes emulated with ARM.
 */

#include "cpu-regs.h"
#include "vm.h"


#if __aarch64__
#   define APSR NZCV
#   define APSR_nzcvq NZCV

#   define Zero(x) \
                mov     x, wzr;
#   define ZeroX(x) \
                mov     x, xzr;

#   define Enter \
                stp     x29, x30, [sp, -16]!; \
                mov     x29, sp; \
                stp     x19, x20, [sp, -16]!; \
                stp     x21, x22, [sp, -16]!; \
                stp     x23, x24, [sp, -16]!; \
                stp     x25, x26, [sp, -16]!; \
                stp     x27, x28, [sp, -16]!;

#   define Exit \
                ldp     x27, x28, [sp], 16; \
                ldp     x25, x26, [sp], 16; \
                ldp     x23, x24, [sp], 16; \
                ldp     x21, x22, [sp], 16; \
                ldp     x19, x20, [sp], 16; \
                ldp     x29, x30, [sp], 16; \
                ret;
#else

#   define Zero(x) \
                eor     x, x, x;
#   define ZeroX(x) \
                eor     x, x, x;

#   define Enter \
                push    {r4, r5, r6, r7, r8, r9, r10, r11, lr};

#   define Exit \
                pop     {r4, r5, r6, r7, r8, r9, r10, r11, pc};
#endif

#define DecodeFlags \
                ldr     xr1, [reg_args, #CPU65_FLAGS_DECODE]; \
                ldrb    F_Reg, [xr1, xr0];

#define EncodeFlags \
                ldr     xr1, [reg_args, #CPU65_FLAGS_ENCODE]; \
                ldrb    wr0, [xr1, xF_Reg];

#define CommonSaveCPUState \
                /* save EA */ \
                strh    EffectiveAddr, [reg_args, #CPU65_EA]; \
                /* save stack pointer */ \
                strb    SP_Reg, [reg_args, #CPU65_SP]; \
                /* save X */ \
                strb    X_Reg, [reg_args, #CPU65_X]; \
                /* save Y */ \
                strb    Y_Reg, [reg_args, #CPU65_Y]; \
                /* save A */ \
                strb    A_Reg, [reg_args, #CPU65_A]; \
                /* save flags */ \
                EncodeFlags \
                strb    wr0, [reg_args, #CPU65_F];

// Tracing is necessary for some CPU tests and to verify operation against other emulators
#if CPU_TRACING
#   define TRACE_PROLOGUE \
                strh    PC_Reg, [reg_args, #CPU65_PC]; \
                bl      CALL(cpu65_trace_prologue);
#   define TRACE_ARG \
                bl      CALL(cpu65_trace_arg);
#   define TRACE_EPILOGUE \
                CommonSaveCPUState; \
                bl      CALL(cpu65_trace_epilogue);
#   define TRACE_IRQ \
                bl      CALL(cpu65_trace_irq);
#else
#   define TRACE_PROLOGUE
#   define TRACE_ARG
#   define TRACE_EPILOGUE
#   define TRACE_IRQ
#endif


#define CPUStatsReset \
                Zero(wr1) \
                strb    wr1, [reg_args, #CPU65_OPCYCLES]; \
                strb    wr1, [reg_args, #CPU65_RW];

// ----------------------------------------------------------------------------
// CPU (6502) helper macros

// Add 16bit x with <=16bit amt
#define AddUint16(x, amt) \
                add     x, x, amt; \
                bic     x, x, #0xFF0000;

// Increment 16bit x
#define IncUint16(x) \
                AddUint16(x, #1)

// Increment 8bit x
#define IncUint8(x) \
                add     x, x, #1; \
                and     x, x, #0xFF;

// Decrement 8bit x
#define DecUint8(x) \
                sub     x, x, #1; \
                and     x, x, #0xFF;

#define GetFromPC_B \
                mov     EffectiveAddr, PC_Reg; \
                lsr     wr1, EffectiveAddr, #8; \
                IncUint16(PC_Reg) \
                ldr     xr1, [reg_vmem_r, xr1, LSL PTR_SHIFT]; \
                BLX     xr1; \
                TRACE_ARG

#define word_reg        PC_Reg
#define hi_byte         wr0
#define lo_byte         wr9

#define GetFromPC_W \
                mov     EffectiveAddr, PC_Reg; \
                lsr     wr1, EffectiveAddr, #8; \
                AddUint16(PC_Reg, #2) \
                lsl     PC_Reg, PC_Reg, #16; \
                ldr     xr1, [reg_vmem_r, xr1, LSL PTR_SHIFT]; \
                BLX     xr1; \
                TRACE_ARG \
                orr     word_reg, word_reg, wr0; \
                IncUint16(EffectiveAddr) \
                lsr     wr1, EffectiveAddr, #8; \
                ldr     xr1, [reg_vmem_r, xr1, LSL PTR_SHIFT]; \
                BLX     xr1; \
                TRACE_ARG; \
                lsl     wr0, wr0, #8; /* hi byte */ \
                orr     wr0, word_reg, wr0; \
                lsl     wr0, wr0, #16; \
                lsr     wr0, wr0, #16; \
                lsr     PC_Reg, PC_Reg, #16;

#define JumpNextInstruction \
                TRACE_PROLOGUE \
                GetFromPC_B \
                strb    wr0, [reg_args, #CPU65_OPCODE]; /* wr0 should be next opcode */ \
                ldr     xr1, [reg_args, #CPU65__OPCODES]; \
                ldr     xr1, [xr1, xr0, LSL PTR_SHIFT]; \
                ZeroX(xr0) \
                msr     APSR_nzcvq, xr0; \
                BX      xr1;

#define GetFromEA_B \
                /* Record CPU read: */ \
                lsr     wr1, EffectiveAddr, #8; \
                ldrb    wr9, [reg_args, #CPU65_RW]; \
                orr     wr9, wr9, #1; \
                strb    wr9, [reg_args, #CPU65_RW]; \
                \
                ldr     xr1, [reg_vmem_r, xr1, LSL PTR_SHIFT]; \
                BLX     xr1;

#define GetFromEA_W \
                lsr     wr1, EffectiveAddr, #8; \
                lsl     PC_Reg, PC_Reg, #16; \
                ldr     xr1, [reg_vmem_r, xr1, LSL PTR_SHIFT]; \
                BLX     xr1; \
                orr     word_reg, word_reg, wr0; \
                IncUint16(EffectiveAddr) \
                lsr     wr1, EffectiveAddr, #8; \
                ldr     xr1, [reg_vmem_r, xr1, LSL PTR_SHIFT]; \
                BLX     xr1; \
                lsl     wr0, wr0, #8; /* hi byte */ \
                orr     wr0, word_reg, wr0; \
                lsl     wr0, wr0, #16; \
                lsr     wr0, wr0, #16; \
                lsr     PC_Reg, PC_Reg, #16;

#define PutToEA_B \
                /* Record CPU write: */ \
                lsr     wr1, EffectiveAddr, #8; \
                strb    wr0, [reg_args, #CPU65_D]; \
                ldrb    wr9, [reg_args, #CPU65_RW]; \
                orr     wr9, wr9, #2; \
                strb    wr9, [reg_args, #CPU65_RW]; \
                ldr     xr9, [reg_args, #CPU65_VMEM_W]; \
                ldr     xr1, [xr9, xr1, LSL PTR_SHIFT]; \
                BLX     xr1;

#define GetFromMem_B(x) \
                mov     EffectiveAddr, x; \
                lsr     wr1, EffectiveAddr, #8; \
                ldr     xr1, [reg_vmem_r, xr1, LSL PTR_SHIFT]; \
                BLX     xr1;

#define GetFromMem_W(x) \
                mov     EffectiveAddr, x; \
                GetFromEA_W

#define Continue \
                b       continue;

#define cbw(x) \
                /* Virtual x86: Convert Byte-to-Word */ \
                lsl     x, x, #24; \
                asr     x, x, #24;

#define _AddOpCycles(x) \
                ldrb    scratch_count, [reg_args, #CPU65_OPCYCLES]; \
                add     scratch_count, scratch_count, x;

#define AddOpCycles(x) \
                _AddOpCycles(x) \
                strb    scratch_count, [reg_args, #CPU65_OPCYCLES];

#define FlagNotZero(x) \
                mrs     arm_flags, APSR; \
                lsr     x, arm_flags, #30; \
                and     x, x, #1; /* Z_Flag */ \
                eor     x, x, #1;

#define scratch_count   wr1
#define pc_hi_prev      wr9
#define pc_hi_next      wr0
#define BranchXCycles \
                _AddOpCycles(#1) \
                lsr     pc_hi_prev, PC_Reg, #8; \
                cbw(wr0); \
                add     PC_Reg, PC_Reg, wr0; /* branch PC */ \
                bic     PC_Reg, PC_Reg, #0xFF0000; /* 16bit under/overflow protection */ \
                lsr     pc_hi_next, PC_Reg, #8; \
                eor     wr0, pc_hi_next, pc_hi_prev; \
                \
                cmp     wr0, #0; \
                FlagNotZero(xr12) \
                \
                add     scratch_count, scratch_count, wr12; /* +0/1 branch not/taken */ \
                strb    scratch_count, [reg_args, #CPU65_OPCYCLES];

#define lahf \
                /* Virtual x86: Load %AH (wr12) from ARM CPU Flags */ \
                mrs     arm_flags, APSR; \
                lsr     arm_flags, arm_flags, #28;

#define FlagC \
                lahf; \
                and     arm_flags, arm_flags, #C_Flag; \
                bic     F_Reg, F_Reg, #C_Flag; \
                orr     xF_Reg, xF_Reg, arm_flags;

#define FlagZ \
                lahf; \
                and     arm_flags, arm_flags, #Z_Flag; \
                bic     F_Reg, F_Reg, #Z_Flag; \
                orr     xF_Reg, xF_Reg, arm_flags;

#define FlagZasV \
                lahf; \
                and     arm_flags, arm_flags, #Z_Flag; \
                bic     F_Reg, F_Reg, #V_Flag; \
                lsr     arm_flags, arm_flags, #2; \
                orr     xF_Reg, xF_Reg, arm_flags;

#define FlagN \
                lahf; \
                and     arm_flags, arm_flags, #N_Flag; \
                bic     F_Reg, F_Reg, #N_Flag; \
                orr     xF_Reg, xF_Reg, arm_flags;

#define FlagNZ \
                lahf; \
                and     arm_flags, arm_flags, #NZ_Flags; \
                bic     F_Reg, F_Reg, #NZ_Flags; \
                orr     xF_Reg, xF_Reg, arm_flags;

#define FlagNZC \
                lahf; \
                and     arm_flags, arm_flags, #NZC_Flags; \
                bic     F_Reg, F_Reg, #NZC_Flags; \
                orr     xF_Reg, xF_Reg, arm_flags;

#define FlagNVZ \
                lahf; \
                and     arm_flags, arm_flags, #NVZ_Flags; \
                bic     F_Reg, F_Reg, #NZ_Flags; \
                bic     F_Reg, F_Reg, #V_Flag; \
                orr     xF_Reg, xF_Reg, arm_flags;

#define FlagNVZC \
                lahf; \
                bic     F_Reg, F_Reg, #NVZC_Flags; \
                orr     xF_Reg, xF_Reg, arm_flags;

#define stack_loc xr1
#ifdef APPLE2_VM
#   define RestoreAltZP \
                ldr     stack_loc, [reg_args, #BASE_STACKZP]; \
                add     stack_loc, stack_loc, #0x100; \
                add     stack_loc, stack_loc, xSP_Reg;
#else
#   error FIXME TODO ...
#endif

#define Push(x) \
                RestoreAltZP \
                strb    x, [stack_loc]; \
                DecUint8(SP_Reg)
#warning TODO FIXME ... need to write a 65c02 stack underflow vm test ... also how does AppleWin handle 65c02 underflow?

#define Pop(x)  \
                IncUint8(SP_Reg) \
                RestoreAltZP \
                ldrb    x, [stack_loc];
#warning TODO FIXME ... need to write a 65c02 stack overflow vm test ... also how does AppleWin handle 65c02 overflow?

// ----------------------------------------------------------------------------
// addressing macros

/* Immediate Addressing - the operand is contained in the second byte of the
   instruction. */
#define _GetImm \
                mov     EffectiveAddr, PC_Reg; \
                IncUint16(PC_Reg)

#if CPU_TRACING
#define GetImm \
                _GetImm \
                lsr     wr1, EffectiveAddr, #8; \
                ldr     xr1, [reg_vmem_r, xr1, LSL PTR_SHIFT]; \
                BLX     xr1; \
                TRACE_ARG
#else
#define GetImm \
                _GetImm
#endif

/* Absolute Addressing - the second byte of the instruction is the low
   order address, and the third byte is the high order byte. */
#define GetAbs \
                GetFromPC_W; \
                mov     EffectiveAddr, wr0;

/* Zero Page Addressing - the second byte of the instruction is an
   address on the zero page */
#define GetZPage \
                GetFromPC_B; \
                mov     EffectiveAddr, wr0;

/* Zero Page Indexed Addressing - The effective address is calculated by
   adding the second byte to the contents of the index register.  Due
   to the zero page addressing nature of this mode, no carry is added
   to the high address byte, and the crossing of page boundaries does
   not occur. */
#define GetZPage_X \
                GetFromPC_B; \
                add     wr0, wr0, X_Reg; \
                bic     wr0, wr0, #0x100; \
                mov     EffectiveAddr, wr0;

#define GetZPage_Y \
                GetFromPC_B; \
                add     wr0, wr0, Y_Reg; \
                bic     wr0, wr0, #0x100; \
                mov     EffectiveAddr, wr0;

#define ea_hi_prev wr1
#define AddIndexReg(IndexReg) \
                lsr     ea_hi_prev, wr0, #8; \
                add     wr0, wr0, IndexReg; \
                bic     wr0, wr0, #0x10000;

#define ea_hi_next wr9
#define TestPageBoundary(x) \
                lsr     ea_hi_next, wr0, #8; \
                eor     ea_hi_next, ea_hi_prev, ea_hi_next; \
                cmp     ea_hi_next, #0; \
                FlagNotZero(x);

/* Absolute Indexed Addressing - The effective address is formed by
   adding the contents of X or Y to the address contained in the
   second and third bytes of the instruction. */

#define GetAbs_X \
                GetFromPC_W \
                AddIndexReg(X_Reg) \
                TestPageBoundary(xr12) \
                AddOpCycles(wr12) \
                mov     EffectiveAddr, wr0;

#define GetAbs_X_STx \
                GetFromPC_W \
                AddIndexReg(X_Reg) \
                mov     EffectiveAddr, wr0;

#define GetAbs_Y \
                GetFromPC_W \
                AddIndexReg(Y_Reg) \
                TestPageBoundary(xr12) \
                AddOpCycles(wr12) \
                mov     EffectiveAddr, wr0;

#define GetAbs_Y_STA \
                GetFromPC_W \
                AddIndexReg(Y_Reg) \
                mov     EffectiveAddr, wr0;

/* Zero Page Indirect Addressing (65c02) - The second byte of the
   instruction points to a memory location on page zero containing the
   low order byte of the effective address.  The next location on page
   zero contains the high order byte of the address. */
#define GetIndZPage \
                GetFromPC_B \
                mov     EffectiveAddr, wr0; \
                GetFromEA_B \
                mov     wr12, wr0; \
                add     EffectiveAddr, EffectiveAddr, #1; \
                bic     EffectiveAddr, EffectiveAddr, #0x100; \
                GetFromEA_B \
                lsl     wr0, wr0, #8; \
                orr     wr0, wr12, wr0; \
                mov     EffectiveAddr, wr0;

/* Zero Page Indexed Indirect Addressing - The second byte is added to
   the contents of the X index register; the carry is discarded.  The
   result of this addition points to a memory location on page zero
   whose contents is the low order byte of the effective address.  The
   next memory location in page zero contains the high-order byte of
   the effective address.  Both memory locations specifying the high
   and low-order bytes must be in page zero. */
#define GetIndZPage_X \
                GetFromPC_B \
                mov     EffectiveAddr, wr0; \
                add     EffectiveAddr, EffectiveAddr, X_Reg; \
                bic     EffectiveAddr, EffectiveAddr, #0x100; \
                GetFromEA_B \
                mov     wr12, wr0; \
                add     EffectiveAddr, EffectiveAddr, #1; \
                bic     EffectiveAddr, EffectiveAddr, #0x100; \
                GetFromEA_B \
                lsl     wr0, wr0, #8; \
                orr     wr0, wr12, wr0; \
                mov     EffectiveAddr, wr0;

/* Indirect Indexed Addressing - The second byte of the instruction
   points to a memory location in page zero.  The contents of this
   memory location are added to the contents of the Y index register,
   the result being the low order byte of the effective address.  The
   carry from this addition is added to the contents of the next page
   zero memory location, the result being the high order byte of the
   effective address. */
#define _GetIndZPage_Y \
                GetFromPC_B \
                mov     EffectiveAddr, wr0; \
                GetFromEA_B \
                mov     wr12, wr0; \
                add     EffectiveAddr, EffectiveAddr, #1; \
                bic     EffectiveAddr, EffectiveAddr, #0x100; \
                GetFromEA_B \
                lsl     wr0, wr0, #8; \
                orr     wr0, wr12, wr0; \
                AddIndexReg(Y_Reg)

#define GetIndZPage_Y \
                _GetIndZPage_Y \
                TestPageBoundary(xr12) \
                AddOpCycles(wr12) \
                mov     EffectiveAddr, wr0;

#define GetIndZPage_Y_STA \
                _GetIndZPage_Y \
                mov     EffectiveAddr, wr0;

#ifndef NDEBUG
#   define DebugBCDCheck \
                tst     A_Reg, #0x80; \
                bz      6f; \
                tst     A_Reg, #0x60; \
                bz      6f; \
                bl      CALL(debug_illegal_bcd); \
6:              tst     A_Reg, #0x08; \
                bz      7f; \
                tst     A_Reg, #0x06; \
                bz      7f; \
                bl      CALL(debug_illegal_bcd); \
7:              tst     wr0, #0x80; \
                bz      8f; \
                tst     wr0, #0x60; \
                bz      8f; \
                bl      CALL(debug_illegal_bcd); \
8:              tst     wr0, #0x08; \
                bz      9f; \
                tst     wr0, #0x06; \
                bz      9f; \
                bl      CALL(debug_illegal_bcd); \
9:
#else
#   define DebugBCDCheck
#endif

/* ----------------------------------------------------------------------
       6502 routines and instructions
   ---------------------------------------------------------------------- */

/* ----------------------------------
       ADC instructions
       ADd memory to accumulator with Carry
   ---------------------------------- */

#define carry wr9
#define lo_nyb wr12

// Add with carry Decimal mode
ENTRY(op_ADC_dec)
                AddOpCycles(#1)
                GetFromEA_B
                DebugBCDCheck
                // isolate lo nybbles
                mov     lo_nyb, A_Reg
                mov     wr9, wr0
                and     lo_nyb, lo_nyb, #0x0F
                and     wr9, wr9, #0x0F
                // lo nybble addition with carry
                and     wr1, F_Reg, #C_Flag
                lsr     wr1, wr1, #1
                add     wr9, wr9, wr1
                add     lo_nyb, lo_nyb, wr9
                // prep 65c02 flags
                bic     F_Reg, F_Reg, #NVZC_Flags
                // lo nybble DAA (Decimal Adjustment after Addition), saving carry (+0, +1, +2)
                cmp     lo_nyb, #0x09
                bls     1f
                add     lo_nyb, lo_nyb, #6
1:              lsr     carry, lo_nyb, #4
                and     lo_nyb, lo_nyb, #0x0F
                // isolate hi nybbles
                lsr     A_Reg, A_Reg, #4
                lsr     wr0, wr0, #4
                // hi nybble addition with carry
                add     wr0, wr0, carry
                add     wr0, A_Reg, wr0
                // hi nybble DAA
                cmp     wr0, #0x09
                bls     2f
                add     wr0, wr0, #6
                orr     F_Reg, F_Reg, #C_Flag
                // merge nybbles
2:              lsl     wr0, wr0, #4
                orr     A_Reg, wr0, lo_nyb
                // NZ flags
                and     wr1, A_Reg, #0x80
                lsr     wr1, wr1, #4 /* N_Flag is bit 4 */
                orr     F_Reg, F_Reg, wr1
                ands    wr1, A_Reg, #0xFF
                FlagZ
                Continue

#define sign_a wr12
#define sign_0 wr9
#define sign_res wr1
DoADC_b:        tst     F_Reg, #D_Flag /* Decimal mode? */
                bne     CALL(op_ADC_dec) /* Yes, jump to decimal version */
                GetFromEA_B
                /* save operand sign bits */
                lsr     sign_a, A_Reg, #7
                lsr     sign_0, wr0, #7
                /* add the incoming carry */
                and     wr1, F_Reg, #C_Flag
                lsr     wr1, wr1, #1
                add     A_Reg, A_Reg, wr1
                /* perform main add */
                add     A_Reg, A_Reg, wr0
                /* clear and set flags */
                bic     F_Reg, F_Reg, #NVZC_Flags
                and     wr1, A_Reg, #0x100
                bic     A_Reg, A_Reg, #0x100
                lsr     wr1, wr1, #7 /* C_Flag */
                orr     F_Reg, F_Reg, wr1
                and     wr1, A_Reg, #0x80
                lsr     wr1, wr1, #4 /* N_Flag */
                orr     F_Reg, F_Reg, wr1
                /* check and set oVerflow (V_Flag) */
                eor     sign_0, sign_a, sign_0 /* 1: no V, 0: V if signRes != signA */
                mvn     sign_0, sign_0
                and     sign_0, sign_0, #1 /* 0: no V, 1: mask */
                lsr     sign_res, A_Reg, #7
                eor     sign_a, sign_a, sign_res
                and     sign_a, sign_a, sign_0
                orr     F_Reg, F_Reg, sign_a /* V_Flag */
                /* finally set Z -- this is here to avoid collision with overloaded xr12 */
                ands    wr1, A_Reg, #0xFF
                FlagZ
                Continue

ENTRY(op_ADC_imm) // 0x69
                GetImm
                b       DoADC_b

ENTRY(op_ADC_zpage) // 0x65
                GetZPage
                b       DoADC_b

ENTRY(op_ADC_zpage_x) // 0x75
                GetZPage_X
                b       DoADC_b

// UNIMPLEMENTED : W65C02S datasheet
ENTRY(op_ADC_zpage_y)
                b CALL(op_NOP)

ENTRY(op_ADC_abs) // 0x6d
                GetAbs
                b       DoADC_b

ENTRY(op_ADC_abs_x) // 0x7d
                GetAbs_X
                b       DoADC_b

ENTRY(op_ADC_abs_y) // 0x79
                GetAbs_Y
                b       DoADC_b

ENTRY(op_ADC_ind_x) // 0x61
                GetIndZPage_X
                b       DoADC_b

ENTRY(op_ADC_ind_y) // 0x71
                GetIndZPage_Y
                b       DoADC_b

// 65c02 : 0x72
ENTRY(op_ADC_ind_zpage)
                GetIndZPage
                b       DoADC_b

.ltorg

/* ----------------------------------
       AND instructions
       logical AND memory with accumulator
   ---------------------------------- */

DoAND:
                GetFromEA_B
                lsl     wr0, wr0, #24
                lsl     A_Reg, A_Reg, #24
                ands    A_Reg, A_Reg, wr0
                FlagNZ
                lsr     A_Reg, A_Reg, #24
                Continue

ENTRY(op_AND_imm) // 0x29
                GetImm
                b       DoAND

ENTRY(op_AND_zpage) // 0x25
                GetZPage
                b       DoAND

ENTRY(op_AND_zpage_x) // 0x35
                GetZPage_X
                b       DoAND

// UNIMPLEMENTED : W65C02S datasheet
ENTRY(op_AND_zpage_y)
                b   CALL(op_NOP)

ENTRY(op_AND_abs) // 0x2d
                GetAbs
                b       DoAND

ENTRY(op_AND_abs_x) // 0x3d
                GetAbs_X
                b       DoAND

ENTRY(op_AND_abs_y) // 0x39
                GetAbs_Y
                b       DoAND

ENTRY(op_AND_ind_x) // 0x21
                GetIndZPage_X
                b       DoAND

ENTRY(op_AND_ind_y) // 0x31
                GetIndZPage_Y
                b       DoAND

// 65c02 : 0x32
ENTRY(op_AND_ind_zpage)
                GetIndZPage
                b       DoAND

.ltorg

/* ----------------------------------
       ASL instructions
       Arithmetic Shift one bit Left, memory or accumulator
   ---------------------------------- */

#define _DoASL(x) \
                lsl     x, x, #24; \
                adcs    x, x, x; \
                FlagNZC \
                lsr     x, x, #24;

#define DoASL \
                GetFromEA_B \
                _DoASL(wr0) \
                PutToEA_B

ENTRY(op_ASL_acc) // 0x0a
                _DoASL(A_Reg)
                Continue

ENTRY(op_ASL_zpage) // 0x06
                GetZPage
                DoASL
                Continue

ENTRY(op_ASL_zpage_x) // 0x16
                GetZPage_X
                DoASL
                Continue

ENTRY(op_ASL_abs) // 0x0e
                GetAbs
                DoASL
                Continue

ENTRY(op_ASL_abs_x) // 0x1e
                GetAbs_X
                DoASL
                Continue

.ltorg

/* ----------------------------------
       BBRx instructions
       UNIMPLEMENTED : These are documented in the W65C02S datasheet ...
       + 1 cycle  if branch within page
       + 2 cycles if branch across page boundary
   ---------------------------------- */

ENTRY(op_BBR0_65c02)
                Continue

ENTRY(op_BBR1_65c02)
                Continue

ENTRY(op_BBR2_65c02)
                Continue

ENTRY(op_BBR3_65c02)
                Continue

ENTRY(op_BBR4_65c02)
                Continue

ENTRY(op_BBR5_65c02)
                Continue

ENTRY(op_BBR6_65c02)
                Continue

ENTRY(op_BBR7_65c02)
                Continue

/* ----------------------------------
       BBSx instructions
       UNIMPLEMENTED : These are documented in the W65C02S datasheet ...
       + 1 cycle  if branch within page
       + 2 cycles if branch across page boundary
   ---------------------------------- */

ENTRY(op_BBS0_65c02)
                Continue

ENTRY(op_BBS1_65c02)
                Continue

ENTRY(op_BBS2_65c02)
                Continue

ENTRY(op_BBS3_65c02)
                Continue

ENTRY(op_BBS4_65c02)
                Continue

ENTRY(op_BBS5_65c02)
                Continue

ENTRY(op_BBS6_65c02)
                Continue

ENTRY(op_BBS7_65c02)
                Continue

.ltorg

/* ----------------------------------
       BCC instruction
       Branch on Carry Clear
   ---------------------------------- */

ENTRY(op_BCC) // 0x90
                GetFromPC_B
                tst     F_Reg, #C_Flag
                bne     continue                // branch not taken
                BranchXCycles
                Continue

/* ----------------------------------
       BCS instruction
       Branch on Carry Set
   ---------------------------------- */

ENTRY(op_BCS) // 0xB0
                GetFromPC_B
                tst     F_Reg, #C_Flag
                beq     continue                // branch not taken
                BranchXCycles
                Continue

/* ----------------------------------
       BEQ instruction
       Branch if EQual
   ---------------------------------- */

ENTRY(op_BEQ) // 0xF0
                GetFromPC_B
                tst     F_Reg, #Z_Flag
                beq     continue                // branch not taken
                BranchXCycles
                Continue

.ltorg

/* ----------------------------------
       BIT instructions
       BIt Test
   ---------------------------------- */

#define _DoBIT \
                GetFromEA_B \
                and     wr1, A_Reg, wr0;

DoBIT:
                _DoBIT
                bic     F_Reg, F_Reg, #NZ_Flags
                bic     F_Reg, F_Reg, #V_Flag
                ands    wr1, wr1, #0xFF
                FlagZ
                and     wr1, wr0, #0x40
                lsr     wr1, wr1, #6 /* V_Flag is bit 1 */
                orr     F_Reg, F_Reg, wr1
                and     wr1, wr0, #0x80
                lsr     wr1, wr1, #4 /* N_Flag is bit 4 */
                orr     F_Reg, F_Reg, wr1
                Continue

ENTRY(op_BIT_zpage) // 0x24
                GetZPage
                b       DoBIT

ENTRY(op_BIT_abs) // 0x2c
                GetAbs
                b       DoBIT

// 65c02 : 0x34
ENTRY(op_BIT_zpage_x)
                GetZPage_X
                b       DoBIT

// 65c02 : 0x3C
ENTRY(op_BIT_abs_x)
                GetAbs_X
                b       DoBIT

                /* BIT immediate is anomalous in that it does not affect the
                 * N and V flags, unlike in other addressing modes.
                 */
// 65c02 : 0x89
ENTRY(op_BIT_imm)
                GetImm
                _DoBIT
                ands    wr1, wr1, #0xFF
                FlagZ
                Continue

.ltorg

/* ----------------------------------
       BMI instruction
       Branch on result MInus
   ---------------------------------- */

ENTRY(op_BMI) // 0x30
                GetFromPC_B
                tst     F_Reg, #N_Flag
                beq     continue
                BranchXCycles
                Continue

/* ----------------------------------
       BNE instruction
       Branch on result Not Equal
   ---------------------------------- */

ENTRY(op_BNE) // 0xD0
                GetFromPC_B
                tst     F_Reg, #Z_Flag
                bne     continue
                BranchXCycles
                Continue

/* ----------------------------------
       BPL instruction
       Branch on result PLus
   ---------------------------------- */

ENTRY(op_BPL) // 0x10
                GetFromPC_B
                tst     F_Reg, #N_Flag
                bne     continue
                BranchXCycles
                Continue

/* ----------------------------------
       BRA instruction
       BRanch Always
   ---------------------------------- */

// 65c02 : 0x80
ENTRY(op_BRA)
                GetFromPC_B
                BranchXCycles
                Continue

.ltorg

/* ----------------------------------
       BRK instruction
   ---------------------------------- */

ENTRY(op_UNK) /* make undefined opcodes fault */
ENTRY(op_BRK)
                IncUint16(PC_Reg)
                mov     wr0, PC_Reg
                ror     wr0, wr0, #8
                Push(wr0)
                lsr     wr0, wr0, #24
                Push(wr0)
                orr     F_Reg, F_Reg, #B_Flag
                orr     F_Reg, F_Reg, #X_Flag
                EncodeFlags
                Push(wr0)
                orr     F_Reg, F_Reg, #I_Flag
                ldrh    EffectiveAddr, [reg_args, #INTERRUPT_VECTOR]
                GetFromEA_W
                mov     PC_Reg, wr0
                Continue

.ltorg

/* ----------------------------------
       BVC instruction
       Branch on oVerflow Clear
   ---------------------------------- */

ENTRY(op_BVC) // 0x50
                GetFromPC_B
                tst     F_Reg, #V_Flag
                bne     continue
                BranchXCycles
                Continue

/* ----------------------------------
       BVS instruction
       Branch on oVerflow Set
   ---------------------------------- */

ENTRY(op_BVS) // 0x70
                GetFromPC_B
                tst     F_Reg, #V_Flag
                beq     continue
                BranchXCycles
                Continue

.ltorg

/* ----------------------------------
       CLC instruction
   ---------------------------------- */

ENTRY(op_CLC) // 0x18
                bic     F_Reg, F_Reg, #C_Flag
                Continue

/* ----------------------------------
       CLD instruction
   ---------------------------------- */

ENTRY(op_CLD) // 0xd8
                bic     F_Reg, F_Reg, #D_Flag
                Continue

/* ----------------------------------
       CLI instruction
   ---------------------------------- */

ENTRY(op_CLI) // 0x58
                bic     F_Reg, F_Reg, #I_Flag
                Continue

/* ----------------------------------
       CLV instruction
   ---------------------------------- */

ENTRY(op_CLV) // 0xB8
                bic     F_Reg, F_Reg, #V_Flag
                Continue

.ltorg

/* ----------------------------------
       CMP instructions
       CoMPare memory and accumulator
   ---------------------------------- */

DoCMP:          GetFromEA_B
                lsl     wr0, wr0, #24
                lsl     wr1, A_Reg, #24
                subs    wr1, wr1, wr0
                FlagNZC
                Continue

ENTRY(op_CMP_imm) // 0xc9
                GetImm
                b       DoCMP

ENTRY(op_CMP_zpage) // 0xc5
                GetZPage
                b       DoCMP

ENTRY(op_CMP_zpage_x) // 0xd5
                GetZPage_X
                b       DoCMP

// UNIMPLEMENTED : W65C02S datasheet
ENTRY(op_CMP_zpage_y)
                b CALL(op_NOP)

ENTRY(op_CMP_abs) // 0xcd
                GetAbs
                b       DoCMP

ENTRY(op_CMP_abs_x) // 0xdd
                GetAbs_X
                b       DoCMP

ENTRY(op_CMP_abs_y) // 0xd9
                GetAbs_Y
                b       DoCMP

ENTRY(op_CMP_ind_x) // 0xc1
                GetIndZPage_X
                b       DoCMP

ENTRY(op_CMP_ind_y) // 0xd1
                GetIndZPage_Y
                b       DoCMP

// 65c02 : 0xD2
ENTRY(op_CMP_ind_zpage)
                GetIndZPage
                b       DoCMP

.ltorg

/* ----------------------------------
       CPX instructions
       ComPare memory and X register
   ---------------------------------- */

DoCPX:          GetFromEA_B
                lsl     wr0, wr0, #24
                lsl     wr1, X_Reg, #24
                subs    wr1, wr1, wr0
                FlagNZC
                Continue

ENTRY(op_CPX_imm) // 0xe0
                GetImm
                b       DoCPX

ENTRY(op_CPX_zpage) // 0xe4
                GetZPage
                b       DoCPX

ENTRY(op_CPX_abs) // 0xec
                GetAbs
                b       DoCPX

/* ----------------------------------
       CPY instructions
       ComPare memory and Y register
   ---------------------------------- */

DoCPY:
                GetFromEA_B
                lsl     wr0, wr0, #24
                lsl     wr1, Y_Reg, #24
                subs    wr1, wr1, wr0
                FlagNZC
                Continue

ENTRY(op_CPY_imm) // 0xc0
                GetImm
                b       DoCPY

ENTRY(op_CPY_zpage) // 0xc4
                GetZPage
                b       DoCPY

ENTRY(op_CPY_abs) // 0xcc
                GetAbs
                b       DoCPY

.ltorg

/* ----------------------------------
       DEA: DEcrement Accumulator
   ---------------------------------- */

#define _DoDEC(x) \
                mov     wr1, #1; \
                lsl     wr1, wr1, #24; \
                lsl     x, x, #24; \
                subs    x, x, wr1; \
                FlagNZ \
                lsr     x, x, #24;

#define DoDEC \
                GetFromEA_B \
                _DoDEC(wr0) \
                PutToEA_B

ENTRY(op_DEC_acc)
ENTRY(op_DEA) // 0x3A
                _DoDEC(A_Reg)
                Continue

/* ----------------------------------
       DEC instructions
       DECrement memory or accumulator by one
   ---------------------------------- */

ENTRY(op_DEC_zpage) // 0xc6
                GetZPage
                DoDEC
                Continue

ENTRY(op_DEC_zpage_x) // 0xd6
                GetZPage_X
                DoDEC
                Continue

ENTRY(op_DEC_abs) // 0xce
                GetAbs
                DoDEC
                Continue

ENTRY(op_DEC_abs_x) // 0xde
                GetAbs_X_STx
                DoDEC
                Continue

/* ----------------------------------
       DEX instruction
   ---------------------------------- */

ENTRY(op_DEX) // 0xca
                _DoDEC(X_Reg)
                Continue

/* ----------------------------------
       DEY instruction
   ---------------------------------- */

ENTRY(op_DEY) // 0x88
                _DoDEC(Y_Reg)
                Continue

.ltorg

/* ----------------------------------
       EOR instructions
       Exclusive OR memory with accumulator
   ---------------------------------- */

DoEOR:          GetFromEA_B
                lsl     wr0, wr0, #24
                lsl     A_Reg, A_Reg, #24
                eor     A_Reg, A_Reg, wr0
                cmp     A_Reg, #0
                FlagNZ
                lsr     A_Reg, A_Reg, #24
                Continue

ENTRY(op_EOR_imm) // 0x49
                GetImm
                b       DoEOR

ENTRY(op_EOR_zpage) // 0x45
                GetZPage
                b       DoEOR

ENTRY(op_EOR_zpage_x) // 0x55
                GetZPage_X
                b       DoEOR

// UNIMPLEMENTED : W65C02S datasheet
ENTRY(op_EOR_zpage_y)
                b CALL(op_NOP)

ENTRY(op_EOR_abs) // 0x4d
                GetAbs
                b       DoEOR

ENTRY(op_EOR_abs_x) // 0x5d
                GetAbs_X
                b       DoEOR

ENTRY(op_EOR_abs_y) // 0x59
                GetAbs_Y
                b       DoEOR

ENTRY(op_EOR_ind_x) // 0x41
                GetIndZPage_X
                b       DoEOR

ENTRY(op_EOR_ind_y) // 0x51
                GetIndZPage_Y
                b       DoEOR

// 65c02 : 0x52
ENTRY(op_EOR_ind_zpage)
                GetIndZPage
                b       DoEOR

.ltorg

/* ----------------------------------
       INA : INcrement Accumulator
   ---------------------------------- */

#define _DoINC(x) \
                mov     wr1, #1; \
                lsl    wr1, wr1, #24; \
                lsl     x, x, #24; \
                adds    x, x, wr1; \
                FlagNZ \
                lsr     x, x, #24;

#define DoINC \
                GetFromEA_B \
                _DoINC(wr0) \
                PutToEA_B

ENTRY(op_INC_acc)
ENTRY(op_INA) // 0x1A
                _DoINC(A_Reg)
                Continue

/* ----------------------------------
       INC instructions
       INCrement memory
   ---------------------------------- */

ENTRY(op_INC_zpage) // 0xe6
                GetZPage
                DoINC
                Continue

ENTRY(op_INC_zpage_x) // 0xf6
                GetZPage_X
                DoINC
                Continue

ENTRY(op_INC_abs) // 0xee
                GetAbs
                DoINC
                Continue

ENTRY(op_INC_abs_x) // 0xfe
                GetAbs_X_STx
                DoINC
                Continue

/* ----------------------------------
       INX instruction
   ---------------------------------- */

ENTRY(op_INX) // 0xe8
                _DoINC(X_Reg)
                Continue

/* ----------------------------------
       INY instruction
   ---------------------------------- */

ENTRY(op_INY) // 0xc8
                _DoINC(Y_Reg)
                Continue

.ltorg

/* ----------------------------------
       JMP instructions
       JuMP to new location
   ---------------------------------- */

ENTRY(op_JMP_abs)
                GetAbs
                mov     PC_Reg, EffectiveAddr
                Continue

ENTRY(op_JMP_ind) // 0x6c
                GetFromPC_W
                lsl     wr1, wr0, #24
                eor     wr1, wr1, #0xFF000000
                cmp     wr1, #0
                bz      jmp_special
                GetFromMem_W(wr0)
                mov     PC_Reg, wr0
                sub     EffectiveAddr, EffectiveAddr, #1
                Continue
jmp_special: // see JMP indirect note in _Understanding the Apple IIe_ 4-25
                mov     PC_Reg, wr0
                subs    PC_Reg, PC_Reg, #0xFF
                lsl     PC_Reg, PC_Reg, #16
                lsr     PC_Reg, PC_Reg, #16
                GetFromMem_B(PC_Reg)
                lsl     wr9, wr0, #8
                add     PC_Reg, PC_Reg, #0xFF
                bic     PC_Reg, PC_Reg, #0x10000
                GetFromMem_B(PC_Reg)
                orr     wr0, wr9, wr0
                mov     PC_Reg, wr0
                Continue

// 65c02 : 0x7C
ENTRY(op_JMP_abs_ind_x)
                GetFromPC_W
                mov     EffectiveAddr, wr0
                mov     wr0, X_Reg
#warning HACK FIXME TODO : need to check the Bible here ... is this a signed addition (in which case we need to cbw)
                //cbw(wr0)
                add     EffectiveAddr, EffectiveAddr, wr0
                lsl     EffectiveAddr, EffectiveAddr, #16 // 16bit under/overflow protection
                lsr     EffectiveAddr, EffectiveAddr, #16
                GetFromMem_W(EffectiveAddr)
                mov     PC_Reg, wr0
                Continue

/* ----------------------------------
       JSR instruction
   ---------------------------------- */

ENTRY(op_JSR) // 0x20
                GetAbs
                mov     wr0, PC_Reg
                sub     wr0, wr0, #1
                lsl     wr0, wr0, #16 // handle underflow -- can this happen in second mem page?
                ror     wr0, wr0, #24
                Push(wr0) // push hi byte
                lsr     wr0, wr0, #24
                Push(wr0) // push lo byte
                mov     PC_Reg, EffectiveAddr
                Continue

.ltorg

/* ----------------------------------
       LDA instructions
       LoaD Accumulator with memory
   ---------------------------------- */

DoLDA:          GetFromEA_B
                mov     A_Reg, wr0
                lsl     wr0, wr0, #24
                orr     wr0, wr0, wr0
                cmp     wr0, #0
                FlagNZ
                Continue

ENTRY(op_LDA_imm) // 0xa9
                GetImm
                b       DoLDA

ENTRY(op_LDA_zpage) // 0xa5
                GetZPage
                b       DoLDA

ENTRY(op_LDA_zpage_x) // 0xb5
                GetZPage_X
                b       DoLDA

// UNIMPLEMENTED : W65C02S datasheet
ENTRY(op_LDA_zpage_y)
                b   CALL(op_NOP)

ENTRY(op_LDA_abs) // 0xad
                GetAbs
                b       DoLDA

ENTRY(op_LDA_abs_x) // 0xbd
                GetAbs_X
                b       DoLDA

ENTRY(op_LDA_abs_y) // 0xb9
                GetAbs_Y
                b       DoLDA

ENTRY(op_LDA_ind_x) // 0xa1
                GetIndZPage_X
                b       DoLDA

ENTRY(op_LDA_ind_y) // 0xb1
                GetIndZPage_Y
                b       DoLDA

// 65c02 : 0xB2
ENTRY(op_LDA_ind_zpage)
                GetIndZPage
                b       DoLDA

.ltorg

/* ----------------------------------
       LDX instructions
   ---------------------------------- */

DoLDX:          GetFromEA_B
                mov     X_Reg, wr0
                lsl     wr0, wr0, #24
                orr     wr0, wr0, wr0
                cmp     wr0, #0
                FlagNZ
                Continue

ENTRY(op_LDX_imm) // 0xa2
                GetImm
                b       DoLDX

ENTRY(op_LDX_zpage) // 0xa6
                GetZPage
                b       DoLDX

ENTRY(op_LDX_zpage_y) // 0xb6
                GetZPage_Y
                b       DoLDX

ENTRY(op_LDX_abs) // 0xae
                GetAbs
                b       DoLDX

ENTRY(op_LDX_abs_y) // 0xbe
                GetAbs_Y
                b       DoLDX

/* ----------------------------------
       LDY instructions
   ---------------------------------- */

DoLDY:          GetFromEA_B
                mov     Y_Reg, wr0
                lsl     wr0, wr0, #24
                orr     wr0, wr0, wr0
                cmp     wr0, #0
                FlagNZ
                Continue

ENTRY(op_LDY_imm) // 0xa0
                GetImm
                b       DoLDY

ENTRY(op_LDY_zpage) // 0xa4
                GetZPage
                b       DoLDY

ENTRY(op_LDY_zpage_x) // 0xb4
                GetZPage_X
                b       DoLDY

ENTRY(op_LDY_abs) // 0xac
                GetAbs
                b       DoLDY

ENTRY(op_LDY_abs_x) // 0xbc
                GetAbs_X
                b       DoLDY

.ltorg

/* ----------------------------------
       LSR instructions
   ---------------------------------- */

#if __aarch64__
// ARM64 does not have lsrs ...
#   define _DoLSR(x) \
                and     wr1, x, #1; \
                lsl     wr1, wr1, #1; /* C_Flag */ \
                bic     F_Reg, F_Reg, #C_Flag; \
                orr     F_Reg, F_Reg, wr1; \
                lsr     x, x, #1; \
                cmp     x, #0; \
                FlagNZ
#else
#   define _DoLSR(x) \
                lsrs    x, x, #1; \
                FlagNZC
#endif

#define DoLSR \
                GetFromEA_B \
                _DoLSR(wr0) \
                PutToEA_B

ENTRY(op_LSR_acc) // 0x4a
                _DoLSR(A_Reg)
                Continue

ENTRY(op_LSR_zpage) // 0x46
                GetZPage
                DoLSR
                Continue

ENTRY(op_LSR_zpage_x) // 0x56
                GetZPage_X
                DoLSR
                Continue

ENTRY(op_LSR_abs) // 0x4e
                GetAbs
                DoLSR
                Continue

ENTRY(op_LSR_abs_x) // 0x5e
                GetAbs_X
                DoLSR
                Continue

/* ----------------------------------
       NOP instruction
   ---------------------------------- */

ENTRY(op_NOP) // 0xea
                Continue

.ltorg

/* ----------------------------------
       ORA instructions
   ---------------------------------- */

DoORA:          GetFromEA_B
                lsl     wr0, wr0, #24
                lsl     A_Reg, A_Reg, #24
                orr     A_Reg, A_Reg, wr0
                cmp     A_Reg, #0
                FlagNZ
                lsr     A_Reg, A_Reg, #24
                Continue

ENTRY(op_ORA_imm) // 0x09
                GetImm
                b       DoORA

ENTRY(op_ORA_zpage) // 0x05
                GetZPage
                b       DoORA

ENTRY(op_ORA_zpage_x) // 0x15
                GetZPage_X
                b       DoORA

// UNIMPLEMENTED : W65C02S datasheet
ENTRY(op_ORA_zpage_y)
                b   CALL(op_NOP)

ENTRY(op_ORA_abs) // 0x0d
                GetAbs
                b       DoORA

ENTRY(op_ORA_abs_x) // 0x1d
                GetAbs_X
                b       DoORA

ENTRY(op_ORA_abs_y) // 0x19
                GetAbs_Y
                b       DoORA

ENTRY(op_ORA_ind_x) // 0x01
                GetIndZPage_X
                b       DoORA

ENTRY(op_ORA_ind_y) // 0x11
                GetIndZPage_Y
                b       DoORA

// 65c02 : 0x12
ENTRY(op_ORA_ind_zpage)
                GetIndZPage
                b       DoORA

.ltorg

/* ----------------------------------
       PHA instruction
   ---------------------------------- */

ENTRY(op_PHA) // 0x48
                Push(A_Reg)
                Continue

/* ----------------------------------
       PHP instruction
   ---------------------------------- */

ENTRY(op_PHP) // 0x08
                EncodeFlags
                Push(wr0)
                Continue

/* ----------------------------------
       PHX instruction
       65c02 : 0xDA
   ---------------------------------- */

ENTRY(op_PHX)
                Push(X_Reg)
                Continue

/* ----------------------------------
       PHY instruction
       65c02 : 0x5A
   ---------------------------------- */

ENTRY(op_PHY)
                Push(Y_Reg)
                Continue

/* ----------------------------------
       PLA instruction
   ---------------------------------- */

#define _DoPLx(x) \
                Pop(x); \
                lsl     wr0, x, #24; \
                orr     wr0, wr0, wr0; \
                cmp     wr0, #0; \
                FlagNZ \
                Continue

ENTRY(op_PLA) // 0x68
                _DoPLx(A_Reg)

/* ----------------------------------
       PLP instruction
   ---------------------------------- */

ENTRY(op_PLP) // 0x28
                Pop(wr0)
                DecodeFlags
                orr     F_Reg, F_Reg, #B_Flag
                orr     F_Reg, F_Reg, #X_Flag
                Continue

/* ----------------------------------
       PLX instruction
       65c02 : 0xFA
   ---------------------------------- */

ENTRY(op_PLX)
                _DoPLx(X_Reg)

/* ----------------------------------
       PLY instruction
       65c02 : 0x7A
   ---------------------------------- */

ENTRY(op_PLY)
                _DoPLx(Y_Reg)

.ltorg

/* ----------------------------------
       ROL instructions
   ---------------------------------- */

#if __aarch64__
// ARM64 does not have lsls ...
#   define _FlagROL(x) \
                and     xr1, x, #0x8000; /* out carry -> */ \
                lsr     wr1, wr1, #14; /* C_FLAG */ \
                bic     F_Reg, F_Reg, #C_Flag; \
                orr     F_Reg, F_Reg, wr1; \
                /* Now calc N & Z */ \
                lsl     x, x, #49; \
                cmp     x, #0; \
                FlagNZ \
                lsr     x, x, #56;
#else
#   define _FlagROL(x) \
                lsls    x, x, #17; \
                FlagNZC \
                lsr     x, x, #24;
#endif

#define _DoROL(x) \
                lsl     x, x, #8; \
                lsl     F_Reg, F_Reg, #6; /* C_FLAG (in carry) -> 0x80 */ \
                and     wr1, F_Reg, #0x80; \
                orr     x, x, xr1; \
                lsr     F_Reg, F_Reg, #6; /* undo */ \
                _FlagROL(x)

#define DoROL \
                GetFromEA_B \
                _DoROL(xr0) \
                PutToEA_B

ENTRY(op_ROL_acc) // 0x2a
                _DoROL(xA_Reg)
                Continue

ENTRY(op_ROL_zpage) // 0x26
                GetZPage
                DoROL
                Continue

ENTRY(op_ROL_zpage_x) // 0x36
                GetZPage_X
                DoROL
                Continue

ENTRY(op_ROL_abs) // 0x2e
                GetAbs
                DoROL
                Continue

ENTRY(op_ROL_abs_x) // 0x3e
                GetAbs_X
                DoROL
                Continue

/* ----------------------------------
       ROR instructions
   ---------------------------------- */

#define _DoROR(x) \
                and     wr1, F_Reg, #C_Flag; \
                lsl     wr1, wr1, #7; /* carry in */ \
                orr     x, x, wr1; \
                \
                ror     x, x, #1; \
                bic     F_Reg, F_Reg, #NZC_Flags; \
                \
                and     wr1, x, #ROR_BIT; \
                bic     x, x, #ROR_BIT; \
                lsr     wr1, wr1, #ROR_SHIFT; \
                orr     F_Reg, F_Reg, wr1; /* C_Flag */ \
                \
                ands    wr1, x, #0xFF; \
                FlagZ \
                \
                and     wr1, x, #0x80; \
                lsr     wr1, wr1, #4; /* N_Flag */ \
                orr     F_Reg, F_Reg, wr1;

#define DoROR \
                GetFromEA_B \
                _DoROR(wr0) \
                PutToEA_B

ENTRY(op_ROR_acc) // 0x6a
                _DoROR(A_Reg)
                Continue

ENTRY(op_ROR_zpage) // 0x66
                GetZPage
                DoROR
                Continue

ENTRY(op_ROR_zpage_x) // 0x76
                GetZPage_X
                DoROR
                Continue

ENTRY(op_ROR_abs) // 0x6e
                GetAbs
                DoROR
                Continue

ENTRY(op_ROR_abs_x) // 0x7e
                GetAbs_X
                DoROR
                Continue

.ltorg

/* ----------------------------------
       RTI instruction
   ---------------------------------- */

ENTRY(op_RTI) // 0x40
                Pop(wr0)
                DecodeFlags
                orr     F_Reg, F_Reg, #B_Flag
                orr     F_Reg, F_Reg, #X_Flag
                Pop(lo_byte)
                Pop(hi_byte)
                lsl     hi_byte, hi_byte, #8
                orr     PC_Reg, hi_byte, lo_byte
                Continue

/* ----------------------------------
       RTS instruction
   ---------------------------------- */

ENTRY(op_RTS) // 0x60
                Pop(lo_byte)
                Pop(hi_byte)
                lsl     hi_byte, hi_byte, #8
                orr     PC_Reg, hi_byte, lo_byte
                IncUint16(PC_Reg)
                Continue

.ltorg

/* ----------------------------------
       SBC instructions
       SuBtract memory from accumulator with Borrow
   ---------------------------------- */

#define borrow carry

// Subtract with carry (borrow) Decimal mode
ENTRY(op_SBC_dec)
                AddOpCycles(#1)
                GetFromEA_B
                DebugBCDCheck
                // isolate lo nybbles
                mov     lo_nyb, A_Reg
                mov     wr9, wr0
                and     lo_nyb, lo_nyb, #0x0F
                and     wr9, wr9, #0x0F
                // lo nybble subtraction with borrow
                sub     lo_nyb, lo_nyb, wr9
                and     lo_nyb, lo_nyb, #0xFF
                and     wr1, F_Reg, #C_Flag
                lsr     wr1, wr1, #1
                mvn     wr1, wr1
                and     wr1, wr1, #1
                sub     lo_nyb, lo_nyb, wr1
                and     lo_nyb, lo_nyb, #0xFF
                // prep 65c02 flags
                bic     F_Reg, F_Reg, #NVZC_Flags
                orr     F_Reg, F_Reg, #C_Flag
                // lo nybble DAS (Decimal Adjustment after Subtraction), saving borrow
                Zero(borrow)
                cmp     lo_nyb, #0x09
                bls     1f
                mov     borrow, #1
                sub     lo_nyb, lo_nyb, #6
                and     lo_nyb, lo_nyb, #0x0F
                // isolate hi nybbles
1:              lsr     A_Reg, A_Reg, #4
                lsr     wr0, wr0, #4
                // hi nybble subtraction with borrow
                sub     wr0, A_Reg, wr0
                and     wr0, wr0, #0xFF
                sub     wr0, wr0, borrow
                and     wr0, wr0, #0xFF
                // hi nybble DAS
                cmp     wr0, #0x09
                bls     2f
                sub     wr0, wr0, #6
                bic     F_Reg, F_Reg, #C_Flag
                and     wr0, wr0, #0x0F
                // merge nybbles
2:              lsl     wr0, wr0, #4
                orr     A_Reg, wr0, lo_nyb
                // NZ flags
                and     wr1, A_Reg, #0x80
                lsr     wr1, wr1, #4 /* N_Flag is bit 4 */
                orr     F_Reg, F_Reg, wr1
                ands    wr1, A_Reg, #0xFF
                FlagZ
                Continue

DoSBC_b:        tst     F_Reg, #D_Flag /* Decimal mode? */
                bne     CALL(op_SBC_dec) /* Yes, jump to decimal version */
                GetFromEA_B
                /* save operand sign bits */
                lsr     sign_a, A_Reg, #7
                lsr     sign_0, wr0, #7
                /* perform SBC with incoming borrow */
                sub     A_Reg, A_Reg, wr0
                and     wr1, F_Reg, #C_Flag
                lsr     wr1, wr1, #1
                mvn     wr1, wr1
                and     wr1, wr1, #1
                sub     A_Reg, A_Reg, wr1
                /* clear and set flags */
                bic     F_Reg, F_Reg, #NVZC_Flags
                and     wr1, A_Reg, #0x80000000
                and     A_Reg, A_Reg, #0xFF
                lsr     wr1, wr1, #30
                mvn     wr1, wr1
                and     wr1, wr1, #2
                orr     F_Reg, F_Reg, wr1 /* C_Flag */
                and     wr1, A_Reg, #0x80
                lsr     wr1, wr1, #4
                orr     F_Reg, F_Reg, wr1 /* N_Flag */
                /* check and set oVerflow (V_Flag) */
                eor     sign_0, sign_a, sign_0 /* 0: no V, 1: V if signRes != signA */
                lsr     sign_res, A_Reg, #7
                eor     sign_a, sign_a, sign_res
                and     sign_a, sign_a, sign_0
                orr     F_Reg, F_Reg, sign_a /* V_Flag */
                /* finally set Z -- this is here to avoid collision with overloaded xr12 */
                ands    wr1, A_Reg, #0xFF
                FlagZ
                Continue

ENTRY(op_SBC_imm) // 0xe9
                GetImm
                b       DoSBC_b

ENTRY(op_SBC_zpage) // 0xe5
                GetZPage
                b       DoSBC_b

ENTRY(op_SBC_zpage_x) // 0xf5
                GetZPage_X
                b       DoSBC_b

// UNIMPLEMENTED : W65C02S datasheet
ENTRY(op_SBC_zpage_y)
                b CALL(op_NOP)

ENTRY(op_SBC_abs) // 0xed
                GetAbs
                b       DoSBC_b

ENTRY(op_SBC_abs_x) // 0xfd
                GetAbs_X
                b       DoSBC_b

ENTRY(op_SBC_abs_y) // 0xf9
                GetAbs_Y
                b       DoSBC_b

ENTRY(op_SBC_ind_x) // 0xe1
                GetIndZPage_X
                b       DoSBC_b

ENTRY(op_SBC_ind_y) // 0xf1
                GetIndZPage_Y
                b       DoSBC_b

// 65c02 : 0xF2
ENTRY(op_SBC_ind_zpage)
                GetIndZPage
                b       DoSBC_b

.ltorg

/* ----------------------------------
       SEC instruction
   ---------------------------------- */

ENTRY(op_SEC) // 0x38
                orr     F_Reg, F_Reg, #C_Flag
                Continue

/* ----------------------------------
       SED instruction
   ---------------------------------- */

ENTRY(op_SED) // 0xf8
                orr     F_Reg, F_Reg, #D_Flag
                Continue

/* ----------------------------------
       SEI instruction
   ---------------------------------- */

ENTRY(op_SEI) // 0x78
                orr     F_Reg, F_Reg, #I_Flag
                Continue

/* ----------------------------------
       SMBx instructions -- Available in Rockwell 65C02 but not NCR 65C02
       UNIMPLEMENTED : These are documented in the W65C02S datasheet ...
   ---------------------------------- */

ENTRY(op_SMB0_65c02)
                Continue

ENTRY(op_SMB1_65c02)
                Continue

ENTRY(op_SMB2_65c02)
                Continue

ENTRY(op_SMB3_65c02)
                Continue

ENTRY(op_SMB4_65c02)
                Continue

ENTRY(op_SMB5_65c02)
                Continue

ENTRY(op_SMB6_65c02)
                Continue

ENTRY(op_SMB7_65c02)
                Continue

.ltorg

/* ----------------------------------
       STA instructions
   ---------------------------------- */

DoSTA:          mov     wr0, A_Reg
                PutToEA_B
                Continue

ENTRY(op_STA_zpage) // 0x85
                GetZPage
                b       DoSTA

ENTRY(op_STA_zpage_x) // 0x95
                GetZPage_X
                b       DoSTA

// UNIMPLEMENTED : W65C02S datasheet
ENTRY(op_STA_zpage_y)
                b   CALL(op_NOP)

ENTRY(op_STA_abs) // 0x8d
                GetAbs
                b       DoSTA

ENTRY(op_STA_abs_x) // 0x9d
                GetAbs_X_STx
                b       DoSTA

ENTRY(op_STA_abs_y) // 0x99
                GetAbs_Y_STA
                b       DoSTA

ENTRY(op_STA_ind_x) // 0x81
                GetIndZPage_X
                b       DoSTA

ENTRY(op_STA_ind_y) // 0x91
                GetIndZPage_Y_STA
                b       DoSTA

// 65c02 : 0x92
ENTRY(op_STA_ind_zpage)
                GetIndZPage
                b       DoSTA

.ltorg

/* ----------------------------------
       STP instruction
       UNIMPLEMENTED : This is documented in the W65C02S datasheet ...
   ---------------------------------- */

ENTRY(op_STP_65c02)
                Continue

/* ----------------------------------
       RMBx instructions -- Available in Rockwell 65C02 but not NCR 65C02
       UNIMPLEMENTED : These are documented in the W65C02S datasheet ...
   ---------------------------------- */

ENTRY(op_RMB0_65c02)
                Continue

ENTRY(op_RMB1_65c02)
                Continue

ENTRY(op_RMB2_65c02)
                Continue

ENTRY(op_RMB3_65c02)
                Continue

ENTRY(op_RMB4_65c02)
                Continue

ENTRY(op_RMB5_65c02)
                Continue

ENTRY(op_RMB6_65c02)
                Continue

ENTRY(op_RMB7_65c02)
                Continue

.ltorg

/* ----------------------------------
       STX instructions
   ---------------------------------- */

DoSTX:          mov     wr0, X_Reg
                PutToEA_B
                Continue

ENTRY(op_STX_zpage) // 0x86
                GetZPage
                b       DoSTX

// HACK : is this used? need to study coverage ...
ENTRY(op_STX_zpage_y) // 0x96
                GetZPage_Y
                b       DoSTX

ENTRY(op_STX_abs) // 0x8e
                GetAbs
                b       DoSTX

/* ----------------------------------
       STY instructions
   ---------------------------------- */

DoSTY:          mov     wr0, Y_Reg
                PutToEA_B
                Continue

ENTRY(op_STY_zpage) // 0x84
                GetZPage
                b       DoSTY

ENTRY(op_STY_zpage_x) // 0x94
                GetZPage_X
                b       DoSTY

ENTRY(op_STY_abs) // 0x8c
                GetAbs
                b       DoSTY

/* ----------------------------------
       STZ instructions
       65c02 only
   ---------------------------------- */

DoSTZ:          mov     wr0, #0
                PutToEA_B
                Continue

// 65c02 : 0x64
ENTRY(op_STZ_zpage)
                GetZPage
                b       DoSTZ

// 65c02 : 0x74
ENTRY(op_STZ_zpage_x)
                GetZPage_X
                b       DoSTZ

// 65c02 : 0x9C
ENTRY(op_STZ_abs)
                GetAbs
                b       DoSTZ

// 65c02 : 0x9E
ENTRY(op_STZ_abs_x)
                GetAbs_X_STx
                b       DoSTZ

.ltorg

/* ----------------------------------
       TAX instruction
   ---------------------------------- */

ENTRY(op_TAX) // 0xaa
                mov     X_Reg, A_Reg
                lsl     wr0, A_Reg, #24
                orr     wr0, wr0, wr0
                cmp     wr0, #0
                FlagNZ
                Continue

/* ----------------------------------
       TAY instruction
   ---------------------------------- */

ENTRY(op_TAY) // 0xa8
                mov     Y_Reg, A_Reg
                lsl     wr0, A_Reg, #24
                orr     wr0, wr0, wr0
                cmp     wr0, #0
                FlagNZ
                Continue

/* ----------------------------------
       TRB instructions
       65c02 only
   ---------------------------------- */

DoTRB:          GetFromEA_B
                tst     wr0, A_Reg
                FlagZ
                mvn     wr1, A_Reg
                and     wr0, wr0, wr1
                PutToEA_B
                Continue

// 65c02 : 0x1C
ENTRY(op_TRB_abs)
                GetAbs
                b       DoTRB

// 65c02 : 0x14
ENTRY(op_TRB_zpage)
                GetZPage
                b       DoTRB

/* ----------------------------------
       TSB instructions
       65c02 only
   ---------------------------------- */

DoTSB:          GetFromEA_B
                tst     wr0, A_Reg
                FlagZ
                orr     wr0, A_Reg, wr0
                PutToEA_B
                Continue

// 65c02 : 0x0C
ENTRY(op_TSB_abs)
                GetAbs
                b       DoTSB

// 65c02 : 0x04
ENTRY(op_TSB_zpage)
                GetZPage
                b       DoTSB

/* ----------------------------------
       TSX instruction
   ---------------------------------- */

ENTRY(op_TSX) // 0xba
                mov     X_Reg, SP_Reg
                lsl     wr0, SP_Reg, #24
                orr     wr0, wr0, wr0
                cmp     wr0, #0
                FlagNZ
                Continue

/* ----------------------------------
       TXA instruction
   ---------------------------------- */

ENTRY(op_TXA) // 0x8a
                mov     A_Reg, X_Reg
                lsl     wr0, X_Reg, #24
                orr     wr0, wr0, wr0
                cmp     wr0, #0
                FlagNZ
                Continue

/* ----------------------------------
       TXS instruction
   ---------------------------------- */

ENTRY(op_TXS) // 0x9a
                mov     SP_Reg, X_Reg
                Continue

/* ----------------------------------
       TYA instruction
   ---------------------------------- */

ENTRY(op_TYA) // 0x98
                mov     A_Reg, Y_Reg
                lsl     wr0, Y_Reg, #24
                orr     wr0, wr0, wr0
                cmp     wr0, #0
                FlagNZ
                Continue

.ltorg

/* ----------------------------------
       ??? instruction - 65c02
       Defined as NOPs by spec
   ---------------------------------- */
ENTRY(op_UNK_65c02)
                Continue

/* ----------------------------------
       WAI instruction - 65c02
       UNIMPLEMENTED : This is documented in the W65C02S datasheet ...
   ---------------------------------- */
ENTRY(op_WAI_65c02)
                Continue

#pragma mark -
#pragma mark cpu main entry

/* -------------------------------------------------------------------------
        CPU continue
        Keep executing until we've executed >= cpu65_cycles_to_execute
   ------------------------------------------------------------------------- */

#define cycles_exe wr12
continue:
                ldr     xr1, [reg_args, #CPU65__OPCYCLES]
                ldrb    wr0, [reg_args, #CPU65_OPCODE]
                ldrb    cycles_exe, [xr1, xr0]
                ldrb    wr1, [reg_args, #CPU65_OPCYCLES]
                add     cycles_exe, cycles_exe, wr1
                strb    cycles_exe, [reg_args, #CPU65_OPCYCLES]
                TRACE_EPILOGUE

                ldr     wr1, [reg_args, #GC_CYCLES_TIMER_0]
                sub     wr1, wr1, cycles_exe
                str     wr1, [reg_args, #GC_CYCLES_TIMER_0]

                ldr     wr1, [reg_args, #GC_CYCLES_TIMER_1]
                sub     wr1, wr1, cycles_exe
                str     wr1, [reg_args, #GC_CYCLES_TIMER_1]

                ldr     wr1, [reg_args, #CPU65_CYCLE_COUNT]
                add     wr1, wr1, cycles_exe
                str     wr1, [reg_args, #CPU65_CYCLE_COUNT]

continue1:      ldr     wr1, [reg_args, #CPU65_CYCLES_TO_EXECUTE]
                subs    wr1, wr1, cycles_exe
                str     wr1, [reg_args, #CPU65_CYCLES_TO_EXECUTE]
                bmi     exit_cpu65_run
                beq     exit_cpu65_run

continue2:      ldrb    wr0, [reg_args, #CPU65__SIGNAL]
                tst     wr0, #0xFF
                bnz     exception
                CPUStatsReset
                JumpNextInstruction

/* -------------------------------------------------------------------------
        Exception handlers
   ------------------------------------------------------------------------- */

exception:      tst     wr0, #ResetSig
                beq     ex_irq
                ldrb    wr0, [reg_args, #JOY_BUTTON0] // OpenApple
                tst     wr0, #0xFF
                bne     exit_reinit
                ldrb    wr0, [reg_args, #JOY_BUTTON1] // ClosedApple
                tst     wr0, #0xFF
                bne     exit_reinit

ex_reset:       Zero(wr0)
                strb    wr0, [reg_args, #CPU65__SIGNAL]
                ldrh    EffectiveAddr, [reg_args, #RESET_VECTOR]
                GetFromEA_W
                mov     PC_Reg, wr0
                CPUStatsReset
                JumpNextInstruction

ex_irq:         tst     F_Reg, #I_Flag                      // Already interrupted?
                beq     1f
                CPUStatsReset
                JumpNextInstruction                         // Yes (ignored) ...
1:              TRACE_IRQ                                   // No (handle IRQ) ...
                mov     wr0, PC_Reg
                ror     wr0, wr0, #8
                Push(wr0)
                lsr     wr0, wr0, #24
                Push(wr0)
                orr     F_Reg, F_Reg, #X_Flag
                EncodeFlags
                Push(wr0)
                orr     F_Reg, F_Reg, #BI_Flags
                //bic   F_Reg, F_Reg, #D_Flag // AppleWin clears Decimal bit?
                ldrh    EffectiveAddr, [reg_args, #INTERRUPT_VECTOR]
                GetFromEA_W
                mov     PC_Reg, wr0
                CPUStatsReset
                ldrb    wr0, [reg_args, #CPU65_OPCYCLES]
                add     wr0, wr0, #7                          // IRQ handling will take additional 7 cycles
                strb    wr0, [reg_args, #CPU65_OPCYCLES]
                JumpNextInstruction

/* -------------------------------------------------------------------------
        65c02 CPU processing loop entry point
   ------------------------------------------------------------------------- */

ENTRY(cpu65_run)
                Enter
                // Restore CPU state when being called from C.
                mov     reg_args, xr0
                ldr     reg_vmem_r, [reg_args, #CPU65_VMEM_R]
                ldrh    EffectiveAddr, [reg_args, #CPU65_EA]
                ldrh    PC_Reg, [reg_args, #CPU65_PC]
                ldrb    A_Reg, [reg_args, #CPU65_A]
                ldrb    wr0, [reg_args, #CPU65_F]
                DecodeFlags
                ldrb    X_Reg, [reg_args, #CPU65_X]
                ldrb    Y_Reg, [reg_args, #CPU65_Y]
                ldrb    SP_Reg, [reg_args, #CPU65_SP]
                ldrb    wr0, [reg_args, #EMUL_REINITIALIZE]
                cmp     wr0, #0
                bz      continue2
                Zero(wr0)
                strb    wr0, [reg_args, #EMUL_REINITIALIZE]
                b       ex_reset

/* -------------------------------------------------------------------------
        65c02 CPU processing loop exit point
   ------------------------------------------------------------------------- */

exit_cpu65_run:
                // Save CPU state when returning from being called from C
                strh    PC_Reg, [reg_args, #CPU65_PC]
                CommonSaveCPUState
                Exit

exit_reinit:    mov     wr0, #0
                strb    wr0, [reg_args, #CPU65__SIGNAL]
                mov     wr0, #1
                strb    wr0, [reg_args, #EMUL_REINITIALIZE]
                Exit

/* -------------------------------------------------------------------------
        Debugger hooks
   ------------------------------------------------------------------------- */

ENTRY(cpu65_direct_write)
#warning FIXME TODO implement cpu65_direct_write ...
                mov     wr0, #42
                ldr     wr0, [xr0] // segfault

.ltorg

