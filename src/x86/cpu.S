/*
 * Apple // emulator for Linux: Virtual 6502/65C02
 *
 * Copyright 1994 Alexander Jean-Claude Bottema
 * Copyright 1995 Stephen Lee
 * Copyright 1997, 1998 Aaron Culliney
 * Copyright 1998, 1999, 2000 Michael Deutschmann
 *
 * This software package is subject to the GNU General Public License
 * version 2 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * THERE ARE NO WARRANTIES WHATSOEVER.
 *
 */

#include "apple2.h"
#include "cpu-regs.h"
#include "misc.h"

#define DebugCurrEA     SN(cpu65_debug)
#define DebugCurrByte   SN(cpu65_debug)+2
#define DebugCurrRW     SN(cpu65_debug)+3
#define DebugCurrOpcode SN(cpu65_debug)+4
#define DebugCycleCount SN(cpu65_debug)+5

/* -------------------------------------------------------------------------
    CPU (6502) Helper Routines
   ------------------------------------------------------------------------- */

#define GetFromPC_B \
                movl    PC_Reg_X, EffectiveAddr_X; \
                incw    PC_Reg; \
                call    *SN(cpu65_vmem)(,EffectiveAddr_X,SZ_PTR*2);

#define GetFromPC_W \
                movl    PC_Reg_X, EffectiveAddr_X; \
                incw    EffectiveAddr; \
                addw    $2, PC_Reg; \
                call    *SN(cpu65_vmem)(,EffectiveAddr_X,SZ_PTR*2); \
                decw    EffectiveAddr; \
                movb    %al, %ah; \
                call    *SN(cpu65_vmem)(,EffectiveAddr_X,SZ_PTR*2);

#define JumpNextInstruction \
                GetFromPC_B \
                movb    %al, DebugCurrOpcode; \
                movb    $0, DebugCycleCount; \
                movb    $0, DebugCurrRW; \
                jmp     *cpu65__opcodes(,_XAX,SZ_PTR);

#define GetFromEA_B \
                orb     $1, DebugCurrRW; \
                call    *SN(cpu65_vmem)(,EffectiveAddr_X,SZ_PTR*2);

#define GetFromEA_W \
                incw    EffectiveAddr; \
                call    *SN(cpu65_vmem)(,EffectiveAddr_X,SZ_PTR*2); \
                decw    EffectiveAddr; \
                movb    %al, %ah; \
                call    *SN(cpu65_vmem)(,EffectiveAddr_X,SZ_PTR*2);

#define PutToEA_B \
                orb     $2, DebugCurrRW; \
                movb    %al, DebugCurrByte; \
                call    *SN(cpu65_vmem)+4(,EffectiveAddr_X,SZ_PTR*2);

#define GetFromMem_B(x) \
                movl    x, EffectiveAddr_X; \
                call    *SN(cpu65_vmem)(,EffectiveAddr_X,SZ_PTR*2);

#define GetFromMem_W(x) \
                movl    x, EffectiveAddr_X; \
                incw    EffectiveAddr; \
                call    *SN(cpu65_vmem)(,EffectiveAddr_X,SZ_PTR*2); \
                decw    EffectiveAddr; \
                movb    %al, %ah; \
                call    *SN(cpu65_vmem)(,EffectiveAddr_X,SZ_PTR*2);

#define Continue \
                jmp     continue;

/*
 * Save CPU state when returning from being called from C
 */
#define SaveState \
                xorl    _XAX, _XAX; \
                movw    EffectiveAddr, DebugCurrEA; \
                movw    PC_Reg, SN(cpu65_current); \
                movb    A_Reg, SN(cpu65_current)+2; \
                movb    F_Reg, %al; \
                movb    SN(cpu65_flags_encode)(,_XAX,1), %al; \
                movb    %al, SN(cpu65_current)+3; \
                movb    X_Reg, SN(cpu65_current)+4; \
                movb    Y_Reg, SN(cpu65_current)+5; \
                movb    SP_Reg_L, SN(cpu65_current)+6;

/* Restore CPU state when being called from C.
 *
 * The xorls clear the high parts of the registers
 * Note: dependent on register assignment
 *
 * The extra bit at the end points the stack pointer at the alternate
 * stack in ALTZP mode.  (Note, this is not good -- I'd prefer to avoid
 * polluting this module with Apple-specific stuff.  But we need to do
 * it, else aux-stack using programs will crash when debugged.)
 */
#define RestoreState \
                movl    $0x0100, SP_Reg_X; \
                movzwl  DebugCurrEA, EffectiveAddr_X; \
                movzwl  SN(cpu65_current), PC_Reg_X; \
                movzbl  SN(cpu65_current)+2, AF_Reg_X; \
                movzbl  SN(cpu65_current)+3, _XAX; \
                movb    SN(cpu65_flags_decode)(,_XAX,1), F_Reg; \
                movzbl  SN(cpu65_current)+4, XY_Reg_X; \
                movb    SN(cpu65_current)+5, Y_Reg; \
                movb    SN(cpu65_current)+6, SP_Reg_L; \
                movl    SN(base_stackzp), _XAX; \
                subl    $SN(apple_ii_64k), _XAX; /* $ for pointer addr */ \
                orl     _XAX, SP_Reg_X; \
                xorl    _XAX, _XAX;

#define BranchXCycles \
                incb    DebugCycleCount; /* +1 branch taken */ \
                pushl   %ebx; \
                movw    PC_Reg, %bx; \
                cbw; \
                addw    %bx, %ax; \
                movw    %ax, PC_Reg; \
                cmpb    %ah, %bh; \
                je      9f; \
                incb    DebugCycleCount; /* +1 branch new page */ \
9:              popl    %ebx;

#define FlagC   lahf; \
                andb    $C_Flag, %ah; \
                andb    $~C_Flag, F_Reg; \
                orb     %ah, F_Reg;

#define FlagZ   lahf; \
                andb    $Z_Flag, %ah; \
                andb    $~Z_Flag, F_Reg; \
                orb     %ah, F_Reg;

#define FlagN   lahf; \
                andb    $N_Flag, %ah; \
                andb    $~N_Flag, F_Reg; \
                orb     %ah, F_Reg;

#define FlagNZ  lahf; \
                andb    $(N_Flag|Z_Flag), %ah; \
                andb    $~(N_Flag|Z_Flag), F_Reg; \
                orb     %ah, F_Reg;

