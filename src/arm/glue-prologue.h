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

#include "misc.h"
#include "cpu-regs.h"

#define GLUE_BANK_MAYBEREAD(func,pointer) \
ENTRY(func)             ldr     r1, SYM(softswitches); \
                        ldr     r0, [r1]; \
                        tst     r0, $SS_CXROM; \
                        beq     1f; \
                        ldr     r1, SYM(pointer); \
                        ldr     r1, [r1]; \
                        ldrb    r0, [r1, EffectiveAddr]; \
                        mov     pc, lr; \
1:                      ldr     r1, SYM(pointer); \
                        push    {lr}; \
                        blx     r1; \
                        pop     {pc}; \

#define GLUE_BANK_READ(func,pointer) \
ENTRY(func)             ldr     r1, SYM(pointer); \
                        ldr     r1, [r1]; \
                        ldrb    r0, [r1, EffectiveAddr]; \
                        mov     pc, lr;

#define GLUE_BANK_WRITE(func,pointer) \
ENTRY(func)             ldr     r1, SYM(pointer); \
                        ldr     r1, [r1]; \
                        strb    r0, [r1, EffectiveAddr]; \
                        mov     pc, lr;

#define GLUE_BANK_MAYBEWRITE(func,pointer) \
ENTRY(func)             ldr     r1, SYM(pointer); \
                        ldr     r1, [r1]; \
                        tst     r1, #0; \
                        strneb  r0, [r1, EffectiveAddr]; \
                        mov     pc, lr;


#define GLUE_C_WRITE(func) \
ENTRY(func)             push    {r0, PC_Reg, SP_Reg, F_Reg, Y_Reg, X_Reg, A_Reg, lr}; \
                        and     r0, #0xff; \
                        mov     r1, r0; \
                        mov     r0, EffectiveAddr; \
                        bl      CALL(c_##func); \
                        pop     {r0, PC_Reg, SP_Reg, F_Reg, Y_Reg, X_Reg, A_Reg, pc};

#define GLUE_C_READ(func) \
ENTRY(func)             push    {PC_Reg, SP_Reg, F_Reg, Y_Reg, X_Reg, A_Reg, lr}; \
                        mov     r0, EffectiveAddr; \
                        bl      CALL(c_##func); \
                        pop     {PC_Reg, SP_Reg, F_Reg, Y_Reg, X_Reg, A_Reg, pc};

#define GLUE_C_READ_ALTZP(FUNC) GLUE_C_READ(FUNC)

