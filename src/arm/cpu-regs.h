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

#ifndef _CPU_REGS_H_
#define _CPU_REGS_H_

#include "cpu.h"
#include "glue-offsets.h"


#if !defined(__APPLE__)
#   define NO_UNDERSCORES 1
#endif

// ARM register mappings

#define bz                 beq
#define bnz                bne

#define ROR_BIT          0x80000000

#ifdef __aarch64__

#   define DOT_ARM
#   define ALIGN            .align 2;
#   define PTR_SHIFT        #3 // 1<<3 = 8
#   define BLX             blr
#   define BX              br

// x: 64bit addressing mode
// w: 32bit addressing mode

#   define xr0             x0              /* scratch/"important byte"   */
#   define wr0             w0              /* scratch/"important byte"   */
#   define xr1             x1              /* scratch                    */
#   define wr1             w1              /* scratch                    */

#   define wr9             w2

// NOTE: ARMv8 Procedure Call Standard indicates that x19-x28 are callee saved ... so we can call back into C without needing to
// first save these ...
#   define xEffectiveAddr  x19             /* 16bit Effective address    */
#   define EffectiveAddr   w19             /* 16bit Effective address    */
#   define PC_Reg          w20             /* 16bit 6502 Program Counter */
#   define xSP_Reg         x21             /* 16bit 6502 Stack pointer   */
#   define SP_Reg          w21             /* 16bit 6502 Stack pointer   */
#   define xF_Reg          x22             /* 8bit 6502 flags            */
#   define F_Reg           w22             /* 8bit 6502 flags            */
#   define Y_Reg           w23             /* 8bit 6502 Y register       */
#   define X_Reg           w24             /* 8bit 6502 X register       */
#   define A_Reg           w25             /* 8bit 6502 A register       */
#   define xA_Reg          x25             /* 8bit 6502 A register       */
#   define reg_args        x26             /* cpu65_run() args register  */
#   define reg_vmem_r      x27             /* cpu65_vmem_r table address */

#   define xr12            x28             /* scratch                    */
#   define wr12            w28             /* scratch                    */
// x29      : frame pointer (callee-saved)
// x30      : return address
// xzr/wzr  : zero register
// sp       : stack pointer
// pc       : instruction pointer

#else

#   define DOT_ARM          .arm;
#   define ALIGN            .balign 4;
#   define PTR_SHIFT        #2 // 1<<2 = 4
#   define BLX             blx
#   define BX              bx

// r0, r1 are scratch regs, with r0 generally as the "important byte"
#   define xr0             r0              /* scratch/"important byte"   */
#   define wr0             r0              /* scratch/"important byte"   */
#   define xr1             r1              /* scratch                    */
#   define wr1             r1              /* scratch                    */
#   define wr9             r9              /* scratch                    */
// r12 is "ARM Intra-Procedure-call scratch register" ... used as a scratch register
#   define xr12            r12             /* scratch                    */
#   define wr12            r12             /* scratch                    */

// NOTE: these need to be preserved in subroutine (C) invocations ... */
#   define EffectiveAddr   r2              /* 16bit Effective address    */
#   define xEffectiveAddr  r2              /* 16bit Effective address    */
#   define PC_Reg          r3              /* 16bit 6502 Program Counter */

// NOTE: ARMv7 PCS states : "A subroutine must preserve the contents of the registers r4-r8, r10, r11 and SP [...]"
#   define xSP_Reg         r4              /* 16bit 6502 Stack pointer   */
#   define SP_Reg          r4              /* 16bit 6502 Stack pointer   */
#   define xF_Reg          r5              /* 8bit 6502 flags            */
#   define F_Reg           r5              /* 8bit 6502 flags            */
#   define Y_Reg           r6              /* 8bit 6502 Y register       */
#   define X_Reg           r7              /* 8bit 6502 X register       */
#   define A_Reg           r8              /* 8bit 6502 A register       */
#   define xA_Reg          r8              /* 8bit 6502 A register       */

// r9 is "ARM platform register" ... used as a scratch register
#   define reg_args        r10             /* cpu65_run() args register  */
#   define reg_vmem_r      r11             /* cpu65_vmem_r table address */
// r13 ARM SP
// r14 ARM LR (return addr)
// r15 ARM PC

#endif

#if NO_UNDERSCORES
#   define ENTRY(x)         .global x; DOT_ARM ALIGN x##:
#   define CALL(x)          x
#else
#   define ENTRY(x)         .global _##x; DOT_ARM ALIGN _##x##:
#   define CALL(x)          _##x
#endif

#endif // whole file