#define FlagNZC lahf; \
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
                pushfl; \
                popl    _XAX; \
                andl    $0x08C1,_XAX; \
                andb    $~(N_Flag|V_Flag|Z_Flag|C_Flag), F_Reg; \
                orb     %ah, F_Reg; \
                orb     %al, F_Reg;

#define Push(x) movb    x, SN(apple_ii_64k)(,SP_Reg_X,1); \
                decb    SP_Reg_L;

#define Pop(x)  incb    SP_Reg_L; \
                movb    SN(apple_ii_64k)(,SP_Reg_X,1), x;

/* Immediate Addressing - the operand is contained in the second byte of the
   instruction. */
#define GetImm  movl    PC_Reg_X, EffectiveAddr_X; \
                incw    PC_Reg;

/* Absolute Addressing - the second byte of the instruction is the low
   order address, and the third byte is the high order byte. */
#define GetAbs  GetFromPC_W; \
                movl    _XAX, EffectiveAddr_X;

/* Zero Page Addressing - the second byte of the instruction is an
   address on the zero page */
#define GetZPage \
                GetFromPC_B; \
                movl    _XAX, EffectiveAddr_X;

/* Zero Page Indexed Addressing - The effective address is calculated by
   adding the second byte to the contents of the index register.  Due
   to the zero page addressing nature of this mode, no carry is added
   to the high address byte, and the crossing of page boundaries does
   not occur. */
#define GetZPage_X \
                GetFromPC_B; \
                addb    X_Reg, %al; \
                movl    _XAX, EffectiveAddr_X;

// HACK IS THIS EVER USED?
#define GetZPage_Y \
                GetFromPC_B; \
                addb    Y_Reg, %al; \
                movl    _XAX, EffectiveAddr_X;

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
                incb    DebugCycleCount; /* +1 cycle on page boundary */ \
9:              movl    _XAX, EffectiveAddr_X;

#define GetAbs_X_STx \
                _GetAbs_X \
9:              movl    _XAX, EffectiveAddr_X;

#define _GetAbs_Y \
                GetFromPC_W; \
                addb    Y_Reg, %al; \
                jnc     9f; \
                adcb    $0, %ah;

#define GetAbs_Y \
                _GetAbs_Y \
                incb    DebugCycleCount; /* +1 cycle on page boundary */ \
9:              movl    _XAX, EffectiveAddr_X;

#define GetAbs_Y_STA \
                _GetAbs_Y \
9:              movl    _XAX, EffectiveAddr_X;

/* Absolute Indirect Addressing - The second and third bytes of the
   instruction are the low and high bytes of an address, respectively.
   The contents of the fully specified memory location is the
   low-order byte of the effective address.  The next memory location
   contains the high order byte of the effective address. */
/* (unused at the moment. It applies to JMP, but JMP's addressing is done
 * without the macro)
 */
#define GetInd  GetFromPC_W; \
                GetFromMem_W(_XAX)

/* Zero Page Indirect Addressing (65c02) - The second byte of the
   instruction points to a memory location on page zero containing the
   low order byte of the effective address.  The next location on page
   zero contains the high order byte of the address. */
#define GetIndZPage \
                GetFromPC_B; \
                incb    %al; \
                movl    _XAX, EffectiveAddr_X; \
                GetFromEA_B; \
                movb    %al, %ah; \
                decl    EffectiveAddr_X; \
                andl    $0xFF, EffectiveAddr_X; \
                GetFromEA_B; \
                movl    _XAX, EffectiveAddr_X;

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
                movl    _XAX, EffectiveAddr_X; \
                GetFromEA_B; \
                movb    %al, %ah; \
                decl    EffectiveAddr_X; \
                andl    $0xFF, EffectiveAddr_X; \
                GetFromEA_B; \
                movl    _XAX, EffectiveAddr_X;

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
                movl    _XAX, EffectiveAddr_X; \
                GetFromEA_B; \
                movb    %al, %ah; \
                decl    EffectiveAddr_X; \
                andl    $0xFF, EffectiveAddr_X; \
                GetFromEA_B; \
                addb    Y_Reg, %al; \
                jnc 9f;

#define GetIndZPage_Y \
                _GetIndZPage_Y \
                adcb    $0, %ah; \
                incb    DebugCycleCount; /* +1 cycle on page boundary */ \
9:              movl    _XAX, EffectiveAddr_X;

#define GetIndZPage_Y_STA \
                _GetIndZPage_Y \
                adcb    $0, %ah; \
9:              movl    _XAX, EffectiveAddr_X;

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
                call    SN(c_debug_illegal_bcd); \
6:              testb   $0x08, A_Reg; \
                jz      7f; \
                testb   $0x06, A_Reg; \
                jz      7f; \
                call    SN(c_debug_illegal_bcd); \
7:              testb   $0x80, %al; \
                jz      8f; \
                testb   $0x60, %al; \
                jz      8f; \
                call    SN(c_debug_illegal_bcd); \
8:              testb   $0x08, %al; \
                jz      9f; \
                testb   $0x06, %al; \
                jz      9f; \
                call    SN(c_debug_illegal_bcd); \
9:
#else
#define DebugBCDCheck
#endif

#define DoADC_d GetFromEA_B \
                DebugBCDCheck \
                bt      $C_Flag_Bit, AF_Reg_X; \
                adcb    A_Reg, %al; \
                daa; \
                movb    %al, A_Reg; \
                FlagNVZC

#define DoAND   GetFromEA_B \
                andb    %al, A_Reg; \
                FlagNZ

#define DoASL   GetFromEA_B \
                addb    %al, %al; \
                FlagNZC \
                PutToEA_B \

/* SAR (and the following AND) effectively moves
 * bit 6 to Bit 3 while leaving Bit 7 unchanged */
#define DoBIT   GetFromEA_B \
                testb   %al, A_Reg; \
                lahf; \
                sarb    $3, %al; \
                andw    $0x4088, %ax; \
                andb    $~(N_Flag|V_Flag|Z_Flag), F_Reg; \
                orb     %al, F_Reg; \
                orb     %ah, F_Reg;

#define DoCMP   GetFromEA_B \
                cmpb    %al, A_Reg; \
                cmc; \
                FlagNZC

#define DoCPX   GetFromEA_B \
                cmpb    %al, X_Reg; \
                cmc; \
                FlagNZC

#define DoCPY   GetFromEA_B \
                cmpb    %al, Y_Reg; \
                cmc; \
                FlagNZC

