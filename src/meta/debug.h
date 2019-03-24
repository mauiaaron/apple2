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


#ifndef _A2_DEBUG_H_
#define _A2_DEBUG_H_

#include "common.h"

extern volatile bool is_debugging;

typedef enum {
    addr_implied = 0,
    addr_accumulator,
    addr_immediate,
    addr_zeropage,
    addr_zeropage_x,
    addr_zeropage_y,
    addr_absolute,
    addr_absolute_x,
    addr_absolute_y,
    addr_indirect,
    addr_indirect_x,
    addr_indirect_y,
    addr_j_indirect,    /* non-zeropage indirects, used in JMP only */
    addr_j_indirect_x,
    addr_relative,
    NUM_ADDRESSING_MODES,
} addressing_mode_t;

typedef struct opcode_struct_s {
    const char *mnemonic;
    addressing_mode_t mode;
} opcode_struct_s;

extern const struct opcode_struct_s *opcodes;
extern const char* const disasm_templates[NUM_ADDRESSING_MODES];

#ifdef INTERFACE_CLASSIC
void c_interface_debugging(void);
#endif

bool debugger_shouldBreak(void) CALL_ON_CPU_THREAD;

#if TESTING
void debugger_setInputText(const char *text, const bool deterministically);
void debugger_setBreakCallback(bool(*cb)(void));
void debugger_go(void);
void debugger_setTimeout(const unsigned int secs);
bool debugger_setWatchpoint(const uint16_t addr);
void debugger_clearWatchpoints(void);
#endif

#endif // _A2_DEBUG_H_
