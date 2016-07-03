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


#ifndef A2_DEBUG_H
#define A2_DEBUG_H

#include "common.h"

#ifdef DEBUGGER
extern volatile bool is_debugging;
#else
#define is_debugging false
#endif

typedef enum {
    STEPPING = 0,
    NEXTING,
    FINISHING,
    UNTILING,
    TYPING,
    LOADING,
    GOING
} stepping_type_t;

typedef struct stepping_struct_t {
    stepping_type_t step_type;
    uint16_t step_count;
    uint16_t step_frame;
    uint16_t step_pc;
    bool should_break;
    time_t timeout;
    const char *step_text;
} stepping_struct_t;

#define DEBUGGER_BUF_X  39
#define DEBUGGER_BUF_Y  22
#define MAX_BRKPTS      16

/* debugger commands */
enum token_type { MEM, DIS, REGS, SETMEM, STEP, FINISH, UNTIL, GO, VM,
                  BREAK, WATCH, CLEAR, IGNORE, STATUS, OPCODES, LC, DRIVE,
                  SEARCH, HELP, LOG, BSAVE, BLOAD, SAVE, FBSHA1, TYPE,
                  LOAD, UNKNOWN };

typedef enum {
    addr_implied,
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
    addr_relative
} addressing_mode_t;

struct opcode_struct
{
    const char *mnemonic;
    addressing_mode_t mode;
};

extern const struct opcode_struct *opcodes;

#ifdef INTERFACE_CLASSIC
void c_interface_debugging();
#endif

void debugger_setInputText(const char *text);
void c_debugger_go(void);
bool c_debugger_should_break(void);
void c_debugger_set_timeout(const unsigned int secs);
bool c_debugger_set_watchpoint(const uint16_t addr);
void c_debugger_clear_watchpoints(void);

extern const struct opcode_struct opcodes_6502[256];
extern const struct opcode_struct opcodes_65c02[256];
extern const struct opcode_struct opcodes_undoc[256];
extern const char* const disasm_templates[15];
extern const uint8_t opcodes_65c02_numargs[256];

#endif
