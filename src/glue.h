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

#ifndef _GLUE_H_
#define _GLUE_H_

#if defined(__ASSEMBLER__)
#   error assembler-specific glue code should be in the arch-specific area
#endif

#define GLUE_BANK_READ(func,pointer) extern void func(void)
#define GLUE_BANK_MAYBE_READ_C3(func,pointer) extern void func(void)
#define GLUE_BANK_MAYBE_READ_CX(func,pointer) extern void func(void)
#define GLUE_BANK_WRITE(func,pointer) extern void func(void)
#define GLUE_BANK_MAYBEWRITE(func,pointer) extern void func(void)

#define GLUE_INLINE_READ(func,arg) extern uint8_t func(uint16_t)

#define GLUE_EXTERN_C_READ(func) extern uint8_t func(uint16_t)

#define GLUE_NOP(func) extern void func(void);

#if VM_TRACING

#define GLUE_C_WRITE(func) \
    extern void func(uint16_t, uint8_t); \
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
    extern uint8_t func(uint16_t); \
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
    extern void func(uint16_t, uint8_t); \
    void c_##func(uint16_t ea, uint8_t b)

#define GLUE_C_READ(func) \
    extern uint8_t func(uint16_t); \
    uint8_t c_##func(uint16_t ea)

#endif

#define GLUE_C_READ_ALTZP(func, ...) GLUE_C_READ(func)