#define DoDEC   GetFromEA_B \
                decb    %al; \
                FlagNZ \
                PutToEA_B

#define DoEOR   GetFromEA_B \
                xorb    %al, A_Reg; \
                FlagNZ

#define DoINC   GetFromEA_B \
                incb    %al; \
                FlagNZ \
                PutToEA_B

#define DoLDA   GetFromEA_B \
                movb    %al, A_Reg; \
                orb     %al, %al; \
                FlagNZ

#define DoLDX   GetFromEA_B \
                movb    %al, X_Reg; \
                orb     %al, %al; \
                FlagNZ

#define DoLDY   GetFromEA_B \
                movb    %al, Y_Reg; \
                orb     %al, %al; \
                FlagNZ

#define DoLSR   GetFromEA_B \
                shrb    $1, %al; \
                FlagNZC \
                PutToEA_B

#define DoORA   GetFromEA_B \
                orb     %al, A_Reg; \
                FlagNZ

#define DoROL   GetFromEA_B \
                bt      $C_Flag_Bit, AF_Reg_X; \
                adcb    %al,%al; \
                FlagNZC \
                PutToEA_B

#define DoROR   GetFromEA_B \
                movb    F_Reg, %ah; \
                rorl    $1, _XAX; \
                orb     %al, %al; \
                btr     $31, _XAX; \
                FlagNZC \
                PutToEA_B

#define DoSBC_b GetFromEA_B \
                notb    %al; \
                bt      $C_Flag_Bit, AF_Reg_X; \
                adcb    %al, A_Reg; \
                FlagNVZC

#define DoSBC_d GetFromEA_B \
                DebugBCDCheck \
                bt      $C_Flag_Bit, AF_Reg_X; \
                cmc; \
                xchgb   A_Reg, %al; \
                sbbb    A_Reg, %al; \
                das; \
                movb    %al, A_Reg; \
                cmc; \
                FlagNVZC

#define DoSTA   movb    A_Reg, %al; \
                PutToEA_B

#define DoSTX   movb    X_Reg, %al; \
                PutToEA_B

#define DoSTY   movb    Y_Reg, %al; \
                PutToEA_B

#define DoSTZ   movb    $0x0, %al; \
                PutToEA_B

#define DoTRB   GetFromEA_B \
                testb   A_Reg, %al; \
                FlagZ \
                notb    A_Reg; \
                andb    A_Reg, %al; \
                notb    A_Reg; \
                PutToEA_B

#define DoTSB   GetFromEA_B \
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
E(op_ADC_dec)
                incb    DebugCycleCount         // +1 cycle
                DoADC_d
                Continue

E(op_ADC_imm) // 0x69
                GetImm
                testb   $D_Flag, F_Reg          // Decimal mode?
                jnz     op_ADC_dec              // Yes, jump to decimal version
                DoADC_b
                Continue

E(op_ADC_zpage) // 0x65
                GetZPage
                testb   $D_Flag, F_Reg          // Decimal mode?
                jnz     op_ADC_dec              // Yes, jump to decimal version
                DoADC_b
                Continue

E(op_ADC_zpage_x) // 0x75
                GetZPage_X
                testb   $D_Flag, F_Reg          // Decimal mode?
                jnz     op_ADC_dec              // Yes, jump to decimal version
                DoADC_b
                Continue

// UNIMPLEMENTED : W65C02S datasheet
E(op_ADC_zpage_y)
                jmp op_NOP

E(op_ADC_abs) // 0x6d
                GetAbs
                testb   $D_Flag, F_Reg          // Decimal mode?
                jnz     op_ADC_dec              // Yes, jump to decimal version
                DoADC_b
                Continue

E(op_ADC_abs_x) // 0x7d
                GetAbs_X
                testb   $D_Flag, F_Reg          // Decimal mode?
                jnz     op_ADC_dec              // Yes, jump to decimal version
                DoADC_b
                Continue

E(op_ADC_abs_y) // 0x79
                GetAbs_Y
                testb   $D_Flag, F_Reg          // Decimal mode?
                jnz     op_ADC_dec              // Yes, jump to decimal version
                DoADC_b
                Continue

E(op_ADC_ind_x) // 0x61
                GetIndZPage_X
                testb   $D_Flag, F_Reg          // Decimal mode?
                jnz     op_ADC_dec              // Yes, jump to decimal version
                DoADC_b
                Continue

E(op_ADC_ind_y) // 0x71
                GetIndZPage_Y
                testb   $D_Flag, F_Reg          // Decimal mode?
                jnz     op_ADC_dec              // Yes, jump to decimal version
                DoADC_b
                Continue

// 65c02 : 0x72
E(op_ADC_ind_zpage)
                GetIndZPage
                testb   $D_Flag, F_Reg          // Decimal mode?
                jnz     op_ADC_dec              // Yes, jump to decimal version
                DoADC_b
                Continue

/* ----------------------------------
       AND instructions
       logical AND memory with accumulator
   ---------------------------------- */

E(op_AND_imm) // 0x29
                GetImm
                DoAND
                Continue

E(op_AND_zpage) // 0x25
                GetZPage
                DoAND
                Continue

E(op_AND_zpage_x) // 0x35
                GetZPage_X
                DoAND
                Continue

// UNIMPLEMENTED : W65C02S datasheet
E(op_AND_zpage_y)
                jmp op_NOP

E(op_AND_abs) // 0x2d
                GetAbs
                DoAND
                Continue

E(op_AND_abs_x) // 0x3d
                GetAbs_X
                DoAND
                Continue

E(op_AND_abs_y) // 0x39
                GetAbs_Y
                DoAND
                Continue

E(op_AND_ind_x) // 0x21
                GetIndZPage_X
                DoAND
                Continue

E(op_AND_ind_y) // 0x31
                GetIndZPage_Y
                DoAND
                Continue

// 65c02 : 0x32
E(op_AND_ind_zpage)
                GetIndZPage
                DoAND
                Continue

/* ----------------------------------
       ASL instructions
       Arithmetic Shift one bit Left, memory or accumulator
   ---------------------------------- */

E(op_ASL_acc) // 0x0a
                addb    A_Reg, A_Reg
                FlagNZC
                Continue

E(op_ASL_zpage) // 0x06
                GetZPage
                DoASL
                Continue

E(op_ASL_zpage_x) // 0x16
                GetZPage_X
                DoASL
                Continue

E(op_ASL_abs) // 0x0e
                GetAbs
                DoASL
                Continue

