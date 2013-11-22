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
#include "cpu.h"
#include "misc.h"

// HACK FIXME TODO : this appears to be fragile because struct size is hardcoded
// here (in addition to implied size in cpu.h and elsewhere).  Please
// investigate if this is necessary...
                .comm   SN(cpu65_vmem),524288,4096
                .comm   SN(cpu65_flags_encode),256
                .comm   SN(cpu65_flags_decode),256
                .comm   SN(cpu65__opcodes),1024

#define DebugCurrEA     SN(cpu65_debug)
#define DebugCurrByte   SN(cpu65_debug)+2
#define DebugCurrRW     SN(cpu65_debug)+3
#define DebugCurrOpcode SN(cpu65_debug)+4
#define DebugCycleCount SN(cpu65_debug)+5               // TODO FIXME : need a better name here and possibly in the .h file too (reflecting the fact that this is an xtra cycles counter

/* -------------------------------------------------------------------------
    CPU (6502) Helper Routines
   ------------------------------------------------------------------------- */

#define GetFromPC_B                                                            \
                movl    PC_Reg_E, EffectiveAddr_E;                             \
                incw    PC_Reg;                                                \
                call    *SN(cpu65_vmem)                                        \
                        (,EffectiveAddr_E,8);

#define GetFromPC_W                                                            \
                movl    PC_Reg_E, EffectiveAddr_E;                             \
                incw    EffectiveAddr;                                         \
                addw    $2, PC_Reg;                                            \
                call    *SN(cpu65_vmem)                                        \
                        (,EffectiveAddr_E,8);                                  \
                decw    EffectiveAddr;                                         \
                movb    %al, %ah;                                              \
                call    *SN(cpu65_vmem)                                        \
                        (,EffectiveAddr_E,8);

#define JumpNextInstruction                                                    \
                GetFromPC_B                                                    \
                movb    %al, DebugCurrOpcode;                                  \
                movb    $0, DebugCycleCount;                                   \
                jmp     *cpu65__opcodes(,%eax,4);

#define GetFromEA_B                                                            \
                orb     $1, DebugCurrRW;                                       \
                call    *SN(cpu65_vmem)                                        \
                        (,EffectiveAddr_E,8);

#define GetFromEA_W                                                            \
                incw    EffectiveAddr;                                         \
                call    *SN(cpu65_vmem)                                        \
                        (,EffectiveAddr_E,8);                                  \
                decw    EffectiveAddr;                                         \
                movb    %al, %ah;                                              \
                call    *SN(cpu65_vmem)                                        \
                (,EffectiveAddr_E,8);

#define PutToEA_B                                                              \
                orb     $2, DebugCurrRW;                                       \
                orb     %al, DebugCurrByte;                                    \
                call    *SN(cpu65_vmem)+4                                      \
                        (,EffectiveAddr_E,8);
#define GetFromMem_B(x)                                                        \
                movl    x, EffectiveAddr_E;                                    \
                call    *SN(cpu65_vmem)                                        \
                        (,EffectiveAddr_E,8);

#define GetFromMem_W(x)                                                        \
                movl    x, EffectiveAddr_E;                                    \
                incw    EffectiveAddr;                                         \
                call    *SN(cpu65_vmem)                                        \
                        (,EffectiveAddr_E,8);                                  \
                decw    EffectiveAddr;                                         \
                movb    %al, %ah;                                              \
                call    *SN(cpu65_vmem)                                        \
                        (,EffectiveAddr_E,8);                                  \

#define Continue                                                               \
                jmp     continue;

/*
 * Save CPU state when returning from being called from C
 */
#define SaveState                                                              \
                movw    EffectiveAddr, DebugCurrEA;                            \
                movw    PC_Reg, SN(cpu65_current);                             \
                movb    A_Reg, SN(cpu65_current)+2;                            \
                movb    F_Reg, SN(cpu65_current)+3;                            \
                movb    X_Reg, SN(cpu65_current)+4;                            \
                movb    Y_Reg, SN(cpu65_current)+5;                            \
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
#define RestoreState                                                           \
                xorl    %eax, %eax;                                            \
                xorl    %ebx, %ebx;                                            \
                xorl    %ecx, %ecx;                                            \
                movl    $0x0100, %edx;                                         \
                xorl    %esi, %esi;                                            \
                xorl    %edi, %edi;                                            \
                movw    DebugCurrEA, EffectiveAddr;                            \
                movw    SN(cpu65_current), PC_Reg;                             \
                movb    SN(cpu65_current)+2, A_Reg;                            \
                movb    SN(cpu65_current)+3, F_Reg;                            \
                movb    SN(cpu65_current)+4, X_Reg;                            \
                movb    SN(cpu65_current)+5, Y_Reg;                            \
                movb    SN(cpu65_current)+6, SP_Reg_L;                         \
                testl   $SS_ALTZP, SN(softswitches);                           \
                jz      9f;                                                    \
                orl     $0x10000, %edx;                                        \
9:

#define BranchXCycles                                                          \
                incb    DebugCycleCount; /* +1 branch taken */                 \
                pushl   %ebx;                                                  \
                movw    PC_Reg, %bx;                                           \
                addb    %al, %bl;                                              \
                jnc     9f;                                                    \
                incb    DebugCycleCount; /* +1 branch across pg boundary */    \
9:              addw    %ax, PC_Reg;                                           \
                popl    %ebx;

#define FlagC   lahf;                                                          \
                andb    $C_Flag, %ah;                                          \
                andb    $~C_Flag, F_Reg;                                       \
                orb     %ah, F_Reg;

#define FlagZ   lahf;                                                          \
                andb    $Z_Flag, %ah;                                          \
                andb    $~Z_Flag, F_Reg;                                       \
                orb     %ah, F_Reg;

#define FlagN   lahf;                                                          \
                andb    $N_Flag, %ah;                                          \
                andb    $~N_Flag, F_Reg;                                       \
                orb     %ah, F_Reg;

#define FlagNZ  lahf;                                                          \
                andb    $(N_Flag|Z_Flag), %ah;                                 \
                andb    $~(N_Flag|Z_Flag), F_Reg;                              \
                orb     %ah, F_Reg;

#define FlagNZC lahf;                                                          \
                andb    $(N_Flag|Z_Flag|C_Flag), %ah;                          \
                andb    $~(N_Flag|Z_Flag|C_Flag), F_Reg;                       \
                orb     %ah, F_Reg;

                /* Have to do things a little differently since
                 * overflow is not read by the LAHF instruction
                 *
                 * Use of long operands wastes two bytes on the AND
                 * constant, but saves three instruction prefixes.
                 * This doesn't affect the cycle count.
                 */
#define FlagNVZC                                                               \
                pushfl;                                                        \
                popl    %eax;                                                  \
                andl    $0x08C1,%eax;                                          \
                andb    $~(N_Flag|V_Flag|Z_Flag|C_Flag), F_Reg;                \
                orb     %ah, F_Reg;                                            \
                orb     %al, F_Reg;

#define Push(x) movb    x, SN(apple_ii_64k)(,SP_Reg,1);                        \
                decb    SP_Reg_L;

#define Pop(x)  incb    SP_Reg_L;                                              \
                movb    SN(apple_ii_64k)(,SP_Reg,1), x;

/* Immediate Addressing - the operand is contained in the second byte of the
   instruction. */
#define GetImm  movl    PC_Reg_E, EffectiveAddr_E;                             \
                incw    PC_Reg;

/* Absolute Addressing - the second byte of the instruction is the low
   order address, and the third byte is the high order byte. */
#define GetAbs  GetFromPC_W;                                                   \
                movl    %eax, EffectiveAddr_E;

/* Zero Page Addressing - the second byte of the instruction is an
   address on the zero page */
#define GetZPage                                                               \
                GetFromPC_B;                                                   \
                movl    %eax, EffectiveAddr_E;

/* Zero Page Indexed Addressing - The effective address is calculated by
   adding the second byte to the contents of the index register.  Due
   to the zero page addressing nature of this mode, no carry is added
   to the high address byte, and the crossing of page boundaries does
   not occur. */
#define GetZPage_X                                                             \
                GetFromPC_B;                                                   \
                addb    X_Reg, %al;                                            \
                movl    %eax, EffectiveAddr_E;

// HACK IS THIS EVER USED?
#define GetZPage_Y                                                             \
                GetFromPC_B;                                                   \
                addb    Y_Reg, %al;                                            \
                movl    %eax, EffectiveAddr_E;

/* Absolute Indexed Addressing - The effective address is formed by
   adding the contents of X or Y to the address contained in the
   second and third bytes of the instruction. */
#define _GetAbs_X                                                              \
                GetFromPC_W;                                                   \
                addb    X_Reg, %al;                                            \
                jnc     9f;                                                    \
                adcb    $0, %ah;

#define GetAbs_X                                                               \
                _GetAbs_X                                                      \
                incb    DebugCycleCount; /* +1 cycle on page boundary */       \
9:              movl    %eax, EffectiveAddr_E;

#define GetAbs_X_STx                                                           \
                _GetAbs_X                                                      \
9:              movl    %eax, EffectiveAddr_E;

#define _GetAbs_Y                                                              \
                GetFromPC_W;                                                   \
                addb    Y_Reg, %al;                                            \
                jnc     9f;                                                    \
                adcb    $0, %ah;

#define GetAbs_Y                                                               \
                _GetAbs_Y                                                      \
                incb    DebugCycleCount; /* +1 cycle on page boundary */       \
9:              movl    %eax, EffectiveAddr_E;

#define GetAbs_Y_STA                                                           \
                _GetAbs_Y                                                      \
9:              movl    %eax, EffectiveAddr_E;

/* Absolute Indirect Addressing - The second and third bytes of the
   instruction are the low and high bytes of an address, respectively.
   The contents of the fully specified memory location is the
   low-order byte of the effective address.  The next memory location
   contains the high order byte of the effective address. */
/* (unused at the moment. It applies to JMP, but JMP's addressing is done
 * without the macro)
 */
#define GetInd  GetFromPC_W;                                                   \
                GetFromMem_W(%eax)

/* Zero Page Indirect Addressing (65c02) - The second byte of the
   instruction points to a memory location on page zero containing the
   low order byte of the effective address.  The next location on page
   zero contains the high order byte of the address. */
#define GetIndZPage                                                            \
                GetFromPC_B;                                                   \
                incb    %al;                                                   \
                movl    %eax, EffectiveAddr_E;                                 \
                GetFromEA_B;                                                   \
                movb    %al, %ah;                                              \
                decl    EffectiveAddr_E;                                       \
                andl    $0xFF, EffectiveAddr_E;                                \
                GetFromEA_B;                                                   \
                movl    %eax, EffectiveAddr_E;

/* Zero Page Indexed Indirect Addressing - The second byte is added to
   the contents of the X index register; the carry is discarded.  The
   result of this addition points to a memory location on page zero
   whose contents is the low order byte of the effective address.  The
   next memory location in page zero contains the high-order byte of
   the effective address.  Both memory locations specifying the high
   and low-order bytes must be in page zero. */
#define GetIndZPage_X                                                          \
                GetFromPC_B;                                                   \
                addb    X_Reg, %al;                                            \
                incb    %al;                                                   \
                movl    %eax, EffectiveAddr_E;                                 \
                GetFromEA_B;                                                   \
                movb    %al, %ah;                                              \
                decl    EffectiveAddr_E;                                       \
                andl    $0xFF, EffectiveAddr_E;                                \
                GetFromEA_B;                                                   \
                movl    %eax, EffectiveAddr_E;

/* Indirect Indexed Addressing - The second byte of the instruction
   points to a memory location in page zero.  The contents of this
   memory location are added to the contents of the Y index register,
   the result being the low order byte of the effective address.  The
   carry from this addition is added to the contents of the next page
   zero memory location, the result being the high order byte of the
   effective address. */
#define _GetIndZPage_Y                                                         \
                GetFromPC_B;                                                   \
                incb    %al;                                                   \
                movl    %eax, EffectiveAddr_E;                                 \
                GetFromEA_B;                                                   \
                movb    %al, %ah;                                              \
                decl    EffectiveAddr_E;                                       \
                andl    $0xFF, EffectiveAddr_E;                                \
                GetFromEA_B;                                                   \
                addb    Y_Reg, %al;                                            \
                jnc 9f;

#define GetIndZPage_Y                                                          \
                _GetIndZPage_Y                                                 \
                adcb    $0, %ah;                                               \
                incb    DebugCycleCount; /* +1 cycle on page boundary */       \
9:              movl    %eax, EffectiveAddr_E;

#define GetIndZPage_Y_STA                                                      \
                _GetIndZPage_Y                                                 \
                adcb    $0, %ah;                                               \
9:              movl    %eax, EffectiveAddr_E;

#define DoADC_b GetFromEA_B                                                    \
                bt      $C_Flag_Bit, FF_Reg;                                   \
                adcb    %al, A_Reg;                                            \
                FlagNVZC

#define DoADC_d GetFromEA_B                                                    \
                bt      $C_Flag_Bit, FF_Reg;                                   \
                adcb    A_Reg, %al;                                            \
                daa;                                                           \
                movb    %al, A_Reg;                                            \
                FlagNVZC

#define DoAND   GetFromEA_B                                                    \
                andb    %al, A_Reg;                                            \
                FlagNZ

#define DoASL   GetFromEA_B                                                    \
                addb    %al, %al;                                              \
                FlagNZC                                                        \
                PutToEA_B                                                      \

/* SAR (and the following AND) effectively moves
 * bit 6 to Bit 3 while leaving Bit 7 unchanged */
#define DoBIT   GetFromEA_B                                                    \
                testb   %al, A_Reg;                                            \
                lahf;                                                          \
                sarb    $3, %al;                                               \
                andw    $0x4088, %ax;                                          \
                andb    $~(N_Flag|V_Flag|Z_Flag), F_Reg;                       \
                orb     %al, F_Reg;                                            \
                orb     %ah, F_Reg;

#define DoCMP   GetFromEA_B                                                    \
                cmpb    %al, A_Reg;                                            \
                cmc;                                                           \
                FlagNZC

