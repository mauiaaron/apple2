/* 
 * Apple // emulator for *ix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software 
 * Foundation.
 *
 * Copyright 1994 Alexander Jean-Claude Bottema
 * Copyright 1995 Stephen Lee
 * Copyright 1997, 1998 Aaron Culliney
 * Copyright 1998, 1999, 2000 Michael Deutschmann
 * Copyright 2013-2015 Aaron Culliney
 *
 */

#include "vm.h"
#include "cpu-regs.h"

/*
 * These "glue" macros code become auto-generated trampoline functions for the exclusive use of cpu65_run() CPU module
 * to make calls back into C that conform with the x86 and x86_64 calling ABIs.
 */

#define GLUE_EXTERN_C_READ(func)

#define GLUE_BANK_MAYBE_READ_CX(func,pointer) \
ENTRY(func)             testLQ  $SS_CXROM, SOFTSWITCHES(reg_args); \
                        jnz     1f; \
                        callLQ  *pointer(reg_args); \
                        ret; \
1:                      addLQ   pointer(reg_args), EffectiveAddr_X; \
                        movb    (EffectiveAddr_X),%al; \
                        subLQ   pointer(reg_args), EffectiveAddr_X; \
                        ret;

#define GLUE_BANK_MAYBE_READ_C3(func,pointer) \
ENTRY(func)             testLQ  $SS_CXROM, SOFTSWITCHES(reg_args); \
                        jnz     1f; \
                        testLQ  $SS_C3ROM, SOFTSWITCHES(reg_args); \
                        jnz     1f; \
                        callLQ  *pointer(reg_args); \
                        ret; \
1:                      addLQ   pointer(reg_args), EffectiveAddr_X; \
                        movb    (EffectiveAddr_X),%al; \
                        subLQ   pointer(reg_args), EffectiveAddr_X; \
                        ret;

#define GLUE_BANK_READ(func,pointer) \
ENTRY(func)             addLQ   pointer(reg_args), EffectiveAddr_X; \
                        movb    (EffectiveAddr_X),%al; \
                        subLQ   pointer(reg_args), EffectiveAddr_X; \
                        ret;

#define GLUE_BANK_WRITE(func,pointer) \
ENTRY(func)             addLQ   pointer(reg_args), EffectiveAddr_X; \
                        movb    %al,(EffectiveAddr_X); \
                        subLQ   pointer(reg_args), EffectiveAddr_X; \
                        ret;

#define GLUE_BANK_MAYBEWRITE(func,pointer) \
ENTRY(func)             addLQ   pointer(reg_args), EffectiveAddr_X; \
                        cmpl    $0, pointer(reg_args); \
                        jz      1f; \
                        movb    %al,(EffectiveAddr_X); \
1:                      subLQ   pointer(reg_args), EffectiveAddr_X; \
                        ret;

#define GLUE_INLINE_READ(func,off) \
ENTRY(func)             movb    off(reg_args), %al; \
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
                        pushLQ  reg_args; \
                        pushLQ  PC_Reg_X; \
                        andLQ   $0xff,_XAX; \
                        _PUSH_ARGS \
                        callLQ  CALL(c_##func); \
                        _POP_ARGS \
                        popLQ   PC_Reg_X; \
                        popLQ   reg_args; \
                        popLQ   AF_Reg_X; \
                        popLQ   XY_Reg_X; \
                        popLQ   _XAX; \
                        ret;

#define _GLUE_C_READ(func) \
ENTRY(func)             pushLQ  XY_Reg_X; \
                        pushLQ  AF_Reg_X; \
                        pushLQ  reg_args; \
                        pushLQ  PC_Reg_X; \
                        pushLQ  _XAX; /* HACK: works around mysterious issue with generated mov(_XAX), _XAX ... */ \
                        pushLQ  EffectiveAddr_X; /* ea is arg0 (and preserved) */ \
                        callLQ  CALL(c_##func); \
                        popLQ   EffectiveAddr_X; /* restore ea */ \
                        movb    %al, %dl; \
                        popLQ   _XAX; /* ... ugh */ \
                        movb    %dl, %al; \
                        popLQ   PC_Reg_X; \
                        popLQ   reg_args; \
                        popLQ   AF_Reg_X; \
                        popLQ   XY_Reg_X; \
                        ret;

#define GLUE_C_READ(FUNC) _GLUE_C_READ(FUNC)

#define GLUE_C_READ_ALTZP(FUNC) _GLUE_C_READ(FUNC)