E(op_ASL_abs_x) // 0x1e
                GetAbs_X
                DoASL
                Continue

/* ----------------------------------
       BBRx instructions
       UNIMPLEMENTED : These are documented in the W65C02S datasheet ...
       + 1 cycle  if branch within page
       + 2 cycles if branch across page boundary
   ---------------------------------- */

E(op_BBR0_65c02)
                Continue

E(op_BBR1_65c02)
                Continue

E(op_BBR2_65c02)
                Continue

E(op_BBR3_65c02)
                Continue

E(op_BBR4_65c02)
                Continue

E(op_BBR5_65c02)
                Continue

E(op_BBR6_65c02)
                Continue

E(op_BBR7_65c02)
                Continue

/* ----------------------------------
       BBSx instructions
       UNIMPLEMENTED : These are documented in the W65C02S datasheet ...
       + 1 cycle  if branch within page
       + 2 cycles if branch across page boundary
   ---------------------------------- */

E(op_BBS0_65c02)
                Continue

E(op_BBS1_65c02)
                Continue

E(op_BBS2_65c02)
                Continue

E(op_BBS3_65c02)
                Continue

E(op_BBS4_65c02)
                Continue

E(op_BBS5_65c02)
                Continue

E(op_BBS6_65c02)
                Continue

E(op_BBS7_65c02)
                Continue

/* ----------------------------------
       BCC instruction
       Branch on Carry Clear
   ---------------------------------- */

E(op_BCC) // 0x90
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

E(op_BCS) // 0xB0
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

E(op_BEQ) // 0xF0
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

E(op_BIT_zpage) // 0x24
                GetZPage
                DoBIT
                Continue

E(op_BIT_abs) // 0x2c
                GetAbs
                DoBIT
                Continue

// 65c02 : 0x34
E(op_BIT_zpage_x)
                GetZPage_X
                DoBIT
                Continue

// 65c02 : 0x3C
E(op_BIT_abs_x)
                GetAbs_X
                DoBIT
                Continue

                /* BIT immediate is anomalous in that it does not affect the
                 * N and V flags, unlike in other addressing modes.
                 */
// 65c02 : 0x89
E(op_BIT_imm)
                GetImm
                GetFromEA_B
                testb   %al, A_Reg
                FlagZ
                Continue

/* ----------------------------------
       BMI instruction
       Branch on result MInus
   ---------------------------------- */

E(op_BMI) // 0x30
                GetFromPC_B
                testb   F_Reg, F_Reg    /* optimized check of N flag,
                                         * which happens to be sign bit */
                jns     op_BMI_not
                BranchXCycles
op_BMI_not:
                Continue

/* ----------------------------------
       BNE instruction
       Branch on result Not Equal
   ---------------------------------- */

E(op_BNE) // 0xD0
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

E(op_BPL) // 0x10
                GetFromPC_B
                testb   F_Reg, F_Reg    /* optimized check of N flag,
                                         * which happens to be sign bit */
                js      op_BPL_not
                BranchXCycles
op_BPL_not:
                Continue

/* ----------------------------------
       BRA instruction
       BRanch Always
   ---------------------------------- */

// 65c02 : 0x80
E(op_BRA)
                GetFromPC_B
                BranchXCycles
                Continue

/* ----------------------------------
       BRK instruction
   ---------------------------------- */

E(op_UNK) /* make undefined opcodes fault */
E(op_BRK)
                incw    PC_Reg
                movw    PC_Reg, %ax
                Push(%ah)
                Push(%al)
                orb     $(B_Flag|X_Flag), F_Reg
                xorl    _XAX,_XAX
                movb    F_Reg, %al
                movb    SN(cpu65_flags_encode)(,_XAX,1), %al
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

E(op_BVC) // 0x50
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

E(op_BVS) // 0x70
                GetFromPC_B
                testb   $V_Flag, F_Reg
                jz      op_BVS_not
                BranchXCycles
op_BVS_not:
                Continue

/* ----------------------------------
       CLC instruction
   ---------------------------------- */

E(op_CLC) // 0x18
                andb    $~C_Flag, F_Reg
                Continue

/* ----------------------------------
       CLD instruction
   ---------------------------------- */

E(op_CLD) // 0xd8
                andb    $~D_Flag, F_Reg
                Continue

/* ----------------------------------
       CLI instruction
   ---------------------------------- */

E(op_CLI) // 0x58
                andb    $~I_Flag, F_Reg
                Continue

/* ----------------------------------
       CLV instruction
   ---------------------------------- */

E(op_CLV) // 0xB8
                andb    $~V_Flag, F_Reg
                Continue

/* ----------------------------------
       CMP instructions
       CoMPare memory and accumulator
   ---------------------------------- */

E(op_CMP_imm) // 0xc9
                GetImm
                DoCMP
                Continue

E(op_CMP_zpage) // 0xc5
                GetZPage
                DoCMP
                Continue

E(op_CMP_zpage_x) // 0xd5
                GetZPage_X
                DoCMP
                Continue

// UNIMPLEMENTED : W65C02S datasheet
E(op_CMP_zpage_y)
                jmp op_NOP

E(op_CMP_abs) // 0xcd
                GetAbs
                DoCMP
                Continue

E(op_CMP_abs_x) // 0xdd
                GetAbs_X
                DoCMP
                Continue

E(op_CMP_abs_y) // 0xd9
                GetAbs_Y
                DoCMP
                Continue

E(op_CMP_ind_x) // 0xc1
                GetIndZPage_X
                DoCMP
                Continue

E(op_CMP_ind_y) // 0xd1
                GetIndZPage_Y
                DoCMP
                Continue

// 65c02 : 0xD2
E(op_CMP_ind_zpage)
                GetIndZPage
                DoCMP
                Continue

/* ----------------------------------
       CPX instructions
       ComPare memory and X register
   ---------------------------------- */

E(op_CPX_imm) // 0xe0
                GetImm
                DoCPX
                Continue

E(op_CPX_zpage) // 0xe4
                GetZPage
                DoCPX
                Continue

E(op_CPX_abs) // 0xec
                GetAbs
                DoCPX
                Continue

/* ----------------------------------
       CPY instructions
       ComPare memory and Y register
   ---------------------------------- */

E(op_CPY_imm) // 0xc0
                GetImm
                DoCPY
                Continue

E(op_CPY_zpage) // 0xc4
                GetZPage
                DoCPY
                Continue

