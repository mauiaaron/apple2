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

                .comm   SN(cpu65_vmem),524288,4096
                .comm   SN(cpu65_flags_encode),256
                .comm   SN(cpu65_flags_decode),256
                .comm   SN(cpu65_delay),4
                .comm   SN(cpu65__opcodes),1024
                .comm   SN(cpu65__signal),1
                .comm   SN(cpu65_debug),4
                .comm   SN(cpu65_current),7

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
                        (,EffectiveAddr_E,8);                                  \

#define GetFromEA_B                                                            \
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

/* h means hooked */
#define GetFromEA_Bh                                                           \
                orb     $1, SN(cpu65_debug)+3;                                 \
                GetFromEA_B

#define PutToEA_Bh                                                             \
                orb     $2, SN(cpu65_debug)+3;                                 \
                orb     %al, SN(cpu65_debug)+2;                                \
                PutToEA_B

/* reset operation code before each instruction. This assumes %al
 * is zero, and is inserted into Continue */
#define ZeroOp  mov     %al, SN(cpu65_debug)+3;

// NOTE: the orb functions as a move, but we want to
// set the flags and we know %ah is zero
#define Continue                                                               \
                xorl    %eax, %eax;                                            \
                orb     SN(cpu65__signal), %ah;                                \
                jnz     exception;                                             \
                jmp     continue;

#define SaveState                                                              \
                movw    EffectiveAddr, SN(cpu65_debug);                        \
                movw    PC_Reg, SN(cpu65_current);                             \
                movb    A_Reg, SN(cpu65_current)+2;                            \
                movb    F_Reg, SN(cpu65_current)+3;                            \
                movb    X_Reg, SN(cpu65_current)+4;                            \
                movb    Y_Reg, SN(cpu65_current)+5;                            \
                movb    SP_Reg_L, SN(cpu65_current)+6;

/* The xorls clear the high parts of the registers
 * Note: dependent on register assignment
 *
 * The extra bit at the end points the stack pointer at the alternate
 * stack in ALTZP mode.  (Note, this is not good -- I'd prefer to avoid
 * polluting this module with Apple-specific stuff.  But we need to do
 * it, else aux-stack using programs will crash when debugged.)
 */
#define ReplaceState                                                           \
                xorl    %eax, %eax;                                            \
                xorl    %ebx, %ebx;                                            \
                xorl    %ecx, %ecx;                                            \
                movl    $0x0100, %edx;                                         \
                xorl    %esi, %esi;                                            \
                xorl    %edi, %edi;                                            \
                movw    SN(cpu65_debug), EffectiveAddr;                        \
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

#define GetZPage_Y                                                             \
                GetFromPC_B;                                                   \
                addb    Y_Reg, %al;                                            \
                movl    %eax, EffectiveAddr_E;

/* Absolute Indexed Addressing - The effective address is formed by
   adding the contents of X or Y to the address contained in the
   second and third bytes of the instruction. */
#define GetAbs_X                                                               \
                GetFromPC_W;                                                   \
                addb    X_Reg, %al;                                            \
                adcb    $0, %ah;                                               \
                movl    %eax, EffectiveAddr_E;

#define GetAbs_Y                                                               \
                GetFromPC_W;                                                   \
                addb    Y_Reg, %al;                                            \
                adcb    $0, %ah;                                               \
                movl    %eax, EffectiveAddr_E;

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
#define GetIndZPage_Y                                                          \
                GetFromPC_B;                                                   \
                incb    %al;                                                   \
                movl    %eax, EffectiveAddr_E;                                 \
                GetFromEA_B;                                                   \
                movb    %al, %ah;                                              \
                decl    EffectiveAddr_E;                                       \
                andl    $0xFF, EffectiveAddr_E;                                \
                GetFromEA_B;                                                   \
                addb    Y_Reg, %al;                                            \
                adcb    $0, %ah;                                               \
                movl    %eax, EffectiveAddr_E;

#define DoADC_b GetFromEA_Bh                                                   \
                bt      $C_Flag_Bit, FF_Reg;                                   \
                adcb    %al, A_Reg;                                            \
                FlagNVZC

#define DoADC_d GetFromEA_Bh                                                   \
                bt      $C_Flag_Bit, FF_Reg;                                   \
                adcb    A_Reg, %al;                                            \
                daa;                                                           \
                movb    %al, A_Reg;                                            \
                FlagNVZC

#define DoAND   GetFromEA_Bh                                                   \
                andb    %al, A_Reg;                                            \
                FlagNZ

#define DoASL   GetFromEA_Bh                                                   \
                addb    %al, %al;                                              \
                FlagNZC                                                        \
                PutToEA_Bh                                                     \

/* SAR (and the following AND) effectively moves
* bit 6 to Bit 3 while leaving Bit 7 unchanged */
#define DoBIT   GetFromEA_Bh                                                   \
                testb   %al, A_Reg;                                            \
                lahf;                                                          \
                sarb    $3, %al;                                               \
                andw    $0x4088, %ax;                                          \
                andb    $~(N_Flag|V_Flag|Z_Flag), F_Reg;                       \
                orb     %al, F_Reg;                                            \
                orb     %ah, F_Reg;

#define DoCMP   GetFromEA_Bh                                                   \
                cmpb    %al, A_Reg;                                            \
                cmc;                                                           \
                FlagNZC

#define DoCPX   GetFromEA_Bh                                                   \
                cmpb    %al, X_Reg;                                            \
                cmc;                                                           \
                FlagNZC

#define DoCPY   GetFromEA_Bh                                                   \
                cmpb    %al, Y_Reg;                                            \
                cmc;                                                           \
                FlagNZC

#define DoDEC   GetFromEA_Bh                                                   \
                decb    %al;                                                   \
                FlagNZ                                                         \
                PutToEA_Bh

#define DoEOR   GetFromEA_Bh                                                   \
                xorb    %al, A_Reg;                                            \
                FlagNZ

#define DoINC   GetFromEA_Bh                                                   \
                incb    %al;                                                   \
                FlagNZ                                                         \
                PutToEA_Bh

#define DoJMP   movw    EffectiveAddr, PC_Reg;

#define DoJSR   movw    PC_Reg, %ax;                                           \
                decw    %ax;                                                   \
                Push(%ah)                                                      \
                Push(%al)                                                      \
                movw    EffectiveAddr, PC_Reg;

#define DoLDA   GetFromEA_Bh                                                   \
                movb    %al, A_Reg;                                            \
                orb     %al, %al;                                              \
                FlagNZ

#define DoLDX   GetFromEA_Bh                                                   \
                movb    %al, X_Reg;                                            \
                orb     %al, %al;                                              \
                FlagNZ

#define DoLDY   GetFromEA_Bh                                                   \
                movb    %al, Y_Reg;                                            \
                orb     %al, %al;                                              \
                FlagNZ

#define DoLSR   GetFromEA_Bh                                                   \
                shrb    $1, %al;                                               \
                FlagNZC                                                        \
                PutToEA_Bh

#define DoORA   GetFromEA_Bh                                                   \
                orb     %al, A_Reg;                                            \
                FlagNZ

