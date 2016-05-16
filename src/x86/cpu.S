/*
 * Apple // emulator for *ix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 1994 Alexander Jean-Claude Bottema
 * Copyright 1995 Stephen Lee
 * Copyright 1997, 1998 Aaron Culliney
 * Copyright 1998, 1999, 2000 Michael Deutschmann
 * Copyright 2013-2015 Aaron Culliney
 *
 */

#include "cpu-regs.h"
#include "vm.h"

#if !__PIC__ || (__APPLE__ && !__LP64__)
#   define SAVE_Y_REG() \
                REG2MEM(movb, Y_Reg, cpu65_y)
#   define _POP_PICREG()
#else
#   define SAVE_Y_REG() \
                movb    Y_Reg, %al; \
                REG2MEM(movb, %al, cpu65_y)
#   if __i386__
#       define _A2_PIC_GOT(reg) 0x0(reg) // Assumes GOT pointer is at head of stack
#       define _POP_PICREG() popl _PICREG
#   else
#       define _POP_PICREG()
#   endif
#endif

#define CommonSaveCPUState \
                REG2MEM(movw, EffectiveAddr, cpu65_ea); \
                REG2MEM(movb, A_Reg, cpu65_a); \
                xorw    %ax, %ax; \
                movb    F_Reg, %al; \
                MEM2REG_IND(movb,cpu65_flags_encode,_XAX,1,%al); \
                REG2MEM(movb, %al, cpu65_f); \
                REG2MEM(movb, X_Reg, cpu65_x); \
                SAVE_Y_REG(); \
                REG2MEM(movb, SP_Reg_L, cpu65_sp)

#if CPU_TRACING
#   define TRACE_PROLOGUE \
                REG2MEM(movw, PC_Reg, cpu65_pc); \
                CALL_FN(callLQ, cpu65_trace_prologue, 0x4);
#   define TRACE_ARG \
                CALL_FN(callLQ, cpu65_trace_arg, 0x4);
#   define TRACE_ARG1 \
                CALL_FN(callLQ, cpu65_trace_arg1, 0x4);
#   define TRACE_ARG2 \
                CALL_FN(callLQ, cpu65_trace_arg2, 0x4);
#   define TRACE_EPILOGUE \
                pushLQ _XAX; \
                CommonSaveCPUState; \
                popLQ _XAX; \
                CALL_FN(callLQ, cpu65_trace_epilogue, 0x4);
#else
#   define TRACE_PROLOGUE
#   define TRACE_ARG
#   define TRACE_ARG1
#   define TRACE_ARG2
#   define TRACE_EPILOGUE
#endif

/* -------------------------------------------------------------------------
    CPU (6502) Helper Routines
   ------------------------------------------------------------------------- */

#define GetFromPC_B \
                movLQ   PC_Reg_X, EffectiveAddr_X; \
                incw    PC_Reg; \
                CALL_IND(cpu65_vmem_r,EffectiveAddr_X,SZ_PTR); \
                TRACE_ARG;

#define GetFromPC_W \
                movLQ   PC_Reg_X, EffectiveAddr_X; \
                incw    EffectiveAddr; \
                addw    $2, PC_Reg; \
                CALL_IND(cpu65_vmem_r,EffectiveAddr_X,SZ_PTR); \
                decw    EffectiveAddr; \
                TRACE_ARG2; \
                movb    %al, %ah; \
                CALL_IND(cpu65_vmem_r,EffectiveAddr_X,SZ_PTR); \
                TRACE_ARG1;

#define JumpNextInstruction \
                TRACE_PROLOGUE; \
                GetFromPC_B \
                REG2MEM(movb, %al, cpu65_opcode); \
                REG2MEM(movb, $0, cpu65_opcycles); \
                REG2MEM(movb, $0, cpu65_rw); \
                JUMP_IND(cpu65__opcodes,_XAX,SZ_PTR);

#define GetFromEA_B \
                REG2MEM(orb, $1, cpu65_rw); \
                CALL_IND(cpu65_vmem_r,EffectiveAddr_X,SZ_PTR);

#define GetFromEA_W \
                incw    EffectiveAddr; \
                CALL_IND(cpu65_vmem_r,EffectiveAddr_X,SZ_PTR); \
                decw    EffectiveAddr; \
                movb    %al, %ah; \
                CALL_IND(cpu65_vmem_r,EffectiveAddr_X,SZ_PTR);

