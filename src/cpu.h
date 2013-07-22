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

#ifndef __ASSEMBLER__
#include <sys/types.h>
#include <stdint.h>

/* types */

typedef void *WMEM;
typedef void *RMEM;

struct memory_vector
{
    RMEM r;
    WMEM w;
};

struct cpu65_state
{
    uint16_t pc;        /* Program counter */
    uint8_t a;          /* Accumulator */
    uint8_t f;          /* Flags (order not same as in real 6502) */
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

/* 6502 CPU models */
#define         CPU65_NMOS      0x0
#define         CPU65_C02       0x1

#define         CPU65_FAULT     0x100   /* Undoc. opcodes are BRK */

/* Set up the processor for a new run. Sets up opcode table.
 */
extern void cpu65_set(int flags);

/* Interrupt the processor */
extern void cpu65_interrupt(int reason);

extern void cpu65_run(void);

extern void cpu65_step(void);

extern void cpu65_direct_write(int ea,int data);

extern struct memory_vector cpu65_vmem[65536];
extern struct cpu65_state cpu65_current;
extern struct cpu65_extra cpu65_debug;

extern unsigned char cpu65_flags_encode[256];
extern unsigned char cpu65_flags_decode[256];

extern unsigned int cpu65_delay;

#endif /* !__ASSEMBLER__ */

#define RebootSig       0x01
#define ResetSig        0x02
#define DebugStepSig    0x04
#define EnterDebugSig   0x08

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

#define X_Reg           %bl             /* 6502 X register in %bl  */
#define Y_Reg           %bh             /* 6502 Y register in %bh  */
#define A_Reg           %cl             /* 6502 A register in %cl  */
#define F_Reg           %ch             /* 6502 flags in %ch       */
#define FF_Reg          %ecx            /* 6502 flags for bt       */
#define SP_Reg          %edx            /* 6502 Stack pointer      */
#define SP_Reg_L        %dl             /* 6502 Stack pointer low  */
#define SP_Reg_H        %dh             /* 6502 Stack pointer high */
#define PC_Reg          %si             /* 6502 Program Counter    */
#define PC_Reg_E        %esi            /* 6502 Program Counter    */
#define EffectiveAddr   %di             /* Effective address       */
#define EffectiveAddr_E %edi            /* Effective address       */

#ifndef __ASSEMBLER__
/* Private data. */
extern void *cpu65__opcodes[256];
extern void *const cpu65__nmos[256];
extern void *const cpu65__nmosbrk[256];
extern void *const cpu65__cmos[256];

extern char cpu65__opcycles[256];// cycle counter

extern unsigned char cpu65__signal;
#endif /* !__ASSEMBLER__ */

