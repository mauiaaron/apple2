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
#define XY_Regs         %ebx            /* 6502 X&Y flags          */
#define A_Reg           %cl             /* 6502 A register in %cl  */
#define F_Reg           %ch             /* 6502 flags in %ch       */
#define FF_Reg          %ecx            /* 6502 F&A flags          */
#define SP_Reg_L        %dl             /* 6502 Stack pointer low  */
#define SP_Reg_H        %dh             /* 6502 Stack pointer high */
#define SP_Reg          %edx            /* 6502 Stack pointer      */
#define PC_Reg          %si             /* 6502 Program Counter    */
#define PC_Reg_E        %esi            /* 6502 Program Counter    */
#define EffectiveAddr   %di             /* Effective address       */
#define EffectiveAddr_E %edi            /* Effective address       */