#define PutToEA_B \
                REG2MEM(orb, $2, cpu65_rw); \
                REG2MEM(movb, %al, cpu65_d); \
                CALL_IND(cpu65_vmem_w,EffectiveAddr_X,SZ_PTR);

#define GetFromMem_B(x) \
                movLQ   x, EffectiveAddr_X; \
                CALL_IND(cpu65_vmem_r,EffectiveAddr_X,SZ_PTR);

#define GetFromMem_W(x) \
                movLQ   x, EffectiveAddr_X; \
                GetFromEA_W

#define Continue \
                jmp     continue;

#define BranchXCycles \
                _2MEM(incb, cpu65_opcycles); /* +1 branch taken */ \
                shlLQ   $16, _XBX; \
                movw    PC_Reg, %bx; \
                cbw; \
                addw    %bx, %ax; \
                movw    %ax, PC_Reg; \
                cmpb    %ah, %bh; \
                je      9f; \
                _2MEM(incb, cpu65_opcycles); /* +1 branch new page */ \
9:              shrLQ   $16, _XBX;

#define FlagC \
                lahf; \
                andb    $C_Flag, %ah; \
                andb    $~C_Flag, F_Reg; \
                orb     %ah, F_Reg;

#define FlagZ \
                lahf; \
                andb    $Z_Flag, %ah; \
                andb    $~Z_Flag, F_Reg; \
                orb     %ah, F_Reg;

#define FlagN \
                lahf; \
                andb    $N_Flag, %ah; \
                andb    $~N_Flag, F_Reg; \
                orb     %ah, F_Reg;

#define FlagNZ \
                lahf; \
                andb    $(N_Flag|Z_Flag), %ah; \
                andb    $~(N_Flag|Z_Flag), F_Reg; \
                orb     %ah, F_Reg;

#define FlagNZC \
                lahf; \
                andb    $(N_Flag|Z_Flag|C_Flag), %ah; \
                andb    $~(N_Flag|Z_Flag|C_Flag), F_Reg; \
                orb     %ah, F_Reg;

                /* Have to do things a little differently since
                 * overflow is not read by the LAHF instruction
                 *
                 * Use of long operands wastes two bytes on the AND
                 * constant, but saves three instruction prefixes.
                 * This doesn't affect the cycle count.
                 */
#define FlagNVZC \
                pushfLQ; \
                popLQ   _XAX; \
                andLQ   $(N_Flag|(V_Flag<<8)|Z_Flag|C_Flag),_XAX; \
                andb    $~(N_Flag|V_Flag|Z_Flag|C_Flag), F_Reg; \
                orb     %ah, F_Reg; \
                orb     %al, F_Reg;

#define Push(x) \
                REG2MEM_IND(movb,x,apple_ii_64k,SP_Reg_X,1); \
                decb    SP_Reg_L;

#define Pop(x) \
                incb    SP_Reg_L; \
                MEM2REG_IND(movb,apple_ii_64k,SP_Reg_X,1,x);

/* Immediate Addressing - the operand is contained in the second byte of the
   instruction. */
#define _GetImm \
                movLQ   PC_Reg_X, EffectiveAddr_X; \
                incw    PC_Reg;
#if CPU_TRACING
#define GetImm \
                _GetImm \
                CALL_IND(cpu65_vmem_r,EffectiveAddr_X,SZ_PTR); \
                TRACE_ARG;
#else
#define GetImm \
                _GetImm
#endif

/* Absolute Addressing - the second byte of the instruction is the low
   order address, and the third byte is the high order byte. */
#define GetAbs  \
                GetFromPC_W; \
                movLQ   _XAX, EffectiveAddr_X;

/* Zero Page Addressing - the second byte of the instruction is an
   address on the zero page */
#define GetZPage \
                GetFromPC_B; \
                movLQ   _XAX, EffectiveAddr_X;

/* Zero Page Indexed Addressing - The effective address is calculated by
   adding the second byte to the contents of the index register.  Due
   to the zero page addressing nature of this mode, no carry is added
   to the high address byte, and the crossing of page boundaries does
   not occur. */
#define GetZPage_X \
                GetFromPC_B; \
                addb    X_Reg, %al; \
                movLQ   _XAX, EffectiveAddr_X;

#define GetZPage_Y \
                GetFromPC_B; \
                addb    Y_Reg, %al; \
                movLQ   _XAX, EffectiveAddr_X;

/* Absolute Indexed Addressing - The effective address is formed by
   adding the contents of X or Y to the address contained in the
   second and third bytes of the instruction. */