#define DoCPX   GetFromEA_B                                                    \
                cmpb    %al, X_Reg;                                            \
                cmc;                                                           \
                FlagNZC

#define DoCPY   GetFromEA_B                                                    \
                cmpb    %al, Y_Reg;                                            \
                cmc;                                                           \
                FlagNZC

#define DoDEC   GetFromEA_B                                                    \
                decb    %al;                                                   \
                FlagNZ                                                         \
                PutToEA_B

#define DoEOR   GetFromEA_B                                                    \
                xorb    %al, A_Reg;                                            \
                FlagNZ

#define DoINC   GetFromEA_B                                                    \
                incb    %al;                                                   \
                FlagNZ                                                         \
                PutToEA_B

#define DoLDA   GetFromEA_B                                                    \
                movb    %al, A_Reg;                                            \
                orb     %al, %al;                                              \
                FlagNZ

#define DoLDX   GetFromEA_B                                                    \
                movb    %al, X_Reg;                                            \
                orb     %al, %al;                                              \
                FlagNZ

#define DoLDY   GetFromEA_B                                                    \
                movb    %al, Y_Reg;                                            \
                orb     %al, %al;                                              \
                FlagNZ

#define DoLSR   GetFromEA_B                                                    \
                shrb    $1, %al;                                               \
                FlagNZC                                                        \
                PutToEA_B

#define DoORA   GetFromEA_B                                                    \
                orb     %al, A_Reg;                                            \
                FlagNZ

#define DoROL   GetFromEA_B                                                    \
                bt      $C_Flag_Bit, FF_Reg;                                   \
                adcb    %al,%al;                                               \
                FlagNZC                                                        \
                PutToEA_B

#define DoROR   GetFromEA_B                                                    \
                movb    F_Reg, %ah;                                            \
                rorl    $1, %eax;                                              \
                orb     %al, %al;                                              \
                btr     $31, %eax;                                             \
                FlagNZC                                                        \
                PutToEA_B

#define DoSBC_b GetFromEA_B                                                    \
                notb    %al;                                                   \
                bt      $C_Flag_Bit, FF_Reg;                                   \
                adcb    %al, A_Reg;                                            \
                FlagNVZC

#define DoSBC_d GetFromEA_B                                                    \
                bt      $C_Flag_Bit, FF_Reg;                                   \
                cmc;                                                           \
                xchgb   A_Reg, %al;                                            \
                sbbb    A_Reg, %al;                                            \
                das;                                                           \
                movb    %al, A_Reg;                                            \
                cmc;                                                           \
                FlagNVZC

#define DoSTA   movb    A_Reg, %al;                                            \
                PutToEA_B

#define DoSTX   movb    X_Reg, %al;                                            \
                PutToEA_B

#define DoSTY   movb    Y_Reg, %al;                                            \
                PutToEA_B

#define DoSTZ   movb    $0x0, %al;                                             \
                PutToEA_B

#define DoTRB   GetFromEA_B                                                    \
                testb   A_Reg, %al;                                            \
                FlagZ                                                          \
                notb    A_Reg;                                                 \
                andb    A_Reg, %al;                                            \
                notb    A_Reg;                                                 \
                PutToEA_B

#define DoTSB   GetFromEA_B                                                    \
                testb   A_Reg, %al;                                            \
                FlagZ                                                          \
                orb     A_Reg, %al;                                            \
                PutToEA_B

/* ----------------------------------------------------------------------
       6502 routines and instructions
   ---------------------------------------------------------------------- */

/* ----------------------------------
       ADC instructions
       ADd memory to accumulator with Carry
   ---------------------------------- */

// Decimal mode
op_ADC_dec:
                incb    DebugCycleCount         // +1 cycle
                DoADC_d
                Continue

op_ADC_imm:
                GetImm
                testb   $D_Flag, F_Reg          // Decimal mode?
                jnz     op_ADC_dec              // Yes, jump to decimal version
                DoADC_b
                Continue

op_ADC_zpage:
                GetZPage
                testb   $D_Flag, F_Reg          // Decimal mode?
                jnz     op_ADC_dec              // Yes, jump to decimal version
                DoADC_b
                Continue

op_ADC_zpage_x:
                GetZPage_X
                testb   $D_Flag, F_Reg          // Decimal mode?
                jnz     op_ADC_dec              // Yes, jump to decimal version
                DoADC_b
                Continue

// UNIMPLEMENTED : W65C02S datasheet
op_ADC_zpage_y:
                hlt

op_ADC_abs:
                GetAbs
                testb   $D_Flag, F_Reg          // Decimal mode?
                jnz     op_ADC_dec              // Yes, jump to decimal version
                DoADC_b
                Continue

op_ADC_abs_x:
                GetAbs_X
                testb   $D_Flag, F_Reg          // Decimal mode?
                jnz     op_ADC_dec              // Yes, jump to decimal version
                DoADC_b
                Continue

op_ADC_abs_y:
                GetAbs_Y
                testb   $D_Flag, F_Reg          // Decimal mode?
                jnz     op_ADC_dec              // Yes, jump to decimal version
                DoADC_b
                Continue

op_ADC_ind_x:
                GetIndZPage_X
                testb   $D_Flag, F_Reg          // Decimal mode?
                jnz     op_ADC_dec              // Yes, jump to decimal version
                DoADC_b
                Continue

op_ADC_ind_y:
                GetIndZPage_Y
                testb   $D_Flag, F_Reg          // Decimal mode?
                jnz     op_ADC_dec              // Yes, jump to decimal version
                DoADC_b
                Continue

// 65c02 : 0x72
op_ADC_ind_zpage:
                GetIndZPage
                testb   $D_Flag, F_Reg          // Decimal mode?
                jnz     op_ADC_dec              // Yes, jump to decimal version
                DoADC_b
                Continue

/* ----------------------------------
       AND instructions
       logical AND memory with accumulator
   ---------------------------------- */

op_AND_imm:
                GetImm
                DoAND
                Continue

op_AND_zpage:
                GetZPage
                DoAND
                Continue

op_AND_zpage_x:
                GetZPage_X
                DoAND
                Continue

// UNIMPLEMENTED : W65C02S datasheet
op_AND_zpage_y:
                hlt

op_AND_abs:
                GetAbs
                DoAND
                Continue

op_AND_abs_x:
                GetAbs_X
                DoAND
                Continue

op_AND_abs_y:
                GetAbs_Y
                DoAND
                Continue

op_AND_ind_x:
                GetIndZPage_X
                DoAND
                Continue

op_AND_ind_y:
                GetIndZPage_Y
                DoAND
                Continue

// 65c02 : 0x32
op_AND_ind_zpage:
                GetIndZPage
                DoAND
                Continue

/* ----------------------------------
       ASL instructions
       Arithmetic Shift one bit Left, memory or accumulator
   ---------------------------------- */

op_ASL_acc:
                addb    A_Reg, A_Reg
                FlagNZC
                Continue

op_ASL_zpage:
                GetZPage
                DoASL
                Continue

op_ASL_zpage_x:
                GetZPage_X
                DoASL
                Continue

op_ASL_abs:
                GetAbs
                DoASL
                Continue

op_ASL_abs_x:
                GetAbs_X
                DoASL
                Continue

/* ----------------------------------
       BBRx instructions
       UNIMPLEMENTED : These are documented in the W65C02S datasheet ...
       + 1 cycle  if branch within page
       + 2 cycles if branch across page boundary
   ---------------------------------- */

op_BBR0_65c02:
                Continue

op_BBR1_65c02:
                Continue

op_BBR2_65c02:
                Continue

op_BBR3_65c02:
                Continue

op_BBR4_65c02:
                Continue

op_BBR5_65c02:
                Continue

op_BBR6_65c02:
                Continue

op_BBR7_65c02:
                Continue

/* ----------------------------------
       BBSx instructions
       UNIMPLEMENTED : These are documented in the W65C02S datasheet ...
       + 1 cycle  if branch within page
       + 2 cycles if branch across page boundary
   ---------------------------------- */

op_BBS0_65c02:
                Continue

op_BBS1_65c02:
                Continue

op_BBS2_65c02:
                Continue

op_BBS3_65c02:
                Continue

op_BBS4_65c02:
                Continue

op_BBS5_65c02:
                Continue

op_BBS6_65c02:
                Continue

op_BBS7_65c02:
                Continue

/* ----------------------------------
       BCC instruction
       Branch on Carry Clear
   ---------------------------------- */

op_BCC:
                GetFromPC_B
                testb   $C_Flag, F_Reg
                jnz     op_BCC_not              // branch not taken
                cbw
                BranchXCycles
op_BCC_not:
                Continue

/* ----------------------------------
       BCS instruction
       Branch on Carry Set
   ---------------------------------- */

op_BCS:
                GetFromPC_B
                testb   $C_Flag, F_Reg
                jz      op_BCS_not              // branch not taken
                cbw
                BranchXCycles
op_BCS_not:
                Continue

/* ----------------------------------
       BEQ instruction
       Branch if EQual
   ---------------------------------- */

op_BEQ:
                GetFromPC_B
                testb   $Z_Flag, F_Reg
                jz      op_BEQ_not              // branch not taken
                cbw
                BranchXCycles
op_BEQ_not:
                Continue

/* ----------------------------------
       BIT instructions
       BIt Test
   ---------------------------------- */

op_BIT_zpage:
                GetZPage
                DoBIT
                Continue

op_BIT_abs:
                GetAbs
                DoBIT
                Continue

// 65c02 : 0x34
op_BIT_zpage_x:
                GetIndZPage
                DoBIT
                Continue

// 65c02 : 0x3C
op_BIT_abs_x:
                GetAbs_X
                DoBIT
                Continue

                /* BIT immediate is anomalous in that it does not affect the
                 * N and V flags, unlike in other addressing modes.
                 */
// 65c02 : 0x89
op_BIT_imm:
                GetImm
                GetFromEA_B
                testb   %al, A_Reg
                FlagZ
                Continue

/* ----------------------------------
       BMI instruction
       Branch on result MInus
   ---------------------------------- */

op_BMI:
                GetFromPC_B
                testb   F_Reg, F_Reg    /* optimized check of N flag,
                                         * which happens to be sign bit */
                jns     op_BMI_not
                cbw
                BranchXCycles
op_BMI_not:
                Continue

/* ----------------------------------
       BNE instruction
       Branch on result Not Equal
   ---------------------------------- */

op_BNE:
                GetFromPC_B
                testb   $Z_Flag, F_Reg
                jnz     op_BNE_not
                cbw
                BranchXCycles
op_BNE_not:
                Continue

/* ----------------------------------
       BPL instruction
       Branch on result PLus
   ---------------------------------- */

op_BPL:
                GetFromPC_B
                testb   F_Reg, F_Reg    /* optimized check of N flag,
                                         * which happens to be sign bit */
                js      op_BPL_not
                cbw
                BranchXCycles
op_BPL_not:
                Continue

/* ----------------------------------
       BRA instruction
       BRanch Always
   ---------------------------------- */

// 65c02 : 0x80
op_BRA:
                GetFromPC_B
                cbw
                BranchXCycles
                Continue

/* ----------------------------------
       BRK instruction
   ---------------------------------- */

op_UNK: /* make undefined nmos opcodes fault */
op_BRK:
                incw    PC_Reg
                movw    PC_Reg, %ax
                Push(%ah)
                Push(%al)
                orb     $(B_Flag|X_Flag), F_Reg
                xorl    %eax,%eax
                movb    F_Reg, %al
                movb    SN(cpu65_flags_encode)(,%eax,1), %al
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

op_BVC:
                GetFromPC_B
                testb   $V_Flag, F_Reg
                jnz     op_BVC_not
                cbw
                BranchXCycles
op_BVC_not:
                Continue

/* ----------------------------------
       BVS instruction
       Branch on oVerflow Set
   ---------------------------------- */

op_BVS:
                GetFromPC_B
                testb   $V_Flag, F_Reg
                jz      op_BVS_not
                cbw
                BranchXCycles
op_BVS_not:
                Continue

/* ----------------------------------
       CLC instruction
   ---------------------------------- */

op_CLC:
                andb    $~C_Flag, F_Reg
                Continue

/* ----------------------------------
       CLD instruction
   ---------------------------------- */

op_CLD:
                andb    $~D_Flag, F_Reg
                Continue

/* ----------------------------------
       CLI instruction
   ---------------------------------- */

op_CLI:
                andb    $~I_Flag, F_Reg
                Continue

/* ----------------------------------
       CLV instruction
   ---------------------------------- */

op_CLV:
                andb    $~V_Flag, F_Reg
                Continue

/* ----------------------------------
       CMP instructions
       CoMPare memory and accumulator
   ---------------------------------- */

op_CMP_imm:
                GetImm
                DoCMP
                Continue

op_CMP_zpage:
                GetZPage
                DoCMP
                Continue

op_CMP_zpage_x:
                GetZPage_X
                DoCMP
                Continue

// UNIMPLEMENTED : W65C02S datasheet
op_CMP_zpage_y:
                hlt

op_CMP_abs:
                GetAbs
                DoCMP
                Continue

op_CMP_abs_x:
                GetAbs_X
                DoCMP
                Continue

op_CMP_abs_y:
                GetAbs_Y
                DoCMP
                Continue

op_CMP_ind_x:
                GetIndZPage_X
                DoCMP
                Continue

op_CMP_ind_y:
                GetIndZPage_Y
                DoCMP
                Continue

// 65c02 : 0xD2
op_CMP_ind_zpage:
                GetIndZPage
                DoCMP
                Continue

/* ----------------------------------
       CPX instructions
       ComPare memory and X register
   ---------------------------------- */

op_CPX_imm:
                GetImm
                DoCPX
                Continue

op_CPX_zpage:
                GetZPage
                DoCPX
                Continue

op_CPX_abs:
                GetAbs
                DoCPX
                Continue

/* ----------------------------------
       CPY instructions
       ComPare memory and Y register
   ---------------------------------- */

op_CPY_imm:
                GetImm
                DoCPY
                Continue

op_CPY_zpage:
                GetZPage
                DoCPY
                Continue

