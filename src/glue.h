/* 
 * Apple // emulator for Linux: Glue macros
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

#define GLUE_FIXED_READ(func,address)
#define GLUE_FIXED_WRITE(func,address)
#define GLUE_BANK_READ(func,pointer)
#define GLUE_BANK_MAYBEREAD(func,pointer)
#define GLUE_BANK_WRITE(func,pointer)
#define GLUE_BANK_MAYBEWRITE(func,pointer)

#define GLUE_C_WRITE(func) \
    extern void func(); \
    void c_##func(uint16_t ea, uint8_t b) /* you complete definition */

#define GLUE_C_READ(func) \
    extern void func(); \
    uint8_t c_##func(uint16_t ea) /* you complete definition */

#define GLUE_C_READ_ALTZP(func, ...) GLUE_C_READ(func)