#define _GetAbs_X \
                GetFromPC_W; \
                addb    X_Reg, %al; \
                jnc     9f; \
                adcb    $0, %ah;

#define GetAbs_X \
                _GetAbs_X \
                _2MEM(incb, cpu65_opcycles); /* +1 cycle on page boundary */ \
9:              movLQ   _XAX, EffectiveAddr_X;

#define GetAbs_X_STx \
                _GetAbs_X \
9:              movLQ   _XAX, EffectiveAddr_X;

#define _GetAbs_Y \
                GetFromPC_W; \
                addb    Y_Reg, %al; \
                jnc     9f; \
                adcb    $0, %ah;

#define GetAbs_Y \
                _GetAbs_Y \
                _2MEM(incb, cpu65_opcycles); /* +1 cycle on page boundary */ \
9:              movLQ   _XAX, EffectiveAddr_X;

#define GetAbs_Y_STA \
                _GetAbs_Y \
9:              movLQ   _XAX, EffectiveAddr_X;

/* Zero Page Indirect Addressing (65c02) - The second byte of the
   instruction points to a memory location on page zero containing the
   low order byte of the effective address.  The next location on page
   zero contains the high order byte of the address. */
#define GetIndZPage \
                GetFromPC_B; \
                incb    %al; \
                movLQ   _XAX, EffectiveAddr_X; \
                GetFromEA_B; \
                movb    %al, %ah; \
                decLQ   EffectiveAddr_X; \
                andLQ   $0xFF, EffectiveAddr_X; \
                GetFromEA_B; \
                movLQ   _XAX, EffectiveAddr_X;

/* Zero Page Indexed Indirect Addressing - The second byte is added to
   the contents of the X index register; the carry is discarded.  The
   result of this addition points to a memory location on page zero
   whose contents is the low order byte of the effective address.  The
   next memory location in page zero contains the high-order byte of
   the effective address.  Both memory locations specifying the high
   and low-order bytes must be in page zero. */
#define GetIndZPage_X \
                GetFromPC_B; \
                addb    X_Reg, %al; \
                incb    %al; \
                movLQ   _XAX, EffectiveAddr_X; \
                GetFromEA_B; \
                movb    %al, %ah; \
                decLQ   EffectiveAddr_X; \
                andLQ   $0xFF, EffectiveAddr_X; \
                GetFromEA_B; \
                movLQ   _XAX, EffectiveAddr_X;

/* Indirect Indexed Addressing - The second byte of the instruction
   points to a memory location in page zero.  The contents of this
   memory location are added to the contents of the Y index register,
   the result being the low order byte of the effective address.  The
   carry from this addition is added to the contents of the next page
   zero memory location, the result being the high order byte of the
   effective address. */
#define _GetIndZPage_Y \
                GetFromPC_B; \
                incb    %al; \
                movLQ   _XAX, EffectiveAddr_X; \
                GetFromEA_B; \
                movb    %al, %ah; \
                decLQ   EffectiveAddr_X; \
                andLQ   $0xFF, EffectiveAddr_X; \
                GetFromEA_B; \
                addb    Y_Reg, %al; \
                jnc 9f;

#define GetIndZPage_Y \
                _GetIndZPage_Y \
                adcb    $0, %ah; \
                _2MEM(incb, cpu65_opcycles); /* +1 cycle on page boundary */ \
9:              movLQ   _XAX, EffectiveAddr_X;

#define GetIndZPage_Y_STA \
                _GetIndZPage_Y \
                adcb    $0, %ah; \
9:              movLQ   _XAX, EffectiveAddr_X;

#define DoADC_b GetFromEA_B \
                bt      $C_Flag_Bit, AF_Reg_X; \
                adcb    %al, A_Reg; \
                FlagNVZC

#ifndef NDEBUG
#define DebugBCDCheck \
                testb   $0x80, A_Reg; \
                jz      6f; \
                testb   $0x60, A_Reg; \
                jz      6f; \
                CALL_FN(callLQ, debug_illegal_bcd, 0x4); \
6:              testb   $0x08, A_Reg; \
                jz      7f; \
                testb   $0x06, A_Reg; \
                jz      7f; \
                CALL_FN(callLQ, debug_illegal_bcd, 0x4); \
7:              testb   $0x80, %al; \
                jz      8f; \
                testb   $0x60, %al; \
                jz      8f; \
                CALL_FN(callLQ, debug_illegal_bcd, 0x4); \
8:              testb   $0x08, %al; \
                jz      9f; \
                testb   $0x06, %al; \
                jz      9f; \
                CALL_FN(callLQ, debug_illegal_bcd, 0x4); \