op_CPY_abs:
                GetAbs
                DoCPY
                Continue

/* ----------------------------------
       DEA : DEcrement Accumulator
   ---------------------------------- */

op_DEC_acc:
op_DEA:                         # 3A
                decb    A_Reg
                FlagNZ
                Continue

/* ----------------------------------
       DEC instructions
       DECrement memory or accumulator by one
   ---------------------------------- */

op_DEC_zpage:
                GetZPage
                DoDEC
                Continue

op_DEC_zpage_x:
                GetZPage_X
                DoDEC
                Continue

op_DEC_abs:
                GetAbs
                DoDEC
                Continue

op_DEC_abs_x:
                GetAbs_X
                DoDEC
                Continue

/* ----------------------------------
       DEX instruction
   ---------------------------------- */

op_DEX:
                decb    X_Reg
                FlagNZ
                Continue

/* ----------------------------------
       DEY instruction
   ---------------------------------- */

op_DEY:
                decb    Y_Reg
                FlagNZ
                Continue

/* ----------------------------------
       EOR instructions
       Exclusive OR memory with accumulator
   ---------------------------------- */

op_EOR_imm:
                GetImm
                DoEOR
                Continue

op_EOR_zpage:
                GetZPage
                DoEOR
                Continue

op_EOR_zpage_x:
                GetZPage_X
                DoEOR
                Continue

// UNIMPLEMENTED : W65C02S datasheet
op_EOR_zpage_y:
                hlt

op_EOR_abs:
                GetAbs
                DoEOR
                Continue

op_EOR_abs_x:
                GetAbs_X
                DoEOR
                Continue

op_EOR_abs_y:
                GetAbs_Y
                DoEOR
                Continue

op_EOR_ind_x:
                GetIndZPage_X
                DoEOR
                Continue

op_EOR_ind_y:
                GetIndZPage_Y
                DoEOR
                Continue

// 65c02 : 0x52
op_EOR_ind_zpage:
                GetIndZPage
                DoEOR
                Continue

/* ----------------------------------
       INA : INcrement Accumulator
   ---------------------------------- */

op_INC_acc:
op_INA:                         # 1A
                incb    A_Reg
                FlagNZ
                Continue

/* ----------------------------------
       INC instructions
       INCrement memory
   ---------------------------------- */

op_INC_zpage:
                GetZPage
                DoINC
                Continue

op_INC_zpage_x:
                GetZPage_X
                DoINC
                Continue

op_INC_abs:
                GetAbs
                DoINC
                Continue

op_INC_abs_x:
                GetAbs_X
                DoINC
                Continue

/* ----------------------------------
       INX instruction
   ---------------------------------- */

op_INX:
                incb    X_Reg
                FlagNZ
                Continue

/* ----------------------------------
       INY instruction
   ---------------------------------- */

op_INY:
                incb    Y_Reg
                FlagNZ
                Continue

/* ----------------------------------
       JMP instructions
       JuMP to new location
   ---------------------------------- */

op_JMP_abs:
                GetAbs
                movw    EffectiveAddr, PC_Reg;
                Continue

op_JMP_ind:
                xorl    %eax, %eax
                GetFromMem_B(PC_Reg_E)
                xchgb   %al, %ah
                cmpb    $0xFF, %ah
                je      special_case
                incw    PC_Reg
                GetFromMem_B(PC_Reg_E)
                xchgb   %al, %ah
                GetFromMem_W(%eax)
                movw    %ax, PC_Reg
                Continue
special_case: /*?*/
                incw    PC_Reg
                subw    $0x100, PC_Reg
                GetFromMem_B(PC_Reg_E)
                xchgb   %al, %ah
                GetFromMem_W(%eax)
                movw    %ax, PC_Reg
                Continue

// 65c02 : 0x6C
op_JMP_ind_65c02:
                xorl    %eax, %eax
                GetFromMem_B(PC_Reg_E)
                xchgb   %al, %ah
                incw    PC_Reg
                GetFromMem_B(PC_Reg_E)
                xchgb   %al, %ah
                GetFromMem_W(%eax)
                movw    %ax, PC_Reg
                Continue

// 65c02 : 0x7C
op_JMP_abs_ind_x:
                GetFromPC_W
                movw    %ax, EffectiveAddr
                movzbl  X_Reg, %eax
                addw    %ax, EffectiveAddr
                GetFromMem_W(EffectiveAddr_E)
                movw    %ax, PC_Reg
                Continue

/* ----------------------------------
       JSR instruction
   ---------------------------------- */

op_JSR:
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

op_LDA_imm:
                GetImm
                DoLDA
                Continue

op_LDA_zpage:
                GetZPage
                DoLDA
                Continue

op_LDA_zpage_x:
                GetZPage_X
                DoLDA
                Continue

// UNIMPLEMENTED : W65C02S datasheet
op_LDA_zpage_y:
                hlt

op_LDA_abs:
                GetAbs
                DoLDA
                Continue

op_LDA_abs_x:
                GetAbs_X
                DoLDA
                Continue

op_LDA_abs_y:
                GetAbs_Y
                DoLDA
                Continue

op_LDA_ind_x:
                GetIndZPage_X
                DoLDA
                Continue

op_LDA_ind_y:
                GetIndZPage_Y
                DoLDA
                Continue

// 65c02 : 0xB2
op_LDA_ind_zpage:               # B2
                GetIndZPage
                DoLDA
                Continue

/* ----------------------------------
       LDX instructions
   ---------------------------------- */

op_LDX_imm:
                GetImm
                DoLDX
                Continue

op_LDX_zpage:
                GetZPage
                DoLDX
                Continue

// HACK : is this used? need to study coverage ...
op_LDX_zpage_y:
                GetZPage_Y
                DoLDX
                Continue

op_LDX_abs:
                GetAbs
                DoLDX
                Continue

op_LDX_abs_y:
                GetAbs_Y
                DoLDX
                Continue

/* ----------------------------------
       LDY instructions
   ---------------------------------- */

op_LDY_imm:
                GetImm
                DoLDY
                Continue

op_LDY_zpage:
                GetZPage
                DoLDY
                Continue

op_LDY_zpage_x:
                GetZPage_X
                DoLDY
                Continue

op_LDY_abs:
                GetAbs
                DoLDY
                Continue

op_LDY_abs_x:
                GetAbs_X
                DoLDY
                Continue

/* ----------------------------------
       LSR instructions
   ---------------------------------- */

op_LSR_acc:
                shrb    $1, A_Reg
                FlagNZC
                Continue

op_LSR_zpage:
                GetZPage
                DoLSR
                Continue

op_LSR_zpage_x:
                GetZPage_X
                DoLSR
                Continue

op_LSR_abs:
                GetAbs
                DoLSR
                Continue

op_LSR_abs_x:
                GetAbs_X
                DoLSR
                Continue

/* ----------------------------------
       NOP instruction
   ---------------------------------- */

op_NOP:
                Continue

/* ----------------------------------
       ORA instructions
   ---------------------------------- */

op_ORA_imm:
                GetImm
                DoORA
                Continue

op_ORA_zpage:
                GetZPage
                DoORA
                Continue

op_ORA_zpage_x:
                GetZPage_X
                DoORA
                Continue

// UNIMPLEMENTED : W65C02S datasheet
op_ORA_zpage_y:
                hlt

op_ORA_abs:
                GetAbs
                DoORA
                Continue

op_ORA_abs_x:
                GetAbs_X
                DoORA
                Continue

op_ORA_abs_y:
                GetAbs_Y
                DoORA
                Continue

op_ORA_ind_x:
                GetIndZPage_X
                DoORA
                Continue

op_ORA_ind_y:
                GetIndZPage_Y
                DoORA
                Continue

// 65c02 : 0x12
op_ORA_ind_zpage:               # 12
                GetIndZPage
                DoORA
                Continue

/* ----------------------------------
       PHA instruction
   ---------------------------------- */

op_PHA:
                Push(A_Reg)
                Continue

/* ----------------------------------
       PHP instruction
   ---------------------------------- */

op_PHP:
                movb    F_Reg, %al
                movb    SN(cpu65_flags_encode)(,%eax,1), %al
                Push(%al)
                Continue

/* ----------------------------------
       PHX instruction
       65c02 : 0xDA
   ---------------------------------- */

op_PHX:
                Push(X_Reg)
                Continue

/* ----------------------------------
       PHY instruction
       65c02 : 0x5A
   ---------------------------------- */

op_PHY:
                Push(Y_Reg)
                Continue

/* ----------------------------------
       PLA instruction
   ---------------------------------- */

op_PLA:
                Pop(A_Reg)
                orb     A_Reg, A_Reg
                FlagNZ
                Continue

/* ----------------------------------
       PLP instruction
   ---------------------------------- */

op_PLP:
                xorl    %eax, %eax
                Pop(%al)
                movb    SN(cpu65_flags_decode)(,%eax,1), F_Reg
                orb     $(B_Flag|X_Flag), F_Reg
                Continue

/* ----------------------------------
       PLX instruction
       65c02 : 0xFA
   ---------------------------------- */

op_PLX:
                Pop(X_Reg)
                orb X_Reg, X_Reg
                FlagNZ
                Continue

/* ----------------------------------
       PLY instruction
       65c02 : 0x7A
   ---------------------------------- */

op_PLY:
                Pop(Y_Reg)
                orb Y_Reg, Y_Reg
                FlagNZ
                Continue

/* ----------------------------------
       ROL instructions
   ---------------------------------- */

op_ROL_acc:     bt      $C_Flag_Bit, FF_Reg
                adcb    A_Reg, A_Reg
                FlagNZC
                Continue

op_ROL_zpage:
                GetZPage
                DoROL
                Continue

op_ROL_zpage_x:
                GetZPage_X
                DoROL
                Continue

op_ROL_abs:
                GetAbs
                DoROL
                Continue

op_ROL_abs_x:
                GetAbs_X
                DoROL
                Continue

/* ----------------------------------
       ROR instructions
   ---------------------------------- */
                /* NB: assumes A_Reg = %cl, F_Reg = %ch */
op_ROR_acc:
                rorw    $1, %cx         /* Roll flags into accum */
                adcb    F_Reg, F_Reg    /* Roll carry into flags */
                orb     A_Reg, A_Reg
                FlagNZ                  /* implied C */
                Continue

op_ROR_zpage:
                GetZPage
                DoROR
                Continue

op_ROR_zpage_x:
                GetZPage_X
                DoROR
                Continue

op_ROR_abs:
                GetAbs
                DoROR
                Continue

op_ROR_abs_x:
                GetAbs_X
                DoROR
                Continue

/* ----------------------------------
       RTI instruction
   ---------------------------------- */

op_RTI:
                xorl    %eax, %eax
                Pop(%al)
                movb    SN(cpu65_flags_decode)(,%eax,1), F_Reg
                orb     $(B_Flag|X_Flag), F_Reg
                Pop(%al)
                Pop(%ah)
                movw    %ax, PC_Reg
                Continue

/* ----------------------------------
       RTS instruction
   ---------------------------------- */

op_RTS:
                Pop(%al)
                Pop(%ah)
                incw    %ax
                movw    %ax, PC_Reg
                Continue

/* ----------------------------------
       SBC instructions
   ---------------------------------- */

op_SBC_dec:
                incb    DebugCycleCount         // +1 cycle
                DoSBC_d
                Continue

op_SBC_imm:
                GetImm
                testb   $D_Flag, F_Reg          // Decimal mode?
                jnz     op_SBC_dec              // Yes, jump to decimal version
                DoSBC_b
                Continue

op_SBC_zpage:
                GetZPage
                testb   $D_Flag, F_Reg          // Decimal mode?
                jnz     op_SBC_dec              // Yes, jump to decimal version
                DoSBC_b
                Continue

op_SBC_zpage_x:
                GetZPage_X
                testb   $D_Flag, F_Reg          // Decimal mode?
                jnz     op_SBC_dec              // Yes, jump to decimal version
                DoSBC_b
                Continue

// UNIMPLEMENTED : W65C02S datasheet
op_SBC_zpage_y:
                hlt

op_SBC_abs:
                GetAbs
                testb   $D_Flag, F_Reg          // Decimal mode?
                jnz     op_SBC_dec              // Yes, jump to decimal version
                DoSBC_b
                Continue

op_SBC_abs_x:
                GetAbs_X
                testb   $D_Flag, F_Reg          // Decimal mode?
                jnz     op_SBC_dec              // Yes, jump to decimal version
                DoSBC_b
                Continue

op_SBC_abs_y:
                GetAbs_Y
                testb   $D_Flag, F_Reg          // Decimal mode?
                jnz     op_SBC_dec              // Yes, jump to decimal version
                DoSBC_b
                Continue

op_SBC_ind_x:
                GetIndZPage_X
                testb   $D_Flag, F_Reg          // Decimal mode?
                jnz     op_SBC_dec              // Yes, jump to decimal version
                DoSBC_b
                Continue

op_SBC_ind_y:
                GetIndZPage_Y
                testb   $D_Flag, F_Reg          // Decimal mode?
                jnz     op_SBC_dec              // Yes, jump to decimal version
                DoSBC_b
                Continue

// 65c02 : 0xF2
op_SBC_ind_zpage:
                GetIndZPage
                testb   $D_Flag, F_Reg          // Decimal mode?
                jnz     op_SBC_dec              // Yes, jump to decimal version
                DoSBC_b
                Continue

/* ----------------------------------
       SEC instruction
   ---------------------------------- */

op_SEC:
                orb     $C_Flag, F_Reg
                Continue

/* ----------------------------------
       SED instruction
   ---------------------------------- */

op_SED:
                orb     $D_Flag, F_Reg
                Continue

/* ----------------------------------
       SEI instruction
   ---------------------------------- */

op_SEI:
                orb     $I_Flag, F_Reg
                Continue

/* ----------------------------------
       SMBx instructions -- Available in Rockwell 65C02 but not NCR 65C02
       UNIMPLEMENTED : These are documented in the W65C02S datasheet ...
   ---------------------------------- */

op_SMB0_65c02:
                Continue

op_SMB1_65c02:
                Continue

op_SMB2_65c02:
                Continue