E(op_CPY_abs) // 0xcc
                GetAbs
                DoCPY
                Continue

/* ----------------------------------
       DEA: DEcrement Accumulator
   ---------------------------------- */

E(op_DEC_acc)
E(op_DEA) // 0x3A
                decb    A_Reg
                FlagNZ
                Continue

/* ----------------------------------
       DEC instructions
       DECrement memory or accumulator by one
   ---------------------------------- */

E(op_DEC_zpage) // 0xc6
                GetZPage
                DoDEC
                Continue

E(op_DEC_zpage_x) // 0xd6
                GetZPage_X
                DoDEC
                Continue

E(op_DEC_abs) // 0xce
                GetAbs
                DoDEC
                Continue

E(op_DEC_abs_x) // 0xde
                GetAbs_X
                DoDEC
                Continue

/* ----------------------------------
       DEX instruction
   ---------------------------------- */

E(op_DEX) // 0xca
                decb    X_Reg
                FlagNZ
                Continue

/* ----------------------------------
       DEY instruction
   ---------------------------------- */

E(op_DEY) // 0x88
                decb    Y_Reg
                FlagNZ
                Continue

/* ----------------------------------
       EOR instructions
       Exclusive OR memory with accumulator
   ---------------------------------- */

E(op_EOR_imm) // 0x49
                GetImm
                DoEOR
                Continue

E(op_EOR_zpage) // 0x45
                GetZPage
                DoEOR
                Continue

E(op_EOR_zpage_x) // 0x55
                GetZPage_X
                DoEOR
                Continue

// UNIMPLEMENTED : W65C02S datasheet
E(op_EOR_zpage_y)
                jmp op_NOP

E(op_EOR_abs) // 0x4d
                GetAbs
                DoEOR
                Continue

E(op_EOR_abs_x) // 0x5d
                GetAbs_X
                DoEOR
                Continue

E(op_EOR_abs_y) // 0x59
                GetAbs_Y
                DoEOR
                Continue

E(op_EOR_ind_x) // 0x41
                GetIndZPage_X
                DoEOR
                Continue

E(op_EOR_ind_y) // 0x51
                GetIndZPage_Y
                DoEOR
                Continue

// 65c02 : 0x52
E(op_EOR_ind_zpage)
                GetIndZPage
                DoEOR
                Continue

/* ----------------------------------
       INA : INcrement Accumulator
   ---------------------------------- */

E(op_INC_acc)
E(op_INA) // 0x1A
                incb    A_Reg
                FlagNZ
                Continue

/* ----------------------------------
       INC instructions
       INCrement memory
   ---------------------------------- */

E(op_INC_zpage) // 0xe6
                GetZPage
                DoINC
                Continue

E(op_INC_zpage_x) // 0xf6
                GetZPage_X
                DoINC
                Continue

E(op_INC_abs) // 0xee
                GetAbs
                DoINC
                Continue

E(op_INC_abs_x) // 0xfe
                GetAbs_X
                DoINC
                Continue

/* ----------------------------------
       INX instruction
   ---------------------------------- */

E(op_INX) // 0xe8
                incb    X_Reg
                FlagNZ
                Continue

/* ----------------------------------
       INY instruction
   ---------------------------------- */

E(op_INY) // 0xc8
                incb    Y_Reg
                FlagNZ
                Continue

/* ----------------------------------
       JMP instructions
       JuMP to new location
   ---------------------------------- */

E(op_JMP_abs)
                GetAbs
                movw    EffectiveAddr, PC_Reg;
                Continue

E(op_JMP_ind) // 0x6c
                xorl    _XAX, _XAX
                GetFromPC_B
                xchgb   %al, %ah
                GetFromPC_B
                xchgb   %al, %ah
                cmpb    $0xFF, %al
                je      jmp_special
                GetFromMem_W(_XAX)
                movw    %ax, PC_Reg
                Continue
jmp_special: // see JMP indirect note in _Understanding the Apple IIe_ 4-25
                movw    %ax, PC_Reg
                subw    $0xff, PC_Reg
                GetFromMem_B(PC_Reg_X)
                xchgb   %al, %ah
                addw    $0xff, PC_Reg
                GetFromMem_B(PC_Reg_X)
                movw    %ax, PC_Reg
                Continue

// 65c02 : 0x7C
E(op_JMP_abs_ind_x)
                GetFromPC_W
                movw    %ax, EffectiveAddr
                movzbl  X_Reg, _XAX
                addw    %ax, EffectiveAddr
                GetFromMem_W(EffectiveAddr_X)
                movw    %ax, PC_Reg
                Continue

/* ----------------------------------
       JSR instruction
   ---------------------------------- */

E(op_JSR) // 0x20
                GetAbs
                movw    PC_Reg, %ax
                decw    %ax
                Push(%ah)
                Push(%al)
                movw    EffectiveAddr, PC_Reg
                Continue

/* ----------------------------------
       LDA instructions
       LoaD Accumulator with memory
   ---------------------------------- */

E(op_LDA_imm) // 0xa9
                GetImm
                DoLDA
                Continue

E(op_LDA_zpage) // 0xa5
                GetZPage
                DoLDA
                Continue

E(op_LDA_zpage_x) // 0xb5
                GetZPage_X
                DoLDA
                Continue

// UNIMPLEMENTED : W65C02S datasheet
E(op_LDA_zpage_y)
                jmp op_NOP

E(op_LDA_abs) // 0xad
                GetAbs
                DoLDA
                Continue

E(op_LDA_abs_x) // 0xbd
                GetAbs_X
                DoLDA
                Continue

E(op_LDA_abs_y) // 0xb9
                GetAbs_Y
                DoLDA
                Continue

E(op_LDA_ind_x) // 0xa1
                GetIndZPage_X
                DoLDA
                Continue

E(op_LDA_ind_y) // 0xb1
                GetIndZPage_Y
                DoLDA
                Continue

// 65c02 : 0xB2
E(op_LDA_ind_zpage)
                GetIndZPage
                DoLDA
                Continue

/* ----------------------------------
       LDX instructions
   ---------------------------------- */

E(op_LDX_imm) // 0xa2
                GetImm
                DoLDX
                Continue

E(op_LDX_zpage) // 0xa6
                GetZPage
                DoLDX
                Continue

// HACK : is this used? need to study coverage ...
E(op_LDX_zpage_y) // 0xb6
                GetZPage_Y
                DoLDX
                Continue