#define DoROL   GetFromEA_Bh                                                   \
                bt      $C_Flag_Bit, FF_Reg;                                   \
                adcb    %al,%al;                                               \
                FlagNZC                                                        \
                PutToEA_Bh

#define DoROR   GetFromEA_Bh                                                   \
                movb    F_Reg, %ah;                                            \
                rorl    $1, %eax;                                              \
                orb     %al, %al;                                              \
                btr     $31, %eax;                                             \
                FlagNZC                                                        \
                PutToEA_Bh

#define DoSBC_b GetFromEA_Bh                                                   \
                notb    %al;                                                   \
                bt      $C_Flag_Bit, FF_Reg;                                   \
                adcb    %al, A_Reg;                                            \
                FlagNVZC

#define DoSBC_d GetFromEA_Bh                                                   \
                bt      $C_Flag_Bit, FF_Reg;                                   \
                cmc;                                                           \
                xchgb   A_Reg, %al;                                            \
                sbbb    A_Reg, %al;                                            \
                das;                                                           \
                movb    %al, A_Reg;                                            \
                cmc;                                                           \
                FlagNVZC

#define DoSTA   movb    A_Reg, %al;                                            \
                PutToEA_Bh

#define DoSTX   movb    X_Reg, %al;                                            \
                PutToEA_Bh

#define DoSTY   movb    Y_Reg, %al;                                            \
                PutToEA_Bh

/* -------------------------------------------------------------------------
    65c02 instructions
   ------------------------------------------------------------------------- */

#define DoSTZ   movb    $0x0, %al;                                             \
                PutToEA_Bh

#define DoTRB   GetFromEA_Bh                                                   \
                testb   A_Reg, %al;                                            \
                FlagZ                                                          \
                notb    A_Reg;                                                 \
                andb    A_Reg, %al;                                            \
                notb    A_Reg;                                                 \
                PutToEA_Bh

#define DoTSB   GetFromEA_Bh                                                   \
                testb   A_Reg, %al;                                            \
                FlagZ                                                          \
                orb     A_Reg, %al;                                            \
                PutToEA_Bh

/* -------------------------------------------------------------------------
    Undocumented 6502 (Illegal instructions)
   ------------------------------------------------------------------------- */

/* AAX = A AND X -> M */
#define DoAAX   movb    A_Reg, %al;                                            \
                andb    X_Reg, %al;                                            \
                FlagNZ                                                         \
                PutToEA_Bh

/* AMA = ORA 238, AND M, TAX */
#define DoAMA   orb     $238, A_Reg;                                           \
                GetFromEA_Bh                                                   \
                andb    %al, A_Reg;                                            \
                movb    A_Reg, X_Reg;                                          \
                FlagNZ

/* ANA = AND M, Carry = BIT 7 */
/* NB: assumes A_Reg = %cl */
#define DoANA   GetFromEA_Bh                                                   \
                andb    %al, A_Reg;                                            \
                bt      $7, %ecx;                                              \
                FlagNZC

/* ANB = same as ANA */
#define DoANB   DoANA

/* AXM = (A AND X) - M -> X */
#define DoAXM   GetFromEA_Bh                                                   \
                andb    A_Reg, X_Reg;                                          \
                notb    %al;                                                   \
                bt      $C_Flag_Bit, FF_Reg;                                   \
                adcb    %al, X_Reg;                                            \
                FlagNVZC

/* AXS = (A AND X) -> S, A AND X AND 17 -> M */
/* HACK!!!!!!!!!!!!!!! */
#define DoAXS   movb    A_Reg, SP_Reg_L;                                       \
                andb    X_Reg, SP_Reg_L;                                       \
                movb    SP_Reg_L, %al;                                         \
                andb    $17, %al;                                              \
                FlagNZ  /* \ wasn't here */                                    \
                PutToEA_Bh

/* DCP = DEC M, CMP M */
#define DoDCP   GetFromEA_Bh                                                   \
                decb    %al;                                                   \
                PutToEA_Bh                                                     \
                negb    %al;                                                   \
                addb    A_Reg, %al;                                            \
                FlagNZC

/* ISB = INC M, SBC M */
#define DoISB_b GetFromEA_Bh                                                   \
                incb    %al;                                                   \
                PutToEA_Bh                                                     \
                notb    %al;                                                   \
                bt      $C_Flag_Bit, FF_Reg;                                   \
                adcb    %al, A_Reg;                                            \
                FlagNVZC

#define DoISB_d GetFromEA_Bh                                                   \
                incb    %al;                                                   \
                PutToEA_Bh                                                     \
                bt      $C_Flag_Bit, FF_Reg;                                   \
                cmc;                                                           \
                xchgb   A_Reg, %al;                                            \
                sbbb    A_Reg, %al;                                            \
                das;                                                           \
                movb    %al, A_Reg;                                            \
                cmc;                                                           \
                FlagNVZC

/* LAN = ROL M, AND M */
#define DoLAN   GetFromEA_Bh                                                   \
                bt      $C_Flag_Bit, FF_Reg;                                   \
                adcl    %eax, %eax;                                            \
                andb    %al, A_Reg;                                            \
                bt      $8, %eax;                                              \
                FlagNZC                                                        \
                PutToEA_Bh

/* LAS = LDA M, TAX, TXS */
#define DoLAS   GetFromEA_Bh                                                   \
                movb    %al, A_Reg;                                            \
                movb    %al, X_Reg;                                            \
                movb    %al, SP_Reg_L;                                         \
                orb     %al, %al;                                              \
                FlagNZ

/* LAX = LDA M, TAX */
#define DoLAX   GetFromEA_Bh                                                   \
                movb    %al, A_Reg;                                            \
                movb    %al, X_Reg;                                            \
                orb     %al, %al;                                              \
                FlagNZ


/* LOR = ASL M, ORA M */
#define DoLOR   GetFromEA_Bh                                                   \
                addb    %al, %al;                                              \
                FlagC                                                          \
                PutToEA_Bh                                                     \
                orb     %al, A_Reg;                                            \
                FlagNZ

/* RAD = ROR M, ADC M */
#define DoRAD_b GetFromEA_Bh                                                   \
                bt      $C_Flag_Bit, FF_Reg;                                   \
                rcrb    $1, %al;                                               \
                adcb    %al, A_Reg;                                            \
                pushl   %eax;                                                  \
                FlagNVZC                                                       \
                popl    %eax;                                                  \
                PutToEA_Bh

#define DoRAD_d GetFromEA_Bh                                                   \
                bt      $C_Flag_Bit, FF_Reg;                                   \
                rcrb    $1, %al;                                               \
                pushfl;                                                        \
                PutToEA_Bh                                                     \
                popfl;                                                         \
                adcb    A_Reg, %al;                                            \
                daa;                                                           \
                movb    %al, A_Reg;                                            \
                FlagNVZC

/* RAM = AND M, LSR A */
#define DoRAM   GetFromEA_Bh                                                   \
                andb    %al, A_Reg;                                            \
                shrb    $1, A_Reg;                                             \
                FlagNZC

