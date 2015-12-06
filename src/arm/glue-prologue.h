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

#define GLUE_BANK_MAYBEREAD(func,pointer) \
ENTRY(func)             SYM(r1, softswitches); \
                        ldr     r0, [r1]; \
                        SYM(r1, pointer); \
                        tst     r0, $SS_CXROM; \
                        bne     1f; \
                        push    {r0, EffectiveAddr, PC_Reg, /*SP_Reg, F_Reg, Y_Reg, X_Reg, A_Reg,*/ lr}; \
                        ldr     r1, [r1]; \
                        blx     r1; \
                        pop     {r0, EffectiveAddr, PC_Reg, /*SP_Reg, F_Reg, Y_Reg, X_Reg, A_Reg,*/ pc}; \
1:                      ldr     r1, [r1]; \
                        ldrb    r0, [r1, EffectiveAddr]; \
                        mov     pc, lr;


#define GLUE_BANK_READ(func,pointer) \
ENTRY(func)             SYM(r1, pointer); \
                        ldr     r1, [r1]; \
                        ldrb    r0, [r1, EffectiveAddr]; \
                        mov     pc, lr;

#define GLUE_BANK_WRITE(func,pointer) \
ENTRY(func)             SYM(r1, pointer); \
                        ldr     r1, [r1]; \
                        strb    r0, [r1, EffectiveAddr]; \
                        mov     pc, lr;

#define GLUE_BANK_MAYBEWRITE(func,pointer) \
ENTRY(func)             SYM(r1, pointer); \
                        ldr     r1, [r1]; \
                        teq     r1, #0; \
                        STRBNE  r0, [r1, EffectiveAddr]; \
                        mov     pc, lr;


#define GLUE_C_WRITE(func) \
ENTRY(func)             push    {r0, EffectiveAddr, PC_Reg, /*SP_Reg, F_Reg, Y_Reg, X_Reg, A_Reg,*/ lr}; \
                        and     r0, r0, #0xff; \
                        mov     r1, r0; \
                        mov     r0, EffectiveAddr; \
                        bl      CALL(c_##func); \
                        pop     {r0, EffectiveAddr, PC_Reg, /*SP_Reg, F_Reg, Y_Reg, X_Reg, A_Reg,*/ pc};

#define GLUE_C_READ(func) \
ENTRY(func)             push    {EffectiveAddr, PC_Reg, /*SP_Reg, F_Reg, Y_Reg, X_Reg, A_Reg,*/ lr}; \
                        mov     r0, EffectiveAddr; \
                        bl      CALL(c_##func); \
                        pop     {EffectiveAddr, PC_Reg, /*SP_Reg, F_Reg, Y_Reg, X_Reg, A_Reg,*/ pc};

#define GLUE_C_READ_ALTZP(FUNC) GLUE_C_READ(FUNC)

