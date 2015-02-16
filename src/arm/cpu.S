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

/*
 * 65c02 CPU opcodes emulated with ARM.
 */

#include "cpu-regs.h"
#include "misc.h"


// debugging is necessary for running CPU test suite and to use the 65c02 debugger
#ifndef NDEBUG
#define CPU_DEBUGGING 1
#endif

#define DecodeFlags \
                ldr     r1, SYM(cpu65_flags_decode); \
                ldrb    F_Reg, [r1, r0];

#define EncodeFlags \
                mov     r0, F_Reg; \
                ldr     r1, SYM(cpu65_flags_encode); \
                ldrb    r0, [r1, r0];

#define CommonSaveCPUState \
                /* save EA */ \
                ldr     r0, SYM(cpu65_ea); \
                strh    EffectiveAddr, [r0]; \
                /* save stack pointer */ \
                ldr     r0, SYM(cpu65_sp); \
                strb    SP_Reg, [r0]; \
                /* save X */ \
                ldr     r0, SYM(cpu65_x); \
                strb    X_Reg, [r0]; \
                /* save Y */ \
                ldr     r0, SYM(cpu65_y); \
                strb    Y_Reg, [r0]; \
                /* save A */ \
                ldr     r0, SYM(cpu65_a); \
                strb    A_Reg, [r0]; \
                /* save flags */ \
                EncodeFlags \
                ldr     r1, SYM(cpu65_f); \
                strb    r0, [r1]


// Tracing is necessary for some CPU tests and to verify operation against other emulators
#if CPU_TRACING
#   define TRACE_PROLOGUE \
                ldr     r1, SYM(cpu65_pc); \
                strh    PC_Reg, [r1]; \
                bl      CALL(cpu65_trace_prologue);
#   define TRACE_ARG \
                bl      CALL(cpu65_trace_arg);
#   define TRACE_ARG1 \
                bl      CALL(cpu65_trace_arg1);
#   define TRACE_ARG2 \
                bl      CALL(cpu65_trace_arg2);
#   define TRACE_EPILOGUE \
                push    {r0}; \
                CommonSaveCPUState; \
                bl      CALL(cpu65_trace_epilogue); \
                pop     {r0};
#else
#   define TRACE_PROLOGUE
#   define TRACE_ARG
#   define TRACE_ARG1
#   define TRACE_ARG2
#   define TRACE_EPILOGUE
#endif


#if CPU_DEBUGGING
#   define CPU_DEBUGGING_RESET \
                ldr     r1, SYM(cpu65_opcode); \
                strb    r0, [r1]; /* r0 should be next opcode */ \
                eor     r9, r9, r9; \
                ldr     r1, SYM(cpu65_opcycles); \
                strb    r9, [r1]; \
                /*ldr     r1, SYM(cpu65_rw);*/ \
                strb    r9, [r1];
#   define CPU_DEBUGGING_SET_READ \
                /*ldr     r1, SYM(cpu65_rw);*/ \
                ldrb    r9, [r1]; \
                orr     r9, r9, #1; \
                strb    r9, [r1];
#   define CPU_DEBUGGING_SET_WRITE \
                /*ldr     r1, SYM(cpu65_rw);*/ \
                ldrb    r9, [r1]; \
                orr     r9, r9, #2; \
                strb    r9, [r1]; \
                ldr     r1, SYM(cpu65_d); \
                strb    r0, [r1];
#else
#   define CPU_DEBUGGING_SET_READ
#   define CPU_DEBUGGING_RESET
#   define CPU_DEBUGGING_SET_WRIT
#endif

// ----------------------------------------------------------------------------
// CPU (6502) helper macros

#define GetFromPC_B \
                mov     EffectiveAddr, PC_Reg; \
                add     PC_Reg, PC_Reg, #1; \
                ldr     r1, [reg_vmem_r, EffectiveAddr, LSL PTR_SHIFT]; \
                blx     r1; \
                TRACE_ARG

#define GetFromPC_W \
                mov     EffectiveAddr, PC_Reg; \
                add     PC_Reg, PC_Reg, #2; \
                add     EffectiveAddr, EffectiveAddr, #1; \
                ldr     r1, [reg_vmem_r, EffectiveAddr, LSL PTR_SHIFT]; \
                blx     r1; \
                TRACE_ARG2 \
                sub     EffectiveAddr, EffectiveAddr, #1; \
                mov     r9, r0, LSL #8; \
                ldr     r1, [reg_vmem_r, EffectiveAddr, LSL PTR_SHIFT]; \
                blx     r1; \
                TRACE_ARG1; \
                orr     r0, r9, r0;

#define JumpNextInstruction \
                TRACE_PROLOGUE \
                GetFromPC_B \
                CPU_DEBUGGING_RESET \
                ldr     r1, SYM(cpu65__opcodes); \
                ldr     r1, [r1, r0, LSL PTR_SHIFT]; \
                bx      r1;

#define GetFromEA_B \
                CPU_DEBUGGING_SET_READ \
                ldr     r1, [reg_vmem_r, EffectiveAddr, LSL PTR_SHIFT]; \
                blx     r1;

#define GetFromEA_W \
                add     EffectiveAddr, EffectiveAddr, #1; \
                ldr     r1, [reg_vmem_r, EffectiveAddr, LSL PTR_SHIFT]; \
                blx     r1; \
                sub     EffectiveAddr, EffectiveAddr, #1; \
                mov     r9, r0, LSL #8; \
                ldr     r1, [reg_vmem_r, EffectiveAddr, LSL PTR_SHIFT]; \
                blx     r1; \
                orr     r0, r9, r0;

#define PutToEA_B \
                CPU_DEBUGGING_SET_WRITE \
                ldr     r1, SYM(cpu65_vmem_w); \
                ldr     r1, [r1, EffectiveAddr, LSL PTR_SHIFT]; \
                blx     r1;

#define GetFromMem_B(x) \
                mov     EffectiveAddr, x; \
                ldr     r1, [reg_vmem_r, EffectiveAddr, LSL PTR_SHIFT]; \
                blx     r1;

#define GetFromMem_W(x) \
                mov     EffectiveAddr, x; \
                add     EffectiveAddr, EffectiveAddr, #1; \
                ldr     r1, [reg_vmem_r, EffectiveAddr, LSL PTR_SHIFT]; \
                blx     r1; \
                sub     EffectiveAddr, EffectiveAddr, #1; \
                mov     r9, r0, LSL #8; \
                ldr     r1, [reg_vmem_r, EffectiveAddr, LSL PTR_SHIFT]; \
                blx     r1; \
                orr     r0, r9, r0;

#define Continue \
                b       continue;

#define cbw \
                /* Virtual x86: Convert Byte-to-Word */ \
                mov     r0, r0, LSL #24; \
                mov     r0, r0, ASR #24;

#define hi_byte         r9
#define lo_byte         r0