/* RBM = same as RAM */
#define DoRBM   DoRAM

/* REO = LSR M, EOR M */
#define DoREO   GetFromEA_Bh                                                   \
                shrb    $1, %al;                                               \
                xorb    %al, A_Reg;                                            \
                FlagNZC                                                        \
                PutToEA_Bh                                                     \

/* DoZBC = same as SBC */
#define DoZBC_b DoSBC_b
#define DoZBC_d DoSBC_d

/* TEA = (A AND X AND (OP+2)+1) -> M */
#define DoTEA   pushl   EffectiveAddr_E;                                       \
                movw    PC_Reg, EffectiveAddr;                                 \
                decw    EffectiveAddr;                                         \
                GetFromEA_Bh                                                   \
                popl    EffectiveAddr_E;                                       \
                incb    %al;                                                   \
                andb    A_Reg, %al;                                            \
                andb    X_Reg, %al;                                            \
                FlagNZ                                                         \
                PutToEA_Bh

/* TEX = (X AND (OP+2)+1) -> M */
#define DoTEX   pushl   EffectiveAddr_E;                                       \
                movw    PC_Reg, EffectiveAddr;                                 \
                decw    EffectiveAddr;                                         \
                GetFromEA_Bh                                                   \
                popl    EffectiveAddr_E;                                       \
                incb    %al;                                                   \
                andb    X_Reg, %al;                                            \
                FlagNZ                                                         \
                PutToEA_Bh

/* TEY = (Y AND 1) -> M */
#define DoTEY   movb    Y_Reg, %al;                                            \
                andb    $1, %al;                                               \
                FlagNZ                                                         \
                PutToEA_Bh

/* XMA = (X AND M) AND (A OR 238) -> A */
/* HACK!!!!!!!!!!!!!!! */
#define DoXMA   /* the \ wasn't here before */                                 \
                GetFromEA_Bh                                                   \
                andb    X_Reg, %al;                                            \
                orb     $238, A_Reg;                                           \
                andb    %al, A_Reg;                                            \
                FlagNZ


/* ----------------------------------------------------------------------
       6502 routines and instructions
   ---------------------------------------------------------------------- */

/* ----------------------------------
       ADC instructions
   ---------------------------------- */

op_ADC_dec:     DoADC_d
                Continue

op_ADC_imm:
                GetImm
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_ADC_dec              # Yes, jump to decimal version
                DoADC_b
                Continue

op_ADC_zpage:
                GetZPage
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_ADC_dec              # Yes, jump to decimal version
                DoADC_b
                Continue

op_ADC_zpage_x:
                GetZPage_X
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_ADC_dec              # Yes, jump to decimal version
                DoADC_b
                Continue

op_ADC_abs:
                GetAbs
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_ADC_dec              # Yes, jump to decimal version
                DoADC_b
                Continue

op_ADC_abs_x:
                GetAbs_X
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_ADC_dec              # Yes, jump to decimal version
                DoADC_b
                Continue

op_ADC_abs_y:
                GetAbs_Y
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_ADC_dec              # Yes, jump to decimal version
                DoADC_b
                Continue

op_ADC_ind_x:
                GetIndZPage_X
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_ADC_dec              # Yes, jump to decimal version
                DoADC_b
                Continue

op_ADC_ind_y:
                GetIndZPage_Y
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_ADC_dec              # Yes, jump to decimal version
                DoADC_b
                Continue

/* ----------------------------------
       AND instructions
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

/* ----------------------------------
       ASL instructions
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
       BCC instruction
   ---------------------------------- */

op_BCC:
                GetFromPC_B
                testb   $C_Flag, F_Reg
                jnz     op_BCC_not
                cbw
                addw    %ax, PC_Reg
op_BCC_not:
                Continue

/* ----------------------------------
       BCS instruction
   ---------------------------------- */

op_BCS:
                GetFromPC_B
                testb   $C_Flag, F_Reg
                jz      op_BCS_not
                cbw
                addw    %ax, PC_Reg
op_BCS_not:
                Continue

/* ----------------------------------
       BEQ instruction
   ---------------------------------- */

op_BEQ:
                GetFromPC_B
                testb   $Z_Flag, F_Reg
                jz      op_BEQ_not
                cbw
                addw    %ax, PC_Reg
op_BEQ_not:
                Continue

/* ----------------------------------
       BIT instructions
   ---------------------------------- */

op_BIT_zpage:
                GetZPage
                DoBIT
                Continue

op_BIT_abs:
                GetAbs
                DoBIT
                Continue

/* ----------------------------------
       BMI instruction
   ---------------------------------- */

op_BMI:
                GetFromPC_B
                testb   F_Reg, F_Reg    /* optimized check of N flag,
                                         * which happens to be sign bit */
                jns     op_BMI_not
                cbw
                addw    %ax, PC_Reg
op_BMI_not:
                Continue

/* ----------------------------------
       BNE instruction
   ---------------------------------- */

op_BNE:
                GetFromPC_B
                testb   $Z_Flag, F_Reg
                jnz     op_BNE_not
                cbw
                addw    %ax, PC_Reg
op_BNE_not:
                Continue

/* ----------------------------------
       BPL instruction
   ---------------------------------- */

op_BPL:
                GetFromPC_B
                testb   F_Reg, F_Reg    /* optimized check of N flag,
                                         * which happens to be sign bit */
                js      op_BPL_not
                cbw
                addw    %ax, PC_Reg
op_BPL_not:
                Continue

/* ----------------------------------
       BRK instruction
   ---------------------------------- */

op_UNK: /* make undefined opcodes fault */
op_BRK:
                incw    PC_Reg
                movw    PC_Reg, %ax
                Push(%ah)
                Push(%al)
                xorl    %eax,%eax
                movb    F_Reg, %al
                orb     $I_Flag, F_Reg
                movb    SN(cpu65_flags_encode)(,%eax,1), %al
                Push(%al)
                movw    $0xFFFE, EffectiveAddr
                GetFromEA_W
                movw    %ax, PC_Reg
                Continue

/* ----------------------------------
       BVC instruction
   ---------------------------------- */

op_BVC:
                GetFromPC_B
                testb   $V_Flag, F_Reg
                jnz     op_BVC_not
                cbw
                addw    %ax, PC_Reg
op_BVC_not:
                Continue

/* ----------------------------------
       BVS instruction
   ---------------------------------- */

op_BVS:
                GetFromPC_B
                testb   $V_Flag, F_Reg
                jz      op_BVS_not
                cbw
                addw    %ax, PC_Reg
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

/* ----------------------------------
       CPX instructions
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
       DEC instructions
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

/* ----------------------------------
       INC instructions
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
   ---------------------------------- */

op_JMP_abs:
                GetAbs
                DoJMP
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

/* ----------------------------------
       JSR instruction
   ---------------------------------- */

op_JSR:
                GetAbs
                DoJSR
                Continue

