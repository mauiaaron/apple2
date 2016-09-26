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

#if __PIC__ && __i386__
#   define _A2_PIC_GOT(reg) 0x4(reg) // Stack offset assumes a CALL has been made that pushed %eip
#endif
#define _PUSH_COUNT ((/*args:*/7+/*ret:*/1) * SZ_PTR)

#define GLUE_EXTERN_C_READ(func)

#define GLUE_BANK_MAYBE_READ_CX(func,pointer) \
ENTRY(func)             REG2MEM(testLQ, $SS_CXROM, softswitches); \
                        jnz     1f; \
                        CALL_IND0(pointer); \
                        ret; \
1:                      MEM2REG(addLQ, pointer, EffectiveAddr_X); \
                        movb    (EffectiveAddr_X),%al; \
                        MEM2REG(subLQ, pointer, EffectiveAddr_X); \
                        ret;

#define GLUE_BANK_MAYBE_READ_C3(func,pointer) \
ENTRY(func)             REG2MEM(testLQ, $SS_CXROM, softswitches); \
                        jnz     1f; \
                        REG2MEM(testLQ, $SS_C3ROM, softswitches); \
                        jnz     1f; \
                        CALL_IND0(pointer); \
                        ret; \
1:                      MEM2REG(addLQ, pointer, EffectiveAddr_X); \
                        movb    (EffectiveAddr_X),%al; \
                        MEM2REG(subLQ, pointer, EffectiveAddr_X); \
                        ret;

#define GLUE_BANK_READ(func,pointer) \
ENTRY(func)             MEM2REG(addLQ, pointer, EffectiveAddr_X); \
                        movb    (EffectiveAddr_X),%al; \
                        MEM2REG(subLQ, pointer, EffectiveAddr_X); \
                        ret;

#define GLUE_BANK_WRITE(func,pointer) \
ENTRY(func)             MEM2REG(addLQ, pointer, EffectiveAddr_X); \
                        movb    %al,(EffectiveAddr_X); \
                        MEM2REG(subLQ, pointer, EffectiveAddr_X); \
                        ret;

#define GLUE_BANK_MAYBEWRITE(func,pointer) \
ENTRY(func)             MEM2REG(addLQ, pointer, EffectiveAddr_X); \
                        REG2MEM(cmpl, $0, pointer); \
                        jz      1f; \
                        movb    %al,(EffectiveAddr_X); \
1:                      MEM2REG(subLQ, pointer, EffectiveAddr_X); \
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
                        CALL_FN(callLQ, c_##func, _PUSH_COUNT); \
                        _POP_ARGS \
                        popLQ   PC_Reg_X; \
                        popLQ   SP_Reg_X; \
                        popLQ   AF_Reg_X; \
                        popLQ   XY_Reg_X; \
                        popLQ   _XAX; \
                        ret;

#if __PIC__ && __i386__
#   define _PUSH_GOT()  movl    _A2_PIC_GOT(%esp), _PICREG; \
                        pushl   _PICREG;
#   define _POP_GOT()   addl    $4, %esp
#else
#   define _PUSH_GOT()
#   define _POP_GOT()
#endif

// TODO FIXME : implement CDECL prologue/epilogues...
#define _GLUE_C_READ(func, ...) \
ENTRY(func)             pushLQ  XY_Reg_X; \
                        pushLQ  AF_Reg_X; \
                        pushLQ  SP_Reg_X; \
                        pushLQ  PC_Reg_X; \
                        pushLQ  _XAX; /* HACK: works around mysterious issue with generated mov(_XAX), _XAX ... */ \
                        pushLQ  EffectiveAddr_X; /* ea is arg0 (and preserved) */ \
                        CALL_FN(callLQ, c_##func, _PUSH_COUNT); \
                        popLQ   EffectiveAddr_X; /* restore ea */ \
                        movb    %al, %dl; \
                        popLQ   _XAX; /* ... ugh */ \
                        movb    %dl, %al; \
                        popLQ   PC_Reg_X; \
                        popLQ   SP_Reg_X; \
                        popLQ   AF_Reg_X; \
                        popLQ   XY_Reg_X; \
                        _PUSH_GOT(); \
                        __VA_ARGS__ \
                        _POP_GOT(); \
                        ret;

// TODO FIXME : implement CDECL prologue/epilogues...
#define GLUE_C_READ(FUNC) _GLUE_C_READ(FUNC)

#define GLUE_C_READ_ALTZP(FUNC) _GLUE_C_READ(FUNC, \
        pushLQ  _XAX; \
        andLQ   $0xFFFF, SP_Reg_X; \
        RestoreAltZP \
        popLQ   _XAX; \
        )