#define pc_hi_byte      r9
#define mem_cycle_count r1
#define scratch_count   r12
#define pc_hi_prev      r9
#define pc_hi_next      r0
#define BranchXCycles \
                ldr     mem_cycle_count, SYM(cpu65_opcycles); \
                eor     scratch_count, scratch_count, scratch_count; /* HACK FIXME TODO VERIFY IN GDB : is this necessary? */ \
                ldrb    scratch_count, [mem_cycle_count]; \
                add     scratch_count, scratch_count, #1; /* +1 branch taken */ \
                mov     pc_hi_prev, PC_Reg; \
                mov     pc_hi_prev, pc_hi_prev, LSR #8; \
                cbw; \
                add     PC_Reg, PC_Reg, r0; /* branch PC */ \
                mov     PC_Reg, PC_Reg, LSL #24; /* 16bit under/overflow protection */ \
                mov     PC_Reg, PC_Reg, LSR #24; \
                mov     pc_hi_next, PC_Reg; \
                mov     pc_hi_next, pc_hi_next, LSR #8; \
                teq     pc_hi_next, pc_hi_prev; \
                addne   scratch_count, scratch_count, #1; /* +1 branch taken */ \
                strb    scratch_count, [mem_cycle_count];

#warning FIXME TODO write tests for 16bit branch over/under protection

#define arm_flags r12
#define lahf \
                /* Virtual x86: Load %AH (r12) from Flags */ \
                mov     arm_flags, r15, LSR #28;

#define FlagC \
                lahf; \
                and     arm_flags, arm_flags, #C_Flag; \
                bic     F_Reg, F_Reg, #C_Flag; \
                orr     F_Reg, F_Reg, arm_flags;

#define FlagZ \
                lahf; \
                and     arm_flags, arm_flags, #Z_Flag; \
                bic     F_Reg, F_Reg, #Z_Flag; \
                orr     F_Reg, F_Reg, arm_flags;

#define FlagN \
                lahf; \
                and     arm_flags, arm_flags, #N_Flag; \
                bic     F_Reg, F_Reg, #N_Flag; \
                orr     F_Reg, F_Reg, arm_flags;

#define FlagNZ \
                lahf; \
                and     arm_flags, arm_flags, #NZ_Flags; \
                bic     F_Reg, F_Reg, #NZ_Flags; \
                orr     F_Reg, F_Reg, arm_flags;

#define FlagNZC \
                lahf; \
                and     arm_flags, arm_flags, #NZC_Flags; \
                bic     F_Reg, F_Reg, #NZC_Flags; \
                orr     F_Reg, F_Reg, arm_flags;

#define FlagNVZ \
                lahf; \
                and     arm_flags, arm_flags, #NVZ_Flags; \
                bic     F_Reg, F_Reg, #NVZ_Flags; \
                orr     F_Reg, F_Reg, arm_flags;

#define FlagNVZC \
                lahf; \
                and     arm_flags, arm_flags, #NVZC_Flags; \
                bic     F_Reg, F_Reg, #NVZC_Flags; \
                orr     F_Reg, F_Reg, arm_flags;

#define stack_loc r1
#ifdef APPLE2_VM
#define RestoreAltZP \
                ldr     stack_loc, SYM(base_stackzp); \
                add     stack_loc, stack_loc, #0x100; \
                add     stack_loc, stack_loc, SP_Reg;
#else
#define RestoreAltZP
#endif

#define Push(x) \
                RestoreAltZP \
                strb    x, [stack_loc]; \
                subs    SP_Reg, SP_Reg, #1; \
                movmi   SP_Reg, #0xFF;
#warning TODO FIXME ... need to write a 65c02 stack_loc underflow vm test ... also how does AppleWin handle 65c02 underflow?

#define Pop(x)  \
                add     SP_Reg, SP_Reg, #1; \
                bic     SP_Reg, #0x0100; \
                RestoreAltZP \
                ldrb    x, [stack_loc];
#warning TODO FIXME ... need to write a 65c02 stack overflow vm test ... also how does AppleWin handle 65c02 underflow?

// ----------------------------------------------------------------------------
// addressing macros

/* Immediate Addressing - the operand is contained in the second byte of the
   instruction. */
#define _GetImm \
                mov     EffectiveAddr, PC_Reg; \
                ldr     r0, SYM(mask_FFFF); \
                ldrh    r0, [r0]; \
                teq     PC_Reg, r0; \
                addne   PC_Reg, PC_Reg, #1; \
                moveq   PC_Reg, #0
#warning TODO write test cases for immediate addressing 16bit overflow
#warning FIXME TODO ... can all/many of the 16bit overflow cases be handled in Continue instead of in various one-off/bespoke ways ?

#if CPU_TRACING
#define GetImm \
                _GetImm \
                ldr     r1, [reg_vmem_r, EffectiveAddr, LSL PTR_SHIFT]; \
                blx     r1; \
                TRACE_ARG
#else
#define GetImm \
                _GetImm
#endif

/* Absolute Addressing - the second byte of the instruction is the low
   order address, and the third byte is the high order byte. */
#define GetAbs \
                GetFromPC_W; \
                mov     EffectiveAddr, r0;

/* Zero Page Addressing - the second byte of the instruction is an
   address on the zero page */
#define GetZPage \
                GetFromPC_B; \
                mov     EffectiveAddr, r0;

/* Zero Page Indexed Addressing - The effective address is calculated by
   adding the second byte to the contents of the index register.  Due
   to the zero page addressing nature of this mode, no carry is added
   to the high address byte, and the crossing of page boundaries does
   not occur. */
#define GetZPage_X \
                GetFromPC_B; \
                add     r0, r0, X_Reg; \
                mov     EffectiveAddr, r0;

#define GetZPage_Y \
                GetFromPC_B; \
                add     r0, r0, Y_Reg; \
                mov     EffectiveAddr, r0;

#define ea_hi_prev r1
#define ea_hi_next r9
#define AddIndexRegAndTestPageBoundary(IndexReg) \
                mov     ea_hi_prev, r0; \
                mov     ea_hi_prev, ea_hi_prev, LSR #8; \
                add     r0, r0, IndexReg; \
                mov     ea_hi_next, r0; \
                mov     ea_hi_next, ea_hi_next, LSR #8; \
                teq     ea_hi_prev, ea_hi_next; \
                beq     9f;

#define PageBoundaryCrossed \
                ldr     mem_cycle_count, SYM(cpu65_opcycles); \
                eor     scratch_count, scratch_count, scratch_count; /* HACK FIXME TODO VERIFY IN GDB : is this necessary? */ \
                ldrb    scratch_count, [mem_cycle_count]; \
                add     scratch_count, scratch_count, #1; /* +1 cycle on page_boundary */ \
                strb    scratch_count, [mem_cycle_count];

/* Absolute Indexed Addressing - The effective address is formed by
   adding the contents of X or Y to the address contained in the
   second and third bytes of the instruction. */

#define GetAbs_X \
                GetFromPC_W \
                AddIndexRegAndTestPageBoundary(X_Reg) \
                PageBoundaryCrossed \