9:
#else
#define DebugBCDCheck
#endif

#define DoAND \
                GetFromEA_B \
                andb    %al, A_Reg; \
                FlagNZ

#define _DoASL(x) \
                addb    x, x; \
                FlagNZC

#define DoASL \
                GetFromEA_B \
                _DoASL(%al) \
                PutToEA_B

#define _DoBIT \
                GetFromEA_B \
                testb   %al, A_Reg;

/* SAR (and the following AND) effectively moves
 * bit 6 to Bit 3 while leaving Bit 7 unchanged */
#define DoBIT \
                _DoBIT \
                lahf; \
                sarb    $3, %al; \
                andw    $0x4088, %ax; \
                andb    $~(N_Flag|V_Flag|Z_Flag), F_Reg; \
                orb     %al, F_Reg; \
                orb     %ah, F_Reg;

#define DoCMP \
                GetFromEA_B \
                cmpb    %al, A_Reg; \
                cmc; \
                FlagNZC

#define DoCPX \
                GetFromEA_B \
                cmpb    %al, X_Reg; \
                cmc; \
                FlagNZC

#define DoCPY \
                GetFromEA_B \
                cmpb    %al, Y_Reg; \
                cmc; \
                FlagNZC

#define _DoDEC(x) \
                decb    x; \
                FlagNZ

#define DoDEC \
                GetFromEA_B \
                _DoDEC(%al) \
                PutToEA_B

#define DoEOR \
                GetFromEA_B \
                xorb    %al, A_Reg; \
                FlagNZ

#define _DoINC(x) \
                incb    x; \
                FlagNZ

#define DoINC \
                GetFromEA_B \
                _DoINC(%al) \
                PutToEA_B

#define DoLDA \
                GetFromEA_B \
                movb    %al, A_Reg; \
                orb     A_Reg, A_Reg; \
                FlagNZ

#define DoLDX \
                GetFromEA_B \
                movb    %al, X_Reg; \
                orb     X_Reg, X_Reg; \
                FlagNZ

#define DoLDY \
                GetFromEA_B \
                movb    %al, Y_Reg; \
                orb     Y_Reg, Y_Reg; \
                FlagNZ

#define _DoLSR(x) \
                shrb    $1, x; \
                FlagNZC

#define DoLSR \
                GetFromEA_B \
                _DoLSR(%al) \
                PutToEA_B

#define DoORA \
                GetFromEA_B \
                orb     %al, A_Reg; \
                FlagNZ

#define _DoROL(x) \
                bt      $C_Flag_Bit, AF_Reg_X; \
                adcb    x, x; \
                FlagNZC

#define DoROL \
                GetFromEA_B \
                _DoROL(%al) \
                PutToEA_B

#define DoROR \
                GetFromEA_B \
                movb    F_Reg, %ah; \
                rorLQ   $1, _XAX; \
                orb     %al, %al; \
                btr     $ROR_BIT, _XAX; \
                FlagNZC \
                PutToEA_B

#define DoSBC_b \
                GetFromEA_B \
                notb    %al; \
                bt      $C_Flag_Bit, AF_Reg_X; \
                adcb    %al, A_Reg; \
                FlagNVZC

#define DoSTA \
                movb    A_Reg, %al; \
                PutToEA_B

#define DoSTX \
                movb    X_Reg, %al; \
                PutToEA_B

#define DoSTY \
                movb    Y_Reg, %al; \
                PutToEA_B

#define DoSTZ \
                movb    $0x0, %al; \
                PutToEA_B

#define DoTRB \
                GetFromEA_B \
                testb   A_Reg, %al; \
                FlagZ \
                notb    A_Reg; \
                andb    A_Reg, %al; \
                notb    A_Reg; \
                PutToEA_B

#define DoTSB \
                GetFromEA_B \
                testb   A_Reg, %al; \
                FlagZ \
                orb     A_Reg, %al; \
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
                _2MEM(incb, cpu65_opcycles)         // +1 cycle
                GetFromEA_B
                DebugBCDCheck
                bt      $C_Flag_Bit, AF_Reg_X
                adcb    A_Reg, %al
#if !__LP64__
                daa
                movb    %al, A_Reg
                FlagNVZC
