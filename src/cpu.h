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

#ifndef __CPU_H_
#define __CPU_H_

// Virtual machine is an Apple //e (not an NES, etc...)
#define APPLE2_VM 1

#if !defined(__ASSEMBLER__)
#include <sys/types.h>
#include <stdint.h>

/* types */

#define MEM_READ_FLAG  (1<<0)
#define MEM_WRITE_FLAG (1<<1)

extern uint16_t cpu65_pc;       // Program counter
extern uint8_t  cpu65_a;        // Accumulator
extern uint8_t  cpu65_f;        // Flags (host-order)
extern uint8_t  cpu65_x;        // X Index register
extern uint8_t  cpu65_y;        // Y Index register
extern uint8_t  cpu65_sp;       // Stack Pointer

extern uint16_t cpu65_ea;       // Last effective address
extern uint8_t  cpu65_d;        // Last data byte written
extern uint8_t  cpu65_rw;       // MEM_READ_FLAG = read occured, MEM_WRITE_FLAG = write
extern uint8_t  cpu65_opcode;   // Last opcode
extern uint8_t  cpu65_opcycles; // Last opcode extra cycles

/* Set up the processor for a new run. Sets up opcode table. */
extern void cpu65_init();

/* Interrupt the processor */
extern void cpu65_interrupt(int reason);
extern void cpu65_uninterrupt(int reason);

extern void cpu65_run(void);
extern void cpu65_reboot(void);

extern bool cpu65_saveState(StateHelper_s *helper);
extern bool cpu65_loadState(StateHelper_s *helper);

extern void cpu65_direct_write(int ea,int data);

extern void *cpu65_vmem_r[65536];
extern void *cpu65_vmem_w[65536];

extern unsigned char cpu65_flags_encode[256];
extern unsigned char cpu65_flags_decode[256];

extern int32_t cpu65_cycle_count;

#if CPU_TRACING
void cpu65_trace_begin(const char *trace_file);
void cpu65_trace_end(void);
void cpu65_trace_toggle(const char *trace_file);
void cpu65_trace_checkpoint(void);
#endif

#endif /* !__ASSEMBLER__ */

#define ResetSig        0x02
#define IRQ6522         0x08
#define IRQSpeech       0x10
#define IRQSSC          0x20
#define IRQMouse        0x40
#define IRQGeneric      0x80

/* Note: These are *not* the bit positions used for the flags in the P
 * register of a real 65c02. Rather, they have been distorted so that C,
 * N, Z, etc match the analogous flags in the host flags register.
 */
#if defined(__i386__) || defined(__x86_64__)
/*
 * x86 NOTE: V matches the position of the overflow flag in the high byte
 * of the 80386 register.
 */
#   define C_Flag_Bit      8               /* 6502 Carry              */
#   define C_Flag          (C_Flag_Bit>>3) /* 6502 Carry              */
#   define X_Flag          0x2             /* 6502 Xtra               */
#   define I_Flag          0x4             /* 6502 Interrupt disable  */
#   define V_Flag          0x8             /* 6502 oVerflow           */
#   define B_Flag          0x10            /* 6502 Break              */
#   define D_Flag          0x20            /* 6502 Decimal mode       */
#   define Z_Flag          0x40            /* 6502 Zero               */
#   define N_Flag          0x80            /* 6502 Negative           */
#elif defined(__arm__)
// VCZN positions match positions of shifted status register
#   define V_Flag          0x1
#   define C_Flag          0x2
#   define Z_Flag          0x4
#   define N_Flag          0x8
#   define NZ_Flags        0xC
#   define NZC_Flags       0xE
#   define NVZ_Flags       0xD
#   define NVZC_Flags      0xF
#   define X_Flag          0x10
#   define I_Flag          0x20
#   define B_Flag          0x40
#   define D_Flag          0x80
#   define BX_Flags        0x50
#   define BI_Flags        0x60
#elif defined(__aarch64__)
#   error soon ...
#else
#   error unknown machine architecture
#endif

/*
 * 6502 flags bit mask
 */
#define C_Flag_6502     0x1         // [C]arry
#define Z_Flag_6502     0x2         // [Z]ero
#define I_Flag_6502     0x4         // [I]nterrupt
#define D_Flag_6502     0x8         // [D]ecimal
#define B_Flag_6502     0x10        // [B]reak
#define X_Flag_6502     0x20        // [X]tra (reserved)...
#define V_Flag_6502     0x40        // o[V]erflow
#define N_Flag_6502     0x80        // [N]egative

#endif // whole file
