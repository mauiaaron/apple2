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

#ifndef _CPU_REGS_H_
#define _CPU_REGS_H_

#include "cpu.h"

// ARM register mappings

// r0, r1 are scratch regs, with r0 generally as the "important byte"
#define EffectiveAddr   r2              /* 16bit Effective address    */
#define PC_Reg          r3              /* 16bit 6502 Program Counter */
#define SP_Reg          r4              /* 16bit 6502 Stack pointer   */
#define F_Reg           r5              /* 8bit 6502 flags            */
#define Y_Reg           r6              /* 8bit 6502 Y register       */
#define X_Reg           r7              /* 8bit 6502 X register       */
#define A_Reg           r8              /* 8bit 6502 A register       */
// r9 is "ARM platform register" ... used as a scratch register
// r10 is another scratch variable
#define reg_vmem_r      r11             /* cpu65_vmem_r table address */
// r12 is "ARM Intra-Procedure-call scratch register" ... used as a scratch register
// r13 ARM SP
// r14 ARM return addr
// r15 ARM PC


#ifdef __aarch64__
#   error 20150205 ARM 64bit untested!!!
#   define PTR_SHIFT        #4 // 4<<1 = 8
#   define ROR_BIT          0x8000000000000000
#else
#   define PTR_SHIFT        #2 // 2<<1 = 4
#   define ROR_BIT          0x80000000
#endif


#define NO_UNDERSCORES 1
#if NO_UNDERSCORES
#   define SYM(x)                   =x
#   define ENTRY(x)                 .globl x; .balign 16; x##:
#   define CALL(x)                  x
#else
#   define SYM(x)                   =_##x
#   define ENTRY(x)                 .globl _##x; .balign 16; _##x##:
#   define CALL(x)                  _##x
#endif

#endif // whole file