op_SMB3_65c02:
                Continue

op_SMB4_65c02:
                Continue

op_SMB5_65c02:
                Continue

op_SMB6_65c02:
                Continue

op_SMB7_65c02:
                Continue

/* ----------------------------------
       STA instructions
   ---------------------------------- */

op_STA_imm:
                GetImm
                DoSTA
                Continue

op_STA_zpage:
                GetZPage
                DoSTA
                incb    DebugCycleCount         // +1 cycle on write
                Continue

op_STA_zpage_x:
                GetZPage_X
                DoSTA
                Continue

// UNIMPLEMENTED : W65C02S datasheet
op_STA_zpage_y:
                hlt

op_STA_abs:
                GetAbs
                DoSTA
                Continue

op_STA_abs_x:
                GetAbs_X_STx
                DoSTA
                incb    DebugCycleCount         // +1 cycle on write
                Continue

op_STA_abs_y:
                GetAbs_Y_STA
                DoSTA
                incb    DebugCycleCount         // +1 cycle on write
                Continue

op_STA_ind_x:
                GetIndZPage_X
                DoSTA
                Continue

op_STA_ind_y:
                GetIndZPage_Y_STA
                DoSTA
                incb    DebugCycleCount         // +1 cycle on write
                Continue

// 65c02 : 0x92
op_STA_ind_zpage:
                GetIndZPage
                DoSTA
                Continue

/* ----------------------------------
       STP instruction
       UNIMPLEMENTED : This is documented in the W65C02S datasheet ...
   ---------------------------------- */

op_STP_65c02:
                Continue

/* ----------------------------------
       RMBx instructions -- Available in Rockwell 65C02 but not NCR 65C02
       UNIMPLEMENTED : These are documented in the W65C02S datasheet ...
   ---------------------------------- */

op_RMB0_65c02:
                Continue

op_RMB1_65c02:
                Continue

op_RMB2_65c02:
                Continue

op_RMB3_65c02:
                Continue

op_RMB4_65c02:
                Continue

op_RMB5_65c02:
                Continue

op_RMB6_65c02:
                Continue

op_RMB7_65c02:
                Continue

/* ----------------------------------
       STX instructions
   ---------------------------------- */

op_STX_zpage:
                GetZPage
                DoSTX
                incb    DebugCycleCount         // +1 cycle on write
                Continue

// HACK : is this used? need to study coverage ...
op_STX_zpage_y:
                GetZPage_Y
                DoSTX
                Continue

op_STX_abs:
                GetAbs
                DoSTX
                Continue

/* ----------------------------------
       STY instructions
   ---------------------------------- */

op_STY_zpage:
                GetZPage
                DoSTY
                incb    DebugCycleCount         // +1 cycle on write
                Continue

op_STY_zpage_x:
                GetZPage_X
                DoSTY
                Continue

op_STY_abs:
                GetAbs
                DoSTY
                Continue

/* ----------------------------------
       STZ instructions
       65c02 only
   ---------------------------------- */

// 65c02 : 0x64
op_STZ_zpage:
                GetZPage
                DoSTZ
                incb    DebugCycleCount         // +1 cycle on write
                Continue

// 65c02 : 0x74
op_STZ_zpage_x:
                GetZPage_X
                Continue

// 65c02 : 0x9C
op_STZ_abs:
                GetAbs
                DoSTZ
                Continue

// 65c02 : 0x9E
op_STZ_abs_x:
                GetAbs_X_STx
                DoSTZ
                incb    DebugCycleCount         // +1 cycle on write
                Continue

/* ----------------------------------
       TAX instruction
   ---------------------------------- */

op_TAX:
                movb    A_Reg, X_Reg
                orb     X_Reg, X_Reg
                FlagNZ
                Continue

/* ----------------------------------
       TAY instruction
   ---------------------------------- */

op_TAY:
                movb    A_Reg, Y_Reg
                orb     Y_Reg, Y_Reg
                FlagNZ
                Continue

/* ----------------------------------
       TRB instructions
       65c02 only
   ---------------------------------- */

// 65c02 : 0x1C
op_TRB_abs:
                GetAbs
                DoTRB
                Continue

// 65c02 : 0x14
op_TRB_zpage:
                GetZPage
                DoTRB
                Continue

/* ----------------------------------
       TSB instructions
       65c02 only
   ---------------------------------- */

op_TSB_abs: // 65c02 : 0x0C
                GetAbs
                DoTSB
                Continue

op_TSB_zpage: // 65c02 : 0x04
                GetZPage
                DoTSB
                Continue

/* ----------------------------------
       TSX instruction
   ---------------------------------- */

op_TSX:
                movb    SP_Reg_L, X_Reg
                orb     X_Reg, X_Reg
                FlagNZ
                Continue

/* ----------------------------------
       TXA instruction
   ---------------------------------- */

op_TXA:
                movb    X_Reg, A_Reg
                orb     A_Reg, A_Reg
                FlagNZ
                Continue

/* ----------------------------------
       TXS instruction
   ---------------------------------- */

op_TXS:
                movb    X_Reg, SP_Reg_L
                Continue

/* ----------------------------------
       TYA instruction
   ---------------------------------- */

op_TYA:
                movb    Y_Reg, A_Reg
                orb     A_Reg, A_Reg
                FlagNZ
                Continue

/* ----------------------------------
       ??? instruction - 65c02
       Defined as NOPs by spec
   ---------------------------------- */
op_UNK_65c02:
                Continue

/* ----------------------------------
       WAI instruction - 65c02
       UNIMPLEMENTED : This is documented in the W65C02S datasheet ...
   ---------------------------------- */
op_WAI_65c02:
                Continue

#pragma mark -
#pragma mark undocumented

/* ----------------------------------------------------------------------
       Undocumented NMOS 6502 (Illegal instructions)
       Specification : Some terminate only by reset. Results are undefined.
   ---------------------------------------------------------------------- */

/* AAX = A AND X -> M */
#define DoAAX   movb    A_Reg, %al;                                            \
                andb    X_Reg, %al;                                            \
                FlagNZ                                                         \
                PutToEA_B

/* AMA = ORA 238, AND M, TAX */
#define DoAMA   orb     $238, A_Reg;                                           \
                GetFromEA_B                                                    \
                andb    %al, A_Reg;                                            \
                movb    A_Reg, X_Reg;                                          \
                FlagNZ

/* ANA = AND M, Carry = BIT 7 */
/* NB: assumes A_Reg = %cl */
#define DoANA   GetFromEA_B                                                    \
                andb    %al, A_Reg;                                            \
                bt      $7, %ecx;                                              \
                FlagNZC

/* ANB = same as ANA */
#define DoANB   DoANA

/* AXM = (A AND X) - M -> X */
#define DoAXM   GetFromEA_B                                                    \
                andb    A_Reg, X_Reg;                                          \
                notb    %al;                                                   \
                bt      $C_Flag_Bit, FF_Reg;                                   \
                adcb    %al, X_Reg;                                            \
                FlagNVZC

/* AXS = (A AND X) -> S, A AND X AND 17 -> M */
#define DoAXS   movb    A_Reg, SP_Reg_L;                                       \
                andb    X_Reg, SP_Reg_L;                                       \
                movb    SP_Reg_L, %al;                                         \
                andb    $17, %al;                                              \
                FlagNZ                                                         \
                PutToEA_B

/* DCP = DEC M, CMP M */
#define DoDCP   GetFromEA_B                                                    \
                decb    %al;                                                   \
                PutToEA_B                                                      \
                negb    %al;                                                   \
                addb    A_Reg, %al;                                            \
                FlagNZC

/* ISB = INC M, SBC M */
#define DoISB_b GetFromEA_B                                                    \
                incb    %al;                                                   \
                PutToEA_B                                                      \
                notb    %al;                                                   \
                bt      $C_Flag_Bit, FF_Reg;                                   \
                adcb    %al, A_Reg;                                            \
                FlagNVZC

#define DoISB_d GetFromEA_B                                                    \
                incb    %al;                                                   \
                PutToEA_B                                                      \
                bt      $C_Flag_Bit, FF_Reg;                                   \
                cmc;                                                           \
                xchgb   A_Reg, %al;                                            \
                sbbb    A_Reg, %al;                                            \
                das;                                                           \
                movb    %al, A_Reg;                                            \
                cmc;                                                           \
                FlagNVZC

/* LAN = ROL M, AND M */
#define DoLAN   GetFromEA_B                                                    \
                bt      $C_Flag_Bit, FF_Reg;                                   \
                adcl    %eax, %eax;                                            \
                andb    %al, A_Reg;                                            \
                bt      $8, %eax;                                              \
                FlagNZC                                                        \
                PutToEA_B

/* LAS = LDA M, TAX, TXS */
#define DoLAS   GetFromEA_B                                                    \
                movb    %al, A_Reg;                                            \
                movb    %al, X_Reg;                                            \
                movb    %al, SP_Reg_L;                                         \
                orb     %al, %al;                                              \
                FlagNZ

/* LAX = LDA M, TAX */
#define DoLAX   GetFromEA_B                                                    \
                movb    %al, A_Reg;                                            \
                movb    %al, X_Reg;                                            \
                orb     %al, %al;                                              \
                FlagNZ


/* LOR = ASL M, ORA M */
#define DoLOR   GetFromEA_B                                                    \
                addb    %al, %al;                                              \
                FlagC                                                          \
                PutToEA_B                                                      \
                orb     %al, A_Reg;                                            \
                FlagNZ

/* RAD = ROR M, ADC M */
#define DoRAD_b GetFromEA_B                                                    \
                bt      $C_Flag_Bit, FF_Reg;                                   \
                rcrb    $1, %al;                                               \
                adcb    %al, A_Reg;                                            \
                pushl   %eax;                                                  \
                FlagNVZC                                                       \
                popl    %eax;                                                  \
                PutToEA_B

#define DoRAD_d GetFromEA_B                                                    \
                bt      $C_Flag_Bit, FF_Reg;                                   \
                rcrb    $1, %al;                                               \
                pushfl;                                                        \
                PutToEA_B                                                      \
                popfl;                                                         \
                adcb    A_Reg, %al;                                            \
                daa;                                                           \
                movb    %al, A_Reg;                                            \
                FlagNVZC

/* RAM = AND M, LSR A */
#define DoRAM   GetFromEA_B                                                    \
                andb    %al, A_Reg;                                            \
                shrb    $1, A_Reg;                                             \
                FlagNZC

/* RBM = same as RAM */
#define DoRBM   DoRAM

/* REO = LSR M, EOR M */
#define DoREO   GetFromEA_B                                                    \
                shrb    $1, %al;                                               \
                xorb    %al, A_Reg;                                            \
                FlagNZC                                                        \
                PutToEA_B                                                      \

/* DoZBC = same as SBC */
#define DoZBC_b DoSBC_b
#define DoZBC_d DoSBC_d

/* TEA = (A AND X AND (OP+2)+1) -> M */
#define DoTEA   pushl   EffectiveAddr_E;                                       \
                movw    PC_Reg, EffectiveAddr;                                 \
                decw    EffectiveAddr;                                         \
                GetFromEA_B                                                    \
                popl    EffectiveAddr_E;                                       \
                incb    %al;                                                   \
                andb    A_Reg, %al;                                            \
                andb    X_Reg, %al;                                            \
                FlagNZ                                                         \
                PutToEA_B

/* TEX = (X AND (OP+2)+1) -> M */
#define DoTEX   pushl   EffectiveAddr_E;                                       \
                movw    PC_Reg, EffectiveAddr;                                 \
                decw    EffectiveAddr;                                         \
                GetFromEA_B                                                    \
                popl    EffectiveAddr_E;                                       \
                incb    %al;                                                   \
                andb    X_Reg, %al;                                            \
                FlagNZ                                                         \
                PutToEA_B

/* TEY = (Y AND 1) -> M */
#define DoTEY   movb    Y_Reg, %al;                                            \
                andb    $1, %al;                                               \
                FlagNZ                                                         \
                PutToEA_B

/* XMA = (X AND M) AND (A OR 238) -> A */
#define DoXMA                                                                  \
                GetFromEA_B                                                    \
                andb    X_Reg, %al;                                            \
                orb     $238, A_Reg;                                           \
                andb    %al, A_Reg;                                            \
                FlagNZ


/* ----------------------------------
       UNK HANG instruction
   ---------------------------------- */

op_UNK_HANG:
                decw    PC_Reg
                Continue

/* ----------------------------------
       UNK NOP_2 instruction
   ---------------------------------- */

op_UNK_NOP_2:
                incw    PC_Reg
                Continue

/* ----------------------------------
       UNK NOP_3 instruction
   ---------------------------------- */

op_UNK_NOP_3:
                addw    $2, PC_Reg
                Continue

/* ----------------------------------
       UNK AAX instructions
   ---------------------------------- */

op_UNK_AAX_abs:
                GetAbs
                DoAAX
                Continue

op_UNK_AAX_zpage:
                GetZPage
                DoAAX
                Continue

op_UNK_AAX_zpage_y:
                GetZPage_Y
                DoAAX
                Continue

op_UNK_AAX_ind_x:
                GetIndZPage_X
                DoAAX
                Continue

op_UNK_AAX_ind_y:
                GetIndZPage_Y
                DoAAX
                Continue

/* ----------------------------------
       UNK AMA instruction
   ---------------------------------- */

op_UNK_AMA_imm:
                GetImm
                DoAMA
                Continue

/* ----------------------------------
       UNK ANA instruction
   ---------------------------------- */

op_UNK_ANA_imm:
                GetImm
                DoANA
                Continue

/* ----------------------------------
       UNK ANB instruction
   ---------------------------------- */

op_UNK_ANB_imm:
                GetImm
                DoANB
                Continue

/* ----------------------------------
       UNK AXM instruction
   ---------------------------------- */

op_UNK_AXM_imm:
                GetImm
                DoAXM
                Continue

/* ----------------------------------
       UNK AXS instruction
   ---------------------------------- */

op_UNK_AXS_abs_y:
                GetAbs_Y
                DoAXS
                Continue

/* ----------------------------------
       UNK DCP instructions
   ---------------------------------- */