/* ----------------------------------
       LDA instructions
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
                DoSBC_d
                Continue

op_SBC_imm:
                GetImm
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_SBC_dec              # Yes, jump to decimal version
                DoSBC_b
                Continue

op_SBC_zpage:
                GetZPage
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_SBC_dec              # Yes, jump to decimal version
                DoSBC_b
                Continue

op_SBC_zpage_x:
                GetZPage_X
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_SBC_dec              # Yes, jump to decimal version
                DoSBC_b
                Continue

op_SBC_abs:
                GetAbs
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_SBC_dec              # Yes, jump to decimal version
                DoSBC_b
                Continue

op_SBC_abs_x:
                GetAbs_X
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_SBC_dec              # Yes, jump to decimal version
                DoSBC_b
                Continue

op_SBC_abs_y:
                GetAbs_Y
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_SBC_dec              # Yes, jump to decimal version
                DoSBC_b
                Continue

op_SBC_ind_x:
                GetIndZPage_X
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_SBC_dec              # Yes, jump to decimal version
                DoSBC_b
                Continue

op_SBC_ind_y:
                GetIndZPage_Y
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_SBC_dec              # Yes, jump to decimal version
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
       STA instructions
   ---------------------------------- */

op_STA_imm:
                GetImm
                DoSTA
                Continue

op_STA_zpage:
                GetZPage
                DoSTA
                Continue

op_STA_zpage_x:
                GetZPage_X
                DoSTA
                Continue

op_STA_abs:
                GetAbs
                DoSTA
                Continue

op_STA_abs_x:
                GetAbs_X
                DoSTA
                Continue

op_STA_abs_y:
                GetAbs_Y
                DoSTA
                Continue

op_STA_ind_x:
                GetIndZPage_X
                DoSTA
                Continue

op_STA_ind_y:
                GetIndZPage_Y
                DoSTA
                Continue

/* ----------------------------------
       STX instructions
   ---------------------------------- */

op_STX_zpage:
                GetZPage
                DoSTX
                Continue

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




#ifdef APPLE_IIE

/* ----------------------------------------------------------------------
       65c02 routines and instructions
   ---------------------------------------------------------------------- */

/* ----------------------------------
       ADC instruction
   ---------------------------------- */

op_ADC_ind_zpage:               # 72
                GetIndZPage
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_ADC_dec              # Yes, jump to decimal version
                DoADC_b
                Continue
op_ADC_ind_zpage_dec:
                DoADC_d
                Continue

/* ----------------------------------
       AND instruction
   ---------------------------------- */

op_AND_ind_zpage:               # 32
                GetIndZPage
                DoAND
                Continue

/* ----------------------------------
       BIT instructions
   ---------------------------------- */

op_BIT_zpage_x:                 # 34
                GetIndZPage
                DoBIT
                Continue

op_BIT_abs_x:                   # 3C
                GetAbs_X
                DoBIT
                Continue

                /* BIT immediate is anomalous in that it does not affect the
                 * N and V flags, unlike in other addressing modes.
                 */
op_BIT_imm:                     # 89
                GetImm
                GetFromEA_Bh
                testb   %al, A_Reg
                FlagZ
                Continue

/* ----------------------------------
       BRA instruction
   ---------------------------------- */

op_BRA:                         # 80
                GetFromPC_B
                cbw
                addw    %ax, PC_Reg
                Continue

/* ----------------------------------
       CMP instruction
   ---------------------------------- */

op_CMP_ind_zpage:               # D2
                GetIndZPage
                DoCMP
                Continue

/* ----------------------------------
       DEA instruction
   ---------------------------------- */

op_DEA:                         # 3A
                decb    A_Reg
                FlagNZ
                Continue

/* ----------------------------------
       EOR instruction
   ---------------------------------- */

op_EOR_ind_zpage:               # 52
                GetIndZPage
                DoEOR
                Continue

/* ----------------------------------
       INA instruction
   ---------------------------------- */

op_INA:                         # 1A
                incb    A_Reg
                FlagNZ
                Continue

/* ----------------------------------
       JMP instructions
   ---------------------------------- */

op_JMP_ind_65c02:               # 6C - different from 6502
                xorl    %eax, %eax
                GetFromMem_B(PC_Reg_E)
                xchgb   %al, %ah
                incw    PC_Reg
                GetFromMem_B(PC_Reg_E)
                xchgb   %al, %ah
                GetFromMem_W(%eax)
                movw    %ax, PC_Reg
                Continue

op_JMP_abs_ind_x:               # 7C
                GetFromPC_W
                movw    %ax, EffectiveAddr
                movzbl  X_Reg, %eax
                addw    %ax, EffectiveAddr
                GetFromMem_W(EffectiveAddr_E)
                movw    %ax, PC_Reg
                Continue

/* ----------------------------------
       LDA instruction
   ---------------------------------- */

op_LDA_ind_zpage:               # B2
                GetIndZPage
                DoLDA
                Continue

/* ----------------------------------
       ORA instruction
   ---------------------------------- */

op_ORA_ind_zpage:               # 12
                GetIndZPage
                DoORA
                Continue

/* ----------------------------------
       PHX instruction
   ---------------------------------- */

op_PHX:                         # DA
                Push(X_Reg)
                Continue

/* ----------------------------------
       PHY instruction
   ---------------------------------- */

op_PHY:                         # 5A
                Push(Y_Reg)
                Continue

/* ----------------------------------
       PLX instruction
   ---------------------------------- */

op_PLX:                         # FA
                Pop(X_Reg)
                orb X_Reg, X_Reg
                FlagNZ
                Continue

/* ----------------------------------
       PLY instruction
   ---------------------------------- */

op_PLY:                         # 7A
                Pop(Y_Reg)
                orb Y_Reg, Y_Reg
                FlagNZ
                Continue

/* ----------------------------------
       STA instruction
   ---------------------------------- */

op_STA_ind_zpage:               # 92
                GetIndZPage
                DoSTA
                Continue

/* ----------------------------------
       SBC instruction
   ---------------------------------- */

op_SBC_ind_zpage:               # F2
                GetIndZPage
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_SBC_dec              # Yes, jump to decimal version
                DoSBC_b
                Continue
op_SBC_ind_zpage_dec:
                DoSBC_d
                Continue

/* ----------------------------------
       STZ instructions
   ---------------------------------- */

op_STZ_zpage:                   # 64
                GetZPage
                DoSTZ
                Continue

op_STZ_zpage_x:                 # 74
                GetZPage_X
                Continue

op_STZ_abs:                     # 9C
                GetAbs
                DoSTZ
                Continue

op_STZ_abs_x:                   # 9E
                GetAbs_X
                DoSTZ
                Continue

/* ----------------------------------
       TRB instructions
   ---------------------------------- */

op_TRB_abs:                     # 1C
                GetAbs
                DoTRB
                Continue

op_TRB_zpage:                   # 14
                GetZPage
                DoTRB
                Continue

/* ----------------------------------
       TSB instructions
   ---------------------------------- */

op_TSB_abs:                     # 0C
                GetAbs
                DoTSB
                Continue

op_TSB_zpage:                   # 04
                GetZPage
                DoTSB
                Continue


/* ----------------------------------
       ??? instruction - 65c02
   ---------------------------------- */
