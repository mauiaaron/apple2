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

#include <stdio.h>

/* debugger defines */
#define BUF_X           39
#define BUF_Y           22
#define MAX_BRKPTS      16
#define SCREEN_X        41
#define SCREEN_Y        24
#define PROMPT_X        2
#define PROMPT_Y        BUF_Y - 1
#define PROMPT_END_X    BUF_X - 2
#define command_line    command_buf[PROMPT_Y]
#define uchar           unsigned char

/* debugger commands */
enum token_type { MEM, DIS, REGS, SETMEM, STEP, FINISH, UNTIL, GO, VM,
                  BREAK, WATCH, CLEAR, IGNORE, STATUS, OPCODES, LC, DRIVE,
                  SEARCH, HELP, LOG, BSAVE, BLOAD, SAVE, UNKNOWN };

enum addressing_mode
{
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
};

struct opcode_struct
{
    const char *mnemonic;
    enum addressing_mode mode;
};

extern const struct opcode_struct *opcodes;

extern int step_next;                   /* stepping over instructions */
extern char second_buf[BUF_Y][BUF_X];   /* scratch buffer for output */
extern int num_buffer_lines;            /* num lines of output */
extern int arg1, arg2, arg3;            /* command arguments */
extern int breakpoints[MAX_BRKPTS];     /* memory breakpoints */
extern int watchpoints[MAX_BRKPTS];     /* memory watchpoints */

void clear_debugger_screen();
void bload(FILE*, char*, int);
void show_misc_info();
unsigned char get_current_opcode();
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
void c_do_step(int);
int at_haltpt();
void end_step();
void set_halt_opcode(unsigned char opcode);
void set_halt_65c02();
void clear_halt_65c02();
void clear_halt_opcode(unsigned char opcode);
void show_opcode_breakpts();

extern const struct opcode_struct opcodes_6502[256];
extern const struct opcode_struct opcodes_65c02[256];
extern const struct opcode_struct opcodes_undoc[256];
extern const char * const disasm_templates[15];

#endif
