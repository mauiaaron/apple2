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
                        bne     1f; \
                        ldr     r1, SYM(pointer); \
                        blx     r1; \
                        ret; \
1:                      ldr     r1, SYM(pointer); \
                        ldrb    r0, [r1, EffectiveAddr]; \
                        ret;

#define GLUE_BANK_READ(func,pointer) \
ENTRY(func)             ldr     r1, SYM(pointer); \
                        ldrb    r0, [r1, EffectiveAddr]; \
                        ret;

#define GLUE_BANK_WRITE(func,pointer) \
ENTRY(func)             ldr     r1, SYM(pointer); \
                        strb    r0, [r1, EffectiveAddr]; \
                        ret;

#define GLUE_BANK_MAYBEWRITE(func,pointer) \
ENTRY(func)             ldr     r1, SYM(pointer); \
                        cmp     r1, #0; \
                        beq     1f; \
                        strb    r0, [r1, EffectiveAddr]; \
1:                      ret;


#define GLUE_C_WRITE(func) \
ENTRY(func)             push    {r0, A_Reg, X_Reg, Y_Reg, F_Reg, SP_Reg, PC_Reg}; \
                        and     r0, #0xff; \
                        mov     r1, r0; \
                        mov     r0, EffectiveAddr; \
                        bl      CALL(c_##func); \
                        pop     {PC_Reg, SP_Reg, F_Reg, Y_Reg, X_Reg, A_Reg, r0}; \
                        ret;

#define _GLUE_C_READ(func, ...) \
ENTRY(func)             push    {A_Reg, X_Reg, Y_Reg, F_Reg, SP_Reg, PC_Reg}; \
                        mov     r0, EffectiveAddr; \
                        bl      CALL(c_##func); \
                        pop     {PC_Reg, SP_Reg, F_Reg, Y_Reg, X_Reg, A_Reg}; \
                        __VA_ARGS__ \
                        ret;

#define GLUE_C_READ(FUNC) _GLUE_C_READ(FUNC)

#define GLUE_C_READ_ALTZP(FUNC) _GLUE_C_READ(FUNC, \
        push    {r0}; \
        RestoreAltZP \
        pop     {r0}; \
        )

