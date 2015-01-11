/*
 * Apple // emulator for Linux: Virtual 6502/65C02 interface
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

#ifndef __CPU_H_
#define __CPU_H_

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

extern void cpu65_direct_write(int ea,int data);

extern void *cpu65_vmem_r[65536];
extern void *cpu65_vmem_w[65536];

extern unsigned char cpu65_flags_encode[256];
extern unsigned char cpu65_flags_decode[256];

extern int16_t cpu65_cycle_count;
extern int16_t cpu65_cycles_to_execute;

#if CPU_TRACING
void cpu65_trace_begin(const char *trace_file);
void cpu65_trace_end(void);
void cpu65_trace_toggle(const char *trace_file);
#endif

#endif /* !__ASSEMBLER__ */

#define ResetSig        0x02
#define IRQ6522         0x08
#define IRQSpeech       0x10
#define IRQSSC          0x20
#define IRQMouse        0x40
#define IRQGeneric      0x80

/* Note: These are *not* the bit positions used for the flags in the P
 * register of a real 6502. Rather, they have been distorted so that C,
 * N and Z match the analogous flags in the _80386_ flags register.
 *
 * Additionally, V matches the position of the overflow flag in the high byte
 * of the 80386 register.
 *
 */
#define C_Flag          0x1             /* 6502 Carry              */
#define X_Flag          0x2             /* 6502 Xtra               */
#define I_Flag          0x4             /* 6502 Interrupt disable  */
#define V_Flag          0x8             /* 6502 Overflow           */
#define B_Flag          0x10            /* 6502 Break              */
#define D_Flag          0x20            /* 6502 Decimal mode       */
#define Z_Flag          0x40            /* 6502 Zero               */
#define N_Flag          0x80            /* 6502 Neg                */

#define C_Flag_Bit      8               /* 6502 Carry              */
#define X_Flag_Bit      9               /* 6502 Xtra               */
#define I_Flag_Bit      10              /* 6502 Interrupt disable  */
#define V_Flag_Bit      11              /* 6502 Overflow           */
#define B_Flag_Bit      12              /* 6502 Break              */
#define D_Flag_Bit      13              /* 6502 Decimal mode       */
#define Z_Flag_Bit      14              /* 6502 Zero               */
#define N_Flag_Bit      15              /* 6502 Neg                */

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
