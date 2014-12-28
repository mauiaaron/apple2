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

#if VM_TRACING

#define GLUE_C_WRITE(func) \
    extern void func(void); \
    void c__##func(uint16_t ea, uint8_t b); \
    void c_##func(uint16_t ea, uint8_t b) { \
        c__##func(ea, b); \
        extern FILE *test_vm_fp; \
        if (test_vm_fp && !vm_trace_is_ignored(ea)) { \
            fprintf(test_vm_fp, "%04X w:%02X %s\n", ea, b, __FUNCTION__); \
            fflush(test_vm_fp); \
        } \
    } \
    void c__##func(uint16_t ea, uint8_t b)

#define GLUE_C_READ(func) \
    extern void func(void); \
    uint8_t c__##func(uint16_t ea); \
    uint8_t c_##func(uint16_t ea) { \
        uint8_t b = c__##func(ea); \
        extern FILE *test_vm_fp; \
        if (test_vm_fp && !vm_trace_is_ignored(ea)) { \
            fprintf(test_vm_fp, "%04X r:%02X %s\n", ea, b, __FUNCTION__); \
            fflush(test_vm_fp); \
        } \
        return b; \
    } \
    uint8_t c__##func(uint16_t ea)

#else

#define GLUE_C_WRITE(func) \
    extern void func(void); \
    void c_##func(uint16_t ea, uint8_t b)

#define GLUE_C_READ(func) \
    extern void func(void); \
    uint8_t c_##func(uint16_t ea)

#endif

#define GLUE_C_READ_ALTZP(func, ...) GLUE_C_READ(func)