#else
                // DAA algorithm : http://www.ray.masmcode.com/BCDdaa.html
                // CF_old = CF
                // IF (al AND 0Fh > 9) or (the Auxiliary Flag is set)
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
                testb   $D_Flag, F_Reg;  /* Decimal mode? */ \
                JUMP_FN(jnz, op_ADC_dec) /* Yes, jump to decimal version */

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
                JUMP_FN(jmp, op_NOP)

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
                JUMP_FN(jmp, op_NOP)

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

/* ----------------------------------
       BCC instruction
       Branch on Carry Clear
   ---------------------------------- */

ENTRY(op_BCC) // 0x90
                GetFromPC_B
                testb   $C_Flag, F_Reg
                jnz     op_BCC_not              // branch not taken
                BranchXCycles
op_BCC_not:
                Continue

/* ----------------------------------
       BCS instruction
       Branch on Carry Set
   ---------------------------------- */

ENTRY(op_BCS) // 0xB0
                GetFromPC_B
                testb   $C_Flag, F_Reg
                jz      op_BCS_not              // branch not taken
                BranchXCycles
op_BCS_not:
                Continue

/* ----------------------------------
       BEQ instruction
       Branch if EQual
   ---------------------------------- */

ENTRY(op_BEQ) // 0xF0
                GetFromPC_B
                testb   $Z_Flag, F_Reg
                jz      op_BEQ_not              // branch not taken
                BranchXCycles
op_BEQ_not:
                Continue

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

/* ----------------------------------
       BMI instruction
       Branch on result MInus
   ---------------------------------- */

ENTRY(op_BMI) // 0x30
                GetFromPC_B
                testb   F_Reg, F_Reg    // check N flag, (which happens to be sign bit)
                jns     op_BMI_not
                BranchXCycles
op_BMI_not:
                Continue

/* ----------------------------------
       BNE instruction
       Branch on result Not Equal
   ---------------------------------- */

ENTRY(op_BNE) // 0xD0
                GetFromPC_B
                testb   $Z_Flag, F_Reg
                jnz     op_BNE_not
                BranchXCycles
op_BNE_not:
                Continue

/* ----------------------------------
       BPL instruction
       Branch on result PLus
   ---------------------------------- */

ENTRY(op_BPL) // 0x10
                GetFromPC_B
                testb   F_Reg, F_Reg    // check N flag, (which happens to be sign bit)
                js      op_BPL_not
                BranchXCycles
op_BPL_not:
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

/* ----------------------------------
       BRK instruction
   ---------------------------------- */

ENTRY(op_UNK) /* make undefined opcodes fault */
ENTRY(op_BRK)
                incw    PC_Reg
                movw    PC_Reg, %ax
#if __PIC__
                xchgb   %al, %ah
                Push(%al)
                shrw    $8, %ax
#else
                Push(%ah)
#endif
                Push(%al)
                orb     $(B_Flag|X_Flag), F_Reg
                xorw    %ax, %ax
                movb    F_Reg, %al
                MEM2REG_IND(movb,cpu65_flags_encode,_XAX,1,%al)
                Push(%al)
                orb     $I_Flag, F_Reg
                movw    $0xFFFE, EffectiveAddr              // ROM interrupt vector
                GetFromEA_W
                movw    %ax, PC_Reg
                Continue

/* ----------------------------------
       BVC instruction
       Branch on oVerflow Clear
   ---------------------------------- */

ENTRY(op_BVC) // 0x50
                GetFromPC_B
                testb   $V_Flag, F_Reg
                jnz     op_BVC_not
                BranchXCycles
op_BVC_not:
                Continue

/* ----------------------------------
       BVS instruction
       Branch on oVerflow Set
   ---------------------------------- */

ENTRY(op_BVS) // 0x70
                GetFromPC_B
                testb   $V_Flag, F_Reg
                jz      op_BVS_not
                BranchXCycles
op_BVS_not:
                Continue

/* ----------------------------------
       CLC instruction
   ---------------------------------- */

ENTRY(op_CLC) // 0x18
                andb    $~C_Flag, F_Reg
                Continue

/* ----------------------------------
       CLD instruction
   ---------------------------------- */

ENTRY(op_CLD) // 0xd8
                andb    $~D_Flag, F_Reg
                Continue

/* ----------------------------------
       CLI instruction
   ---------------------------------- */

ENTRY(op_CLI) // 0x58
                andb    $~I_Flag, F_Reg
                Continue

/* ----------------------------------
       CLV instruction
   ---------------------------------- */

ENTRY(op_CLV) // 0xB8
                andb    $~V_Flag, F_Reg
                Continue

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
                JUMP_FN(jmp, op_NOP)

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
                JUMP_FN(jmp, op_NOP)

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