op_UNK_65c02:
                Continue

#endif /* APPLE_IIE */

/* ----------------------------------------------------------------------
       Undocumented 6502 (Illegal instructions)
   ---------------------------------------------------------------------- */

/* ----------------------------------
       HANG instruction
   ---------------------------------- */

op_HANG:
                decw    PC_Reg
                Continue

/* ----------------------------------
       NOP_2 instruction
   ---------------------------------- */

op_NOP_2:
                incw    PC_Reg
                Continue

/* ----------------------------------
       NOP_3 instruction
   ---------------------------------- */

op_NOP_3:
                addw    $2, PC_Reg
                Continue

/* ----------------------------------
       AAX instructions
   ---------------------------------- */

op_AAX_abs:
                GetAbs
                DoAAX
                Continue

op_AAX_zpage:
                GetZPage
                DoAAX
                Continue

op_AAX_zpage_y:
                GetZPage_Y
                DoAAX
                Continue

op_AAX_ind_x:
                GetIndZPage_X
                DoAAX
                Continue

op_AAX_ind_y:
                GetIndZPage_Y
                DoAAX
                Continue

/* ----------------------------------
       AMA instruction
   ---------------------------------- */

op_AMA_imm:
                GetImm
                DoAMA
                Continue

/* ----------------------------------
       ANA instruction
   ---------------------------------- */

op_ANA_imm:
                GetImm
                DoANA
                Continue

/* ----------------------------------
       ANB instruction
   ---------------------------------- */

op_ANB_imm:
                GetImm
                DoANB
                Continue

/* ----------------------------------
       AXM instruction
   ---------------------------------- */

op_AXM_imm:
                GetImm
                DoAXM
                Continue

/* ----------------------------------
       AXS instruction
   ---------------------------------- */

op_AXS_abs_y:
                GetAbs_Y
                DoAXS
                Continue

/* ----------------------------------
       DCP instructions
   ---------------------------------- */

op_DCP_zpage:
                GetZPage
                DoDCP
                Continue

op_DCP_zpage_x:
                GetZPage_X
                DoDCP
                Continue

op_DCP_abs:
                GetAbs
                DoDCP
                Continue

op_DCP_abs_x:
                GetAbs_X
                DoDCP
                Continue

op_DCP_abs_y:
                GetAbs_Y
                DoDCP
                Continue

op_DCP_ind_x:
                GetIndZPage_X
                DoDCP
                Continue

op_DCP_ind_y:
                GetIndZPage_Y
                DoDCP
                Continue

/* ----------------------------------
       ISB instructions
   ---------------------------------- */

op_ISB_dec:
                DoISB_d
                Continue

op_ISB_zpage:
                GetZPage
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_ISB_dec              # Yes, jump to decimal version
                DoISB_b
                Continue

op_ISB_zpage_x:
                GetZPage_X
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_ISB_dec              # Yes, jump to decimal version
                DoISB_b
                Continue


op_ISB_abs:
                GetAbs
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_ISB_dec              # Yes, jump to decimal version
                DoISB_b
                Continue

op_ISB_abs_x:
                GetAbs_X
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_ISB_dec              # Yes, jump to decimal version
                DoISB_b
                Continue

op_ISB_abs_y:
                GetAbs_Y
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_ISB_dec              # Yes, jump to decimal version
                DoISB_b
                Continue

op_ISB_ind_x:
                GetIndZPage_X
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_ISB_dec              # Yes, jump to decimal version
                DoISB_b
                Continue

op_ISB_ind_y:
                GetIndZPage_Y
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_ISB_dec              # Yes, jump to decimal version
                DoISB_b
                Continue

/* ----------------------------------
       LAN instructions
   ---------------------------------- */

op_LAN_zpage:
                GetZPage
                DoLAN
                Continue

op_LAN_zpage_x:
                GetZPage_X
                DoLAN
                Continue

op_LAN_abs:
                GetAbs
                DoLAN
                Continue

op_LAN_abs_x:
                GetAbs_X
                DoLAN
                Continue

op_LAN_abs_y:
                GetAbs_Y
                DoLAN
                Continue

op_LAN_ind_x:
                GetIndZPage_X
                DoLAN
                Continue

op_LAN_ind_y:
                GetIndZPage_Y
                DoLAN
                Continue

/* ----------------------------------
       LAS instruction
   ---------------------------------- */

op_LAS_abs_y:
                GetAbs_Y
                DoLAS
                Continue

/* ----------------------------------
       LAX instructions
   ---------------------------------- */

op_LAX_zpage:
                GetZPage
                DoLAX
                Continue

op_LAX_zpage_y:
                GetZPage_Y
                DoLAX
                Continue

op_LAX_abs:
                GetAbs
                DoLAX
                Continue

op_LAX_abs_y:
                GetAbs_Y
                DoLAX
                Continue

op_LAX_ind_x:
                GetIndZPage_X
                DoLAX
                Continue

op_LAX_ind_y:
                GetIndZPage_Y
                DoLAX
                Continue

/* ----------------------------------
       LOR instructions
   ---------------------------------- */

op_LOR_zpage:
                GetZPage
                DoLOR
                Continue

op_LOR_zpage_x:
                GetZPage_X
                DoLOR
                Continue

op_LOR_abs:
                GetAbs
                DoLOR
                Continue

op_LOR_abs_x:
                GetAbs_X
                DoLOR
                Continue

op_LOR_abs_y:
                GetAbs_Y
                DoLOR
                Continue

op_LOR_ind_x:
                GetIndZPage_X
                DoLOR
                Continue

op_LOR_ind_y:
                GetIndZPage_Y
                DoLOR
                Continue

/* ----------------------------------
       RAD instructions
   ---------------------------------- */

op_RAD_dec:
                DoRAD_d
                Continue

op_RAD_zpage:
                GetZPage
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_RAD_dec              # Yes, jump to decimal version
                DoRAD_b
                Continue

op_RAD_zpage_x:
                GetZPage_X
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_RAD_dec              # Yes, jump to decimal version
                DoRAD_b
                Continue

op_RAD_abs:
                GetAbs
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_RAD_dec              # Yes, jump to decimal version
                DoRAD_b
                Continue

op_RAD_abs_x:
                GetAbs_X
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_RAD_dec              # Yes, jump to decimal version
                DoRAD_b
                Continue

op_RAD_abs_y:
                GetAbs_Y
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_RAD_dec              # Yes, jump to decimal version
                DoRAD_b
                Continue

op_RAD_ind_x:
                GetIndZPage_X
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_RAD_dec              # Yes, jump to decimal version
                DoRAD_b
                Continue

op_RAD_ind_y:
                GetIndZPage_Y
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_RAD_dec              # Yes, jump to decimal version
                DoRAD_b
                Continue

/* ----------------------------------
       RAM instruction
   ---------------------------------- */

op_RAM_imm:
                GetImm
                DoRAM
                Continue

/* ----------------------------------
       RBM instruction
   ---------------------------------- */

op_RBM_imm:
                GetImm
                DoRBM
                Continue