9:              mov     EffectiveAddr, r0;

#define GetAbs_X_STx \
                GetFromPC_W \
                AddIndexRegAndTestPageBoundary(X_Reg) \
                /* PageBoundaryCrossed */ \
9:              mov     EffectiveAddr, r0;

#define GetAbs_Y \
                GetFromPC_W \
                AddIndexRegAndTestPageBoundary(Y_Reg) \
                PageBoundaryCrossed \
9:              mov     EffectiveAddr, r0;

#define GetAbs_Y_STA \
                GetFromPC_W \
                AddIndexRegAndTestPageBoundary(Y_Reg) \
                /* PageBoundaryCrossed */ \
9:              mov     EffectiveAddr, r0;

/* Zero Page Indirect Addressing (65c02) - The second byte of the
   instruction points to a memory location on page zero containing the
   low order byte of the effective address.  The next location on page
   zero contains the high order byte of the address. */
#define GetIndZPage \
                GetFromPC_B \
                add     r0, r0, #1; \
                and     r0, r0, #0xFF; \
                mov     EffectiveAddr, r0; \
                GetFromEA_B \
                mov     r9, r0, LSL #8; \
                sub     EffectiveAddr, EffectiveAddr, #1; \
                and     EffectiveAddr, EffectiveAddr, #0xFF; \
                GetFromEA_B \
                orr     r0, r9, r0; \
                mov     EffectiveAddr, r0;

/* Zero Page Indexed Indirect Addressing - The second byte is added to
   the contents of the X index register; the carry is discarded.  The
   result of this addition points to a memory location on page zero
   whose contents is the low order byte of the effective address.  The
   next memory location in page zero contains the high-order byte of
   the effective address.  Both memory locations specifying the high
   and low-order bytes must be in page zero. */
#define GetIndZPage_X \
                GetFromPC_B \
                add     r0, r0, X_Reg; \
                add     r0, r0, #1; \
                and     r0, r0, #0xFF; \
                mov     EffectiveAddr, r0; \
                GetFromEA_B \
                mov     r9, r0, LSL #8; \
                sub     EffectiveAddr, EffectiveAddr, #1; \
                and     EffectiveAddr, EffectiveAddr, #0xFF; \
                GetFromEA_B \
                orr     r0, r9, r0; \
                mov     EffectiveAddr, r0;

/* Indirect Indexed Addressing - The second byte of the instruction
   points to a memory location in page zero.  The contents of this
   memory location are added to the contents of the Y index register,
   the result being the low order byte of the effective address.  The
   carry from this addition is added to the contents of the next page
   zero memory location, the result being the high order byte of the
   effective address. */
#define _GetIndZPage_Y \
                GetFromPC_B \
                add     r0, r0, #1; \
                and     r0, r0, #0xFF; \
                mov     EffectiveAddr, r0; \
                GetFromEA_B \
                mov     r9, r0, LSL #8; \
                sub     EffectiveAddr, EffectiveAddr, #1; \
                and     EffectiveAddr, EffectiveAddr, #0xFF; \
                GetFromEA_B \
                orr     r0, r9, r0; \
                AddIndexRegAndTestPageBoundary(Y_Reg)

#define GetIndZPage_Y \
                _GetIndZPage_Y \
                PageBoundaryCrossed \
9:              mov     EffectiveAddr, r0;

#define GetIndZPage_Y_STA \
                _GetIndZPage_Y \
                PageBoundaryCrossed \
9:              mov     EffectiveAddr, r0;

#define bt \
                /* Virtual x86: Bit Test (carry flag only) */ \
                tst     F_Reg, #C_Flag; \
                biceq   r15, r15, #30; \
                orrne   r15, r15, #30;

// ----------------------------------------------------------------------------
// 65c02 instruction macros

#define DoADC_b \
                GetFromEA_B \
                mov     r0, r0, LSL #24; \
                mov     A_Reg, A_Reg, LSL #24; \
                bt \
                adcs    A_Reg, A_Reg, r0; \
                FlagNVZC \
                mov     A_Reg, A_Reg, LSR #24;

#ifndef NDEBUG
#define DebugBCDCheck \
                tst     A_Reg, #0x80; \
                tstne   A_Reg, #0x60; \
                blne    CALL(debug_illegal_bcd); \
                tst     A_Reg, #0x08; \
                tstne   A_Reg, #0x06; \
                blne    CALL(debug_illegal_bcd); \
                tst     r0, #0x80; \
                tstne   r0, #0x60; \
                blne    CALL(debug_illegal_bcd); \
                tst     r0, #0x08; \
                tstne   r0, #0x06; \
                blne    CALL(debug_illegal_bcd);
#else
#define DebugBCDCheck
#endif

#define DoAND \
                GetFromEA_B \
                mov     r0, r0, LSL #24; \
                mov     A_Reg, A_Reg, LSL #24; \
                ands    A_Reg, r0; \
                FlagNZ

#define _DoASL(x) \
                mov     x, x, LSL #24; \
                adcs    x, x, x; \
                FlagNZC \
                mov     x, x, LSR #24;

#define DoASL \
                GetFromEA_B \
                _DoASL(r0) \
                PutToEA_B

#define _DoBIT \
                GetFromEA_B \
                mov     r0, r0, LSL #24; \
                mov     r1, A_Reg, LSL #24; \
                tst     r1, r0;

#define DoBIT \
                _DoBIT \
                FlagNVZ

#define DoCMP \
                GetFromEA_B \
                /* TODO FIXME : maybe actually use cmp or cmn instruction? */ \
                mov     r0, r0, LSL #24; \
                mov     r1, A_Reg, LSL #24; \
                subs    r1, r1, r0; \
                /*cmc;*/ \
                FlagNZC

#define DoCPX \
                GetFromEA_B \
                mov     r0, r0, LSL #24; \
                mov     r1, X_Reg, LSL #24; \
                subs    r1, r1, r0; \
                /*cmc;*/ \
                FlagNZC

#define DoCPY \
                GetFromEA_B \
                mov     r0, r0, LSL #24; \
                mov     r1, Y_Reg, LSL #24; \
                subs    r1, r1, r0; \
                /*cmc;*/ \
                FlagNZC

#define _DoDEC(x) \
                mov     r1, #1; \
                mov     r1, r1, LSL #24; \
                mov     x, x, LSL #24; \
                subs    x, x, r1; \
                FlagNZ \
                mov     x, x, LSR #24;

#define DoDEC \
                GetFromEA_B \
                _DoDEC(r0) \
                PutToEA_B

#define DoEOR \
                GetFromEA_B \
                mov     r0, r0, LSL #24; \
                mov     A_Reg, A_Reg, LSL #24; \
                eors    A_Reg, A_Reg, r0; \
                FlagNZ \
                mov     A_Reg, A_Reg, LSR #24;

#define _DoINC(x) \
                mov     r1, #1; \
                mov     r1, r1, LSL #24; \
                mov     x, x, LSL #24; \
                adds    x, x, r1; \
                FlagNZ \
                mov     x, x, LSR #24;