/* ----------------------------------
       JMP instructions
       JuMP to new location
   ---------------------------------- */

ENTRY(op_JMP_abs)
                GetAbs
                movw    EffectiveAddr, PC_Reg;
                Continue

ENTRY(op_JMP_ind) // 0x6c
                GetFromPC_W
                cmpb    $0xFF, %al
                je      jmp_special
                GetFromMem_W(_XAX)
                movw    %ax, PC_Reg
                Continue
jmp_special: // see JMP indirect note in _Understanding the Apple IIe_ 4-25
                movw    %ax, PC_Reg
                subw    $0xFF, PC_Reg
                GetFromMem_B(PC_Reg_X)
                xchgb   %al, %ah
                addw    $0xFF, PC_Reg
                GetFromMem_B(PC_Reg_X)
                movw    %ax, PC_Reg
                Continue

// 65c02 : 0x7C
ENTRY(op_JMP_abs_ind_x)
                GetFromPC_W
                movw    %ax, EffectiveAddr
                movzbLQ X_Reg, _XAX
#warning HACK FIXME TODO : need to check the Bible here ... is this a signed addition (in which case we need to cbw)
                //cbw
                addw    %ax, EffectiveAddr
                GetFromMem_W(EffectiveAddr_X)
                movw    %ax, PC_Reg
                Continue

/* ----------------------------------
       JSR instruction
   ---------------------------------- */

ENTRY(op_JSR) // 0x20
                GetAbs
                movw    PC_Reg, %ax
                decw    %ax
#if __PIC__
                xchgb   %al, %ah
                Push(%al)
                shrw    $8, %ax
#else
                Push(%ah)
#endif
                Push(%al)
                movw    EffectiveAddr, PC_Reg
                Continue

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
                JUMP_FN(jmp, op_NOP)

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

/* ----------------------------------
       LSR instructions
   ---------------------------------- */

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
                JUMP_FN(jmp, op_NOP)

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
                movb    F_Reg, %al
                MEM2REG_IND(movb,cpu65_flags_encode,_XAX,1,%al)
                Push(%al)
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
#if __PIC__
                movb    Y_Reg, %al
                Push(%al)
#else
                Push(Y_Reg)
#endif
                Continue

/* ----------------------------------
       PLA instruction
   ---------------------------------- */

ENTRY(op_PLA) // 0x68
                Pop(A_Reg)
                orb     A_Reg, A_Reg
                FlagNZ
                Continue

/* ----------------------------------
       PLP instruction
   ---------------------------------- */

ENTRY(op_PLP) // 0x28
                Pop(%al)
#if __PIC__
                MEM2REG_IND(movb,cpu65_flags_decode,_XAX,1,%al)
                movb    %al, F_Reg
#else
                MEM2REG_IND(movb,cpu65_flags_decode,_XAX,1,F_Reg)
#endif
                orb     $(B_Flag|X_Flag), F_Reg
                Continue

/* ----------------------------------
       PLX instruction
       65c02 : 0xFA
   ---------------------------------- */

ENTRY(op_PLX)
                Pop(X_Reg)
                orb     X_Reg, X_Reg
                FlagNZ
                Continue

/* ----------------------------------
       PLY instruction
       65c02 : 0x7A
   ---------------------------------- */

ENTRY(op_PLY)
#if __PIC__
                Pop(%al)
                movb    %al, Y_Reg
#else
                Pop(Y_Reg)
#endif
                orb     Y_Reg, Y_Reg
                FlagNZ
                Continue

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
                // NB: assumes A_Reg = %cl, F_Reg = %ch
                rorw    $1, %cx         // Roll flags into accum
                adcb    F_Reg, F_Reg    // Roll carry into flags
                orb     A_Reg, A_Reg
                FlagNZ                  // implied C
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

/* ----------------------------------
       RTI instruction
   ---------------------------------- */

ENTRY(op_RTI) // 0x40
                Pop(%al)
#if __PIC__
                MEM2REG_IND(movb,cpu65_flags_decode,_XAX,1,%al)
                movb    %al, F_Reg
#else
                MEM2REG_IND(movb,cpu65_flags_decode,_XAX,1,F_Reg)
#endif
                orb     $(B_Flag|X_Flag), F_Reg
                Pop(%al)
#if __PIC__
                shlw    $8, %ax
                Pop(%al)
                xchgb   %al, %ah
#else
                Pop(%ah)
#endif
                movw    %ax, PC_Reg
                Continue

/* ----------------------------------
       RTS instruction
   ---------------------------------- */