// Stack struct assembly bridge (avoiding all PIC nastiness)
typedef struct cpu65_run_args_s {

    void (*unused0)(uint16_t, uint8_t);
#define OUTPUT_UNUSED0() printf("#define UNUSED0 %ld\n", offsetof(cpu65_run_args_s, unused0))

    void (*cpu65_trace_prologue)(uint16_t, uint8_t);
#define OUTPUT_CPU65_TRACE_PROLOGUE() printf("#define CPU65_TRACE_PROLOGUE %ld\n", offsetof(cpu65_run_args_s, cpu65_trace_prologue))
    void (*cpu65_trace_arg)(uint16_t, uint8_t);
#define OUTPUT_CPU65_TRACE_ARG() printf("#define CPU65_TRACE_ARG %ld\n", offsetof(cpu65_run_args_s, cpu65_trace_arg))
    void (*unused1)(uint16_t, uint8_t);
#define OUTPUT_UNUSED1() printf("#define UNUSED1 %ld\n", offsetof(cpu65_run_args_s, unused1))
    void (*unused2)(uint16_t, uint8_t);
#define OUTPUT_UNUSED2() printf("#define UNUSED2 %ld\n", offsetof(cpu65_run_args_s, unused2))

    void (*cpu65_trace_epilogue)(uint16_t, uint8_t);
#define OUTPUT_CPU65_TRACE_EPILOGUE() printf("#define CPU65_TRACE_EPILOGUE %ld\n", offsetof(cpu65_run_args_s, cpu65_trace_epilogue))

    void (*cpu65_trace_irq)(uint16_t, uint8_t);
#define OUTPUT_CPU65_TRACE_IRQ() printf("#define CPU65_TRACE_IRQ %ld\n", offsetof(cpu65_run_args_s, cpu65_trace_irq))

    uint8_t (*debug_illegal_bcd)(uint16_t);
#define OUTPUT_DEBUG_ILLEGAL_BCD() printf("#define DEBUG_ILLEGAL_BCD %ld\n", offsetof(cpu65_run_args_s, debug_illegal_bcd))

    void *cpu65_vmem_r;
#define OUTPUT_CPU65_VMEM_R() printf("#define CPU65_VMEM_R %ld\n", offsetof(cpu65_run_args_s, cpu65_vmem_r))
    void *cpu65_vmem_w;
#define OUTPUT_CPU65_VMEM_W() printf("#define CPU65_VMEM_W %ld\n", offsetof(cpu65_run_args_s, cpu65_vmem_w))
    void *cpu65_flags_encode;
#define OUTPUT_CPU65_FLAGS_ENCODE() printf("#define CPU65_FLAGS_ENCODE %ld\n", offsetof(cpu65_run_args_s, cpu65_flags_encode))
    void *cpu65_flags_decode;
#define OUTPUT_CPU65_FLAGS_DECODE() printf("#define CPU65_FLAGS_DECODE %ld\n", offsetof(cpu65_run_args_s, cpu65_flags_decode))
    void *cpu65__opcodes;
#define OUTPUT_CPU65__OPCODES() printf("#define CPU65__OPCODES %ld\n", offsetof(cpu65_run_args_s, cpu65__opcodes))
    uint8_t *cpu65__opcycles;
#define OUTPUT_CPU65__OPCYCLES() printf("#define CPU65__OPCYCLES %ld\n", offsetof(cpu65_run_args_s, cpu65__opcycles))

    uint8_t *base_ramrd;
#define OUTPUT_BASE_RAMRD() printf("#define BASE_RAMRD %ld\n", offsetof(cpu65_run_args_s, base_ramrd))
    uint8_t *base_ramwrt;
#define OUTPUT_BASE_RAMWRT() printf("#define BASE_RAMWRT %ld\n", offsetof(cpu65_run_args_s, base_ramwrt))
    uint8_t *base_textrd;
#define OUTPUT_BASE_TEXTRD() printf("#define BASE_TEXTRD %ld\n", offsetof(cpu65_run_args_s, base_textrd))
    uint8_t *base_textwrt;
#define OUTPUT_BASE_TEXTWRT() printf("#define BASE_TEXTWRT %ld\n", offsetof(cpu65_run_args_s, base_textwrt))
    uint8_t *base_hgrrd;
#define OUTPUT_BASE_HGRRD() printf("#define BASE_HGRRD %ld\n", offsetof(cpu65_run_args_s, base_hgrrd))
    uint8_t *base_hgrwrt;
#define OUTPUT_BASE_HGRWRT() printf("#define BASE_HGRWRT %ld\n", offsetof(cpu65_run_args_s, base_hgrwrt))

    uint8_t *base_stackzp;
#define OUTPUT_BASE_STACKZP() printf("#define BASE_STACKZP %ld\n", offsetof(cpu65_run_args_s, base_stackzp))
    uint8_t *base_d000_rd;
#define OUTPUT_BASE_D000_RD() printf("#define BASE_D000_RD %ld\n", offsetof(cpu65_run_args_s, base_d000_rd))
    uint8_t *base_e000_rd;
#define OUTPUT_BASE_E000_RD() printf("#define BASE_E000_RD %ld\n", offsetof(cpu65_run_args_s, base_e000_rd))
    uint8_t *base_d000_wrt;
#define OUTPUT_BASE_D000_WRT() printf("#define BASE_D000_WRT %ld\n", offsetof(cpu65_run_args_s, base_d000_wrt))
    uint8_t *base_e000_wrt;
#define OUTPUT_BASE_E000_WRT() printf("#define BASE_E000_WRT %ld\n", offsetof(cpu65_run_args_s, base_e000_wrt))

    uint8_t *base_c3rom;
#define OUTPUT_BASE_C3ROM() printf("#define BASE_C3ROM %ld\n", offsetof(cpu65_run_args_s, base_c3rom))
    uint8_t *base_c4rom;
#define OUTPUT_BASE_C4ROM() printf("#define BASE_C4ROM %ld\n", offsetof(cpu65_run_args_s, base_c4rom))
    uint8_t *base_c5rom;
#define OUTPUT_BASE_C5ROM() printf("#define BASE_C5ROM %ld\n", offsetof(cpu65_run_args_s, base_c5rom))
    uint8_t *base_cxrom;
#define OUTPUT_BASE_CXROM() printf("#define BASE_CXROM %ld\n", offsetof(cpu65_run_args_s, base_cxrom))

    uint32_t softswitches;
#define OUTPUT_SOFTSWITCHES() printf("#define SOFTSWITCHES %ld\n", offsetof(cpu65_run_args_s, softswitches))

    int32_t gc_cycles_timer_0; // joystick timer values
#define OUTPUT_GC_CYCLES_TIMER_0() printf("#define GC_CYCLES_TIMER_0 %ld\n", offsetof(cpu65_run_args_s, gc_cycles_timer_0))
    int32_t gc_cycles_timer_1;
#define OUTPUT_GC_CYCLES_TIMER_1() printf("#define GC_CYCLES_TIMER_1 %ld\n", offsetof(cpu65_run_args_s, gc_cycles_timer_1))

    int32_t cpu65_cycles_to_execute; // cycles-to-execute by cpu65_run()
#define OUTPUT_CPU65_CYCLES_TO_EXECUTE() printf("#define CPU65_CYCLES_TO_EXECUTE %ld\n", offsetof(cpu65_run_args_s, cpu65_cycles_to_execute))
    int32_t cpu65_cycle_count; // cycles currently excuted by cpu65_run()
#define OUTPUT_CPU65_CYCLE_COUNT() printf("#define CPU65_CYCLE_COUNT %ld\n", offsetof(cpu65_run_args_s, cpu65_cycle_count))

    int32_t unused3;
#define OUTPUT_UNUSED3() printf("#define UNUSED3 %ld\n", offsetof(cpu65_run_args_s, unused3))

    uint16_t interrupt_vector;
#define OUTPUT_INTERRUPT_VECTOR() printf("#define INTERRUPT_VECTOR %ld\n", offsetof(cpu65_run_args_s, interrupt_vector))
    uint16_t reset_vector;
#define OUTPUT_RESET_VECTOR() printf("#define RESET_VECTOR %ld\n", offsetof(cpu65_run_args_s, reset_vector))

    uint16_t cpu65_pc;       // Program counter
#define OUTPUT_CPU65_PC() printf("#define CPU65_PC %ld\n", offsetof(cpu65_run_args_s, cpu65_pc))
    uint16_t cpu65_ea;       // Last effective address
#define OUTPUT_CPU65_EA() printf("#define CPU65_EA %ld\n", offsetof(cpu65_run_args_s, cpu65_ea))

    uint8_t  cpu65_a;        // Accumulator
#define OUTPUT_CPU65_A() printf("#define CPU65_A %ld\n", offsetof(cpu65_run_args_s, cpu65_a))
    uint8_t  cpu65_f;        // Flags (host-order)
#define OUTPUT_CPU65_F() printf("#define CPU65_F %ld\n", offsetof(cpu65_run_args_s, cpu65_f))
    uint8_t  cpu65_x;        // X Index register
#define OUTPUT_CPU65_X() printf("#define CPU65_X %ld\n", offsetof(cpu65_run_args_s, cpu65_x))
    uint8_t  cpu65_y;        // Y Index register
#define OUTPUT_CPU65_Y() printf("#define CPU65_Y %ld\n", offsetof(cpu65_run_args_s, cpu65_y))
    uint8_t  cpu65_sp;       // Stack Pointer
#define OUTPUT_CPU65_SP() printf("#define CPU65_SP %ld\n", offsetof(cpu65_run_args_s, cpu65_sp))

    uint8_t  cpu65_d;        // Last data byte written
#define OUTPUT_CPU65_D() printf("#define CPU65_D %ld\n", offsetof(cpu65_run_args_s, cpu65_d))
    uint8_t  cpu65_rw;       // MEM_READ_FLAG = read occured, MEM_WRITE_FLAG = write
#define OUTPUT_CPU65_RW() printf("#define CPU65_RW %ld\n", offsetof(cpu65_run_args_s, cpu65_rw))
    uint8_t  cpu65_opcode;   // Last opcode
#define OUTPUT_CPU65_OPCODE() printf("#define CPU65_OPCODE %ld\n", offsetof(cpu65_run_args_s, cpu65_opcode))
    uint8_t  cpu65_opcycles; // Last opcode cycles
#define OUTPUT_CPU65_OPCYCLES() printf("#define CPU65_OPCYCLES %ld\n", offsetof(cpu65_run_args_s, cpu65_opcycles))

    uint8_t cpu65__signal;
#define OUTPUT_CPU65__SIGNAL() printf("#define CPU65__SIGNAL %ld\n", offsetof(cpu65_run_args_s, cpu65__signal))

    uint8_t joy_button0;
#define OUTPUT_JOY_BUTTON0() printf("#define JOY_BUTTON0 %ld\n", offsetof(cpu65_run_args_s, joy_button0))
    uint8_t joy_button1;
#define OUTPUT_JOY_BUTTON1() printf("#define JOY_BUTTON1 %ld\n", offsetof(cpu65_run_args_s, joy_button1))

    uint8_t emul_reinitialize;
#define OUTPUT_EMUL_REINITIALIZE() printf("#define EMUL_REINITIALIZE %ld\n", offsetof(cpu65_run_args_s, emul_reinitialize))

} cpu65_run_args_s;

#endif // whole file

