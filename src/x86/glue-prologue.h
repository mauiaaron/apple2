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
E(func)                 movb    SN(address)(EffectiveAddr_X),%al; \
                        ret;

#define GLUE_FIXED_WRITE(func,address) \
E(func)                 movb    %al,SN(address)(EffectiveAddr_X); \
                        ret;

#define GLUE_BANK_MAYBEREAD(func,pointer) \
E(func)                 testl   $SS_CXROM, SN(softswitches); \
                        jnz     1f; \
                        call    *SN(pointer); \
                        ret; \
1:                      addl    SN(pointer),EffectiveAddr_X; \
                        movb    (EffectiveAddr_X),%al; \
                        subl    SN(pointer),EffectiveAddr_X; \
                        ret;

#define GLUE_BANK_READ(func,pointer) \
E(func)                 addl    SN(pointer),EffectiveAddr_X; \
                        movb    (EffectiveAddr_X),%al; \
                        subl    SN(pointer),EffectiveAddr_X; \
                        ret;

#define GLUE_BANK_WRITE(func,pointer) \
E(func)                 addl    SN(pointer),EffectiveAddr_X; \
                        movb    %al,(EffectiveAddr_X); \
                        subl    SN(pointer),EffectiveAddr_X; \
                        ret;

#define GLUE_BANK_MAYBEWRITE(func,pointer) \
E(func)                 addl    SN(pointer),EffectiveAddr_X; \
                        cmpl    $0,SN(pointer); \
                        jz      1f; \
                        movb    %al,(EffectiveAddr_X); \
1:                      ret;


// TODO FIXME : implement CDECL prologue/epilogues...
#define GLUE_C_WRITE(func) \
E(func)                 pushl   _XAX; \
                        pushl   XY_Reg_X; \
                        pushl   AF_Reg_X; \
                        pushl   SP_Reg_X; \
                        pushl   PC_Reg_X; \
                        andl    $0xff,_XAX; \
                        pushl   _XAX; \
                        pushl   EffectiveAddr_X; \
                        call    SN(c_##func); \
                        popl    EffectiveAddr_X; /* dummy */ \
                        popl    _XAX; /* dummy */ \
                        popl    PC_Reg_X; \
                        popl    SP_Reg_X; \
                        popl    AF_Reg_X; \
                        popl    XY_Reg_X; \
                        popl    _XAX; \
                        ret;

// TODO FIXME : implement CDECL prologue/epilogues...
#define _GLUE_C_READ(func, ...) \
E(func)                 pushl   XY_Reg_X; \
                        pushl   AF_Reg_X; \
                        pushl   SP_Reg_X; \
                        pushl   PC_Reg_X; \
                        pushl   _XAX; /* HACK: works around mysterious issue with generated mov(_XAX), _XAX ... */ \
                        pushl   EffectiveAddr_X; \
                        call    SN(c_##func); \
                        popl    EffectiveAddr_X; /* dummy */ \
                        movb    %al, %dl; \
                        popl    _XAX; /* ... ugh */ \
                        movb    %dl, %al; \
                        popl    PC_Reg_X; \
                        popl    SP_Reg_X; \
                        popl    AF_Reg_X; \
                        popl    XY_Reg_X; \
                        __VA_ARGS__ \
                        ret;

// TODO FIXME : implement CDECL prologue/epilogues...
#define GLUE_C_READ(FUNC) _GLUE_C_READ(FUNC)

#define GLUE_C_READ_ALTZP(FUNC) _GLUE_C_READ(FUNC, \
        pushl   _XAX; \
        andl    $0xFFFF, SP_Reg_X; \
        movl    SN(base_stackzp), _XAX; \
        subl    $SN(apple_ii_64k), _XAX; \
        orl     _XAX, SP_Reg_X; \
        popl    _XAX; \
        )