#define DoINC \
                GetFromEA_B \
                _DoINC(r0) \
                PutToEA_B

#define DoLDA \
                GetFromEA_B \
                mov     A_Reg, r0; \
                mov     r0, r0, LSL #24; \
                orrs    r0, r0, r0; \
                FlagNZ

#define DoLDX \
                GetFromEA_B \
                mov     X_Reg, r0; \
                mov     r0, r0, LSL #24; \
                orrs    r0, r0, r0; \
                FlagNZ

#define DoLDY \
                GetFromEA_B \
                mov     Y_Reg, r0; \
                mov     r0, r0, LSL #24; \
                orrs    r0, r0, r0; \
                FlagNZ

#define DoLSR \
                GetFromEA_B \
                lsrs    r0, r0, #1; \
                FlagNZC \
                PutToEA_B

#define DoORA \
                GetFromEA_B \
                mov     r0, r0, LSL #24; \
                mov     A_Reg, A_Reg, LSL #24; \
                orrs    A_Reg, A_Reg, r0; \
                FlagNZ \
                mov     A_Reg, A_Reg, LSR #24;

#define _DoROL(x) \
                mov     x, x, LSL #8; \
                tst     F_Reg, #C_Flag; \
                orrne   x, x, #0x80; \
                lsls    x, x, #17; \
                FlagNZC \
                mov     x, x, LSR #24;

#define DoROL \
                GetFromEA_B \
                _DoROL(r0) \
                PutToEA_B


#define _DoROR(x) \
                tst     F_Reg, #C_Flag; \
                orrne   x, x, #0x100; \
                rors    x, x, #1; \
                FlagNZC

#define DoROR \
                GetFromEA_B \
                _DoROR(r0) \
                PutToEA_B

#define DoSBC_b \
                GetFromEA_B \
                mvn     r0, r0; \
                bt; \
                adcs    A_Reg, A_Reg, r0; \
                FlagNVZC

#define DoSTA \
                mov     r0, A_Reg; \
                PutToEA_B

#define DoSTX \
                mov     r0, X_Reg; \
                PutToEA_B

#define DoSTY \
                mov     r0, Y_Reg; \
                PutToEA_B

#define DoSTZ \
                mov     r0, #0; \
                PutToEA_B

#define DoTRB \
                GetFromEA_B \
                teq     r0, A_Reg; \
                FlagZ \
                mvn     r1, A_Reg; \
                and     r0, r0, r1; \
                PutToEA_B

#define DoTSB \
                GetFromEA_B \
                teq     r0, A_Reg; \
                FlagZ \
                orr     r0, A_Reg, r0; \
                PutToEA_B

/* ----------------------------------------------------------------------
       6502 routines and instructions
   ---------------------------------------------------------------------- */

/* ----------------------------------
       ADC instructions
       ADd memory to accumulator with Carry
   ---------------------------------- */

// Decimal mode
ENTRY(op_ADC_dec)
#warning FIXME TODO op_ADC_dec
#if 0
                incb    SYM(cpu65_opcycles) // +1 cycle
                GetFromEA_B
                DebugBCDCheck
                bt      $C_Flag_Bit, AF_Reg_X
                adcb    A_Reg, %al
                // DAA algorithm : http://www.ray.masmcode.com/BCDdaa.html
                // CF_old = CF
                // IF (al AND 0Fh > 9) or (the Auxilliary Flag is set)
                //    al = al + 6
                //    CF = CF (automagically) or CF_old
                //    AF set (automagically)
                // ENDIF
                // IF (al > 99h) or (Carry Flag is set)
                //    al = al + 60h
                //    CF set
                // ENDIF
                pushq   _XBX
                pushfq
                popq    _XBX
                andb    $~(N_Flag|V_Flag|Z_Flag|C_Flag), F_Reg
                movb    %al, %ah
                andb    $0x0f, %ah
                cmpb    $9, %ah
                jg      _daa_lo_nyb
                btq     $X86_AF_Bit, _XBX
                jnc     _daa_next0
_daa_lo_nyb:    addb    $6, %al         // adjust lo nybble
                jc      _daa_hi_nyb
_daa_next0:     btq     $X86_CF_Bit, _XBX
                jc      _daa_hi_nyb
                xorb    %ah, %ah
                cmpw    $0x99, %ax
                jle     _daa_next1
_daa_hi_nyb:    addb    $0x60, %al      // adjust hi nybble
                orb     $C_Flag, F_Reg  // FlagC
_daa_next1:     testq   $0x80, %rax
                jz      _daa_next2
                orb     $N_Flag, F_Reg  // FlagN
_daa_next2:     testq   $0xFF, %rax
                jnz     _daa_finish
                orb     $Z_Flag, F_Reg  // FlagZ
_daa_finish:    popq    _XBX
                movb    %al, A_Reg
#endif
                Continue

#define maybe_DoADC_d \
                tst     F_Reg, #D_Flag;  /* Decimal mode? */ \
                bne     CALL(op_ADC_dec) /* Yes, jump to decimal version */

ENTRY(op_ADC_imm) // 0x69
                GetImm
          maybe_DoADC_d
                DoADC_b
                Continue

ENTRY(op_ADC_zpage) // 0x65
                GetZPage
          maybe_DoADC_d
                DoADC_b
                Continue

ENTRY(op_ADC_zpage_x) // 0x75
                GetZPage_X
          maybe_DoADC_d
                DoADC_b
                Continue

// UNIMPLEMENTED : W65C02S datasheet
ENTRY(op_ADC_zpage_y)
                b CALL(op_NOP)

ENTRY(op_ADC_abs) // 0x6d
                GetAbs
          maybe_DoADC_d
                DoADC_b
                Continue

ENTRY(op_ADC_abs_x) // 0x7d
                GetAbs_X
          maybe_DoADC_d
                DoADC_b
                Continue

ENTRY(op_ADC_abs_y) // 0x79
                GetAbs_Y
          maybe_DoADC_d
                DoADC_b
                Continue

ENTRY(op_ADC_ind_x) // 0x61
                GetIndZPage_X
          maybe_DoADC_d
                DoADC_b
                Continue

ENTRY(op_ADC_ind_y) // 0x71
                GetIndZPage_Y
          maybe_DoADC_d
                DoADC_b
                Continue

// 65c02 : 0x72
ENTRY(op_ADC_ind_zpage)
                GetIndZPage
          maybe_DoADC_d
                DoADC_b
                Continue

mask_FFFF:        .hword 0xFFFF
interrupt_vector: .hword 0xFFFE
reset_vector:     .hword 0xFFFC
.ltorg

/* ----------------------------------
       AND instructions
       logical AND memory with accumulator
   ---------------------------------- */

ENTRY(op_AND_imm) // 0x29
                GetImm
                DoAND
                Continue

ENTRY(op_AND_zpage) // 0x25
                GetZPage
                DoAND
                Continue

ENTRY(op_AND_zpage_x) // 0x35
                GetZPage_X
                DoAND
                Continue