ENTRY(op_RTS) // 0x60
                Pop(%al)
#if __PIC__
                shlw    $8, %ax
                Pop(%al)
                xchgb   %al, %ah
#else
                Pop(%ah)
#endif
                incw    %ax
                movw    %ax, PC_Reg
                Continue

/* ----------------------------------
       SBC instructions
       SuBtract memory from accumulator with Borrow
   ---------------------------------- */

ENTRY(op_SBC_dec)
                _2MEM(incb, cpu65_opcycles)         // +1 cycle
                GetFromEA_B
                DebugBCDCheck
                bt      $C_Flag_Bit, AF_Reg_X
                cmc
                xchgb   A_Reg, %al
#if !__LP64__
                sbbb    A_Reg, %al
                das
                movb    %al, A_Reg
                cmc
                FlagNVZC
#else
                sbbb    A_Reg, %al
                // DAS algorithm : http://www.ray.masmcode.com/BCDdas.html
                // CF_old = CF
                // IF (al AND 0Fh > 9) or (the Auxiliary Flag is set)
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
                testb   $D_Flag, F_Reg;  /* Decimal mode? */ \
                JUMP_FN(jnz, op_SBC_dec) /* Yes, jump to decimal version */

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
                JUMP_FN(jmp, op_NOP)

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

/* ----------------------------------
       SEC instruction
   ---------------------------------- */

ENTRY(op_SEC) // 0x38
                orb     $C_Flag, F_Reg
                Continue

/* ----------------------------------
       SED instruction
   ---------------------------------- */

ENTRY(op_SED) // 0xf8
                orb     $D_Flag, F_Reg
                Continue

/* ----------------------------------
       SEI instruction
   ---------------------------------- */

ENTRY(op_SEI) // 0x78
                orb     $I_Flag, F_Reg
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
                JUMP_FN(jmp, op_NOP)

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

/* ----------------------------------
       STX instructions
   ---------------------------------- */

ENTRY(op_STX_zpage) // 0x86
                GetZPage
                DoSTX
                Continue

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

/* ----------------------------------
       TAX instruction
   ---------------------------------- */

ENTRY(op_TAX) // 0xaa
                movb    A_Reg, X_Reg
                orb     X_Reg, X_Reg
                FlagNZ
                Continue

/* ----------------------------------
       TAY instruction
   ---------------------------------- */

ENTRY(op_TAY) // 0xa8
                movb    A_Reg, Y_Reg
                orb     Y_Reg, Y_Reg
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
                movb    SP_Reg_L, X_Reg
                orb     X_Reg, X_Reg
                FlagNZ
                Continue

/* ----------------------------------
       TXA instruction
   ---------------------------------- */

ENTRY(op_TXA) // 0x8a
                movb    X_Reg, A_Reg
                orb     A_Reg, A_Reg
                FlagNZ
                Continue

/* ----------------------------------
       TXS instruction
   ---------------------------------- */

ENTRY(op_TXS) // 0x9a
                movb    X_Reg, SP_Reg_L
                Continue

/* ----------------------------------
       TYA instruction
   ---------------------------------- */

ENTRY(op_TYA) // 0x98
                movb    Y_Reg, A_Reg
                orb     A_Reg, A_Reg
                FlagNZ
                Continue

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

continue:
                MEM2REG(movzbLQ, cpu65_opcode, _XAX)
                MEM2REG_IND(movb,cpu65__opcycles,_XAX,1,%al)
                MEM2REG(addb, cpu65_opcycles, %al)
                REG2MEM(movb, %al, cpu65_opcycles)
                TRACE_EPILOGUE
                REG2MEM(addl, %eax, cpu65_cycle_count)
                REG2MEM(subl, %eax, gc_cycles_timer_0)
                REG2MEM(subl, %eax, gc_cycles_timer_1)
                REG2MEM(subl, %eax, cpu65_cycles_to_execute)
                jle     exit_cpu65_run

continue1:      xorLQ   _XAX, _XAX
                MEM2REG(orb, cpu65__signal, %al)
                jnz     exception
                JumpNextInstruction

/* -------------------------------------------------------------------------
        Exception handlers
   ------------------------------------------------------------------------- */

exception:      testb   $ResetSig, %al
                jz      ex_irq
                REG2MEM(testb, $0xff, joy_button0)             // OpenApple
                jnz     exit_reinit
                REG2MEM(testb, $0xff, joy_button1)             // ClosedApple
                jnz     exit_reinit