/* ----------------------------------
       REO instructions
   ---------------------------------- */

op_REO_zpage:
                GetZPage
                DoREO
                Continue

op_REO_zpage_x:
                GetZPage_X
                DoREO
                Continue

op_REO_abs:
                GetAbs
                DoREO
                Continue

op_REO_abs_x:
                GetAbs_X
                DoREO
                Continue

op_REO_abs_y:
                GetAbs_Y
                DoREO
                Continue

op_REO_ind_x:
                GetIndZPage_X
                DoREO
                Continue

op_REO_ind_y:
                GetIndZPage_Y
                DoREO
                Continue

/* ----------------------------------
       ZBC instruction
   ---------------------------------- */

op_ZBC_imm:
                GetImm
                testb   $D_Flag, F_Reg          # Decimal mode?
                jnz     op_ZBC_dec              # Yes, jump to decimal version
                DoZBC_b
                Continue
op_ZBC_dec:
                DoZBC_d
                Continue

/* ----------------------------------
       TEA instruction
   ---------------------------------- */

op_TEA_abs_y:
                GetAbs_Y
                DoTEA
                Continue

/* ----------------------------------
       TEX instruction
   ---------------------------------- */

op_TEX_abs_y:
                GetAbs_Y
                DoTEX
                Continue

/* ----------------------------------
       TEY instruction
   ---------------------------------- */

op_TEY_abs_x:
                GetAbs_X
                DoTEY
                Continue

/* ----------------------------------
       XMA instruction
   ---------------------------------- */

op_XMA_imm:
                GetImm
                DoXMA
                Continue

/* cpu throttling and continue */
continue:       SaveState
                pushal
                call    SN(timing_throttle)
                popal
                ReplaceState
                xorb    %ah, %ah
                ZeroOp
                GetFromPC_B
                jmp     *cpu65__opcodes(,%eax,4)

/* Exception handler */

exception:      cmpb    $RebootSig, %ah
                jz      ex_reboot
                cmpb    $ResetSig, %ah
                jz      ex_reset
                cmpb    $DebugStepSig, %ah
                jz      ex_step
                jmp     ex_enter

ex_step:        SaveState
                movb    $0, SN(cpu65__signal)
                popal
                ret

ex_enter:       SaveState
                movb    $0, SN(cpu65__signal)
                pushal
                call    SN(enter_debugger)
                popal
                ReplaceState
                xorb    %ah, %ah
                GetFromPC_B
                jmp     *cpu65__opcodes(,%eax,4)

ex_reboot:      popal
                movb    $0, SN(cpu65__signal)
                ret

ex_reset:       movb    $0, SN(cpu65__signal)
                movw    $0xfffc, EffectiveAddr
                GetFromEA_W
                movw    %ax, PC_Reg
                xorb    %ah, %ah
                GetFromPC_B
                jmp     *cpu65__opcodes(,%eax,4)

/* -----------------------------------------------------------------
 * Begin emulation.
 * ----------------------------------------------------------------- */
E(cpu65_run)
                pushal
                /* Zero all registers, as well as the unused 32-bit parts
                 * of variables. (which may need to be kept 0)
                 *
                 * Note: dependent on assignment of registers */
                xorl    %eax, %eax
                xorl    %ebx, %ebx
                xorl    %ecx, %ecx
                xorl    %esi, %esi
                xorl    %edi, %edi
                movl    $0x1FF, %edx # Stack pointer
                jmp     ex_reset

E (cpu65_direct_write)
                /* NB: dependent on register choices */
                pushl   %edi
                movl    8(%esp),%edi
                movl    12(%esp),%eax
                call    * SN(cpu65_vmem)+4(,EffectiveAddr_E,8)
                popl    %edi
                ret

/* -------------------------------------------------------------------------
        steps the simulation while remaining in the debugger's control
   ------------------------------------------------------------------------- */
E(cpu65_step)
                pushal
                movb    $DebugStepSig,SN(cpu65__signal)
                ReplaceState
                GetFromPC_B
                jmp     *cpu65__opcodes(,%eax,4)


/* Tables */

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

#ifdef APPLE_IIE