E(op_LDX_abs) // 0xae
                GetAbs
                DoLDX
                Continue

E(op_LDX_abs_y) // 0xbe
                GetAbs_Y
                DoLDX
                Continue

/* ----------------------------------
       LDY instructions
   ---------------------------------- */

E(op_LDY_imm) // 0xa0
                GetImm
                DoLDY
                Continue

E(op_LDY_zpage) // 0xa4
                GetZPage
                DoLDY
                Continue

E(op_LDY_zpage_x) // 0xb4
                GetZPage_X
                DoLDY
                Continue

E(op_LDY_abs) // 0xac
                GetAbs
                DoLDY
                Continue

E(op_LDY_abs_x) // 0xbc
                GetAbs_X
                DoLDY
                Continue

/* ----------------------------------
       LSR instructions
   ---------------------------------- */

E(op_LSR_acc) // 0x4a
                shrb    $1, A_Reg
                FlagNZC
                Continue

E(op_LSR_zpage) // 0x46
                GetZPage
                DoLSR
                Continue

E(op_LSR_zpage_x) // 0x56
                GetZPage_X
                DoLSR
                Continue

E(op_LSR_abs) // 0x4e
                GetAbs
                DoLSR
                Continue

E(op_LSR_abs_x) // 0x5e
                GetAbs_X
                DoLSR
                Continue

/* ----------------------------------
       NOP instruction
   ---------------------------------- */

E(op_NOP) // 0xea
                Continue

/* ----------------------------------
       ORA instructions
   ---------------------------------- */

E(op_ORA_imm) // 0x09
                GetImm
                DoORA
                Continue

E(op_ORA_zpage) // 0x05
                GetZPage
                DoORA
                Continue

E(op_ORA_zpage_x) // 0x15
                GetZPage_X
                DoORA
                Continue

// UNIMPLEMENTED : W65C02S datasheet
E(op_ORA_zpage_y)
                jmp op_NOP

E(op_ORA_abs) // 0x0d
                GetAbs
                DoORA
                Continue

E(op_ORA_abs_x) // 0x1d
                GetAbs_X
                DoORA
                Continue

E(op_ORA_abs_y) // 0x19
                GetAbs_Y
                DoORA
                Continue

E(op_ORA_ind_x) // 0x01
                GetIndZPage_X
                DoORA
                Continue

E(op_ORA_ind_y) // 0x11
                GetIndZPage_Y
                DoORA
                Continue

// 65c02 : 0x12
E(op_ORA_ind_zpage)
                GetIndZPage
                DoORA
                Continue

/* ----------------------------------
       PHA instruction
   ---------------------------------- */

E(op_PHA) // 0x48
                Push(A_Reg)
                Continue

/* ----------------------------------
       PHP instruction
   ---------------------------------- */

E(op_PHP) // 0x08
                movb    F_Reg, %al
                movb    SN(cpu65_flags_encode)(,_XAX,1), %al
                Push(%al)
                Continue

/* ----------------------------------
       PHX instruction
       65c02 : 0xDA
   ---------------------------------- */

E(op_PHX)
                Push(X_Reg)
                Continue

/* ----------------------------------
       PHY instruction
       65c02 : 0x5A
   ---------------------------------- */

E(op_PHY)
                Push(Y_Reg)
                Continue

/* ----------------------------------
       PLA instruction
   ---------------------------------- */

E(op_PLA) // 0x68
                Pop(A_Reg)
                orb     A_Reg, A_Reg
                FlagNZ
                Continue

/* ----------------------------------
       PLP instruction
   ---------------------------------- */

E(op_PLP) // 0x28
                xorl    _XAX, _XAX
                Pop(%al)
                movb    SN(cpu65_flags_decode)(,_XAX,1), F_Reg
                orb     $(B_Flag|X_Flag), F_Reg
                Continue

/* ----------------------------------
       PLX instruction
       65c02 : 0xFA
   ---------------------------------- */

E(op_PLX)
                Pop(X_Reg)
                orb X_Reg, X_Reg
                FlagNZ
                Continue

/* ----------------------------------
       PLY instruction
       65c02 : 0x7A
   ---------------------------------- */

E(op_PLY)
                Pop(Y_Reg)
                orb Y_Reg, Y_Reg
                FlagNZ
                Continue

/* ----------------------------------
       ROL instructions
   ---------------------------------- */

E(op_ROL_acc) // 0x2a
                bt      $C_Flag_Bit, AF_Reg_X
                adcb    A_Reg, A_Reg
                FlagNZC
                Continue

E(op_ROL_zpage) // 0x26
                GetZPage
                DoROL
                Continue

E(op_ROL_zpage_x) // 0x36
                GetZPage_X
                DoROL
                Continue

E(op_ROL_abs) // 0x2e
                GetAbs
                DoROL
                Continue

E(op_ROL_abs_x) // 0x3e
                GetAbs_X
                DoROL
                Continue

/* ----------------------------------
       ROR instructions
   ---------------------------------- */
                /* NB: assumes A_Reg = %cl, F_Reg = %ch */
E(op_ROR_acc) // 0x6a
                rorw    $1, %cx         /* Roll flags into accum */
                adcb    F_Reg, F_Reg    /* Roll carry into flags */
                orb     A_Reg, A_Reg
                FlagNZ                  /* implied C */
                Continue

E(op_ROR_zpage) // 0x66
                GetZPage
                DoROR
                Continue

E(op_ROR_zpage_x) // 0x76
                GetZPage_X
                DoROR
                Continue

E(op_ROR_abs) // 0x6e
                GetAbs
                DoROR
                Continue

E(op_ROR_abs_x) // 0x7e
                GetAbs_X
                DoROR
                Continue

/* ----------------------------------
       RTI instruction
   ---------------------------------- */

E(op_RTI) // 0x40
                xorl    _XAX, _XAX
                Pop(%al)
                movb    SN(cpu65_flags_decode)(,_XAX,1), F_Reg
                orb     $(B_Flag|X_Flag), F_Reg
                Pop(%al)
                Pop(%ah)
                movw    %ax, PC_Reg
                Continue

/* ----------------------------------
       RTS instruction
   ---------------------------------- */

E(op_RTS) // 0x60
                Pop(%al)
                Pop(%ah)
                incw    %ax
                movw    %ax, PC_Reg
                Continue

/* ----------------------------------
       SBC instructions
       SuBtract memory from accumulator with Borrow
   ---------------------------------- */

