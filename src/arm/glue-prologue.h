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

#include "vm.h"
#include "cpu-regs.h"

#define GLUE_EXTERN_C_READ(func)

#define _GLUE_BANK_MAYBE_READ_CX(func,x,pointer) \
ENTRY(func)             ldr     r0, [reg_args, #SOFTSWITCHES]; \
                        ldr     r1, [reg_args, x ## pointer]; \
                        tst     r0, $SS_CXROM; \
                        bne     1f; \
                        push    {EffectiveAddr, PC_Reg, lr}; \
                        blx     r1; \
                        pop     {EffectiveAddr, PC_Reg, pc}; \
1:                      ldrb    r0, [r1, EffectiveAddr]; \
                        mov     pc, lr;

#define GLUE_BANK_MAYBE_READ_CX(func,pointer) _GLUE_BANK_MAYBE_READ_CX(func,#,pointer)


#define _GLUE_BANK_MAYBE_READ_C3(func,x,pointer) \
ENTRY(func)             ldr     r0, [reg_args, #SOFTSWITCHES]; \
                        ldr     r1, [reg_args, x ## pointer]; \
                        tst     r0, $SS_CXROM; \
                        bne     1f; \
                        tst     r0, $SS_C3ROM; \
                        bne     1f; \
                        push    {EffectiveAddr, PC_Reg, lr}; \
                        blx     r1; \
                        pop     {EffectiveAddr, PC_Reg, pc}; \
1:                      ldrb    r0, [r1, EffectiveAddr]; \
                        mov     pc, lr;
#define GLUE_BANK_MAYBE_READ_C3(func,pointer) _GLUE_BANK_MAYBE_READ_C3(func,#,pointer)


#define _GLUE_BANK_READ(func,x,pointer) \
ENTRY(func)             ldr     r1, [reg_args, x ## pointer]; \
                        ldrb    r0, [r1, EffectiveAddr]; \
                        mov     pc, lr;
#define GLUE_BANK_READ(func,pointer) _GLUE_BANK_READ(func,#,pointer)


#define _GLUE_BANK_WRITE(func,x,pointer) \
ENTRY(func)             ldr     r1, [reg_args, x ## pointer]; \
                        strb    r0, [r1, EffectiveAddr]; \
                        mov     pc, lr;
#define GLUE_BANK_WRITE(func,pointer) _GLUE_BANK_WRITE(func,#,pointer)


#define _GLUE_BANK_MAYBEWRITE(func,x,pointer) \
ENTRY(func)             ldr     r1, [reg_args, x ## pointer]; \
                        teq     r1, #0; \
                        STRBNE  r0, [r1, EffectiveAddr]; \
                        mov     pc, lr;
#define GLUE_BANK_MAYBEWRITE(func,pointer) _GLUE_BANK_MAYBEWRITE(func,#,pointer)


#define _GLUE_INLINE_READ(func,x,off) \
ENTRY(func)             ldrb    r0, [reg_args, x ## off]; \
                        mov     pc, lr;
#define GLUE_INLINE_READ(func,off) _GLUE_INLINE_READ(func,#,off)


#define GLUE_C_WRITE(func) \
ENTRY(func)             push    {r0, EffectiveAddr, PC_Reg, lr}; \
                        and     r0, r0, #0xff; \
                        mov     r1, r0; \
                        mov     r0, EffectiveAddr; \
                        bl      CALL(c_##func); \
                        pop     {r0, EffectiveAddr, PC_Reg, pc};


#define GLUE_C_READ(func) \
ENTRY(func)             push    {EffectiveAddr, PC_Reg, lr}; \
                        mov     r0, EffectiveAddr; \
                        bl      CALL(c_##func); \
                        pop     {EffectiveAddr, PC_Reg, pc};


#define GLUE_C_READ_ALTZP(FUNC) GLUE_C_READ(FUNC)

