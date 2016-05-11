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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cpu.h"

#define X_Reg           %bl             /* 6502 X register in %bl  */
#define Y_Reg           %bh             /* 6502 Y register in %bh  */
#define A_Reg           %cl             /* 6502 A register in %cl  */
#define F_Reg           %ch             /* 6502 flags in %ch       */
#define SP_Reg_L        %dl             /* 6502 Stack pointer low  */
#define SP_Reg_H        %dh             /* 6502 Stack pointer high */
#define PC_Reg          %si             /* 6502 Program Counter    */
#define PC_Reg_L        %sil            /* 6502 PC low             */
#define PC_Reg_H        %sih            /* 6502 PC high            */
#define EffectiveAddr   %di             /* Effective address       */
#define EffectiveAddr_L %dil            /* Effective address low   */
#define EffectiveAddr_H %dih            /* Effective address high   */

#define X86_CF_Bit 0x0                  /* x86 carry               */
#define X86_AF_Bit 0x4                  /* x86 adj (nybble carry)  */

#define RestoreAltZP \
    /* Apple //e set stack point to ALTZP (or not) */ \
    MEM2REG(movLQ, base_stackzp, _XAX); \
    MEM2REG(subLQ, base_vmem, _XAX); \
    orLQ    $0x0100, SP_Reg_X; \
    orLQ    _XAX, SP_Reg_X;

#if __LP64__
#   define SZ_PTR           8
#   define ROR_BIT          63
// x86_64 registers
#   define _XBP             %rbp        /* x86_64 base pointer     */
#   define _XSP             %rsp        /* x86_64 stack pointer    */
#   define _XDI             %rdi
#   define _XSI             %rsi
#   define _XAX             %rax        /* scratch                 */
#   define _XBX             %rbx        /* scratch2                */
#   define _X8              %r8
// full-length Apple ][ registers
#   define XY_Reg_X         %rbx        /* 6502 X&Y flags          */
#   define AF_Reg_X         %rcx        /* 6502 F&A flags          */
#   define SP_Reg_X         %rdx        /* 6502 Stack pointer      */
#   define PC_Reg_X         %rsi        /* 6502 Program Counter    */
#   define EffectiveAddr_X  %rdi        /* Effective address       */
// full-length assembly instructions
#   define addLQ            addq
#   define andLQ            andq
#   define callLQ           callq
#   define decLQ            decq
#   define leaLQ            leaq
#   define orLQ             orq
#   define movLQ            movq
#   define movzbLQ          movzbq
#   define movzwLQ          movzwq
#   define popaLQ           popaq
#   define popLQ            popq
#   define pushaLQ          pushaq
#   define pushfLQ          pushfq
#   define pushLQ           pushq
#   define rorLQ            rorq
#   define shlLQ            shlq
#   define shrLQ            shrq
#   define subLQ            subq
#   define testLQ           testq
#   define xorLQ            xorq
#else
#   define SZ_PTR           4
#   define ROR_BIT          31
// x86 registers
#   define _XBP             %ebp        /* x86 base pointer        */
#   define _PICREG          %ebp        /* used for accessing GOT  */
#   define _XSP             %esp        /* x86 stack pointer       */
#   define _XDI             %edi
#   define _XSI             %esi
#   define _XAX             %eax        /* scratch                 */
#   define _XBX             %ebx        /* scratch2                */
#   define _X8              %eax // WRONG!!! FIXMENOW
// full-length Apple ][ registers
#   define XY_Reg_X         %ebx        /* 6502 X&Y flags          */
#   define AF_Reg_X         %ecx        /* 6502 F&A flags          */
#   define SP_Reg_X         %edx        /* 6502 Stack pointer      */
#   define PC_Reg_X         %esi        /* 6502 Program Counter    */
#   define EffectiveAddr_X  %edi        /* Effective address       */
// full-length assembly instructions
#   define addLQ            addl
#   define andLQ            andl
#   define callLQ           calll
#   define decLQ            decl
#   define leaLQ            leal
#   define orLQ             orl
#   define movLQ            movl
#   define movzbLQ          movzbl
#   define movzwLQ          movzwl
#   define popaLQ           popal
#   define popLQ            popl
#   define pushaLQ          pushal
#   define pushfLQ          pushfl
#   define pushLQ           pushl
#   define rorLQ            rorl
#   define shlLQ            shll
#   define shrLQ            shrl
#   define subLQ            subl
#   define testLQ           testl
#   define xorLQ            xorl
#endif

/* Symbol naming issues */
#if NO_UNDERSCORES
#   define _UNDER(x)                    x
#else
#   define _UNDER(x)                    _##x
#endif

#define ENTRY(x)                        .globl _UNDER(x); .balign 16; _UNDER(x)##:

#if !__PIC__

// For non-Position Independent Code, the assembly is relatively simple...

