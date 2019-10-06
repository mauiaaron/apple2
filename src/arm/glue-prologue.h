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

#if __aarch64__

#   define _GLUE_REG_SAVE \
                        stp     x29, x30, [sp, -16]!;

#   define _GLUE_REG_RESTORE \
                        ldp     x29, x30, [sp], 16; \
                        ret

#   define _GLUE_REG_SAVE0 \
                        stp     x0, x30, [sp, -16]!;

#   define _GLUE_REG_RESTORE0 \
                        ldp     x0, x30, [sp], 16; \
                        ret

#   define _GLUE_RET \
                        ret

#else

#   define _GLUE_REG_SAVE \
                        push    {EffectiveAddr, PC_Reg, lr};

#   define _GLUE_REG_RESTORE \
                        pop     {EffectiveAddr, PC_Reg, pc};

#   define _GLUE_REG_SAVE0 \
                        push    {r0, EffectiveAddr, PC_Reg, lr};

#   define _GLUE_REG_RESTORE0 \
                        pop     {r0, EffectiveAddr, PC_Reg, pc};

#   define _GLUE_RET \
                        mov     pc, lr;

#endif

#define GLUE_EXTERN_C_READ(func)

#define _GLUE_BANK_MAYBE_READ_CX(func,x,pointer) \
ENTRY(func)             ldr     wr0, [reg_args, #SOFTSWITCHES]; \
                        ldr     xr1, [reg_args, x ## pointer]; \
                        tst     wr0, #SS_CXROM; \
                        bnz     1f; \
                        _GLUE_REG_SAVE; \
                        BLX     xr1; \
                        _GLUE_REG_RESTORE; \
1:                      ldrb    wr0, [xr1, xEffectiveAddr]; \
                        _GLUE_RET

#define GLUE_BANK_MAYBE_READ_CX(func,pointer) _GLUE_BANK_MAYBE_READ_CX(func,#,pointer)


#define _GLUE_BANK_MAYBE_READ_C3(func,x,pointer) \
ENTRY(func)             ldr     wr0, [reg_args, #SOFTSWITCHES]; \
                        ldr     xr1, [reg_args, x ## pointer]; \
                        tst     wr0, #SS_CXROM; \
                        bnz     1f; \
                        tst     wr0, #SS_C3ROM; \
                        bnz     1f; \
                        _GLUE_REG_SAVE; \
                        BLX     xr1; \
                        _GLUE_REG_RESTORE; \
1:                      ldrb    wr0, [xr1, xEffectiveAddr]; \
                        _GLUE_RET
#define GLUE_BANK_MAYBE_READ_C3(func,pointer) _GLUE_BANK_MAYBE_READ_C3(func,#,pointer)


#define _GLUE_BANK_READ(func,x,pointer) \
ENTRY(func)             ldr     xr1, [reg_args, x ## pointer]; \
                        ldrb    wr0, [xr1, xEffectiveAddr]; \
                        _GLUE_RET
#define GLUE_BANK_READ(func,pointer) _GLUE_BANK_READ(func,#,pointer)


#define _GLUE_BANK_WRITE(func,x,pointer) \
ENTRY(func)             ldr     xr1, [reg_args, x ## pointer]; \
                        strb    wr0, [xr1, xEffectiveAddr]; \
                        _GLUE_RET
#define GLUE_BANK_WRITE(func,pointer) _GLUE_BANK_WRITE(func,#,pointer)


#define _GLUE_BANK_MAYBEWRITE(func,x,pointer) \
ENTRY(func)             ldr     xr1, [reg_args, x ## pointer]; \
                        eor     xr12, xr12, xr12; \
                        eor     xr1, xr1, xr12; \
                        cmp     xr1, #0; \
                        bz      1f; \
                        strb    wr0, [xr1, xEffectiveAddr]; \
1:                      _GLUE_RET
#define GLUE_BANK_MAYBEWRITE(func,pointer) _GLUE_BANK_MAYBEWRITE(func,#,pointer)


#define _GLUE_INLINE_READ(func,x,off) \
ENTRY(func)             ldrb    wr0, [reg_args, x ## off]; \
                        _GLUE_RET
#define GLUE_INLINE_READ(func,off) _GLUE_INLINE_READ(func,#,off)


#define GLUE_C_WRITE(func) \
ENTRY(func)             _GLUE_REG_SAVE0; \
                        and     wr0, wr0, #0xff; \
                        mov     wr1, wr0; \
                        mov     wr0, EffectiveAddr; \
                        bl      CALL(c_##func); \
                        _GLUE_REG_RESTORE0;


#define GLUE_C_READ(func) \
ENTRY(func)             _GLUE_REG_SAVE; \
                        mov     wr0, EffectiveAddr; \
                        bl      CALL(c_##func); \
                        _GLUE_REG_RESTORE;


#define GLUE_C_READ_ALTZP(FUNC) GLUE_C_READ(FUNC)