ex_reset:       REG2MEM(movb, $0, cpu65__signal)
                movw    $0xFFFC, EffectiveAddr              // ROM reset vector
                GetFromEA_W
                movw    %ax, PC_Reg
                xorb    %ah, %ah
                JumpNextInstruction

ex_irq:         testb   $I_Flag, F_Reg                      // Already interrupted?
                jz      1f
                JumpNextInstruction                         // Yes (ignored) ...
1:              movw    PC_Reg, %ax                         // No (handle IRQ) ...
#if __PIC__
                xchgb   %al, %ah
                Push(%al)
                shrw    $8, %ax
#else
                Push(%ah)
#endif
                Push(%al)
                orb     $X_Flag, F_Reg
                xorw    %ax, %ax
                movb    F_Reg, %al
                MEM2REG_IND(movb,cpu65_flags_encode,_XAX,1,%al)
                Push(%al)
                orb     $(B_Flag | I_Flag), F_Reg
                //andb    $~D_Flag, F_Reg // AppleWin clears Decimal bit?
                movLQ   $0xFFFE, EffectiveAddr_X// HACK FIXME : there is a bug somewhere that is occasionally corrupting EffectiveAddr_X
                GetFromEA_W
                movw    %ax, PC_Reg
                xorb    %ah, %ah
                JumpNextInstruction

/* -------------------------------------------------------------------------
        65c02 CPU processing loop entry point
   ------------------------------------------------------------------------- */

ENTRY(cpu65_run)
                pushLQ  _XBP
                movLQ   _XSP, _XBP
                pushLQ  _XDI
                pushLQ  _XSI
                pushLQ  _XBX
#if __LP64__
                // NOTE: should we be also preserving r12-r15?
#endif

#if __PIC__ && __i386__ && !__APPLE__
                calll   .Lget_pc_thunk0
.Lget_pc_thunk0:
                _POP_PICREG()
.Lget_pc_thunk1:
                addl    $_GLOBAL_OFFSET_TABLE_+(.Lget_pc_thunk1 - .Lget_pc_thunk0), _PICREG
                pushl   _PICREG
#endif

                // Restore CPU state when being called from C.
                MEM2REG(movzwLQ, cpu65_ea, EffectiveAddr_X)
                MEM2REG(movzwLQ, cpu65_pc, PC_Reg_X)
                MEM2REG(movzbLQ, cpu65_a, AF_Reg_X)
                MEM2REG(movzbLQ, cpu65_x, XY_Reg_X)
                MEM2REG(movzbLQ, cpu65_f, _XAX)
#if __PIC__
                MEM2REG_IND(movb,cpu65_flags_decode,_XAX,1,%al)
                movb    %al, F_Reg
                MEM2REG(movb, cpu65_y, %al)
                movb    %al, Y_Reg
#else
                MEM2REG_IND(movb,cpu65_flags_decode,_XAX,1,F_Reg)
                MEM2REG(movb, cpu65_y, Y_Reg)
#endif
                MEM2REG(movzbLQ, cpu65_sp, SP_Reg_X)
#ifdef APPLE2_VM
                RestoreAltZP
#endif
                REG2MEM(cmpb, $0, emul_reinitialize)
                jnz     enter_reinit
                jmp     continue1

enter_reinit:   REG2MEM(movb, $0, emul_reinitialize)
                jmp     ex_reset

/* -------------------------------------------------------------------------
        65c02 CPU processing loop exit point
   ------------------------------------------------------------------------- */

exit_cpu65_run:
                // Save CPU state when returning from being called from C
                REG2MEM(movw, PC_Reg, cpu65_pc)
                CommonSaveCPUState
                _POP_PICREG()
                popLQ   _XBX
                popLQ   _XSI
                popLQ   _XDI
                popLQ   _XBP
                ret

exit_reinit:    REG2MEM(movb, $0, cpu65__signal)
                REG2MEM(movb, $1, emul_reinitialize)
                _POP_PICREG()
                popLQ   _XBX
                popLQ   _XSI
                popLQ   _XDI
                popLQ   _XBP
                ret

/* -------------------------------------------------------------------------
        Debugger hooks
   ------------------------------------------------------------------------- */

ENTRY(cpu65_direct_write)
                pushLQ  EffectiveAddr_X
                movLQ   8(_XSP),EffectiveAddr_X
                movLQ   12(_XSP),_XAX
                CALL_IND(cpu65_vmem_w,EffectiveAddr_X,SZ_PTR)
                popLQ   EffectiveAddr_X
                ret