op_UNK_DCP_zpage:
                GetZPage
                DoDCP
                Continue

op_UNK_DCP_zpage_x:
                GetZPage_X
                DoDCP
                Continue

op_UNK_DCP_abs:
                GetAbs
                DoDCP
                Continue

op_UNK_DCP_abs_x:
                GetAbs_X
                DoDCP
                Continue

op_UNK_DCP_abs_y:
                GetAbs_Y
                DoDCP
                Continue

op_UNK_DCP_ind_x:
                GetIndZPage_X
                DoDCP
                Continue

op_UNK_DCP_ind_y:
                GetIndZPage_Y
                DoDCP
                Continue

/* ----------------------------------
       UNK ISB instructions
   ---------------------------------- */

op_UNK_ISB_dec:
                DoISB_d
                Continue

op_UNK_ISB_zpage:
                GetZPage
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_UNK_ISB_dec              # Yes, jump to decimal version
                DoISB_b
                Continue

op_UNK_ISB_zpage_x:
                GetZPage_X
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_UNK_ISB_dec              # Yes, jump to decimal version
                DoISB_b
                Continue


op_UNK_ISB_abs:
                GetAbs
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_UNK_ISB_dec              # Yes, jump to decimal version
                DoISB_b
                Continue

op_UNK_ISB_abs_x:
                GetAbs_X
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_UNK_ISB_dec              # Yes, jump to decimal version
                DoISB_b
                Continue

op_UNK_ISB_abs_y:
                GetAbs_Y
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_UNK_ISB_dec              # Yes, jump to decimal version
                DoISB_b
                Continue

op_UNK_ISB_ind_x:
                GetIndZPage_X
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_UNK_ISB_dec              # Yes, jump to decimal version
                DoISB_b
                Continue

op_UNK_ISB_ind_y:
                GetIndZPage_Y
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_UNK_ISB_dec              # Yes, jump to decimal version
                DoISB_b
                Continue

/* ----------------------------------
       UNK LAN instructions
   ---------------------------------- */

op_UNK_LAN_zpage:
                GetZPage
                DoLAN
                Continue

op_UNK_LAN_zpage_x:
                GetZPage_X
                DoLAN
                Continue

op_UNK_LAN_abs:
                GetAbs
                DoLAN
                Continue

op_UNK_LAN_abs_x:
                GetAbs_X
                DoLAN
                Continue

op_UNK_LAN_abs_y:
                GetAbs_Y
                DoLAN
                Continue

op_UNK_LAN_ind_x:
                GetIndZPage_X
                DoLAN
                Continue

op_UNK_LAN_ind_y:
                GetIndZPage_Y
                DoLAN
                Continue

/* ----------------------------------
       UNK LAS instruction
   ---------------------------------- */

op_UNK_LAS_abs_y:
                GetAbs_Y
                DoLAS
                Continue

/* ----------------------------------
       UNK LAX instructions
   ---------------------------------- */

op_UNK_LAX_zpage:
                GetZPage
                DoLAX
                Continue

op_UNK_LAX_zpage_y:
                GetZPage_Y
                DoLAX
                Continue

op_UNK_LAX_abs:
                GetAbs
                DoLAX
                Continue

op_UNK_LAX_abs_y:
                GetAbs_Y
                DoLAX
                Continue

op_UNK_LAX_ind_x:
                GetIndZPage_X
                DoLAX
                Continue

op_UNK_LAX_ind_y:
                GetIndZPage_Y
                DoLAX
                Continue

/* ----------------------------------
       UNK LOR instructions
   ---------------------------------- */

op_UNK_LOR_zpage:
                GetZPage
                DoLOR
                Continue

op_UNK_LOR_zpage_x:
                GetZPage_X
                DoLOR
                Continue

op_UNK_LOR_abs:
                GetAbs
                DoLOR
                Continue

op_UNK_LOR_abs_x:
                GetAbs_X
                DoLOR
                Continue

op_UNK_LOR_abs_y:
                GetAbs_Y
                DoLOR
                Continue

op_UNK_LOR_ind_x:
                GetIndZPage_X
                DoLOR
                Continue

op_UNK_LOR_ind_y:
                GetIndZPage_Y
                DoLOR
                Continue

/* ----------------------------------
       UNK RAD instructions
   ---------------------------------- */

op_UNK_RAD_dec:
                DoRAD_d
                Continue

op_UNK_RAD_zpage:
                GetZPage
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_UNK_RAD_dec              # Yes, jump to decimal version
                DoRAD_b
                Continue

op_UNK_RAD_zpage_x:
                GetZPage_X
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_UNK_RAD_dec          # Yes, jump to decimal version
                DoRAD_b
                Continue

op_UNK_RAD_abs:
                GetAbs
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_UNK_RAD_dec          # Yes, jump to decimal version
                DoRAD_b
                Continue

op_UNK_RAD_abs_x:
                GetAbs_X
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_UNK_RAD_dec          # Yes, jump to decimal version
                DoRAD_b
                Continue

op_UNK_RAD_abs_y:
                GetAbs_Y
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_UNK_RAD_dec          # Yes, jump to decimal version
                DoRAD_b
                Continue

op_UNK_RAD_ind_x:
                GetIndZPage_X
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_UNK_RAD_dec          # Yes, jump to decimal version
                DoRAD_b
                Continue

op_UNK_RAD_ind_y:
                GetIndZPage_Y
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_UNK_RAD_dec          # Yes, jump to decimal version
                DoRAD_b
                Continue

/* ----------------------------------
       UNK RAM instruction
   ---------------------------------- */

op_UNK_RAM_imm:
                GetImm
                DoRAM
                Continue

/* ----------------------------------
       UNK RBM instruction
   ---------------------------------- */

op_UNK_RBM_imm:
                GetImm
                DoRBM
                Continue

/* ----------------------------------
       UNK REO instructions
   ---------------------------------- */

op_UNK_REO_zpage:
                GetZPage
                DoREO
                Continue

op_UNK_REO_zpage_x:
                GetZPage_X
                DoREO
                Continue

op_UNK_REO_abs:
                GetAbs
                DoREO
                Continue

op_UNK_REO_abs_x:
                GetAbs_X
                DoREO
                Continue

op_UNK_REO_abs_y:
                GetAbs_Y
                DoREO
                Continue

op_UNK_REO_ind_x:
                GetIndZPage_X
                DoREO
                Continue

op_UNK_REO_ind_y:
                GetIndZPage_Y
                DoREO
                Continue

/* ----------------------------------
       UNK ZBC instruction
   ---------------------------------- */

op_UNK_ZBC_imm:
                GetImm
                testb   $D_Flag, F_Reg          // Decimal mode?
                jnz     op_UNK_ZBC_dec          // Yes, jump to decimal version
                DoZBC_b
                Continue
op_UNK_ZBC_dec:
                DoZBC_d
                Continue

/* ----------------------------------
       UNK TEA instruction
   ---------------------------------- */

op_UNK_TEA_abs_y:
                GetAbs_Y
                DoTEA
                Continue

/* ----------------------------------
       UNK TEX instruction
   ---------------------------------- */

op_UNK_TEX_abs_y:
                GetAbs_Y
                DoTEX
                Continue

/* ----------------------------------
       UNK TEY instruction
   ---------------------------------- */

op_UNK_TEY_abs_x:
                GetAbs_X
                DoTEY
                Continue

/* ----------------------------------
       UNK XMA instruction
   ---------------------------------- */

op_UNK_XMA_imm:
                GetImm
                DoXMA
                Continue

#pragma mark -
#pragma mark cpu main entry

/* -------------------------------------------------------------------------
        CPU continue
        Keep executing until we've executed >= cpu65_cycles_to_execute
   ------------------------------------------------------------------------- */

continue:
                xorl    %eax, %eax
                movb    DebugCurrOpcode, %al
                movb    SN(cpu65__opcycles)(,%eax,1), %al
                addb    DebugCycleCount, %al
                addw    %ax, SN(cpu65_cycle_count)          // TODO: cycle counting is slightly incorrect, it should be done earlier per instruction ...
                subw    %ax, SN(cpu65_cycles_to_execute)    //       ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ orly?
                jle     exit_cpu65_run
                xorl    %eax, %eax
                orb     SN(cpu65__signal), %al
                jnz     exception
1:              JumpNextInstruction

/* -------------------------------------------------------------------------
        Exception handlers
   ------------------------------------------------------------------------- */

exception:      testb   $RebootSig, %al
                jnz     emul_reinit
                testb   $ResetSig, %al
                jnz     ex_reset
                testb   $DebugStepSig, %al
                jnz     ex_step
                jmp     ex_irq

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
                xorl    %eax,%eax
                movb    F_Reg, %al
                movb    SN(cpu65_flags_encode)(,%eax,1), %al
                Push(%al)
                orb     $(B_Flag | I_Flag), F_Reg
                //andb    $~D_Flag, F_Reg // AppleWin clears Decimal bit?
                andl    $0xFFFF, EffectiveAddr_E// HACK FIXME : there is a bug somewhere that is occasionally corrupting EffectiveAddr_E
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
                JumpNextInstruction
1:              movb    $0, SN(emul_reinitialize)
                /* Zero all registers, as well as the unused 32-bit parts
                 * of variables. (which may need to be kept 0)
                 *
                 * Note: dependent on assignment of registers
                 */
                xorl    %eax, %eax
                xorl    %ebx, %ebx
                xorl    %ecx, %ecx
                xorl    %esi, %esi
                xorl    %edi, %edi
                movl    $0x1FF, %edx # Stack pointer
                jmp     ex_reset

exit_cpu65_run: SaveState                                   // Return to timing loop ...
                popal
                ret

emul_reinit:    movb    $0, SN(cpu65__signal)               // EXIT CPURUN
                movb    $1, SN(emul_reinitialize)
                popal
                ret

/* -------------------------------------------------------------------------
        Debugger hooks
   ------------------------------------------------------------------------- */

ex_step:        orb     $~DebugStepSig, SN(cpu65__signal)
                xorl    %eax, %eax
                SaveState
                call    SN(c_stepping_yield)
                RestoreState
                JumpNextInstruction

E(cpu65_direct_write)
                /* NB: dependent on register choices */
                pushl   %edi
                movl    8(%esp),%edi
                movl    12(%esp),%eax
                call    *SN(cpu65_vmem)+4(,EffectiveAddr_E,8)
                popl    %edi
                ret

#pragma mark -
#pragma mark tables

/* -------------------------------------------------------------------------
        Opcode base cycle counts table
        Derived from data from WDC 65C02S document dated Feb 2004.  Some of the
        timings may be off due to bugs in this, bugs in the documentation, and
        of course since we're not running a Real Time emulator...
   ------------------------------------------------------------------------- */
                .align  1 // 65c02              nmosbrk         nmos undoc
E(cpu65__opcycles)        // ---------------------------------------------------
                .byte   7 // op_BRK             op_BRK          op_BRK        00