E(cpu65__cmos)
                .long   op_BRK
                .long   op_ORA_ind_x
                .long   op_UNK_65c02
                .long   op_UNK_65c02
                .long   op_TSB_zpage
                .long   op_ORA_zpage
                .long   op_ASL_zpage
                .long   op_UNK_65c02
                .long   op_PHP
                .long   op_ORA_imm
                .long   op_ASL_acc
                .long   op_UNK_65c02
                .long   op_TSB_abs
                .long   op_ORA_abs
                .long   op_ASL_abs
                .long   op_UNK_65c02
                .long   op_BPL
                .long   op_ORA_ind_y
                .long   op_ORA_ind_zpage
                .long   op_UNK_65c02
                .long   op_TRB_zpage
                .long   op_ORA_zpage_x
                .long   op_ASL_zpage_x
                .long   op_UNK_65c02
                .long   op_CLC
                .long   op_ORA_abs_y
                .long   op_INA
                .long   op_UNK_65c02
                .long   op_TRB_abs
                .long   op_ORA_abs_x
                .long   op_ASL_abs_x
                .long   op_UNK_65c02
                .long   op_JSR
                .long   op_AND_ind_x
                .long   op_UNK_65c02
                .long   op_UNK_65c02
                .long   op_BIT_zpage
                .long   op_AND_zpage
                .long   op_ROL_zpage
                .long   op_UNK_65c02
                .long   op_PLP
                .long   op_AND_imm
                .long   op_ROL_acc
                .long   op_UNK_65c02
                .long   op_BIT_abs
                .long   op_AND_abs
                .long   op_ROL_abs
                .long   op_UNK_65c02
                .long   op_BMI
                .long   op_AND_ind_y
                .long   op_AND_ind_zpage
                .long   op_UNK_65c02
                .long   op_BIT_zpage_x
                .long   op_AND_zpage_x
                .long   op_ROL_zpage_x
                .long   op_UNK_65c02
                .long   op_SEC
                .long   op_AND_abs_y
                .long   op_DEA
                .long   op_UNK_65c02
                .long   op_BIT_abs_x
                .long   op_AND_abs_x
                .long   op_ROL_abs_x
                .long   op_UNK_65c02
                .long   op_RTI
                .long   op_EOR_ind_x
                .long   op_UNK_65c02
                .long   op_UNK_65c02
                .long   op_UNK_65c02
                .long   op_EOR_zpage
                .long   op_LSR_zpage
                .long   op_UNK_65c02
                .long   op_PHA
                .long   op_EOR_imm
                .long   op_LSR_acc
                .long   op_UNK_65c02
                .long   op_JMP_abs
                .long   op_EOR_abs
                .long   op_LSR_abs
                .long   op_UNK_65c02
                .long   op_BVC
                .long   op_EOR_ind_y
                .long   op_EOR_ind_zpage
                .long   op_UNK_65c02
                .long   op_UNK_65c02
                .long   op_EOR_zpage_x
                .long   op_LSR_zpage_x
                .long   op_UNK_65c02
                .long   op_CLI
                .long   op_EOR_abs_y
                .long   op_PHY
                .long   op_UNK_65c02
                .long   op_UNK_65c02
                .long   op_EOR_abs_x
                .long   op_LSR_abs_x
                .long   op_UNK_65c02
                .long   op_RTS
                .long   op_ADC_ind_x
                .long   op_UNK_65c02
                .long   op_UNK_65c02
                .long   op_STZ_zpage
                .long   op_ADC_zpage
                .long   op_ROR_zpage
                .long   op_UNK_65c02
                .long   op_PLA
                .long   op_ADC_imm
                .long   op_ROR_acc
                .long   op_UNK_65c02
                .long   op_JMP_ind_65c02
                .long   op_ADC_abs
                .long   op_ROR_abs
                .long   op_UNK_65c02
                .long   op_BVS
                .long   op_ADC_ind_y
                .long   op_ADC_ind_zpage
                .long   op_UNK_65c02
                .long   op_STZ_zpage_x
                .long   op_ADC_zpage_x
                .long   op_ROR_zpage_x
                .long   op_UNK_65c02
                .long   op_SEI
                .long   op_ADC_abs_y
                .long   op_PLY
                .long   op_UNK_65c02
                .long   op_JMP_abs_ind_x
                .long   op_ADC_abs_x
                .long   op_ROR_abs_x
                .long   op_UNK_65c02
                .long   op_BRA
                .long   op_STA_ind_x
                .long   op_UNK_65c02
                .long   op_UNK_65c02
                .long   op_STY_zpage
                .long   op_STA_zpage
                .long   op_STX_zpage
                .long   op_UNK_65c02
                .long   op_DEY
                .long   op_BIT_imm
                .long   op_TXA
                .long   op_UNK_65c02
                .long   op_STY_abs
                .long   op_STA_abs
                .long   op_STX_abs
                .long   op_UNK_65c02
                .long   op_BCC
                .long   op_STA_ind_y
                .long   op_STA_ind_zpage
                .long   op_UNK_65c02
                .long   op_STY_zpage_x
                .long   op_STA_zpage_x
                .long   op_STX_zpage_y
                .long   op_UNK_65c02
                .long   op_TYA
                .long   op_STA_abs_y
                .long   op_TXS
                .long   op_UNK_65c02
                .long   op_STZ_abs
                .long   op_STA_abs_x
                .long   op_STZ_abs_x
                .long   op_UNK_65c02
                .long   op_LDY_imm
                .long   op_LDA_ind_x
                .long   op_LDX_imm
                .long   op_UNK_65c02
                .long   op_LDY_zpage
                .long   op_LDA_zpage
                .long   op_LDX_zpage
                .long   op_UNK_65c02
                .long   op_TAY
                .long   op_LDA_imm
                .long   op_TAX
                .long   op_UNK_65c02
                .long   op_LDY_abs
                .long   op_LDA_abs
                .long   op_LDX_abs
                .long   op_UNK_65c02
                .long   op_BCS
                .long   op_LDA_ind_y
                .long   op_LDA_ind_zpage
                .long   op_UNK_65c02
                .long   op_LDY_zpage_x
                .long   op_LDA_zpage_x
                .long   op_LDX_zpage_y
                .long   op_UNK_65c02
                .long   op_CLV
                .long   op_LDA_abs_y
                .long   op_TSX
                .long   op_UNK_65c02
                .long   op_LDY_abs_x
                .long   op_LDA_abs_x
                .long   op_LDX_abs_y
                .long   op_UNK_65c02
                .long   op_CPY_imm
                .long   op_CMP_ind_x
                .long   op_UNK_65c02
                .long   op_UNK_65c02
                .long   op_CPY_zpage
                .long   op_CMP_zpage
                .long   op_DEC_zpage
                .long   op_UNK_65c02
                .long   op_INY
                .long   op_CMP_imm
                .long   op_DEX
                .long   op_UNK_65c02
                .long   op_CPY_abs
                .long   op_CMP_abs
                .long   op_DEC_abs
                .long   op_UNK_65c02
                .long   op_BNE
                .long   op_CMP_ind_y
                .long   op_CMP_ind_zpage
                .long   op_UNK_65c02
                .long   op_UNK_65c02
                .long   op_CMP_zpage_x
                .long   op_DEC_zpage_x
                .long   op_UNK_65c02
                .long   op_CLD
                .long   op_CMP_abs_y
                .long   op_PHX
                .long   op_UNK_65c02
                .long   op_UNK_65c02
                .long   op_CMP_abs_x
                .long   op_DEC_abs_x
                .long   op_UNK_65c02
                .long   op_CPX_imm
                .long   op_SBC_ind_x
                .long   op_UNK_65c02
                .long   op_UNK_65c02
                .long   op_CPX_zpage
                .long   op_SBC_zpage
                .long   op_INC_zpage
                .long   op_UNK_65c02
                .long   op_INX
                .long   op_SBC_imm
                .long   op_NOP
                .long   op_UNK_65c02
                .long   op_CPX_abs
                .long   op_SBC_abs
                .long   op_INC_abs
                .long   op_UNK_65c02
                .long   op_BEQ
                .long   op_SBC_ind_y
                .long   op_SBC_ind_zpage
                .long   op_UNK_65c02
                .long   op_UNK_65c02
                .long   op_SBC_zpage_x
                .long   op_INC_zpage_x
                .long   op_UNK_65c02
                .long   op_SED
                .long   op_SBC_abs_y
                .long   op_PLX
                .long   op_UNK_65c02
                .long   op_UNK_65c02
                .long   op_SBC_abs_x
                .long   op_INC_abs_x
                .long   op_UNK_65c02

#endif /* HAVE_IIE */

