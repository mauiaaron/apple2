/*
 * Apple // emulator for Linux: Definitions for debugger
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
    GOING
} stepping_type_t;

typedef struct stepping_struct_t {
    stepping_type_t step_type;
    uint16_t step_count;
    uint16_t step_frame;
    uint16_t step_pc;
    bool should_break;
} stepping_struct_t;

#define DEBUGGER_BUF_X  39
#define DEBUGGER_BUF_Y  22
#define MAX_BRKPTS      16

/* debugger commands */
enum token_type { MEM, DIS, REGS, SETMEM, STEP, FINISH, UNTIL, GO, VM,
                  BREAK, WATCH, CLEAR, IGNORE, STATUS, OPCODES, LC, DRIVE,
                  SEARCH, HELP, LOG, BSAVE, BLOAD, SAVE, UNKNOWN };

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

// Debugger commands
void clear_debugger_screen();
void bload(FILE*, char*, int);
void show_misc_info();
uint8_t get_current_opcode();
void dump_mem(int, int, int, int, int);
void search_mem(char*, int, int);
void set_mem(int, char*);
void set_lc_mem(int, int, char*);
void disasm(int, int, int, int);
void clear_halt(int*, int);
void set_halt(int*, int);
void show_breakpts();
void show_regs();
void display_help();
void show_lc_info();
void show_disk_info();
void set_halt_opcode(uint8_t opcode);
void set_halt_65c02();
void clear_halt_65c02();
void clear_halt_opcode(uint8_t opcode);
void show_opcode_breakpts();

bool c_debugger_should_break();
void c_debugger_begin_stepping(stepping_struct_t s);
void c_interface_debugging();

extern const struct opcode_struct opcodes_6502[256];
extern const struct opcode_struct opcodes_65c02[256];
extern const struct opcode_struct opcodes_undoc[256];
extern const char* const disasm_templates[15];

#endif