/*6*/           .byte   6 // op_ORA_ind_x       op_ORA_ind_x    op_ORA_ind_x
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_HANG
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_LOR_ind_x
                .byte   5 // op_TSB_zpage       op_UNK          op_UNK_NOP_2
                .byte   3 // op_ORA_zpage       op_ORA_zpage    op_ORA_zpage
                .byte   5 // op_ASL_zpage       op_ASL_zpage    op_ASL_zpage
                .byte   5 // op_RMB0_65c02      op_UNK          op_UNK_LOR_zpage
                .byte   3 // op_PHP             op_PHP          op_PHP        08
                .byte   2 // op_ORA_imm         op_ORA_imm      op_ORA_imm
                .byte   2 // op_ASL_acc         op_ASL_acc      op_ASL_acc
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_ANA_imm
                .byte   6 // op_TSB_abs         op_UNK          op_UNK_NOP_3
                .byte   4 // op_ORA_abs         op_ORA_abs      op_ORA_abs
                .byte   6 // op_ASL_abs         op_ASL_abs      op_ASL_abs
                .byte   5 // op_BBR0_65c02      op_UNK          op_UNK_LOR_abs
                .byte   2 // op_BPL             op_BPL          op_BPL        10
                .byte   5 // op_ORA_ind_y       op_ORA_ind_y    op_ORA_ind_y
                .byte   5 // op_ORA_ind_zpage   op_UNK          op_UNK_HANG
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_LOR_ind_y
                .byte   5 // op_TRB_zpage       op_UNK          op_UNK_NOP_2
                .byte   4 // op_ORA_zpage_x     op_ORA_zpage_x  op_ORA_zpage_x
                .byte   6 // op_ASL_zpage_x     op_ASL_zpage_x  op_ASL_zpage_x
                .byte   5 // op_RMB1_65c02      op_UNK          op_UNK_LOR_zpage_x
                .byte   2 // op_CLC             op_CLC          op_CLC        18
                .byte   4 // op_ORA_abs_y       op_ORA_abs_y    op_ORA_abs_y
                .byte   2 // op_INA             op_UNK          op_NOP
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_LOR_abs_y
                .byte   6 // op_TRB_abs         op_UNK          op_UNK_NOP_3
                .byte   4 // op_ORA_abs_x       op_ORA_abs_x    op_ORA_abs_x
                .byte   6 // op_ASL_abs_x       op_ASL_abs_x    op_ASL_abs_x
                .byte   5 // op_BBR1_65c02      op_UNK          op_UNK_LOR_abs_x
                .byte   6 // op_JSR             op_JSR          op_JSR        20
                .byte   6 // op_AND_ind_x       op_AND_ind_x    op_AND_ind_x
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_HANG
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_LAN_ind_x
                .byte   3 // op_BIT_zpage       op_BIT_zpage    op_BIT_zpage
                .byte   3 // op_AND_zpage       op_AND_zpage    op_AND_zpage
                .byte   5 // op_ROL_zpage       op_ROL_zpage    op_ROL_zpage
                .byte   5 // op_RMB2_65c02      op_UNK          op_UNK_LAN_zpage
                .byte   4 // op_PLP             op_PLP          op_PLP        28
                .byte   2 // op_AND_imm         op_AND_imm      op_AND_imm
                .byte   2 // op_ROL_acc         op_ROL_acc      op_ROL_acc
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_ANB_imm
                .byte   4 // op_BIT_abs         op_BIT_abs      op_BIT_abs
                .byte   4 // op_AND_abs         op_AND_abs      op_AND_abs
                .byte   6 // op_ROL_abs         op_ROL_abs      op_ROL_abs
                .byte   5 // op_BBR2_65c02      op_UNK          op_UNK_LAN_abs
                .byte   2 // op_BMI             op_BMI          op_BMI        30
                .byte   5 // op_AND_ind_y       op_AND_ind_y    op_AND_ind_y
                .byte   5 // op_AND_ind_zpage   op_UNK          op_UNK_HANG
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_LAN_ind_y
                .byte   4 // op_BIT_zpage_x     op_UNK          op_UNK_NOP_2
                .byte   4 // op_AND_zpage_x     op_AND_zpage_x  op_AND_zpage_x
                .byte   6 // op_ROL_zpage_x     op_ROL_zpage_x  op_ROL_zpage_x
                .byte   5 // op_RMB3_65c02      op_UNK          op_UNK_LAN_zpage_x
                .byte   2 // op_SEC             op_SEC          op_SEC        38
                .byte   4 // op_AND_abs_y       op_AND_abs_y    op_AND_abs_y
                .byte   2 // op_DEA             op_UNK          op_NOP
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_LAN_abs_y
                .byte   4 // op_BIT_abs_x       op_UNK          op_UNK_NOP_3
                .byte   4 // op_AND_abs_x       op_AND_abs_x    op_AND_abs_x
                .byte   6 // op_ROL_abs_x       op_ROL_abs_x    op_ROL_abs_x
                .byte   5 // op_BBR3_65c02      op_UNK          op_UNK_LAN_abs_x
                .byte   6 // op_RTI             op_RTI          op_RTI        40
                .byte   6 // op_EOR_ind_x       op_EOR_ind_x    op_EOR_ind_x
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_HANG
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_REO_ind_x
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_NOP_2
                .byte   3 // op_EOR_zpage       op_EOR_zpage    op_EOR_zpage
                .byte   5 // op_LSR_zpage       op_LSR_zpage    op_LSR_zpage
                .byte   5 // op_RMB4_65c02      op_UNK          op_UNK_REO_zpage
                .byte   3 // op_PHA             op_PHA          op_PHA        48
                .byte   2 // op_EOR_imm         op_EOR_imm      op_EOR_imm
                .byte   2 // op_LSR_acc         op_LSR_acc      op_LSR_acc
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_RAM_imm
                .byte   3 // op_JMP_abs         op_JMP_abs      op_JMP_abs
                .byte   4 // op_EOR_abs         op_EOR_abs      op_EOR_abs
                .byte   6 // op_LSR_abs         op_LSR_abs      op_LSR_abs
                .byte   5 // op_BBR4_65c02      op_UNK          op_UNK_REO_abs
                .byte   2 // op_BVC             op_BVC          op_BVC        50
                .byte   5 // op_EOR_ind_y       op_EOR_ind_y    op_EOR_ind_y
                .byte   5 // op_EOR_ind_zpage   op_UNK          op_UNK_HANG
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_REO_ind_y
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_NOP_2
                .byte   4 // op_EOR_zpage_x     op_EOR_zpage_x  op_EOR_zpage_x
                .byte   6 // op_LSR_zpage_x     op_LSR_zpage_x  op_LSR_zpage_x
                .byte   5 // op_RMB5_65c02      op_UNK          op_UNK_REO_zpage_x
                .byte   2 // op_CLI             op_CLI          op_CLI        58
                .byte   4 // op_EOR_abs_y       op_EOR_abs_y    op_EOR_abs_y
                .byte   3 // op_PHY             op_UNK          op_NOP
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_REO_abs_y
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_NOP_3
                .byte   4 // op_EOR_abs_x       op_EOR_abs_x    op_EOR_abs_x
                .byte   6 // op_LSR_abs_x       op_LSR_abs_x    op_LSR_abs_x
                .byte   5 // op_BBR5_65c02      op_UNK          op_UNK_REO_abs_x
                .byte   6 // op_RTS             op_RTS          op_RTS        60
                .byte   6 // op_ADC_ind_x       op_ADC_ind_x    op_ADC_ind_x
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_HANG
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_RAD_ind_x
                .byte   3 // op_STZ_zpage       op_UNK          op_UNK_NOP_2
                .byte   3 // op_ADC_zpage       op_ADC_zpage    op_ADC_zpage
                .byte   5 // op_ROR_zpage       op_ROR_zpage    op_ROR_zpage
                .byte   5 // op_RMB6_65c02      op_UNK          op_UNK_RAD_zpage
                .byte   4 // op_PLA             op_PLA          op_PLA        68
                .byte   2 // op_ADC_imm         op_ADC_imm      op_ADC_imm
                .byte   2 // op_ROR_acc         op_ROR_acc      op_ROR_acc
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_RBM_imm
                .byte   6 // op_JMP_ind_65c02   op_JMP_ind      op_JMP_ind
                .byte   4 // op_ADC_abs         op_ADC_abs      op_ADC_abs
                .byte   6 // op_ROR_abs         op_ROR_abs      op_ROR_abs
                .byte   5 // op_BBR6_65c02      op_UNK          op_UNK_RAD_abs
                .byte   2 // op_BVS             op_BVS          op_BVS        70
                .byte   5 // op_ADC_ind_y       op_ADC_ind_y    op_ADC_ind_y
                .byte   5 // op_ADC_ind_zpage   op_UNK          op_UNK_HANG
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_RAD_ind_y
                .byte   4 // op_STZ_zpage_x     op_UNK          op_UNK_NOP_2
                .byte   4 // op_ADC_zpage_x     op_ADC_zpage_x  op_ADC_zpage_x
                .byte   6 // op_ROR_zpage_x     op_ROR_zpage_x  op_ROR_zpage_x
                .byte   5 // op_RMB7_65c02      op_UNK          op_UNK_RAD_zpage_x
                .byte   2 // op_SEI             op_SEI          op_SEI        78
                .byte   4 // op_ADC_abs_y       op_ADC_abs_y    op_ADC_abs_y
                .byte   4 // op_PLY             op_UNK          op_NOP
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_RAD_abs_y
                .byte   6 // op_JMP_abs_ind_x   op_UNK          op_UNK_NOP_3
                .byte   4 // op_ADC_abs_x       op_ADC_abs_x    op_ADC_abs_x
                .byte   6 // op_ROR_abs_x       op_ROR_abs_x    op_ROR_abs_x
                .byte   5 // op_BBR7_65c02      op_UNK          op_UNK_RAD_abs_x
                .byte   2 // op_BRA             op_UNK          op_UNK_NOP_2  80
                .byte   6 // op_STA_ind_x       op_STA_ind_x    op_STA_ind_x
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_NOP_2
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_AAX_ind_x
                .byte   3 // op_STY_zpage       op_STY_zpage    op_STY_zpage
                .byte   3 // op_STA_zpage       op_STA_zpage    op_STA_zpage
                .byte   3 // op_STX_zpage       op_STX_zpage    op_STX_zpage
                .byte   5 // op_SMB0_65c02      op_UNK          op_UNK_AAX_zpage
                .byte   2 // op_DEY             op_DEY          op_DEY        88
                .byte   2 // op_BIT_imm         op_UNK          op_UNK_NOP_2
                .byte   2 // op_TXA             op_TXA          op_TXA
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_XMA_imm
                .byte   4 // op_STY_abs         op_STY_abs      op_STY_abs
                .byte   4 // op_STA_abs         op_STA_abs      op_STA_abs
                .byte   4 // op_STX_abs         op_STX_abs      op_STX_abs
                .byte   5 // op_BBS0_65c02      op_UNK          op_UNK_AAX_abs
                .byte   2 // op_BCC             op_BCC          op_BCC        90
                .byte   6 // op_STA_ind_y       op_STA_ind_y    op_STA_ind_y
                .byte   5 // op_STA_ind_zpage   op_UNK          op_UNK_HANG
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_AAX_ind_y
                .byte   4 // op_STY_zpage_x     op_STY_zpage_x  op_STY_zpage_x
                .byte   4 // op_STA_zpage_x     op_STA_zpage_x  op_STA_zpage_x
                .byte   4 // op_STX_zpage_y     op_STX_zpage_y  op_STX_zpage_y
                .byte   5 // op_SMB1_65c02      op_UNK          op_UNK_AAX_zpage_y
                .byte   2 // op_TYA             op_TYA          op_TYA        98
                .byte   5 // op_STA_abs_y       op_STA_abs_y    op_STA_abs_y
                .byte   2 // op_TXS             op_TXS          op_TXS
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_AXS_abs_y
                .byte   4 // op_STZ_abs         op_UNK          op_UNK_TEY_abs_x
                .byte   5 // op_STA_abs_x       op_STA_abs_x    op_STA_abs_x
                .byte   5 // op_STZ_abs_x       op_UNK          op_UNK_TEX_abs_y
                .byte   5 // op_BBS1_65c02      op_UNK          op_UNK_TEA_abs_y
                .byte   2 // op_LDY_imm         op_LDY_imm      op_LDY_imm    A0
                .byte   6 // op_LDA_ind_x       op_LDA_ind_x    op_LDA_ind_x
                .byte   2 // op_LDX_imm         op_LDX_imm      op_LDX_imm
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_LAX_ind_x
                .byte   3 // op_LDY_zpage       op_LDY_zpage    op_LDY_zpage
                .byte   3 // op_LDA_zpage       op_LDA_zpage    op_LDA_zpage
                .byte   3 // op_LDX_zpage       op_LDX_zpage    op_LDX_zpage
                .byte   5 // op_SMB2_65c02      op_UNK          op_UNK_LAX_zpage
                .byte   2 // op_TAY             op_TAY          op_TAY        A8
                .byte   2 // op_LDA_imm         op_LDA_imm      op_LDA_imm
                .byte   2 // op_TAX             op_TAX          op_TAX
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_AMA_imm
                .byte   4 // op_LDY_abs         op_LDY_abs      op_LDY_abs
                .byte   4 // op_LDA_abs         op_LDA_abs      op_LDA_abs
                .byte   4 // op_LDX_abs         op_LDX_abs      op_LDX_abs
                .byte   5 // op_BBS2_65c02      op_UNK          op_UNK_LAX_abs
                .byte   2 // op_BCS             op_BCS          op_BCS        B0
                .byte   5 // op_LDA_ind_y       op_LDA_ind_y    op_LDA_ind_y
                .byte   5 // op_LDA_ind_zpage   op_UNK          op_UNK_HANG
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_LAX_ind_y
                .byte   4 // op_LDY_zpage_x     op_LDY_zpage_x  op_LDY_zpage_x
                .byte   4 // op_LDA_zpage_x     op_LDA_zpage_x  op_LDA_zpage_x
                .byte   4 // op_LDX_zpage_y     op_LDX_zpage_y  op_LDX_zpage_y
                .byte   5 // op_SMB3_65c02      op_UNK          op_UNK_LAX_zpage_y
                .byte   2 // op_CLV             op_CLV          op_CLV        B8
                .byte   4 // op_LDA_abs_y       op_LDA_abs_y    op_LDA_abs_y
                .byte   2 // op_TSX             op_TSX          op_TSX
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_LAS_abs_y
                .byte   4 // op_LDY_abs_x       op_LDY_abs_x    op_LDY_abs_x
                .byte   4 // op_LDA_abs_x       op_LDA_abs_x    op_LDA_abs_x
                .byte   4 // op_LDX_abs_y       op_LDX_abs_y    op_LDX_abs_y
                .byte   5 // op_BBS3_65c02      op_UNK          op_UNK_LAX_abs_y
                .byte   2 // op_CPY_imm         op_CPY_imm      op_CPY_imm    C0
                .byte   6 // op_CMP_ind_x       op_CMP_ind_x    op_CMP_ind_x
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_NOP_2
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_DCP_ind_x
                .byte   3 // op_CPY_zpage       op_CPY_zpage    op_CPY_zpage
                .byte   3 // op_CMP_zpage       op_CMP_zpage    op_CMP_zpage
                .byte   5 // op_DEC_zpage       op_DEC_zpage    op_DEC_zpage
                .byte   5 // op_SMB4_65c02      op_UNK          op_UNK_DCP_zpage
                .byte   2 // op_INY             op_INY          op_INY        C8
                .byte   2 // op_CMP_imm         op_CMP_imm      op_CMP_imm
                .byte   2 // op_DEX             op_DEX          op_DEX
                .byte   7 // op_WAI_65c02       op_UNK          op_UNK_AXM_imm
                .byte   4 // op_CPY_abs         op_CPY_abs      op_CPY_abs
                .byte   4 // op_CMP_abs         op_CMP_abs      op_CMP_abs
                .byte   6 // op_DEC_abs         op_DEC_abs      op_DEC_abs
                .byte   5 // op_BBS4_65c02      op_UNK          op_UNK_DCP_abs
                .byte   2 // op_BNE             op_BNE          op_BNE        D0
                .byte   5 // op_CMP_ind_y       op_CMP_ind_y    op_CMP_ind_y
                .byte   5 // op_CMP_ind_zpage   op_UNK          op_UNK_HANG
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_DCP_ind_y
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_NOP_2
                .byte   4 // op_CMP_zpage_x     op_CMP_zpage_x  op_CMP_zpage_x
                .byte   6 // op_DEC_zpage_x     op_DEC_zpage_x  op_DEC_zpage_x
                .byte   5 // op_SMB5_65c02      op_UNK          op_UNK_DCP_zpage_x
                .byte   2 // op_CLD             op_CLD          op_CLD        D8
                .byte   4 // op_CMP_abs_y       op_CMP_abs_y    op_CMP_abs_y
                .byte   3 // op_PHX             op_UNK          op_NOP
                .byte   7 // op_STP_65c02       op_UNK          op_UNK_DCP_abs_y
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_NOP_3
                .byte   4 // op_CMP_abs_x       op_CMP_abs_x    op_CMP_abs_x
                .byte   6 // op_DEC_abs_x       op_DEC_abs_x    op_DEC_abs_x
                .byte   5 // op_BBS5_65c02      op_UNK          op_UNK_DCP_abs_x
                .byte   2 // op_CPX_imm         op_CPX_imm      op_CPX_imm    E0
                .byte   6 // op_SBC_ind_x       op_SBC_ind_x    op_SBC_ind_x
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_NOP_2
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_ISB_ind_x
                .byte   3 // op_CPX_zpage       op_CPX_zpage    op_CPX_zpage
                .byte   3 // op_SBC_zpage       op_SBC_zpage    op_SBC_zpage
                .byte   5 // op_INC_zpage       op_INC_zpage    op_INC_zpage
                .byte   5 // op_SMB6_65c02      op_UNK          op_UNK_ISB_zpage
                .byte   2 // op_INX             op_INX          op_INX        E8
                .byte   2 // op_SBC_imm         op_SBC_imm      op_SBC_imm
                .byte   2 // op_NOP             op_NOP          op_NOP
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_ZBC_imm
                .byte   4 // op_CPX_abs         op_CPX_abs      op_CPX_abs
                .byte   4 // op_SBC_abs         op_SBC_abs      op_SBC_abs
                .byte   6 // op_INC_abs         op_INC_abs      op_INC_abs
                .byte   5 // op_BBS6_65c02      op_UNK          op_UNK_ISB_abs
                .byte   2 // op_BEQ             op_BEQ          op_BEQ        F0
                .byte   5 // op_SBC_ind_y       op_SBC_ind_y    op_SBC_ind_y
                .byte   5 // op_SBC_ind_zpage   op_UNK          op_UNK_HANG
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_ISB_ind_y
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_NOP_2
                .byte   4 // op_SBC_zpage_x     op_SBC_zpage_x  op_SBC_zpage_x
                .byte   6 // op_INC_zpage_x     op_INC_zpage_x  op_INC_zpage_x
                .byte   5 // op_SMB7_65c02      op_UNK          op_UNK_ISB_zpage_x
                .byte   2 // op_SED             op_SED          op_SED        F8
                .byte   4 // op_SBC_abs_y       op_SBC_abs_y    op_SBC_abs_y
                .byte   4 // op_PLX             op_UNK          op_NOP
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_ISB_abs_y
                .byte   7 // op_UNK_65c02       op_UNK          op_UNK_NOP_3
                .byte   4 // op_SBC_abs_x       op_SBC_abs_x    op_SBC_abs_x
                .byte   6 // op_INC_abs_x       op_INC_abs_x    op_INC_abs_x
                .byte   5 // op_BBS7_65c02      op_UNK          op_UNK_ISB_abs_x