E(cpu65__nmos)
                .long   op_BRK
                .long   op_ORA_ind_x
                .long   op_HANG
                .long   op_LOR_ind_x
                .long   op_NOP_2
                .long   op_ORA_zpage
                .long   op_ASL_zpage
                .long   op_LOR_zpage
                .long   op_PHP
                .long   op_ORA_imm
                .long   op_ASL_acc
                .long   op_ANA_imm
                .long   op_NOP_3
                .long   op_ORA_abs
                .long   op_ASL_abs
                .long   op_LOR_abs
                .long   op_BPL
                .long   op_ORA_ind_y
                .long   op_HANG
                .long   op_LOR_ind_y
                .long   op_NOP_2
                .long   op_ORA_zpage_x
                .long   op_ASL_zpage_x
                .long   op_LOR_zpage_x
                .long   op_CLC
                .long   op_ORA_abs_y
                .long   op_NOP
                .long   op_LOR_abs_y
                .long   op_NOP_3
                .long   op_ORA_abs_x
                .long   op_ASL_abs_x
                .long   op_LOR_abs_x
                .long   op_JSR
                .long   op_AND_ind_x
                .long   op_HANG
                .long   op_LAN_ind_x
                .long   op_BIT_zpage
                .long   op_AND_zpage
                .long   op_ROL_zpage
                .long   op_LAN_zpage
                .long   op_PLP
                .long   op_AND_imm
                .long   op_ROL_acc
                .long   op_ANB_imm
                .long   op_BIT_abs
                .long   op_AND_abs
                .long   op_ROL_abs
                .long   op_LAN_abs
                .long   op_BMI
                .long   op_AND_ind_y
                .long   op_HANG
                .long   op_LAN_ind_y
                .long   op_NOP_2
                .long   op_AND_zpage_x
                .long   op_ROL_zpage_x
                .long   op_LAN_zpage_x
                .long   op_SEC
                .long   op_AND_abs_y
                .long   op_NOP
                .long   op_LAN_abs_y
                .long   op_NOP_3
                .long   op_AND_abs_x
                .long   op_ROL_abs_x
                .long   op_LAN_abs_x
                .long   op_RTI
                .long   op_EOR_ind_x
                .long   op_HANG
                .long   op_REO_ind_x
                .long   op_NOP_2
                .long   op_EOR_zpage
                .long   op_LSR_zpage
                .long   op_REO_zpage
                .long   op_PHA
                .long   op_EOR_imm
                .long   op_LSR_acc
                .long   op_RAM_imm
                .long   op_JMP_abs
                .long   op_EOR_abs
                .long   op_LSR_abs
                .long   op_REO_abs
                .long   op_BVC
                .long   op_EOR_ind_y
                .long   op_HANG
                .long   op_REO_ind_y
                .long   op_NOP_2
                .long   op_EOR_zpage_x
                .long   op_LSR_zpage_x
                .long   op_REO_zpage_x
                .long   op_CLI
                .long   op_EOR_abs_y
                .long   op_NOP
                .long   op_REO_abs_y
                .long   op_NOP_3
                .long   op_EOR_abs_x
                .long   op_LSR_abs_x
                .long   op_REO_abs_x
                .long   op_RTS
                .long   op_ADC_ind_x
                .long   op_HANG
                .long   op_RAD_ind_x
                .long   op_NOP_2
                .long   op_ADC_zpage
                .long   op_ROR_zpage
                .long   op_RAD_zpage
                .long   op_PLA
                .long   op_ADC_imm
                .long   op_ROR_acc
                .long   op_RBM_imm
                .long   op_JMP_ind
                .long   op_ADC_abs
                .long   op_ROR_abs
                .long   op_RAD_abs
                .long   op_BVS
                .long   op_ADC_ind_y
                .long   op_HANG
                .long   op_RAD_ind_y
                .long   op_NOP_2
                .long   op_ADC_zpage_x
                .long   op_ROR_zpage_x
                .long   op_RAD_zpage_x
                .long   op_SEI
                .long   op_ADC_abs_y
                .long   op_NOP
                .long   op_RAD_abs_y
                .long   op_NOP_3
                .long   op_ADC_abs_x
                .long   op_ROR_abs_x
                .long   op_RAD_abs_x
                .long   op_NOP_2
                .long   op_STA_ind_x
                .long   op_NOP_2
                .long   op_AAX_ind_x
                .long   op_STY_zpage
                .long   op_STA_zpage
                .long   op_STX_zpage
                .long   op_AAX_zpage
                .long   op_DEY
                .long   op_NOP_2
                .long   op_TXA
                .long   op_XMA_imm
                .long   op_STY_abs
                .long   op_STA_abs
                .long   op_STX_abs
                .long   op_AAX_abs
                .long   op_BCC
                .long   op_STA_ind_y
                .long   op_HANG
                .long   op_AAX_ind_y
                .long   op_STY_zpage_x
                .long   op_STA_zpage_x
                .long   op_STX_zpage_y
                .long   op_AAX_zpage_y
                .long   op_TYA
                .long   op_STA_abs_y
                .long   op_TXS
                .long   op_AXS_abs_y
                .long   op_TEY_abs_x
                .long   op_STA_abs_x
                .long   op_TEX_abs_y
                .long   op_TEA_abs_y
                .long   op_LDY_imm
                .long   op_LDA_ind_x
                .long   op_LDX_imm
                .long   op_LAX_ind_x
                .long   op_LDY_zpage
                .long   op_LDA_zpage
                .long   op_LDX_zpage
                .long   op_LAX_zpage
                .long   op_TAY
                .long   op_LDA_imm
                .long   op_TAX
                .long   op_AMA_imm
                .long   op_LDY_abs
                .long   op_LDA_abs
                .long   op_LDX_abs
                .long   op_LAX_abs
                .long   op_BCS
                .long   op_LDA_ind_y
                .long   op_HANG
                .long   op_LAX_ind_y
                .long   op_LDY_zpage_x
                .long   op_LDA_zpage_x
                .long   op_LDX_zpage_y
                .long   op_LAX_zpage_y
                .long   op_CLV
                .long   op_LDA_abs_y
                .long   op_TSX
                .long   op_LAS_abs_y
                .long   op_LDY_abs_x
                .long   op_LDA_abs_x
                .long   op_LDX_abs_y
                .long   op_LAX_abs_y
                .long   op_CPY_imm
                .long   op_CMP_ind_x
                .long   op_NOP_2
                .long   op_DCP_ind_x
                .long   op_CPY_zpage
                .long   op_CMP_zpage
                .long   op_DEC_zpage
                .long   op_DCP_zpage
                .long   op_INY
                .long   op_CMP_imm
                .long   op_DEX
                .long   op_AXM_imm
                .long   op_CPY_abs
                .long   op_CMP_abs
                .long   op_DEC_abs
                .long   op_DCP_abs
                .long   op_BNE
                .long   op_CMP_ind_y
                .long   op_HANG
                .long   op_DCP_ind_y
                .long   op_NOP_2
                .long   op_CMP_zpage_x
                .long   op_DEC_zpage_x
                .long   op_DCP_zpage_x
                .long   op_CLD
                .long   op_CMP_abs_y
                .long   op_NOP
                .long   op_DCP_abs_y
                .long   op_NOP_3
                .long   op_CMP_abs_x
                .long   op_DEC_abs_x
                .long   op_DCP_abs_x
                .long   op_CPX_imm
                .long   op_SBC_ind_x
                .long   op_NOP_2
                .long   op_ISB_ind_x
                .long   op_CPX_zpage
                .long   op_SBC_zpage
                .long   op_INC_zpage
                .long   op_ISB_zpage
                .long   op_INX
                .long   op_SBC_imm
                .long   op_NOP
                .long   op_ZBC_imm
                .long   op_CPX_abs
                .long   op_SBC_abs
                .long   op_INC_abs
                .long   op_ISB_abs
                .long   op_BEQ
                .long   op_SBC_ind_y
                .long   op_HANG
                .long   op_ISB_ind_y
                .long   op_NOP_2
                .long   op_SBC_zpage_x
                .long   op_INC_zpage_x
                .long   op_ISB_zpage_x
                .long   op_SED
                .long   op_SBC_abs_y
                .long   op_NOP
                .long   op_ISB_abs_y
                .long   op_NOP_3
                .long   op_SBC_abs_x
                .long   op_INC_abs_x
                .long   op_ISB_abs_x