// UNIMPLEMENTED : W65C02S datasheet
ENTRY(op_AND_zpage_y)
                b   CALL(op_NOP)

ENTRY(op_AND_abs) // 0x2d
                GetAbs
                DoAND
                Continue

ENTRY(op_AND_abs_x) // 0x3d
                GetAbs_X
                DoAND
                Continue

ENTRY(op_AND_abs_y) // 0x39
                GetAbs_Y
                DoAND
                Continue

ENTRY(op_AND_ind_x) // 0x21
                GetIndZPage_X
                DoAND
                Continue

ENTRY(op_AND_ind_y) // 0x31
                GetIndZPage_Y
                DoAND
                Continue

// 65c02 : 0x32
ENTRY(op_AND_ind_zpage)
                GetIndZPage
                DoAND
                Continue

.ltorg

/* ----------------------------------
       ASL instructions
       Arithmetic Shift one bit Left, memory or accumulator
   ---------------------------------- */

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

ENTRY(op_BIT_zpage) // 0x24
                GetZPage
                DoBIT
                Continue

ENTRY(op_BIT_abs) // 0x2c
                GetAbs
                DoBIT
                Continue

// 65c02 : 0x34
ENTRY(op_BIT_zpage_x)
                GetZPage_X
                DoBIT
                Continue

// 65c02 : 0x3C
ENTRY(op_BIT_abs_x)
                GetAbs_X
                DoBIT
                Continue

                /* BIT immediate is anomalous in that it does not affect the
                 * N and V flags, unlike in other addressing modes.
                 */
// 65c02 : 0x89
ENTRY(op_BIT_imm)
                GetImm
                _DoBIT
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
                ldr     r0, SYM(mask_FFFF)
                ldrh    r0, [r0]
                teq     PC_Reg, r0
                addne   PC_Reg, PC_Reg, #1
                moveq   PC_Reg, #0
#warning TODO FIXME ... write test for overflow in op_BRK
                mov     r0, PC_Reg
                mov     r0, r0, ROR #8
                Push(r0)
                mov     r0, r0, LSR #24
                Push(r0)
                orr     F_Reg, F_Reg, #BX_Flags
                EncodeFlags
                Push(r0)
                orr     F_Reg, F_Reg, #I_Flag
                ldr     EffectiveAddr, SYM(interrupt_vector) 
                ldrh    EffectiveAddr, [EffectiveAddr]
                GetFromEA_W
                mov     PC_Reg, r0
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
                bic     F_Reg, #C_Flag
                Continue

/* ----------------------------------
       CLD instruction
   ---------------------------------- */

ENTRY(op_CLD) // 0xd8
                bic     F_Reg, #D_Flag
                Continue

/* ----------------------------------
       CLI instruction
   ---------------------------------- */

ENTRY(op_CLI) // 0x58
                bic     F_Reg, #I_Flag
                Continue

/* ----------------------------------
       CLV instruction
   ---------------------------------- */

ENTRY(op_CLV) // 0xB8
                bic     F_Reg, #V_Flag
                Continue

.ltorg

/* ----------------------------------
       CMP instructions
       CoMPare memory and accumulator
   ---------------------------------- */

ENTRY(op_CMP_imm) // 0xc9
                GetImm
                DoCMP
                Continue

ENTRY(op_CMP_zpage) // 0xc5
                GetZPage
                DoCMP
                Continue

ENTRY(op_CMP_zpage_x) // 0xd5
                GetZPage_X
                DoCMP
                Continue

// UNIMPLEMENTED : W65C02S datasheet
ENTRY(op_CMP_zpage_y)
                b CALL(op_NOP)

ENTRY(op_CMP_abs) // 0xcd
                GetAbs
                DoCMP
                Continue

ENTRY(op_CMP_abs_x) // 0xdd
                GetAbs_X
                DoCMP
                Continue

ENTRY(op_CMP_abs_y) // 0xd9
                GetAbs_Y
                DoCMP
                Continue

ENTRY(op_CMP_ind_x) // 0xc1
                GetIndZPage_X
                DoCMP
                Continue

ENTRY(op_CMP_ind_y) // 0xd1
                GetIndZPage_Y
                DoCMP
                Continue

// 65c02 : 0xD2
ENTRY(op_CMP_ind_zpage)
                GetIndZPage
                DoCMP
                Continue

.ltorg

/* ----------------------------------
       CPX instructions
       ComPare memory and X register
   ---------------------------------- */

ENTRY(op_CPX_imm) // 0xe0
                GetImm
                DoCPX
                Continue

ENTRY(op_CPX_zpage) // 0xe4
                GetZPage
                DoCPX
                Continue

ENTRY(op_CPX_abs) // 0xec
                GetAbs
                DoCPX
                Continue

/* ----------------------------------
       CPY instructions
       ComPare memory and Y register
   ---------------------------------- */

ENTRY(op_CPY_imm) // 0xc0
                GetImm
                DoCPY
                Continue

ENTRY(op_CPY_zpage) // 0xc4
                GetZPage
                DoCPY
                Continue

ENTRY(op_CPY_abs) // 0xcc
                GetAbs
                DoCPY
                Continue

.ltorg

/* ----------------------------------
       DEA: DEcrement Accumulator
   ---------------------------------- */

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
                GetAbs_X
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

ENTRY(op_EOR_imm) // 0x49
                GetImm
                DoEOR
                Continue

ENTRY(op_EOR_zpage) // 0x45
                GetZPage
                DoEOR
                Continue

ENTRY(op_EOR_zpage_x) // 0x55
                GetZPage_X
                DoEOR
                Continue

// UNIMPLEMENTED : W65C02S datasheet
ENTRY(op_EOR_zpage_y)
                b CALL(op_NOP)

ENTRY(op_EOR_abs) // 0x4d
                GetAbs
                DoEOR
                Continue

ENTRY(op_EOR_abs_x) // 0x5d
                GetAbs_X
                DoEOR
                Continue

ENTRY(op_EOR_abs_y) // 0x59
                GetAbs_Y
                DoEOR
                Continue

ENTRY(op_EOR_ind_x) // 0x41
                GetIndZPage_X
                DoEOR
                Continue

ENTRY(op_EOR_ind_y) // 0x51
                GetIndZPage_Y
                DoEOR
                Continue

// 65c02 : 0x52
ENTRY(op_EOR_ind_zpage)
                GetIndZPage
                DoEOR
                Continue

.ltorg

/* ----------------------------------
       INA : INcrement Accumulator
   ---------------------------------- */

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
                GetAbs_X
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
                and     r1, r0, #0xFF
                eor     r1, #0xFF
                beq     jmp_special
                GetFromMem_W(r0)
                mov     PC_Reg, r0
                Continue
jmp_special: // see JMP indirect note in _Understanding the Apple IIe_ 4-25
                mov     PC_Reg, r0
                sub     PC_Reg, #0xFF
                GetFromMem_B(PC_Reg)
                mov     r9, r0, LSL #8
                add     PC_Reg, #0xFF
                GetFromMem_B(PC_Reg)
                orr     r0, r9, r0
                mov     PC_Reg, r0
                Continue

