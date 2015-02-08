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
#include "misc.h"
#include "cpu-regs.h"

#define GLUE_BANK_MAYBEREAD(func,pointer) \
ENTRY(func)             testLQ  $SS_CXROM, SYM(softswitches); \
                        jnz     1f; \
                        callLQ  *SYM(pointer); \
                        ret; \
1:                      addLQ   SYM(pointer),EffectiveAddr_X; \
                        movb    (EffectiveAddr_X),%al; \
                        subLQ   SYM(pointer),EffectiveAddr_X; \
                        ret;

#define GLUE_BANK_READ(func,pointer) \
ENTRY(func)             addLQ   SYM(pointer),EffectiveAddr_X; \
                        movb    (EffectiveAddr_X),%al; \
                        subLQ   SYM(pointer),EffectiveAddr_X; \
                        ret;

#define GLUE_BANK_WRITE(func,pointer) \
ENTRY(func)             addLQ   SYM(pointer),EffectiveAddr_X; \
                        movb    %al,(EffectiveAddr_X); \
                        subLQ   SYM(pointer),EffectiveAddr_X; \
                        ret;

#define GLUE_BANK_MAYBEWRITE(func,pointer) \
ENTRY(func)             addLQ   SYM(pointer),EffectiveAddr_X; \
                        cmpl    $0,SYM(pointer); \
                        jz      1f; \
                        movb    %al,(EffectiveAddr_X); \
1:                      subLQ   SYM(pointer),EffectiveAddr_X; \
                        ret;


#ifdef __LP64__
#   define _PUSH_ARGS   pushLQ  EffectiveAddr_X; /* preserve */ \
                        movLQ   _XAX, %rsi; /* %rdi = ea, %rsi = byte */
#   define _POP_ARGS    popLQ   EffectiveAddr_X; /* restore */
#else
#   define _PUSH_ARGS   pushLQ  _XAX; /* byte is arg2 */ \
                        pushLQ  EffectiveAddr_X; /* ea is arg1 (and preserved) */
#   define _POP_ARGS    popLQ   EffectiveAddr_X; /* restore ea */ \
                        popLQ   _XAX;
#endif

#define GLUE_C_WRITE(func) \
ENTRY(func)             pushLQ  _XAX; \
                        pushLQ  XY_Reg_X; \
                        pushLQ  AF_Reg_X; \
                        pushLQ  SP_Reg_X; \
                        pushLQ  PC_Reg_X; \
                        andLQ   $0xff,_XAX; \
                        _PUSH_ARGS \
                        callLQ  CALL(c_##func); \
                        _POP_ARGS \
                        popLQ   PC_Reg_X; \
                        popLQ   SP_Reg_X; \
                        popLQ   AF_Reg_X; \
                        popLQ   XY_Reg_X; \
                        popLQ   _XAX; \
                        ret;

// TODO FIXME : implement CDECL prologue/epilogues...
#define _GLUE_C_READ(func, ...) \
ENTRY(func)             pushLQ  XY_Reg_X; \
                        pushLQ  AF_Reg_X; \
                        pushLQ  SP_Reg_X; \
                        pushLQ  PC_Reg_X; \
                        pushLQ  _XAX; /* HACK: works around mysterious issue with generated mov(_XAX), _XAX ... */ \
                        pushLQ  EffectiveAddr_X; /* ea is arg0 (and preserved) */ \
                        callLQ  CALL(c_##func); \
                        popLQ   EffectiveAddr_X; /* restore ea */ \
                        movb    %al, %dl; \
                        popLQ   _XAX; /* ... ugh */ \
                        movb    %dl, %al; \
                        popLQ   PC_Reg_X; \
                        popLQ   SP_Reg_X; \
                        popLQ   AF_Reg_X; \
                        popLQ   XY_Reg_X; \
                        __VA_ARGS__ \
                        ret;

// TODO FIXME : implement CDECL prologue/epilogues...
#define GLUE_C_READ(FUNC) _GLUE_C_READ(FUNC)

#define GLUE_C_READ_ALTZP(FUNC) _GLUE_C_READ(FUNC, \
        pushLQ  _XAX; \
        andLQ   $0xFFFF, SP_Reg_X; \
        RestoreAltZP \
        popLQ   _XAX; \
        )