/* -------------------------------------------------------------------------
        Opcode Routine Jump Tables
   ------------------------------------------------------------------------- */

                .align  4
E(cpu65__nmosbrk)
                .long   op_BRK
                .long   op_ORA_ind_x
                .long   op_UNK
                .long   op_UNK
                .long   op_UNK
                .long   op_ORA_zpage
                .long   op_ASL_zpage
                .long   op_UNK
                .long   op_PHP
                .long   op_ORA_imm
                .long   op_ASL_acc
                .long   op_UNK
                .long   op_UNK
                .long   op_ORA_abs
                .long   op_ASL_abs
                .long   op_UNK
                .long   op_BPL
                .long   op_ORA_ind_y
                .long   op_UNK
                .long   op_UNK
                .long   op_UNK
                .long   op_ORA_zpage_x
                .long   op_ASL_zpage_x
                .long   op_UNK
                .long   op_CLC
                .long   op_ORA_abs_y
                .long   op_UNK
                .long   op_UNK
                .long   op_UNK
                .long   op_ORA_abs_x
                .long   op_ASL_abs_x
                .long   op_UNK
                .long   op_JSR
                .long   op_AND_ind_x
                .long   op_UNK
                .long   op_UNK
                .long   op_BIT_zpage
                .long   op_AND_zpage
                .long   op_ROL_zpage
                .long   op_UNK
                .long   op_PLP
                .long   op_AND_imm
                .long   op_ROL_acc
                .long   op_UNK
                .long   op_BIT_abs
                .long   op_AND_abs
                .long   op_ROL_abs
                .long   op_UNK
                .long   op_BMI
                .long   op_AND_ind_y
                .long   op_UNK
                .long   op_UNK
                .long   op_UNK
                .long   op_AND_zpage_x
                .long   op_ROL_zpage_x
                .long   op_UNK
                .long   op_SEC
                .long   op_AND_abs_y
                .long   op_UNK
                .long   op_UNK
                .long   op_UNK
                .long   op_AND_abs_x
                .long   op_ROL_abs_x
                .long   op_UNK
                .long   op_RTI
                .long   op_EOR_ind_x
                .long   op_UNK
                .long   op_UNK
                .long   op_UNK
                .long   op_EOR_zpage
                .long   op_LSR_zpage
                .long   op_UNK
                .long   op_PHA
                .long   op_EOR_imm
                .long   op_LSR_acc
                .long   op_UNK
                .long   op_JMP_abs
                .long   op_EOR_abs
                .long   op_LSR_abs
                .long   op_UNK
                .long   op_BVC
                .long   op_EOR_ind_y
                .long   op_UNK
                .long   op_UNK
                .long   op_UNK
                .long   op_EOR_zpage_x
                .long   op_LSR_zpage_x
                .long   op_UNK
                .long   op_CLI
                .long   op_EOR_abs_y
                .long   op_UNK
                .long   op_UNK
                .long   op_UNK
                .long   op_EOR_abs_x
                .long   op_LSR_abs_x
                .long   op_UNK
                .long   op_RTS
                .long   op_ADC_ind_x
                .long   op_UNK
                .long   op_UNK
                .long   op_UNK
                .long   op_ADC_zpage
                .long   op_ROR_zpage
                .long   op_UNK
                .long   op_PLA
                .long   op_ADC_imm
                .long   op_ROR_acc
                .long   op_UNK
                .long   op_JMP_ind
                .long   op_ADC_abs
                .long   op_ROR_abs
                .long   op_UNK
                .long   op_BVS
                .long   op_ADC_ind_y
                .long   op_UNK
                .long   op_UNK
                .long   op_UNK
                .long   op_ADC_zpage_x
                .long   op_ROR_zpage_x
                .long   op_UNK
                .long   op_SEI
                .long   op_ADC_abs_y
                .long   op_UNK
                .long   op_UNK
                .long   op_UNK
                .long   op_ADC_abs_x
                .long   op_ROR_abs_x
                .long   op_UNK
                .long   op_UNK
                .long   op_STA_ind_x
                .long   op_UNK
                .long   op_UNK
                .long   op_STY_zpage
                .long   op_STA_zpage
                .long   op_STX_zpage
                .long   op_UNK
                .long   op_DEY
                .long   op_UNK
                .long   op_TXA
                .long   op_UNK
                .long   op_STY_abs
                .long   op_STA_abs
                .long   op_STX_abs
                .long   op_UNK
                .long   op_BCC
                .long   op_STA_ind_y
                .long   op_UNK
                .long   op_UNK
                .long   op_STY_zpage_x
                .long   op_STA_zpage_x
                .long   op_STX_zpage_y
                .long   op_UNK
                .long   op_TYA
                .long   op_STA_abs_y
                .long   op_TXS
                .long   op_UNK
                .long   op_UNK
                .long   op_STA_abs_x
                .long   op_UNK
                .long   op_UNK
                .long   op_LDY_imm
                .long   op_LDA_ind_x
                .long   op_LDX_imm
                .long   op_UNK
                .long   op_LDY_zpage
                .long   op_LDA_zpage
                .long   op_LDX_zpage
                .long   op_UNK
                .long   op_TAY
                .long   op_LDA_imm
                .long   op_TAX
                .long   op_UNK
                .long   op_LDY_abs
                .long   op_LDA_abs
                .long   op_LDX_abs
                .long   op_UNK
                .long   op_BCS
                .long   op_LDA_ind_y
                .long   op_UNK
                .long   op_UNK
                .long   op_LDY_zpage_x
                .long   op_LDA_zpage_x
                .long   op_LDX_zpage_y
                .long   op_UNK
                .long   op_CLV
                .long   op_LDA_abs_y
                .long   op_TSX
                .long   op_UNK
                .long   op_LDY_abs_x
                .long   op_LDA_abs_x
                .long   op_LDX_abs_y
                .long   op_UNK
                .long   op_CPY_imm
                .long   op_CMP_ind_x
                .long   op_UNK
                .long   op_UNK
                .long   op_CPY_zpage
                .long   op_CMP_zpage
                .long   op_DEC_zpage
                .long   op_UNK
                .long   op_INY
                .long   op_CMP_imm
                .long   op_DEX
                .long   op_UNK
                .long   op_CPY_abs
                .long   op_CMP_abs
                .long   op_DEC_abs
                .long   op_UNK
                .long   op_BNE
                .long   op_CMP_ind_y
                .long   op_UNK
                .long   op_UNK
                .long   op_UNK
                .long   op_CMP_zpage_x
                .long   op_DEC_zpage_x
                .long   op_UNK
                .long   op_CLD
                .long   op_CMP_abs_y
                .long   op_UNK
                .long   op_UNK
                .long   op_UNK
                .long   op_CMP_abs_x
                .long   op_DEC_abs_x
                .long   op_UNK
                .long   op_CPX_imm
                .long   op_SBC_ind_x
                .long   op_UNK
                .long   op_UNK
                .long   op_CPX_zpage
                .long   op_SBC_zpage
                .long   op_INC_zpage
                .long   op_UNK
                .long   op_INX
                .long   op_SBC_imm
                .long   op_NOP
                .long   op_UNK
                .long   op_CPX_abs
                .long   op_SBC_abs
                .long   op_INC_abs
                .long   op_UNK
                .long   op_BEQ
                .long   op_SBC_ind_y
                .long   op_UNK
                .long   op_UNK
                .long   op_UNK
                .long   op_SBC_zpage_x
                .long   op_INC_zpage_x
                .long   op_UNK
                .long   op_SED
                .long   op_SBC_abs_y
                .long   op_UNK
                .long   op_UNK
                .long   op_UNK
                .long   op_SBC_abs_x
                .long   op_INC_abs_x
                .long   op_UNK