// 65c02 : 0x7C
ENTRY(op_JMP_abs_ind_x)
                GetFromPC_W
                mov     EffectiveAddr, r0
                mov     r0, X_Reg
                cbw
                add     EffectiveAddr, r0
                mov     EffectiveAddr, EffectiveAddr, LSL #24 // 16bit under/overflow protection
                mov     EffectiveAddr, EffectiveAddr, LSR #24
#warning FIXME TODO write test for op_JMP_abs_ind_x under/overflow protection
                GetFromMem_W(EffectiveAddr)
                mov     PC_Reg, r0
                Continue

/* ----------------------------------
       JSR instruction
   ---------------------------------- */

ENTRY(op_JSR) // 0x20
                GetAbs
                mov     r0, PC_Reg
                subs    r0, r0, #1
                ldrmi   r0, SYM(mask_FFFF) // handle underflow -- considered highly unlikely
                ldrmih  r0, [r0]
                mov     r0, r0, ROR #8
#warning TODO FIXME write test for op_JSR underflow ...
                Push(r0) // push hi_byte
                mov     r0, r0, LSR #24
                Push(r0) // push lo_byte
                mov     PC_Reg, EffectiveAddr
                Continue

.ltorg

/* ----------------------------------
       LDA instructions
       LoaD Accumulator with memory
   ---------------------------------- */

ENTRY(op_LDA_imm) // 0xa9
                GetImm
                DoLDA
                Continue

ENTRY(op_LDA_zpage) // 0xa5
                GetZPage
                DoLDA
                Continue

ENTRY(op_LDA_zpage_x) // 0xb5
                GetZPage_X
                DoLDA
                Continue

// UNIMPLEMENTED : W65C02S datasheet
ENTRY(op_LDA_zpage_y)
                b   CALL(op_NOP)

ENTRY(op_LDA_abs) // 0xad
                GetAbs
                DoLDA
                Continue

ENTRY(op_LDA_abs_x) // 0xbd
                GetAbs_X
                DoLDA
                Continue

ENTRY(op_LDA_abs_y) // 0xb9
                GetAbs_Y
                DoLDA
                Continue

ENTRY(op_LDA_ind_x) // 0xa1
                GetIndZPage_X
                DoLDA
                Continue

ENTRY(op_LDA_ind_y) // 0xb1
                GetIndZPage_Y
                DoLDA
                Continue

// 65c02 : 0xB2
ENTRY(op_LDA_ind_zpage)
                GetIndZPage
                DoLDA
                Continue

.ltorg

/* ----------------------------------
       LDX instructions
   ---------------------------------- */

ENTRY(op_LDX_imm) // 0xa2
                GetImm
                DoLDX
                Continue

ENTRY(op_LDX_zpage) // 0xa6
                GetZPage
                DoLDX
                Continue

// HACK : is this used? need to study coverage ...
ENTRY(op_LDX_zpage_y) // 0xb6
                GetZPage_Y
                DoLDX
                Continue

ENTRY(op_LDX_abs) // 0xae
                GetAbs
                DoLDX
                Continue

ENTRY(op_LDX_abs_y) // 0xbe
                GetAbs_Y
                DoLDX
                Continue

/* ----------------------------------
       LDY instructions
   ---------------------------------- */

ENTRY(op_LDY_imm) // 0xa0
                GetImm
                DoLDY
                Continue

ENTRY(op_LDY_zpage) // 0xa4
                GetZPage
                DoLDY
                Continue

ENTRY(op_LDY_zpage_x) // 0xb4
                GetZPage_X
                DoLDY
                Continue

ENTRY(op_LDY_abs) // 0xac
                GetAbs
                DoLDY
                Continue

ENTRY(op_LDY_abs_x) // 0xbc
                GetAbs_X
                DoLDY
                Continue

.ltorg

/* ----------------------------------
       LSR instructions
   ---------------------------------- */

ENTRY(op_LSR_acc) // 0x4a
                lsrs    r0, r0, #1
                FlagNZC
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

ENTRY(op_ORA_imm) // 0x09
                GetImm
                DoORA
                Continue

ENTRY(op_ORA_zpage) // 0x05
                GetZPage
                DoORA
                Continue

ENTRY(op_ORA_zpage_x) // 0x15
                GetZPage_X
                DoORA
                Continue

// UNIMPLEMENTED : W65C02S datasheet
ENTRY(op_ORA_zpage_y)
                b   CALL(op_NOP)

ENTRY(op_ORA_abs) // 0x0d
                GetAbs
                DoORA
                Continue

ENTRY(op_ORA_abs_x) // 0x1d
                GetAbs_X
                DoORA
                Continue

ENTRY(op_ORA_abs_y) // 0x19
                GetAbs_Y
                DoORA
                Continue

ENTRY(op_ORA_ind_x) // 0x01
                GetIndZPage_X
                DoORA
                Continue

ENTRY(op_ORA_ind_y) // 0x11
                GetIndZPage_Y
                DoORA
                Continue

// 65c02 : 0x12
ENTRY(op_ORA_ind_zpage)
                GetIndZPage
                DoORA
                Continue

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
                Push(r0)
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

ENTRY(op_PLA) // 0x68
                Pop(A_Reg)
                mov     r0, A_Reg, LSL #24
                orr     r0, r0, r0
                FlagNZ
                Continue

/* ----------------------------------
       PLP instruction
   ---------------------------------- */

ENTRY(op_PLP) // 0x28
                Pop(r0)
                DecodeFlags
                orr     F_Reg, F_Reg, #BX_Flags
                Continue

/* ----------------------------------
       PLX instruction
       65c02 : 0xFA
   ---------------------------------- */

ENTRY(op_PLX)
                Pop(X_Reg)
                mov     r0, X_Reg, LSL #24
                orr     r0, r0, r0
                FlagNZ
                Continue

/* ----------------------------------
       PLY instruction
       65c02 : 0x7A
   ---------------------------------- */

ENTRY(op_PLY)
                Pop(Y_Reg)
                mov     r0, Y_Reg, LSL #24
                orr     Y_Reg, Y_Reg
                FlagNZ
                Continue

.ltorg

/* ----------------------------------
       ROL instructions
   ---------------------------------- */

ENTRY(op_ROL_acc) // 0x2a
                _DoROL(A_Reg)
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
                Pop(r0)
                DecodeFlags
                orr     F_Reg, F_Reg, #BX_Flags
                Pop(lo_byte)
                Pop(hi_byte)
                mov     hi_byte, hi_byte, LSL #8
                orr     r0, hi_byte, lo_byte
                mov     PC_Reg, r0
                Continue

/* ----------------------------------
       RTS instruction
   ---------------------------------- */