E(op_SBC_dec)
                incb    DebugCycleCount         // +1 cycle
                DoSBC_d
                Continue

E(op_SBC_imm) // 0xe9
                GetImm
                testb   $D_Flag, F_Reg          // Decimal mode?
                jnz     op_SBC_dec              // Yes, jump to decimal version
                DoSBC_b
                Continue

E(op_SBC_zpage) // 0xe5
                GetZPage
                testb   $D_Flag, F_Reg          // Decimal mode?
                jnz     op_SBC_dec              // Yes, jump to decimal version
                DoSBC_b
                Continue

E(op_SBC_zpage_x) // 0xf5
                GetZPage_X
                testb   $D_Flag, F_Reg          // Decimal mode?
                jnz     op_SBC_dec              // Yes, jump to decimal version
                DoSBC_b
                Continue

// UNIMPLEMENTED : W65C02S datasheet
E(op_SBC_zpage_y)
                jmp op_NOP

E(op_SBC_abs) // 0xed
                GetAbs
                testb   $D_Flag, F_Reg          // Decimal mode?
                jnz     op_SBC_dec              // Yes, jump to decimal version
                DoSBC_b
                Continue

E(op_SBC_abs_x) // 0xfd
                GetAbs_X
                testb   $D_Flag, F_Reg          // Decimal mode?
                jnz     op_SBC_dec              // Yes, jump to decimal version
                DoSBC_b
                Continue

E(op_SBC_abs_y) // 0xf9
                GetAbs_Y
                testb   $D_Flag, F_Reg          // Decimal mode?
                jnz     op_SBC_dec              // Yes, jump to decimal version
                DoSBC_b
                Continue

E(op_SBC_ind_x) // 0xe1
                GetIndZPage_X
                testb   $D_Flag, F_Reg          // Decimal mode?
                jnz     op_SBC_dec              // Yes, jump to decimal version
                DoSBC_b
                Continue

E(op_SBC_ind_y) // 0xf1
                GetIndZPage_Y
                testb   $D_Flag, F_Reg          // Decimal mode?
                jnz     op_SBC_dec              // Yes, jump to decimal version
                DoSBC_b
                Continue

// 65c02 : 0xF2
E(op_SBC_ind_zpage)
                GetIndZPage
                testb   $D_Flag, F_Reg          // Decimal mode?
                jnz     op_SBC_dec              // Yes, jump to decimal version
                DoSBC_b
                Continue

/* ----------------------------------
       SEC instruction
   ---------------------------------- */

E(op_SEC) // 0x38
                orb     $C_Flag, F_Reg
                Continue

/* ----------------------------------
       SED instruction
   ---------------------------------- */

E(op_SED) // 0xf8
                orb     $D_Flag, F_Reg
                Continue

/* ----------------------------------
       SEI instruction
   ---------------------------------- */

E(op_SEI) // 0x78
                orb     $I_Flag, F_Reg
                Continue

/* ----------------------------------
       SMBx instructions -- Available in Rockwell 65C02 but not NCR 65C02
       UNIMPLEMENTED : These are documented in the W65C02S datasheet ...
   ---------------------------------- */

E(op_SMB0_65c02)
                Continue

E(op_SMB1_65c02)
                Continue

E(op_SMB2_65c02)
                Continue

E(op_SMB3_65c02)
                Continue

E(op_SMB4_65c02)
                Continue

E(op_SMB5_65c02)
                Continue

E(op_SMB6_65c02)
                Continue

E(op_SMB7_65c02)
                Continue

/* ----------------------------------
       STA instructions
   ---------------------------------- */

E(op_STA_zpage) // 0x85
                GetZPage
                DoSTA
                Continue

E(op_STA_zpage_x) // 0x95
                GetZPage_X
                DoSTA
                Continue

// UNIMPLEMENTED : W65C02S datasheet
E(op_STA_zpage_y)
                jmp op_NOP

E(op_STA_abs) // 0x8d
                GetAbs
                DoSTA
                Continue

E(op_STA_abs_x) // 0x9d
                GetAbs_X_STx
                DoSTA
                Continue

E(op_STA_abs_y) // 0x99
                GetAbs_Y_STA
                DoSTA
                Continue

E(op_STA_ind_x) // 0x81
                GetIndZPage_X
                DoSTA
                Continue

E(op_STA_ind_y) // 0x91
                GetIndZPage_Y_STA
                DoSTA
                Continue

// 65c02 : 0x92
E(op_STA_ind_zpage)
                GetIndZPage
                DoSTA
                Continue

/* ----------------------------------
       STP instruction
       UNIMPLEMENTED : This is documented in the W65C02S datasheet ...
   ---------------------------------- */

E(op_STP_65c02)
                Continue

/* ----------------------------------
       RMBx instructions -- Available in Rockwell 65C02 but not NCR 65C02
       UNIMPLEMENTED : These are documented in the W65C02S datasheet ...
   ---------------------------------- */

E(op_RMB0_65c02)
                Continue

E(op_RMB1_65c02)
                Continue

E(op_RMB2_65c02)
                Continue

E(op_RMB3_65c02)
                Continue

E(op_RMB4_65c02)
                Continue

E(op_RMB5_65c02)
                Continue

E(op_RMB6_65c02)
                Continue

E(op_RMB7_65c02)
                Continue

/* ----------------------------------
       STX instructions
   ---------------------------------- */

E(op_STX_zpage) // 0x86
                GetZPage
                DoSTX
                Continue

// HACK : is this used? need to study coverage ...
E(op_STX_zpage_y) // 0x96
                GetZPage_Y
                DoSTX
                Continue

E(op_STX_abs) // 0x8e
                GetAbs
                DoSTX
                Continue

/* ----------------------------------
       STY instructions
   ---------------------------------- */

E(op_STY_zpage) // 0x84
                GetZPage
                DoSTY
                Continue

E(op_STY_zpage_x) // 0x94
                GetZPage_X
                DoSTY
                Continue

E(op_STY_abs) // 0x8c
                GetAbs
                DoSTY
                Continue

/* ----------------------------------
       STZ instructions
       65c02 only
   ---------------------------------- */

// 65c02 : 0x64
E(op_STZ_zpage)
                GetZPage
                DoSTZ
                Continue

// 65c02 : 0x74
E(op_STZ_zpage_x)
                GetZPage_X
                DoSTZ
                Continue

// 65c02 : 0x9C
E(op_STZ_abs)
                GetAbs
                DoSTZ
                Continue

