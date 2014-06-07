/* 
 * Apple // emulator for Linux: Glue file prologue for Intel 386
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

#define __ASSEMBLY__
#include "apple2.h"
#include "misc.h"
#include "cpu-regs.h"

#define GLUE_FIXED_READ(func,address) \
E(func)                 movb    SN(address)(EffectiveAddr_E),%al; \
                        ret;

#define GLUE_FIXED_WRITE(func,address) \
E(func)                 movb    %al,SN(address)(EffectiveAddr_E); \
                        ret;

#define GLUE_BANK_MAYBEREAD(func,pointer) \
E(func)                 testl   $SS_CXROM, SN(softswitches); \
                        jnz     1f; \
                        call    *SN(pointer); \
                        ret; \
1:                      addl    SN(pointer),EffectiveAddr_E; \
                        movb    (EffectiveAddr_E),%al; \
                        subl    SN(pointer),EffectiveAddr_E; \
                        ret;

#define GLUE_BANK_READ(func,pointer) \
E(func)                 addl    SN(pointer),EffectiveAddr_E; \
                        movb    (EffectiveAddr_E),%al; \
                        subl    SN(pointer),EffectiveAddr_E; \
                        ret;

#define GLUE_BANK_WRITE(func,pointer) \
E(func)                 addl    SN(pointer),EffectiveAddr_E; \
                        movb    %al,(EffectiveAddr_E); \
                        subl    SN(pointer),EffectiveAddr_E; \
                        ret;

#define GLUE_BANK_MAYBEWRITE(func,pointer) \
E(func)                 addl    SN(pointer),EffectiveAddr_E; \
                        cmpl    $0,SN(pointer); \
                        jz      1f; \
                        movb    %al,(EffectiveAddr_E); \
1:                      ret;


// TODO FIXME : implement CDECL prologue/epilogues...
#define GLUE_C_WRITE(func) \
E(func)                 pushl   %eax; \
                        pushl   XY_Regs; \
                        pushl   FF_Reg; \
                        pushl   SP_Reg; \
                        pushl   PC_Reg_E; \
                        andl    $0xff,%eax; \
                        pushl   %eax; \
                        pushl   EffectiveAddr_E; \
                        call    SN(c_##func); \
                        popl    %edx; /* dummy */ \
                        popl    %edx; /* dummy */ \
                        popl    PC_Reg_E; \
                        popl    SP_Reg; \
                        popl    FF_Reg; \
                        popl    XY_Regs; \
                        popl    %eax; \
                        ret;

// TODO FIXME : implement CDECL prologue/epilogues...
#define _GLUE_C_READ(func, ...) \
E(func)                 pushl   XY_Regs; \
                        pushl   FF_Reg; \
                        pushl   SP_Reg; \
                        pushl   PC_Reg_E; \
                        pushl   %eax; /* HACK: works around mysterious issue with generated mov(%eax), %eax ... */ \
                        pushl   EffectiveAddr_E; \
                        call    SN(c_##func); \
                        popl    %edx; /* dummy */ \
                        movb    %al, %dl; \
                        popl    %eax; /* ... ugh */ \
                        movb    %dl, %al; \
                        popl    PC_Reg_E; \
                        popl    SP_Reg; \
                        popl    FF_Reg; \
                        popl    XY_Regs; \
                        __VA_ARGS__ \
                        ret;

// TODO FIXME : implement CDECL prologue/epilogues...
#define GLUE_C_READ(FUNC) _GLUE_C_READ(FUNC)

#define GLUE_C_READ_ALTZP(FUNC) _GLUE_C_READ(FUNC, \
        pushl   %eax; \
        andl    $0xFFFF, SP_Reg; \
        movl    SN(base_stackzp), %eax; \
        subl    $SN(apple_ii_64k), %eax; \
        orl     %eax, SP_Reg; \
        popl    %eax; \
        )