ENTRY(op_RTS) // 0x60
                Pop(lo_byte)
                Pop(hi_byte)
                mov     hi_byte, hi_byte, LSL #8
                orr     r0, hi_byte, lo_byte
                ldr     r1, SYM(mask_FFFF)
                ldr     r1, [r1]
                teq     r0, r1
                addne   r0, r0, #1
                moveq   r0, #0 // handle overflow -- considered highly unlikely
#warning TODO FIXME write test for this overflow ...
                mov     PC_Reg, r0
                Continue

.ltorg

/* ----------------------------------
       SBC instructions
       SuBtract memory from accumulator with Borrow
   ---------------------------------- */
ENTRY(op_SBC_dec)
#warning FIXME TODO op_ADC_dec
#if 0
                incb    SYM(cpu65_opcycles) // +1 cycle
                GetFromEA_B
                DebugBCDCheck
                btc     $C_Flag_Bit, AF_Reg_X
                xchgb   A_Reg, %al
                cmc
                sbbb    A_Reg, %al
                // DAS algorithm : http://www.ray.masmcode.com/BCDdas.html
                // CF_old = CF
                // IF (al AND 0Fh > 9) or (the Auxilliary Flag is set)
                //    al = al - 6
                //    CF = CF (automagically) or CF_old
                //    AF set (automagically)
                // ENDIF
                // IF (al > 99h) or (Carry Flag is set)
                //    al = al - 60h
                //    ^CF set
                // ENDIF
                pushq   _XBX
                pushfq
                popq    _XBX
                andb    $~(N_Flag|V_Flag|Z_Flag), F_Reg
                orb     $C_Flag, F_Reg
                movb    %al, %ah
                andb    $0x0f, %ah
                cmpb    $9, %ah
                jg      _das_lo_nyb
                btq     $X86_AF_Bit, _XBX
                jnc     _das_next0
_das_lo_nyb:    subb    $6, %al         // adjust lo nybble
                jc      _das_hi_nyb
_das_next0:     btq     $X86_CF_Bit, _XBX
                jc      _das_hi_nyb
                xorb    %ah, %ah
                cmpw    $0x99, %ax
                jle     _das_next1
_das_hi_nyb:    subb    $0x60, %al      // adjust hi nybble
                andb    $~C_Flag, F_Reg // !FlagC
_das_next1:     testq   $0x80, %rax
                jz      _das_next2
                orb     $N_Flag, F_Reg  // FlagN
_das_next2:     testq   $0xFF, %rax
                jnz     _das_finish
                orb     $Z_Flag, F_Reg  // FlagZ
_das_finish:    popq    _XBX
                movb    %al, A_Reg
#endif
                Continue

#define maybe_DoSBC_d \
                tst     F_Reg, #D_Flag;  /* Decimal mode? */ \
                bne     CALL(op_SBC_dec) /* Yes, jump to decimal version */

ENTRY(op_SBC_imm) // 0xe9
                GetImm
          maybe_DoSBC_d
                DoSBC_b
                Continue

ENTRY(op_SBC_zpage) // 0xe5
                GetZPage
          maybe_DoSBC_d
                DoSBC_b
                Continue

ENTRY(op_SBC_zpage_x) // 0xf5
                GetZPage_X
          maybe_DoSBC_d
                DoSBC_b
                Continue

// UNIMPLEMENTED : W65C02S datasheet
ENTRY(op_SBC_zpage_y)
                b CALL(op_NOP)

ENTRY(op_SBC_abs) // 0xed
                GetAbs
          maybe_DoSBC_d
                DoSBC_b
                Continue

ENTRY(op_SBC_abs_x) // 0xfd
                GetAbs_X
          maybe_DoSBC_d
                DoSBC_b
                Continue

ENTRY(op_SBC_abs_y) // 0xf9
                GetAbs_Y
          maybe_DoSBC_d
                DoSBC_b
                Continue

ENTRY(op_SBC_ind_x) // 0xe1
                GetIndZPage_X
          maybe_DoSBC_d
                DoSBC_b
                Continue

ENTRY(op_SBC_ind_y) // 0xf1
                GetIndZPage_Y
          maybe_DoSBC_d
                DoSBC_b
                Continue

// 65c02 : 0xF2
ENTRY(op_SBC_ind_zpage)
                GetIndZPage
          maybe_DoSBC_d
                DoSBC_b
                Continue

.ltorg

/* ----------------------------------
       SEC instruction
   ---------------------------------- */

ENTRY(op_SEC) // 0x38
                orr     F_Reg, #C_Flag
                Continue

/* ----------------------------------
       SED instruction
   ---------------------------------- */

ENTRY(op_SED) // 0xf8
                orr     F_Reg, #D_Flag
                Continue

/* ----------------------------------
       SEI instruction
   ---------------------------------- */

ENTRY(op_SEI) // 0x78
                orr     F_Reg, #I_Flag
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

ENTRY(op_STA_zpage) // 0x85
                GetZPage
                DoSTA
                Continue

ENTRY(op_STA_zpage_x) // 0x95
                GetZPage_X
                DoSTA
                Continue

// UNIMPLEMENTED : W65C02S datasheet
ENTRY(op_STA_zpage_y)
                b   CALL(op_NOP)

ENTRY(op_STA_abs) // 0x8d
                GetAbs
                DoSTA
                Continue

ENTRY(op_STA_abs_x) // 0x9d
                GetAbs_X_STx
                DoSTA
                Continue

ENTRY(op_STA_abs_y) // 0x99
                GetAbs_Y_STA
                DoSTA
                Continue

ENTRY(op_STA_ind_x) // 0x81
                GetIndZPage_X
                DoSTA
                Continue

ENTRY(op_STA_ind_y) // 0x91
                GetIndZPage_Y_STA
                DoSTA
                Continue

// 65c02 : 0x92
ENTRY(op_STA_ind_zpage)
                GetIndZPage
                DoSTA
                Continue

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

ENTRY(op_STX_zpage) // 0x86
                GetZPage
                DoSTX
                Continue

// HACK : is this used? need to study coverage ...
ENTRY(op_STX_zpage_y) // 0x96
                GetZPage_Y
                DoSTX
                Continue

ENTRY(op_STX_abs) // 0x8e
                GetAbs
                DoSTX
                Continue

/* ----------------------------------
       STY instructions
   ---------------------------------- */

ENTRY(op_STY_zpage) // 0x84
                GetZPage
                DoSTY
                Continue

ENTRY(op_STY_zpage_x) // 0x94
                GetZPage_X
                DoSTY
                Continue

ENTRY(op_STY_abs) // 0x8c
                GetAbs
                DoSTY
                Continue

/* ----------------------------------
       STZ instructions
       65c02 only
   ---------------------------------- */

// 65c02 : 0x64
ENTRY(op_STZ_zpage)
                GetZPage
                DoSTZ
                Continue

// 65c02 : 0x74
ENTRY(op_STZ_zpage_x)
                GetZPage_X
                DoSTZ
                Continue

// 65c02 : 0x9C
ENTRY(op_STZ_abs)
                GetAbs
                DoSTZ
                Continue