#   define      CALL_FN(op,fn,stk)      op _UNDER(fn)
#   define      JUMP_FN(op,fn)          op _UNDER(fn)
#   define      CALL_IND0(sym)          callLQ  *_UNDER(sym)
#   define      CALL_IND(sym,off,sz)    callLQ  *_UNDER(sym)(,off,sz)
#   define      JUMP_IND(sym,off,sz)    jmp     *_UNDER(sym)(,off,sz)
#   define      MEM2REG_IND(op,sym,off,sz,x) op     _UNDER(sym)(,off,sz), x
#   define      REG2MEM_IND(op,x,sym,off,sz) op  x, _UNDER(sym)(,off,sz)
#   define      _2MEM(op,sym)           op      _UNDER(sym) // op to-memory
#   define      REG2MEM(op,x,sym)       op   x, _UNDER(sym) // op register-to-memory
#   define      MEM2REG(op,sym,x)       op   _UNDER(sym), x // op memory-to-register
#else

// For PIC code, the assembly is more convoluted, because we have to access symbols only indirectly through the Global
// Offset Table and the Procedure Linkage Table.  There is some redundancy in the codegen from these macros (e.g.,
// access to the same symbol back-to-back results in duplicate register loads, when we could keep using the previously
// calculated value).

#   if __APPLE__
#       if !__LP64__
#           error unsure of __PIC__ code on i386 Mac
#       endif
#       define  _AT_PLT
#       define  _LEA(sym)               leaq _UNDER(sym)(%rip), _X8
#       define  CALL_IND0(fn)           callq *_UNDER(fn)(%rip)
#       define  _2MEM(op,sym)           op   _UNDER(sym)(%rip)    // op to-memory
#       define  REG2MEM(op,x,sym)       op   x, _UNDER(sym)(%rip) // op register-to-memory
#       define  MEM2REG(op,sym,x)       op   _UNDER(sym)(%rip), x // op memory-to-register
#   elif __LP64__
#       define  _AT_PLT @PLT
#       define  _LEA(sym)               movq _UNDER(sym)@GOTPCREL(%rip), _X8
#       define  CALL_IND0(fn)           callq *_UNDER(fn)_AT_PLT
#       define  _2MEM(op,sym)           _LEA(sym); op (_X8)    // op to-memory
#       define  REG2MEM(op,x,sym)       _LEA(sym); op x, (_X8) // op register-to-memory
#       define  MEM2REG(op,sym,x)       _LEA(sym); op (_X8), x // op memory-to-register
#   endif

#   if __LP64__
#       define  CALL_FN(op,fn,stk)           op _UNDER(fn)_AT_PLT
#       define  JUMP_FN(op,fn)               op _UNDER(fn)_AT_PLT
#       define  CALL_IND(sym,off,sz)         _LEA(sym); callq  *(_X8,off,sz)
#       define  JUMP_IND(sym,off,sz)         _LEA(sym); jmp    *(_X8,off,sz)
#       define  MEM2REG_IND(op,sym,off,sz,x) _LEA(sym); op      (_X8,off,sz), x
#       define  REG2MEM_IND(op,x,sym,off,sz) _LEA(sym); op x, (_X8,off,sz)
#   else

#       if !__i386__
#           error what architecture is this?!
#       endif

// http://ewontfix.com/18/ -- "32-bit x86 Position Independent Code - It's that bad"

// 2016/05/01 : Strategy here is to (ab)use _PICREG in cpu65_run() to contain the offset to the GOT for symbol access.
// %ebx is used only for actual calls to the fn@PLT (per ABI convention).  Similar to x64 PIC, use of these macros does
// result in some code duplication...

#       define  CALL_FN(op,fn,stk)      movl stk(%esp), %ebx; \
                                        op _UNDER(fn)@PLT;

#       define  _GOT_PRE(sym,reg)       movl _A2_PIC_GOT(%esp), reg; \
                                        movl _UNDER(sym)@GOT(reg), reg;

#       define  CALL_IND0(fn)           _GOT_PRE(fn, _PICREG); calll *(_PICREG);

#       define  CALL_IND(sym,off,sz)    _GOT_PRE(sym,_PICREG); calll *(_PICREG,off,sz);

#       define  JUMP_FN(op,fn)          op _UNDER(fn)

#       define  JUMP_IND(sym,off,sz)         _GOT_PRE(sym,_PICREG); jmp  *(_PICREG,off,sz);
#       define  MEM2REG_IND(op,sym,off,sz,x) _GOT_PRE(sym,_PICREG); op (_PICREG,off,sz), x;
#       define  REG2MEM_IND(op,x,sym,off,sz) _GOT_PRE(sym,_PICREG); op x, (_PICREG,off,sz);
#       define  _2MEM(op,sym)                _GOT_PRE(sym,_PICREG); op (_PICREG);    // op to-memory
#       define  REG2MEM(op,x,sym)            _GOT_PRE(sym,_PICREG); op x, (_PICREG); // op register-to-memory
#       define  MEM2REG(op,sym,x)            _GOT_PRE(sym,_PICREG); op (_PICREG), x; // op memory-to-register
#   endif
#endif

#endif // whole file