// 65c02 : 0x9E
E(op_STZ_abs_x)
                GetAbs_X_STx
                DoSTZ
                Continue

/* ----------------------------------
       TAX instruction
   ---------------------------------- */

E(op_TAX) // 0xaa
                movb    A_Reg, X_Reg
                orb     X_Reg, X_Reg
                FlagNZ
                Continue

/* ----------------------------------
       TAY instruction
   ---------------------------------- */

E(op_TAY) // 0xa8
                movb    A_Reg, Y_Reg
                orb     Y_Reg, Y_Reg
                FlagNZ
                Continue

/* ----------------------------------
       TRB instructions
       65c02 only
   ---------------------------------- */

// 65c02 : 0x1C
E(op_TRB_abs)
                GetAbs
                DoTRB
                Continue

// 65c02 : 0x14
E(op_TRB_zpage)
                GetZPage
                DoTRB
                Continue

/* ----------------------------------
       TSB instructions
       65c02 only
   ---------------------------------- */

// 65c02 : 0x0C
E(op_TSB_abs)
                GetAbs
                DoTSB
                Continue

// 65c02 : 0x04
E(op_TSB_zpage)
                GetZPage
                DoTSB
                Continue

/* ----------------------------------
       TSX instruction
   ---------------------------------- */

E(op_TSX) // 0xba
                movb    SP_Reg_L, X_Reg
                orb     X_Reg, X_Reg
                FlagNZ
                Continue

/* ----------------------------------
       TXA instruction
   ---------------------------------- */

E(op_TXA) // 0x8a
                movb    X_Reg, A_Reg
                orb     A_Reg, A_Reg
                FlagNZ
                Continue

/* ----------------------------------
       TXS instruction
   ---------------------------------- */

E(op_TXS) // 0x9a
                movb    X_Reg, SP_Reg_L
                Continue

/* ----------------------------------
       TYA instruction
   ---------------------------------- */

E(op_TYA) // 0x98
                movb    Y_Reg, A_Reg
                orb     A_Reg, A_Reg
                FlagNZ
                Continue

/* ----------------------------------
       ??? instruction - 65c02
       Defined as NOPs by spec
   ---------------------------------- */
E(op_UNK_65c02)
                Continue

/* ----------------------------------
       WAI instruction - 65c02
       UNIMPLEMENTED : This is documented in the W65C02S datasheet ...
   ---------------------------------- */
E(op_WAI_65c02)
                Continue

#pragma mark -
#pragma mark cpu main entry

/* -------------------------------------------------------------------------
        CPU continue
        Keep executing until we've executed >= cpu65_cycles_to_execute
   ------------------------------------------------------------------------- */

continue:
                xorl    _XAX, _XAX
                movb    DebugCurrOpcode, %al
                movb    SN(cpu65__opcycles)(,_XAX,1), %al
                addb    DebugCycleCount, %al
                movb    %al, DebugCycleCount
                addw    %ax, SN(cpu65_cycle_count)
                subl    _XAX, SN(gc_cycles_timer_0)
                subl    _XAX, SN(gc_cycles_timer_1)
                subw    %ax, SN(cpu65_cycles_to_execute)
                jle     exit_cpu65_run
continue1:      xorl    _XAX, _XAX
                orb     SN(cpu65__signal), %al
                jnz     exception
1:              JumpNextInstruction

/* -------------------------------------------------------------------------
        Exception handlers
   ------------------------------------------------------------------------- */

exception:      testb   $ResetSig, %al
                jnz     ex_reset0
                jmp     ex_irq

ex_reset0:      testb   $0xff, SN(joy_button0)              // OpenApple
                jnz     emul_reinit
                testb   $0xff, SN(joy_button1)              // ClosedApple
                jnz     emul_reinit
ex_reset:       movb    $0, SN(cpu65__signal)
                movw    $0xFFFC, EffectiveAddr              // ROM reset vector
                GetFromEA_W
                movw    %ax, PC_Reg
                xorb    %ah, %ah
                JumpNextInstruction

ex_irq:         testb   $I_Flag, F_Reg                      // Already interrupted?
                jz      1f
                JumpNextInstruction                         // Yes (ignored) ...
1:              movw    PC_Reg, %ax                         // No (handle IRQ) ...
                Push(%ah)
                Push(%al)
                orb     $X_Flag, F_Reg
                xorl    _XAX,_XAX
                movb    F_Reg, %al
                movb    SN(cpu65_flags_encode)(,_XAX,1), %al
                Push(%al)
                orb     $(B_Flag | I_Flag), F_Reg
                //andb    $~D_Flag, F_Reg // AppleWin clears Decimal bit?
                andl    $0xFFFF, EffectiveAddr_X// HACK FIXME : there is a bug somewhere that is occasionally corrupting EffectiveAddr_X
                movw    $0xFFFE, EffectiveAddr              // ROM interrupt vector
                GetFromEA_W
                movw    %ax, PC_Reg
                xorb    %ah, %ah
                JumpNextInstruction

/* -------------------------------------------------------------------------
        CPU thread main entry and exit points
   ------------------------------------------------------------------------- */
E(cpu65_run)
                pushal                                      // ENTER CPURUN
                cmpb    $0, SN(emul_reinitialize)
                jnz     1f
                RestoreState
                jmp     continue1
1:              movb    $0, SN(emul_reinitialize)
                /* Zero all registers, as well as the unused 32-bit parts
                 * of variables. (which may need to be kept 0)
                 *
                 * Note: dependent on assignment of registers
                 */
                xorl    _XAX, _XAX
                xorl    %ebx, %ebx
                xorl    %ecx, %ecx
                xorl    %esi, %esi
                xorl    %edi, %edi
                movl    $0x1FF, SP_Reg_X
                jmp     ex_reset

exit_cpu65_run: SaveState                                   // Return to timing loop ...
                popal
                ret

emul_reinit:    movb    $0, SN(cpu65__signal)               // Return to timing loop ...
                movb    $1, SN(emul_reinitialize)
                popal
                ret

/* -------------------------------------------------------------------------
        Debugger hooks
   ------------------------------------------------------------------------- */

E(cpu65_direct_write)
                /* NB: dependent on register choices */
                pushl   %edi
                movl    8(%esp),%edi
                movl    12(%esp),_XAX
                call    *SN(cpu65_vmem)+4(,EffectiveAddr_X,SZ_PTR*2)
                popl    %edi
                ret