// 65c02 : 0x9E
ENTRY(op_STZ_abs_x)
                GetAbs_X_STx
                DoSTZ
                Continue

.ltorg

/* ----------------------------------
       TAX instruction
   ---------------------------------- */

ENTRY(op_TAX) // 0xaa
                mov     X_Reg, A_Reg
                mov     r0, A_Reg, LSL #24
                orrs    r0, r0, r0
                FlagNZ
                Continue

/* ----------------------------------
       TAY instruction
   ---------------------------------- */

ENTRY(op_TAY) // 0xa8
                mov     Y_Reg, A_Reg
                mov     r0, A_Reg, LSL #24
                orrs    r0, r0, r0
                FlagNZ
                Continue

/* ----------------------------------
       TRB instructions
       65c02 only
   ---------------------------------- */

// 65c02 : 0x1C
ENTRY(op_TRB_abs)
                GetAbs
                DoTRB
                Continue

// 65c02 : 0x14
ENTRY(op_TRB_zpage)
                GetZPage
                DoTRB
                Continue

/* ----------------------------------
       TSB instructions
       65c02 only
   ---------------------------------- */

// 65c02 : 0x0C
ENTRY(op_TSB_abs)
                GetAbs
                DoTSB
                Continue

// 65c02 : 0x04
ENTRY(op_TSB_zpage)
                GetZPage
                DoTSB
                Continue

/* ----------------------------------
       TSX instruction
   ---------------------------------- */

ENTRY(op_TSX) // 0xba
                mov     X_Reg, SP_Reg
                mov     r0, SP_Reg, LSL #24
                orrs    r0, r0, r0
                FlagNZ
                Continue

/* ----------------------------------
       TXA instruction
   ---------------------------------- */

ENTRY(op_TXA) // 0x8a
                mov     A_Reg, X_Reg
                mov     r0, X_Reg, LSL #24
                orrs    r0, r0, r0
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
                mov     r0, Y_Reg, LSL #24
                orrs    r0, r0, r0
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

#define cycles_exe r0
continue:
                ldr     r1, SYM(cpu65__opcycles)
                ldr     r0, SYM(cpu65_opcode)
                ldrb    r0, [r0]
                ldrb    cycles_exe, [r1, r0]
                ldr     r1, SYM(cpu65_opcycles)
                ldrb    r2, [r1]
                add     cycles_exe, cycles_exe, r2
                strb    cycles_exe, [r1]
                TRACE_EPILOGUE

                ldr     r1, SYM(gc_cycles_timer_0)
                ldr     r2, [r1]
                sub     r2, r2, cycles_exe
                str     r2, [r1]

                ldr     r1, SYM(gc_cycles_timer_1)
                ldr     r2, [r1]
                sub     r2, r2, cycles_exe
                str     r2, [r1]

                ldr     r1, SYM(cpu65_cycle_count)
                ldr     r2, [r1]
                add     r2, r2, cycles_exe
                str     r2, [r1]

                ldr     r1, SYM(cpu65_cycles_to_execute)
                ldr     r2, [r1]
                subs    r2, r2, cycles_exe
                str     r2, [r1]
                bmi     exit_cpu65_run
                beq     exit_cpu65_run

continue1:      eor     r0, r0, r0
                ldr     r1, SYM(cpu65__signal)
                ldrb    r0, [r1]
                orr     r0, r0, r0
                bne     exception
                JumpNextInstruction

/* -------------------------------------------------------------------------
        Exception handlers
   ------------------------------------------------------------------------- */

exception:      tst     r0, #ResetSig
                beq     ex_irq
                ldr     r1, SYM(joy_button0)                // OpenApple
                ldrb    r0, [r1]
                tst     r0, #0xFF
                bne     exit_reinit
                ldr     r1, SYM(joy_button1)                // ClosedApple
                ldrb    r0, [r1]
                tst     r0, #0xFF
                bne     exit_reinit

ex_reset:       mov     r0, #0
                ldr     r1, SYM(cpu65__signal)
                strb    r0, [r1]
                ldr     EffectiveAddr, SYM(reset_vector)
                ldrh    EffectiveAddr, [EffectiveAddr]
                GetFromEA_W
                mov     PC_Reg, r0
                JumpNextInstruction

ex_irq:         tst     F_Reg, #I_Flag                      // Already interrupted?
                beq     1f
                JumpNextInstruction
1:              mov     r0, PC_Reg
                mov     r0, r0, ROR #8
                Push(r0)
                mov     r0, r0, LSR #24
                Push(r0)
                orr     F_Reg, F_Reg, #X_Flag
                EncodeFlags
                Push(r0)
                orr     F_Reg, F_Reg, #BI_Flags
                //bic   F_Reg, F_Reg, #D_Flag // AppleWin clears Decimal bit?
                ldr     EffectiveAddr, SYM(interrupt_vector)
                ldrh    EffectiveAddr, [EffectiveAddr]
                GetFromEA_W
                mov     PC_Reg, r0
                JumpNextInstruction

/* -------------------------------------------------------------------------
        65c02 CPU processing loop entry point
   ------------------------------------------------------------------------- */

ENTRY(cpu65_run)
                // Restore CPU state when being called from C.
#warning FIXME TODO ... do all the ldrb's zero-out the high-24bits of the destination reg?
                ldr     r1, SYM(cpu65_ea)
                ldrh    EffectiveAddr, [r1]
                ldr     r1, SYM(cpu65_pc)
                ldrh    PC_Reg, [r1]
                ldr     r1, SYM(cpu65_a)
                ldrb    A_Reg, [r1]
                ldr     r1, SYM(cpu65_f)
                ldrb    r0, [r1]
                DecodeFlags
                ldr     r1, SYM(cpu65_x)
                ldrb    X_Reg, [r1]
                ldr     r1, SYM(cpu65_y)
                ldrb    Y_Reg, [r1]
                ldr     r1, SYM(cpu65_sp)
                ldrb    SP_Reg, [r1]
                ldr     r1, SYM(emul_reinitialize)
                ldrb    r0, [r1]
                tst     r0, #0
                movne   r0, #0
                strneb  r0, [r1]
                bne     ex_reset
                b       continue1

/* -------------------------------------------------------------------------
        65c02 CPU processing loop exit point
   ------------------------------------------------------------------------- */

exit_cpu65_run:
                // Save CPU state when returning from being called from C
                ldr     r1, SYM(cpu65_pc)
                strh    PC_Reg, [r1]
                CommonSaveCPUState
                ret

exit_reinit:    ldr     r1, SYM(cpu65__signal)
                mov     r0, #0
                strb    r0, [r1]
                ldr     r1, SYM(emul_reinitialize)
                mov     r0, #1
                strb    r0, [r1]
                ret

/* -------------------------------------------------------------------------
        Debugger hooks
   ------------------------------------------------------------------------- */

ENTRY(cpu65_direct_write)
#warning FIXME TODO cpu65_direct_write should be implemented in plain C ...
                ret

