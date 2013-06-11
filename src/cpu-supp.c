/* 
 * Apple // emulator for Linux: C support for 6502 on i386
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

#include <string.h>

#include "cpu.h"

/* different than in defs.h! */
#define C_Flag_6502	0x1
#define X_Flag_6502	0x20
#define I_Flag_6502	0x4
#define V_Flag_6502	0x40
#define B_Flag_6502	0x10
#define D_Flag_6502	0x8
#define Z_Flag_6502	0x2
#define N_Flag_6502	0x80

static void initialize_code_tables(void)
{
    int		i;

    for (i = 0; i < 256; i++)
    {
	unsigned char	val = 0;

	if (i & C_Flag)
	    val |= C_Flag_6502;
	if (i & X_Flag)
	    val |= X_Flag_6502;
	if (i & I_Flag)
	    val |= I_Flag_6502;
	if (i & V_Flag)
	    val |= V_Flag_6502;
	if (i & B_Flag)
	    val |= B_Flag_6502;
	if (i & D_Flag)
	    val |= D_Flag_6502;
	if (i & Z_Flag)
	    val |= Z_Flag_6502;
	if (i & N_Flag)
	    val |= N_Flag_6502;

	cpu65_flags_encode[ i ] = val | 0x20;
	cpu65_flags_decode[ val ] = i;
    }
}


void cpu65_set(int flags)
{
     initialize_code_tables();

     switch (flags & 0xf)
     {
     case CPU65_NMOS:
	if (flags & CPU65_FAULT)
            memcpy(cpu65__opcodes,cpu65__nmosbrk,1024);	
        else
	    memcpy(cpu65__opcodes,cpu65__nmos,1024);
	break;
     case CPU65_C02:
        memcpy(cpu65__opcodes,cpu65__cmos,1024);
        break;
     default:
        abort();
     }
	
     cpu65__signal = 0;
}

void cpu65_interrupt(int reason)
{
    cpu65__signal = reason;
}

