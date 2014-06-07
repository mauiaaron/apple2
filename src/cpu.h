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


typedef struct memory_vector_t {
    void *r;
    void *w;
} memory_vector_t;

struct cpu65_state
{
    uint16_t pc;        /* Program counter */
    uint8_t a;          /* Accumulator */
    uint8_t f;          /* Flags (host-order) */
    uint8_t x;          /* X Index register */
    uint8_t y;          /* Y Index register */
    uint8_t sp;         /* Stack Pointer */
};

struct cpu65_extra
{
    uint16_t ea;        /* Last effective address */
    uint8_t d;          /* Last data byte written */
    uint8_t rw;         /* 1 = read occured, 2 = write, 3 = both */
    uint8_t opcode;     /* Last opcode */
    uint8_t opcycles;   /* Last opcode extra cycles */
};

/* Set up the processor for a new run. Sets up opcode table. */
extern void cpu65_init();

/* Interrupt the processor */
extern void cpu65_interrupt(int reason);
extern void cpu65_uninterrupt(int reason);

extern void cpu65_run(void);

extern void cpu65_direct_write(int ea,int data);

extern memory_vector_t cpu65_vmem[65536];
extern struct cpu65_state cpu65_current;
extern struct cpu65_extra cpu65_debug;

extern unsigned char cpu65_flags_encode[256];
extern unsigned char cpu65_flags_decode[256];

extern int16_t cpu65_cycle_count;
extern int16_t cpu65_cycles_to_execute;

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
 * These are the 6502 Flags bit positions
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