E(cpu65__cmos)
                .long   op_BRK
                .long   op_ORA_ind_x
                .long   op_UNK_65c02
                .long   op_UNK_65c02
                .long   op_TSB_zpage
                .long   op_ORA_zpage
                .long   op_ASL_zpage
                .long   op_RMB0_65c02
                .long   op_PHP
                .long   op_ORA_imm
                .long   op_ASL_acc
                .long   op_UNK_65c02
                .long   op_TSB_abs
                .long   op_ORA_abs
                .long   op_ASL_abs
                .long   op_BBR0_65c02
                .long   op_BPL
                .long   op_ORA_ind_y
                .long   op_ORA_ind_zpage
                .long   op_UNK_65c02
                .long   op_TRB_zpage
                .long   op_ORA_zpage_x
                .long   op_ASL_zpage_x
                .long   op_RMB1_65c02
                .long   op_CLC
                .long   op_ORA_abs_y
                .long   op_INA
                .long   op_UNK_65c02
                .long   op_TRB_abs
                .long   op_ORA_abs_x
                .long   op_ASL_abs_x
                .long   op_BBR1_65c02
                .long   op_JSR
                .long   op_AND_ind_x
                .long   op_UNK_65c02
                .long   op_UNK_65c02
                .long   op_BIT_zpage
                .long   op_AND_zpage
                .long   op_ROL_zpage
                .long   op_RMB2_65c02
                .long   op_PLP
                .long   op_AND_imm
                .long   op_ROL_acc
                .long   op_UNK_65c02
                .long   op_BIT_abs
                .long   op_AND_abs
                .long   op_ROL_abs
                .long   op_BBR2_65c02
                .long   op_BMI
                .long   op_AND_ind_y
                .long   op_AND_ind_zpage
                .long   op_UNK_65c02
                .long   op_BIT_zpage_x
                .long   op_AND_zpage_x
                .long   op_ROL_zpage_x
                .long   op_RMB3_65c02
                .long   op_SEC
                .long   op_AND_abs_y
                .long   op_DEA
                .long   op_UNK_65c02
                .long   op_BIT_abs_x
                .long   op_AND_abs_x
                .long   op_ROL_abs_x
                .long   op_BBR3_65c02
                .long   op_RTI
                .long   op_EOR_ind_x
                .long   op_UNK_65c02
                .long   op_UNK_65c02
                .long   op_UNK_65c02
                .long   op_EOR_zpage
                .long   op_LSR_zpage
                .long   op_RMB4_65c02
                .long   op_PHA
                .long   op_EOR_imm
                .long   op_LSR_acc
                .long   op_UNK_65c02
                .long   op_JMP_abs
                .long   op_EOR_abs
                .long   op_LSR_abs
                .long   op_BBR4_65c02
                .long   op_BVC
                .long   op_EOR_ind_y
                .long   op_EOR_ind_zpage
                .long   op_UNK_65c02
                .long   op_UNK_65c02
                .long   op_EOR_zpage_x
                .long   op_LSR_zpage_x
                .long   op_RMB5_65c02
                .long   op_CLI
                .long   op_EOR_abs_y
                .long   op_PHY
                .long   op_UNK_65c02
                .long   op_UNK_65c02
                .long   op_EOR_abs_x
                .long   op_LSR_abs_x
                .long   op_BBR5_65c02
                .long   op_RTS
                .long   op_ADC_ind_x
                .long   op_UNK_65c02
                .long   op_UNK_65c02
                .long   op_STZ_zpage
                .long   op_ADC_zpage
                .long   op_ROR_zpage
                .long   op_RMB6_65c02
                .long   op_PLA
                .long   op_ADC_imm
                .long   op_ROR_acc
                .long   op_UNK_65c02
                .long   op_JMP_ind_65c02
                .long   op_ADC_abs
                .long   op_ROR_abs
                .long   op_BBR6_65c02
                .long   op_BVS
                .long   op_ADC_ind_y
                .long   op_ADC_ind_zpage
                .long   op_UNK_65c02
                .long   op_STZ_zpage_x
                .long   op_ADC_zpage_x
                .long   op_ROR_zpage_x
                .long   op_RMB7_65c02
                .long   op_SEI
                .long   op_ADC_abs_y
                .long   op_PLY
                .long   op_UNK_65c02
                .long   op_JMP_abs_ind_x
                .long   op_ADC_abs_x
                .long   op_ROR_abs_x
                .long   op_BBR7_65c02
                .long   op_BRA
                .long   op_STA_ind_x
                .long   op_UNK_65c02
                .long   op_UNK_65c02
                .long   op_STY_zpage
                .long   op_STA_zpage
                .long   op_STX_zpage
                .long   op_SMB0_65c02
                .long   op_DEY
                .long   op_BIT_imm
                .long   op_TXA
                .long   op_UNK_65c02
                .long   op_STY_abs
                .long   op_STA_abs
                .long   op_STX_abs
                .long   op_BBS0_65c02
                .long   op_BCC
                .long   op_STA_ind_y
                .long   op_STA_ind_zpage
                .long   op_UNK_65c02
                .long   op_STY_zpage_x
                .long   op_STA_zpage_x
                .long   op_STX_zpage_y
                .long   op_SMB1_65c02
                .long   op_TYA
                .long   op_STA_abs_y
                .long   op_TXS
                .long   op_UNK_65c02
                .long   op_STZ_abs
                .long   op_STA_abs_x
                .long   op_STZ_abs_x
                .long   op_BBS1_65c02
                .long   op_LDY_imm
                .long   op_LDA_ind_x
                .long   op_LDX_imm
                .long   op_UNK_65c02
                .long   op_LDY_zpage
                .long   op_LDA_zpage
                .long   op_LDX_zpage
                .long   op_SMB2_65c02
                .long   op_TAY
                .long   op_LDA_imm
                .long   op_TAX
                .long   op_UNK_65c02
                .long   op_LDY_abs
                .long   op_LDA_abs
                .long   op_LDX_abs
                .long   op_BBS2_65c02
                .long   op_BCS
                .long   op_LDA_ind_y
                .long   op_LDA_ind_zpage
                .long   op_UNK_65c02
                .long   op_LDY_zpage_x
                .long   op_LDA_zpage_x
                .long   op_LDX_zpage_y
                .long   op_SMB3_65c02
                .long   op_CLV
                .long   op_LDA_abs_y
                .long   op_TSX
                .long   op_UNK_65c02
                .long   op_LDY_abs_x
                .long   op_LDA_abs_x
                .long   op_LDX_abs_y
                .long   op_BBS3_65c02
                .long   op_CPY_imm
                .long   op_CMP_ind_x
                .long   op_UNK_65c02
                .long   op_UNK_65c02
                .long   op_CPY_zpage
                .long   op_CMP_zpage
                .long   op_DEC_zpage
                .long   op_SMB4_65c02
                .long   op_INY
                .long   op_CMP_imm
                .long   op_DEX
                .long   op_WAI_65c02
                .long   op_CPY_abs
                .long   op_CMP_abs
                .long   op_DEC_abs
                .long   op_BBS4_65c02
                .long   op_BNE
                .long   op_CMP_ind_y
                .long   op_CMP_ind_zpage
                .long   op_UNK_65c02
                .long   op_UNK_65c02
                .long   op_CMP_zpage_x
                .long   op_DEC_zpage_x
                .long   op_SMB5_65c02
                .long   op_CLD
                .long   op_CMP_abs_y
                .long   op_PHX
                .long   op_STP_65c02
                .long   op_UNK_65c02
                .long   op_CMP_abs_x
                .long   op_DEC_abs_x
                .long   op_BBS5_65c02
                .long   op_CPX_imm
                .long   op_SBC_ind_x
                .long   op_UNK_65c02
                .long   op_UNK_65c02
                .long   op_CPX_zpage
                .long   op_SBC_zpage
                .long   op_INC_zpage
                .long   op_SMB6_65c02
                .long   op_INX
                .long   op_SBC_imm
                .long   op_NOP
                .long   op_UNK_65c02
                .long   op_CPX_abs
                .long   op_SBC_abs
                .long   op_INC_abs
                .long   op_BBS6_65c02
                .long   op_BEQ
                .long   op_SBC_ind_y
                .long   op_SBC_ind_zpage
                .long   op_UNK_65c02
                .long   op_UNK_65c02
                .long   op_SBC_zpage_x
                .long   op_INC_zpage_x
                .long   op_SMB7_65c02
                .long   op_SED
                .long   op_SBC_abs_y
                .long   op_PLX
                .long   op_UNK_65c02
                .long   op_UNK_65c02
                .long   op_SBC_abs_x
                .long   op_INC_abs_x
                .long   op_BBS7_65c02

E(cpu65__nmos)
                .long   op_BRK
                .long   op_ORA_ind_x
                .long   op_UNK_HANG
                .long   op_UNK_LOR_ind_x
                .long   op_UNK_NOP_2
                .long   op_ORA_zpage
                .long   op_ASL_zpage
                .long   op_UNK_LOR_zpage
                .long   op_PHP
                .long   op_ORA_imm
                .long   op_ASL_acc
                .long   op_UNK_ANA_imm
                .long   op_UNK_NOP_3
                .long   op_ORA_abs
                .long   op_ASL_abs
                .long   op_UNK_LOR_abs
                .long   op_BPL
                .long   op_ORA_ind_y
                .long   op_UNK_HANG
                .long   op_UNK_LOR_ind_y
                .long   op_UNK_NOP_2
                .long   op_ORA_zpage_x
                .long   op_ASL_zpage_x
                .long   op_UNK_LOR_zpage_x
                .long   op_CLC
                .long   op_ORA_abs_y
                .long   op_NOP
                .long   op_UNK_LOR_abs_y
                .long   op_UNK_NOP_3
                .long   op_ORA_abs_x
                .long   op_ASL_abs_x
                .long   op_UNK_LOR_abs_x
                .long   op_JSR
                .long   op_AND_ind_x
                .long   op_UNK_HANG
                .long   op_UNK_LAN_ind_x
                .long   op_BIT_zpage
                .long   op_AND_zpage
                .long   op_ROL_zpage
                .long   op_UNK_LAN_zpage
                .long   op_PLP
                .long   op_AND_imm
                .long   op_ROL_acc
                .long   op_UNK_ANB_imm
                .long   op_BIT_abs
                .long   op_AND_abs
                .long   op_ROL_abs
                .long   op_UNK_LAN_abs
                .long   op_BMI
                .long   op_AND_ind_y
                .long   op_UNK_HANG
                .long   op_UNK_LAN_ind_y
                .long   op_UNK_NOP_2
                .long   op_AND_zpage_x
                .long   op_ROL_zpage_x
                .long   op_UNK_LAN_zpage_x
                .long   op_SEC
                .long   op_AND_abs_y
                .long   op_NOP
                .long   op_UNK_LAN_abs_y
                .long   op_UNK_NOP_3
                .long   op_AND_abs_x
                .long   op_ROL_abs_x
                .long   op_UNK_LAN_abs_x
                .long   op_RTI
                .long   op_EOR_ind_x
                .long   op_UNK_HANG
                .long   op_UNK_REO_ind_x
                .long   op_UNK_NOP_2
                .long   op_EOR_zpage
                .long   op_LSR_zpage
                .long   op_UNK_REO_zpage
                .long   op_PHA
                .long   op_EOR_imm
                .long   op_LSR_acc
                .long   op_UNK_RAM_imm
                .long   op_JMP_abs
                .long   op_EOR_abs
                .long   op_LSR_abs
                .long   op_UNK_REO_abs
                .long   op_BVC
                .long   op_EOR_ind_y
                .long   op_UNK_HANG
                .long   op_UNK_REO_ind_y
                .long   op_UNK_NOP_2
                .long   op_EOR_zpage_x
                .long   op_LSR_zpage_x
                .long   op_UNK_REO_zpage_x
                .long   op_CLI
                .long   op_EOR_abs_y
                .long   op_NOP
                .long   op_UNK_REO_abs_y
                .long   op_UNK_NOP_3
                .long   op_EOR_abs_x
                .long   op_LSR_abs_x
                .long   op_UNK_REO_abs_x
                .long   op_RTS
                .long   op_ADC_ind_x
                .long   op_UNK_HANG
                .long   op_UNK_RAD_ind_x
                .long   op_UNK_NOP_2
                .long   op_ADC_zpage
                .long   op_ROR_zpage
                .long   op_UNK_RAD_zpage
                .long   op_PLA
                .long   op_ADC_imm
                .long   op_ROR_acc
                .long   op_UNK_RBM_imm
                .long   op_JMP_ind
                .long   op_ADC_abs
                .long   op_ROR_abs
                .long   op_UNK_RAD_abs
                .long   op_BVS
                .long   op_ADC_ind_y
                .long   op_UNK_HANG
                .long   op_UNK_RAD_ind_y
                .long   op_UNK_NOP_2
                .long   op_ADC_zpage_x
                .long   op_ROR_zpage_x
                .long   op_UNK_RAD_zpage_x
                .long   op_SEI
                .long   op_ADC_abs_y
                .long   op_NOP
                .long   op_UNK_RAD_abs_y
                .long   op_UNK_NOP_3
                .long   op_ADC_abs_x
                .long   op_ROR_abs_x
                .long   op_UNK_RAD_abs_x
                .long   op_UNK_NOP_2
                .long   op_STA_ind_x
                .long   op_UNK_NOP_2
                .long   op_UNK_AAX_ind_x
                .long   op_STY_zpage
                .long   op_STA_zpage
                .long   op_STX_zpage
                .long   op_UNK_AAX_zpage
                .long   op_DEY
                .long   op_UNK_NOP_2
                .long   op_TXA
                .long   op_UNK_XMA_imm
                .long   op_STY_abs
                .long   op_STA_abs
                .long   op_STX_abs
                .long   op_UNK_AAX_abs
                .long   op_BCC
                .long   op_STA_ind_y
                .long   op_UNK_HANG
                .long   op_UNK_AAX_ind_y
                .long   op_STY_zpage_x
                .long   op_STA_zpage_x
                .long   op_STX_zpage_y
                .long   op_UNK_AAX_zpage_y
                .long   op_TYA
                .long   op_STA_abs_y
                .long   op_TXS
                .long   op_UNK_AXS_abs_y
                .long   op_UNK_TEY_abs_x
                .long   op_STA_abs_x
                .long   op_UNK_TEX_abs_y
                .long   op_UNK_TEA_abs_y
                .long   op_LDY_imm
                .long   op_LDA_ind_x
                .long   op_LDX_imm
                .long   op_UNK_LAX_ind_x
                .long   op_LDY_zpage
                .long   op_LDA_zpage
                .long   op_LDX_zpage
                .long   op_UNK_LAX_zpage
                .long   op_TAY
                .long   op_LDA_imm
                .long   op_TAX
                .long   op_UNK_AMA_imm
                .long   op_LDY_abs
                .long   op_LDA_abs
                .long   op_LDX_abs
                .long   op_UNK_LAX_abs
                .long   op_BCS
                .long   op_LDA_ind_y
                .long   op_UNK_HANG
                .long   op_UNK_LAX_ind_y
                .long   op_LDY_zpage_x
                .long   op_LDA_zpage_x
                .long   op_LDX_zpage_y
                .long   op_UNK_LAX_zpage_y
                .long   op_CLV
                .long   op_LDA_abs_y
                .long   op_TSX
                .long   op_UNK_LAS_abs_y
                .long   op_LDY_abs_x
                .long   op_LDA_abs_x
                .long   op_LDX_abs_y
                .long   op_UNK_LAX_abs_y
                .long   op_CPY_imm
                .long   op_CMP_ind_x
                .long   op_UNK_NOP_2
                .long   op_UNK_DCP_ind_x
                .long   op_CPY_zpage
                .long   op_CMP_zpage
                .long   op_DEC_zpage
                .long   op_UNK_DCP_zpage
                .long   op_INY
                .long   op_CMP_imm
                .long   op_DEX
                .long   op_UNK_AXM_imm
                .long   op_CPY_abs
                .long   op_CMP_abs
                .long   op_DEC_abs
                .long   op_UNK_DCP_abs
                .long   op_BNE
                .long   op_CMP_ind_y
                .long   op_UNK_HANG
                .long   op_UNK_DCP_ind_y
                .long   op_UNK_NOP_2
                .long   op_CMP_zpage_x
                .long   op_DEC_zpage_x
                .long   op_UNK_DCP_zpage_x
                .long   op_CLD
                .long   op_CMP_abs_y
                .long   op_NOP
                .long   op_UNK_DCP_abs_y
                .long   op_UNK_NOP_3
                .long   op_CMP_abs_x
                .long   op_DEC_abs_x
                .long   op_UNK_DCP_abs_x
                .long   op_CPX_imm
                .long   op_SBC_ind_x
                .long   op_UNK_NOP_2
                .long   op_UNK_ISB_ind_x
                .long   op_CPX_zpage
                .long   op_SBC_zpage
                .long   op_INC_zpage
                .long   op_UNK_ISB_zpage
                .long   op_INX
                .long   op_SBC_imm
                .long   op_NOP
                .long   op_UNK_ZBC_imm
                .long   op_CPX_abs
                .long   op_SBC_abs
                .long   op_INC_abs
                .long   op_UNK_ISB_abs
                .long   op_BEQ
                .long   op_SBC_ind_y
                .long   op_UNK_HANG
                .long   op_UNK_ISB_ind_y
                .long   op_UNK_NOP_2
                .long   op_SBC_zpage_x
                .long   op_INC_zpage_x
                .long   op_UNK_ISB_zpage_x
                .long   op_SED
                .long   op_SBC_abs_y
                .long   op_NOP
                .long   op_UNK_ISB_abs_y
                .long   op_UNK_NOP_3
                .long   op_SBC_abs_x
                .long   op_INC_abs_x
                .long   op_UNK_ISB_abs_x

