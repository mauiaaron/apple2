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

#include "cpu.h"

#define X_Reg           %bl             /* 6502 X register in %bl  */
#define Y_Reg           %bh             /* 6502 Y register in %bh  */
#define A_Reg           %cl             /* 6502 A register in %cl  */
#define F_Reg           %ch             /* 6502 flags in %ch       */
#define SP_Reg_L        %dl             /* 6502 Stack pointer low  */
#define SP_Reg_H        %dh             /* 6502 Stack pointer high */
#define PC_Reg          %si             /* 6502 Program Counter    */
#define EffectiveAddr   %di             /* Effective address       */

#if __LP64__
#   error not ready
#else
#   define SZ_PTR           4
#   define _XSP             %esp        /* x86 stack pointer       */
#   define _XAX             %eax        /* scratch                 */
#   define _XBX             %ebx        /* scratch2                */
// full-length Apple ][ registers
#   define XY_Reg_X         %ebx        /* 6502 X&Y flags          */
#   define AF_Reg_X         %ecx        /* 6502 F&A flags          */
#   define SP_Reg_X         %edx        /* 6502 Stack pointer      */
#   define PC_Reg_X         %esi        /* 6502 Program Counter    */
#   define EffectiveAddr_X  %edi        /* Effective address       */
// full-length assembly instructions
#   define addLQ            addl
#   define andLQ            andl
#   define decLQ            decl
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

