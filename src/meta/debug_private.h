/*
 * Apple // emulator for *ix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 2019 Aaron Culliney
 *
 */

#ifndef _DEBUG_PRIVATE_H_
#define _DEBUG_PRIVATE_H_ 1

#define DEBUGGER_BUF_X  39
#define DEBUGGER_BUF_Y  22
#define MAX_BRKPTS      16

typedef enum stepping_type_t {
    STEPPING = 0,
    NEXTING,
    FINISHING,
    UNTILING,
    TYPING,
    LOADING,
    GOING
} stepping_type_t;

typedef struct stepping_struct_s {
    stepping_type_t step_type;
    uint16_t step_count;
    uint16_t step_frame;
    uint16_t step_pc;
    bool should_break;
    time_t timeout;
    const char *step_text;
    const bool step_deterministically;
} stepping_struct_s;

// debugger commands
enum {
    BLOAD,
    BREAK,
    BSAVE,
    CLEAR,
    DIS,
    DRIVE,
    FBSHA1,
    FINISH,
    GO,
    HELP,
    IGNORE,
    LC,
    LOAD,
    LOG,
    MEM,
    OPCODES,
    REGS,
    SAVE,
    SEARCH,
    SETMEM,
    STATUS,
    STEP,
    TYPE,
    UNTIL,
    VM,
    WATCH,
    // ...
    UNKNOWN,
};

#endif
