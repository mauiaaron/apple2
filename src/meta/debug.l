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

/*  ASSUMPTIONS:
 *	flex version 2.5.2 or later
 *
 */

%{

/* process includes only the second time we parse this file. */
#include "common.h"

#define debugtext yytext

#define DEBUG_BUFSZ 256

YY_BUFFER_STATE buffer = 0;

extern char second_buf[DEBUGGER_BUF_Y][DEBUGGER_BUF_X];/* scratch buffer for output */
extern int num_buffer_lines;            /* num lines of output */
extern int arg1, arg2, arg3;            /* command arguments */
extern int breakpoints[MAX_BRKPTS];     /* memory breakpoints */
extern int watchpoints[MAX_BRKPTS];     /* memory watchpoints */

// internal debugger commands
extern void clear_debugger_screen();
extern void bload(FILE*, char*, int, int);
extern void show_misc_info();
extern uint8_t get_current_opcode();
extern void dump_mem(int, int, int, int, int);
extern void search_mem(char*, int, int);
extern void set_mem(int, char*);
extern void set_lc_mem(int, int, char*);
extern void disasm(int, int, int, int);
extern void clear_halt(int*, int);
extern bool set_halt(int*, int);
extern void show_breakpts();
extern void show_regs();
extern void display_help();
extern void show_lc_info();
extern void show_disk_info();
extern void set_halt_opcode(uint8_t opcode);
extern void set_halt_65c02();
extern void clear_halt_65c02();
extern void clear_halt_opcode(uint8_t opcode);
extern void show_opcode_breakpts();
extern int debugger_go(stepping_struct_t s);
extern void fb_sha1();

/*
  d{is} {lc1|lc2} {/bank/}{addr} {+}{len}
  m{em} {lc1|lc2} {/bank/}{addr} {+}{len}
  a{scii} {lc1|lc2} {/bank/}{addr} {+}{len}
  r{egs}
  <addr> {lc1|lc2} : <byteseq>
  bload <file> /<bank>/<addrs>
  bsave <file> /<bank>/<addrs> <len>
  (s{tep} | n{ext}) {len}
  f{inish}
  u{ntil}
  g{o} {addr}
  sea{rch} {lc1|lc2} <bytes>
  (b{reak} | w{atch}) {addr}
  (b{reak} | w{atch}) {addr}
  b{reak} op <byte>
  (c{lear} | i{gnore}) {num}
  c{lear} op <byte>
  key <byte>
  sta{tus}
  l{ang}
  dr{ive}
  vm
  fr{esh}
  (? | h{elp})
*/

%}


DEC	[0-9]
HEX	[0-9a-fA-F]
WS	[\n\r\t" "]
CHAR	[^\n\r\t" "]
BOS	^
EOS	{WS}*\0
BANK	\/0?[01]\/
ADDRS	[0-9a-fA-F]+


%%



{BOS}(me?m?|as?c?i?i?){WS}+{ADDRS}{WS}\+?+{HEX}+{EOS} {
    /* mem <addrs> <len> */
    int do_ascii = 0;

    if (tolower(debugtext[0]) == 'a')
	do_ascii = 1;
    while (!isspace(*debugtext)) ++debugtext;

    arg1 = (int)strtol(debugtext, &debugtext, 16);
    arg2 = (int)strtol(debugtext, &debugtext, 16);
    dump_mem(arg1, arg2, 0, do_ascii, -1);
    return MEM;
}

{BOS}(me?m?|as?c?i?i?){WS}+{BANK}{ADDRS}{WS}+\+?{HEX}+{EOS} {
    /* mem /<bank>/<addrs> <len> */
    int do_ascii = 0;

    if (tolower(debugtext[0]) == 'a')
	do_ascii = 1;
    while (*debugtext != '/') ++debugtext;
    ++debugtext;	/* after / */
    arg3 = (int)strtol(debugtext, &debugtext, 10);
    ++debugtext;	/* after / */

    arg1 = (int)strtol(debugtext, &debugtext, 16);
    arg2 = (int)strtol(debugtext, &debugtext, 16);
    dump_mem(arg1, arg2, 0, do_ascii, arg3);
    return MEM;
}

{BOS}(me?m?|as?c?i?i?){WS}+{ADDRS}{EOS} {
    /* mem <addrs> */
    int do_ascii = 0;

    if (tolower(debugtext[0]) == 'a')
	do_ascii = 1;
    while (!isspace(*debugtext)) ++debugtext;

    arg1 = (int)strtol(debugtext, &debugtext, 16);
    dump_mem(arg1, 256, 0, do_ascii, -1);
    return MEM;
}

{BOS}(me?m?|as?c?i?i?){WS}+{BANK}{ADDRS}{EOS} {
    /* mem /<bank>/<addrs> */
    int do_ascii = 0;

    if (tolower(debugtext[0]) == 'a')
	do_ascii = 1;
    while (*debugtext != '/') ++debugtext;
    ++debugtext;	/* after / */
    arg3 = (int)strtol(debugtext, &debugtext, 10);
    ++debugtext;	/* after / */

    arg1 = (int)strtol(debugtext, &debugtext, 16);
    dump_mem(arg1, 256, 0, do_ascii, arg3);
    return MEM;
}

{BOS}(me?m?|as?c?i?i?){WS}+\+{HEX}+{EOS} {
    /* mem +<len> */
    int do_ascii = 0;

    if (tolower(debugtext[0]) == 'a')
	do_ascii = 1;
    while (*debugtext != '+') ++debugtext;
    ++debugtext;

    arg1 = (int)strtol(debugtext, &debugtext, 16);
    dump_mem(cpu65_pc, arg1, 0, do_ascii, -1);
    return MEM;
}

{BOS}(me?m?|as?c?i?i?){EOS} {
    /* dump mem from current location */
    int do_ascii = 0;

    if (tolower(debugtext[0]) == 'a')
	do_ascii = 1;
    dump_mem(cpu65_pc, 256, 0, do_ascii, -1);
    return MEM;
}

{BOS}(me?m?|as?c?i?i?){WS}*(lc?1|lc?2){WS}+{ADDRS}{WS}+\+?{HEX}+{EOS} {
    /* dump mem from lc<bank> <addrs> <len> */
    int do_ascii = 0;
    int lc;

    if (tolower(debugtext[0]) == 'a')
	do_ascii = 1;
    while (tolower(*debugtext) != 'l') ++debugtext; ++debugtext;
    if (tolower(*debugtext) == 'c') ++debugtext;

    lc = (int)strtol(debugtext, &debugtext, 10);
    arg1 = (int)strtol(debugtext, &debugtext, 16);
    arg2 = (int)strtol(debugtext, &debugtext, 16);

    dump_mem(arg1, arg2, lc, do_ascii, -1);
    return MEM;
}

{BOS}(me?m?|as?c?i?i?){WS}*(lc?1|lc?2){WS}+{BANK}{ADDRS}{WS}+\+?{HEX}+{EOS} {
    /* dump mem from lc<bank> /<bank>/<addrs> <len> */
    int do_ascii = 0;
    int lc;

    if (tolower(debugtext[0]) == 'a')
	do_ascii = 1;
    while (tolower(*debugtext) != 'l') ++debugtext; ++debugtext;
    if (tolower(*debugtext) == 'c') ++debugtext;
    lc = (int)strtol(debugtext, &debugtext, 10);

    while (*debugtext != '/') ++debugtext;
    ++debugtext;	/* after / */
    arg3 = (int)strtol(debugtext, &debugtext, 10);
    ++debugtext;	/* after / */
    arg1 = (int)strtol(debugtext, &debugtext, 16);
    arg2 = (int)strtol(debugtext, &debugtext, 16);

    dump_mem(arg1, arg2, lc, do_ascii, arg3);
    return MEM;
}

{BOS}(me?m?|as?c?i?i?){WS}*(lc?1|lc?2){WS}+{ADDRS}{EOS} {
    /* dump mem from lc<bank> <addrs> */
    int do_ascii = 0;
    int lc;

    if (tolower(debugtext[0]) == 'a')
	do_ascii = 1;
    while (tolower(*debugtext) != 'l') ++debugtext; ++debugtext;
    if (tolower(*debugtext) == 'c') ++debugtext;

    lc = (int)strtol(debugtext, &debugtext, 10);
    arg1 = (int)strtol(debugtext, &debugtext, 16);

    dump_mem(arg1, 256, lc, do_ascii, -1);
    return MEM;
}

{BOS}(me?m?|as?c?i?i?){WS}*(lc?1|lc?2){WS}+{BANK}{ADDRS}{EOS} {
    /* dump mem from lc<bank> /<bank>/<addrs> */
    int do_ascii = 0;
    int lc;

    if (tolower(debugtext[0]) == 'a')
	do_ascii = 1;
    while (tolower(*debugtext) != 'l') ++debugtext; ++debugtext;
    if (tolower(*debugtext) == 'c') ++debugtext;
    lc = (int)strtol(debugtext, &debugtext, 10);

    while (*debugtext != '/') ++debugtext;
    ++debugtext;	/* after / */
    arg3 = (int)strtol(debugtext, &debugtext, 10);
    ++debugtext;	/* after / */
    arg1 = (int)strtol(debugtext, &debugtext, 16);

    dump_mem(arg1, 256, lc, do_ascii, arg3);
    return MEM;
}

{BOS}di?s?{WS}+{ADDRS}{WS}+\+?{HEX}+{EOS} {
    /* disassemble at <addrs> <len> */
    while (!isspace(*debugtext)) ++debugtext;

    arg1 = (int)strtol(debugtext, &debugtext, 16);
    arg2 = (int)strtol(debugtext, &debugtext, 16);

    disasm(arg1, arg2, 0, -1);
    return DIS;
}

{BOS}di?s?{WS}+{BANK}{ADDRS}{WS}+\+?{HEX}+{EOS} {
    /* disassemble at /<bank>/<addrs> <len> */
    while (*debugtext != '/') ++debugtext;
    ++debugtext;	/* after / */
    arg3 = (int)strtol(debugtext, &debugtext, 10);
    ++debugtext;	/* after / */

    arg1 = (int)strtol(debugtext, &debugtext, 16);
    arg2 = (int)strtol(debugtext, &debugtext, 16);

    disasm(arg1, arg2, 0, arg3);
    return DIS;
}

{BOS}di?s?{WS}+{ADDRS}{EOS} {
    /* disassemble at <addrs> */
    while (!isspace(*debugtext)) ++debugtext;

    arg1 = (int)strtol(debugtext, &debugtext, 16);
    arg2 = 256;
    if ((arg1 < 0) || (arg1 > 65535)) arg1 = cpu65_pc;

    disasm(arg1, arg2, 0, -1);
    return DIS;
}

{BOS}di?s?{WS}+{BANK}{ADDRS}{EOS} {
    /* disassemble at /<bank>/<addrs> */
    while (*debugtext != '/') ++debugtext;
    ++debugtext;	/* after / */
    arg3 = (int)strtol(debugtext, &debugtext, 10);
    ++debugtext;	/* after / */

    arg1 = (int)strtol(debugtext, &debugtext, 16);
    arg2 = 256;
    if ((arg1 < 0) || (arg1 > 65535)) arg1 = cpu65_pc;

    disasm(arg1, arg2, 0, arg3);
    return DIS;
}

{BOS}di?s?{WS}+\+{HEX}+{EOS} {
    /* disassemble current location +<len> */
    while (*debugtext != '+') ++debugtext;
    ++debugtext;

    arg1 = (int)strtol(debugtext, &debugtext, 16);
    disasm(cpu65_pc, arg1, 0, -1);
    return DIS;
}

{BOS}di?s?{EOS} {
    /* disassemble current location */
    disasm(cpu65_pc, 256, 0, -1);
    return DIS;
}

{BOS}di?s?{WS}*(lc?1|lc?2){WS}+{ADDRS}{WS}+\+?{HEX}+{EOS} {
    /* disassemble language<bank> <addr> <len> */
    int lc;

    while (tolower(*debugtext) != 'l') ++debugtext; ++debugtext;
    if (tolower(*debugtext) == 'c') ++debugtext;

    lc = (int)strtol(debugtext, &debugtext, 10);
    arg1 = (int)strtol(debugtext, &debugtext, 16);
    arg2 = (int)strtol(debugtext, &debugtext, 16);
    
    disasm(arg1, arg2, lc, -1);
    return DIS;
}

{BOS}di?s?{WS}*(lc?1|lc?2){WS}+{BANK}{ADDRS}{WS}+\+?{HEX}+{EOS} {
    /* disassemble language<bank> /<bank>/<addr> <len> */
    int lc;

    while (tolower(*debugtext) != 'l') ++debugtext; ++debugtext;
    if (tolower(*debugtext) == 'c') ++debugtext;
    lc = (int)strtol(debugtext, &debugtext, 10);

    while (*debugtext != '/') ++debugtext;
    ++debugtext;	/* after / */
    arg3 = (int)strtol(debugtext, &debugtext, 10);
    ++debugtext;	/* after / */

    arg1 = (int)strtol(debugtext, &debugtext, 16);
    arg2 = (int)strtol(debugtext, &debugtext, 16);
    
    disasm(arg1, arg2, lc, arg3);
    return DIS;
}

{BOS}di?s?{WS}*(lc?1|lc?2){WS}+{ADDRS}{EOS} {
    /* disassemble language<bank> <addr> */
    int lc;

    while (tolower(*debugtext) != 'l') ++debugtext; ++debugtext;
    if (tolower(*debugtext) == 'c') ++debugtext;

    lc = (int)strtol(debugtext, &debugtext, 10);
    arg1 = (int)strtol(debugtext, &debugtext, 16);

    disasm(arg1, 256, lc, -1);
    return DIS;
}

{BOS}di?s?{WS}*(lc?1|lc?2){WS}+{BANK}{ADDRS}{EOS} {
    /* disassemble language<bank> /<bank>/<addr> */
    int lc;

    while (tolower(*debugtext) != 'l') ++debugtext; ++debugtext;
    if (tolower(*debugtext) == 'c') ++debugtext;
    lc = (int)strtol(debugtext, &debugtext, 10);

    while (*debugtext != '/') ++debugtext;
    ++debugtext;	/* after / */
    arg3 = (int)strtol(debugtext, &debugtext, 10);
    ++debugtext;	/* after / */

    arg1 = (int)strtol(debugtext, &debugtext, 16);

    disasm(arg1, 256, lc, arg3);
    return DIS;
}

{BOS}re?g?s?{EOS} {
    /* show cpu state */
    show_regs();
    return REGS;
}

{BOS}{ADDRS}{WS}*\:{WS}*{HEX}+{EOS} {
    /* set memory <addr> : <hex string> */
    arg1 = (int)strtol(debugtext, &debugtext, 16);

    while (*debugtext != ':') ++debugtext; ++debugtext;
    while (isspace(*debugtext)) ++debugtext;

    set_mem(arg1, debugtext);
    return SETMEM;
}

{BOS}{ADDRS}{WS}*(lc1|lc2)\:{WS}*{HEX}+{EOS} {
    /* set LC memory <addr> lc1|lc2 : <hex string> */
    int lc;

    arg1 = (int)strtol(debugtext, &debugtext, 16);

    while (tolower(*debugtext) != 'l') ++debugtext; ++debugtext;
    if (tolower(*debugtext) == 'c') ++debugtext;
    lc = (int)strtol(debugtext, &debugtext, 10);
    ++debugtext; while (isspace(*debugtext)) ++debugtext;

    set_lc_mem(arg1, lc, debugtext);
    return SETMEM;
}

{BOS}bload{WS}+{CHAR}+{WS}+{BANK}{ADDRS}{EOS} {
    /* bload <file> /<bank>/<addr> */
    FILE *fp = NULL;
    char *ptr = NULL;
    char buf[DEBUG_BUFSZ];

    while (!isspace(*debugtext)) ++debugtext;
    while (isspace(*debugtext)) ++debugtext;
    ptr = debugtext;
    while (!isspace(*debugtext)) ++debugtext;
    int len = MIN((int)(debugtext-ptr), DEBUG_BUFSZ-1);

    /* filename */
    strncpy(buf, ptr, len);
    buf[len] = '\0';

    /* get bank info */
    while (*debugtext != '/') ++debugtext;
    ++debugtext;
    int bank = (int)strtol(debugtext, &debugtext, 10);

    /* extract addrs */
    ++debugtext;
    arg1 = (int)strtol(debugtext, (char**)NULL, 16);

    fp = fopen(buf, "r");
    if (fp == NULL) {
	perror(buf);
	sprintf(second_buf[num_buffer_lines++], "problem: %s", buf);
	return BLOAD;
    }

    bload(fp, buf, bank, arg1);
    fclose(fp);
    return BLOAD;
}

{BOS}(st?e?p?|ne?x?t?){EOS} {

    stepping_struct_t s = {
        .step_type = STEPPING,
        .step_count = 1
    };

    if (*debugtext == 'n') {
        s.step_type = NEXTING;
    }

    debugger_go(s);
    return STEP;
}

{BOS}(st?e?p?|ne?x?t?){WS}+{HEX}+{EOS} {

    while (!isspace(*debugtext)) ++debugtext;

    arg1 = (int)strtol(debugtext, (char**)NULL, 16);
    if ((arg1 < 1) || (arg1 > 255)) arg1 = 255;

    stepping_struct_t s = {
        .step_type = STEPPING,
        .step_count = arg1
    };

    if (*debugtext == 'n') {
        s.step_type = NEXTING;
    }

    debugger_go(s);
    return STEP;
}

{BOS}fi?n?i?s?h?{EOS} {

    stepping_struct_t s = {
        .step_type = FINISHING,
        .step_frame = 1
    };

    debugger_go(s);
    return FINISH;
}

{BOS}un?t?i?l?{EOS} {
    /* step until PC == next instruction.  good for finishing backward
       loops */
    uint8_t op = get_current_opcode();
    int delta=0;
	
    switch (opcodes[op].mode)
    {
    case addr_implied:
    case addr_accumulator:
	delta = 1;
        break;
    case addr_immediate:
    case addr_zeropage:
    case addr_zeropage_x:
    case addr_zeropage_y:
    case addr_indirect:
    case addr_indirect_x:
    case addr_indirect_y:
    case addr_relative:
        delta = 2;
        break;
    case addr_absolute:
    case addr_absolute_x:
    case addr_absolute_y:
    case addr_j_indirect:
    case addr_j_indirect_x:
	delta = 3;
        break; 
    }

    stepping_struct_t s = {
        .step_type = UNTILING,
        .step_pc = cpu65_pc + delta
    };

    debugger_go(s);
    return UNTIL;
}

{BOS}go?{WS}+{HEX}+{EOS} {
    /* jump to addrs and run while remaining in debugger console */
    while (!isspace(*debugtext)) ++debugtext;

    /* DANGEROUS! */
    cpu65_pc = (int)strtol(debugtext, (char**)NULL, 16);

    stepping_struct_t s = {
        .step_type = GOING
    };

    debugger_go(s);
    return GO;
}

{BOS}go?{EOS} {
    stepping_struct_t s = {
        .step_type = GOING
    };

    debugger_go(s);
    return GO;
}

{BOS}wa?t?c?h?{EOS} {
    /* set watchpoint */
    set_halt(watchpoints, cpu65_pc);
    return WATCH;
}

{BOS}wa?t?c?h?{WS}+{HEX}+{EOS} {
    /* set watchpoint */
    while (!isspace(*debugtext)) ++debugtext;

    arg1 = (int)strtol(debugtext, (char**)NULL, 16);
    if ((arg1 < 0) || (arg1 > 65535)) {
	sprintf(second_buf[num_buffer_lines++], "invalid address");
	return WATCH;
    }

    set_halt(watchpoints, arg1);
    return WATCH;
}


{BOS}br?e?a?k?{EOS} {
    /* set breakpoint */
    set_halt(breakpoints, cpu65_pc);
    return BREAK;
}

{BOS}br?e?a?k?{WS}+{HEX}+{EOS} {
    /* set breakpoint */
    while (!isspace(*debugtext)) ++debugtext;

    arg1 = (int)strtol(debugtext, (char**)NULL, 16);
    if ((arg1 < 0) || (arg1 > 65535)) {
	sprintf(second_buf[num_buffer_lines++], "invalid address");
	return BREAK;
    }

    set_halt(breakpoints, arg1);
    return BREAK;
}

{BOS}br?e?a?k?{WS}*op{WS}+{HEX}+{EOS} {
    /* set breakpoint */
    while (!(*debugtext == 'p')) ++debugtext;
    ++debugtext;

    arg1 = (int)strtol(debugtext, (char**)NULL, 16);
    if ((arg1 < 0) || (arg1 > 0xFF)) {
	sprintf(second_buf[num_buffer_lines++], "invalid opcode");
	return BREAK;
    }

    set_halt_opcode((uint8_t)arg1);
    return BREAK;
}

{BOS}br65c02{EOS} {
    /* set opcode breakpoints on 65c02 instructions */
    set_halt_65c02();
    return BREAK;
}

{BOS}ig?n?o?r?e?{EOS} {
    /* ignore everything */
    clear_halt(watchpoints, 0);
    sprintf(second_buf[num_buffer_lines++], "ignored all");
    return IGNORE;
}

{BOS}ig?n?o?r?e?({WS}+{DEC}+)+{EOS} {
    /* ignore <watchpt> ... */

    while (!isspace(*debugtext)) ++debugtext;

    while (*debugtext) {
	arg1 = (int)strtol(debugtext, &debugtext, 10);
	if ((arg1 < 1) || (arg1 > MAX_BRKPTS)) {
	    sprintf(second_buf[num_buffer_lines++], "invalid watchpoint");
	    return IGNORE;
	}
	clear_halt(watchpoints, arg1);
	sprintf(second_buf[num_buffer_lines++], "ignored %d", arg1);
    }
    return IGNORE;
}


{BOS}cl?e?a?r?{EOS} {
    /* clear everything */
    clear_halt(breakpoints, 0);
    sprintf(second_buf[num_buffer_lines++], "cleared all");
    return CLEAR;
}

{BOS}cl?e?a?r?({WS}+{DEC}+)+{EOS} {
    /* clear <breakpt> ... */
    while (!isspace(*debugtext)) ++debugtext;

    while (*debugtext) {
	arg1 = (int)strtol(debugtext, &debugtext, 10);
	if ((arg1 < 1) || (arg1 > MAX_BRKPTS)) {
	    sprintf(second_buf[num_buffer_lines++], "invalid breakpoint");
	    return CLEAR;
	}
	clear_halt(breakpoints, arg1);
	sprintf(second_buf[num_buffer_lines++], "cleared %d", arg1);
    }
    return CLEAR;
}

{BOS}cl?e?a?r?{WS}*op({WS}+{HEX}+)+{EOS} {
    /* clear <opcode breakpt> ... */
    while (!(*debugtext == 'p')) ++debugtext;
    ++debugtext;

    while (*debugtext) {
	arg1 = (int)strtol(debugtext, &debugtext, 16);
	if ((arg1 < 0) || (arg1 > 255)) {
	    sprintf(second_buf[num_buffer_lines++], "invalid opcode");
	    return CLEAR;
	}
	clear_halt_opcode((uint8_t)arg1);
	sprintf(second_buf[num_buffer_lines++], "cleared opcode %02X",
		(uint8_t)arg1);
    }
    return CLEAR;
}

{BOS}cl65c02{EOS} {
    /* clear 65c02 ... */
    clear_halt_65c02();
    return CLEAR;
}

{BOS}stat?u?s?{EOS} {
    /* show breakpoints and watchpoints */
    show_breakpts();
    return STATUS;
}

{BOS}opc?o?d?e?s?{EOS} {
    /* show opcode breakpoints */
    show_opcode_breakpts();
    return OPCODES;
}

{BOS}sea?r?c?h?{WS}+{HEX}+{EOS} {
    /* search main memory for <bytes> */
    while (!isspace(*debugtext)) ++debugtext;
    while (isspace(*debugtext)) ++debugtext;

    search_mem(debugtext, 0, -1);

    return SEARCH;
}

{BOS}sea?r?c?h?{WS}+{BANK}{WS}+{HEX}+{EOS} {
    /* search memory<bank> for <bytes> */
    while (*debugtext != '/') ++debugtext;
    ++debugtext;	/* after / */
    arg3 = (int)strtol(debugtext, &debugtext, 10);
    ++debugtext;	/* after / */
    while (isspace(*debugtext)) ++debugtext;

    search_mem(debugtext, 0, arg3);

    return SEARCH;
}

{BOS}sea?r?c?h?{WS}*(lc?1|lc?2){WS}+{HEX}+{EOS} {
    /* search LC<bank> for <bytes> */
    int lc;

    while (tolower(*debugtext) != 'l') ++debugtext; ++debugtext;
    if (tolower(*debugtext) == 'c') ++debugtext;
    lc = (int)strtol(debugtext, &debugtext, 10);

    while (!isspace(*debugtext)) ++debugtext;
    while (isspace(*debugtext)) ++debugtext;

    search_mem(debugtext, lc, -1);

    return SEARCH;
}

{BOS}sea?r?c?h?{WS}*(lc?1|lc?2){WS}+{BANK}{WS}+{HEX}+{EOS} {
    /* search LC<bank> memory<bank> for <bytes> */
    int lc;

    while (tolower(*debugtext) != 'l') ++debugtext; ++debugtext;
    if (tolower(*debugtext) == 'c') ++debugtext;
    lc = (int)strtol(debugtext, &debugtext, 10);

    while (*debugtext != '/') ++debugtext;
    ++debugtext;	/* after / */
    arg3 = (int)strtol(debugtext, &debugtext, 10);
    ++debugtext;	/* after / */
    while (isspace(*debugtext)) ++debugtext;

    search_mem(debugtext, lc, arg3);

    return SEARCH;
}

{BOS}type{WS}+{CHAR}+{EOS} {
    /* type ASCII to emulator */
    char buf[DEBUG_BUFSZ];

    while (!isspace(*debugtext)) ++debugtext;
    while (isspace(*debugtext)) ++debugtext;

    strncpy(buf, debugtext, DEBUG_BUFSZ-2);
    int len = (int)strlen(buf);
    buf[len] = '\r';
    buf[len+1] = '\0';

    stepping_struct_t s = {
        .step_type = TYPING,
        .step_text = buf
    };

    debugger_go(s);

    return TYPE;
}

{BOS}load{WS}+{CHAR}+{EOS} {
    /* load/type ASCII <file> */
    FILE *fp = NULL;
    char buf[DEBUG_BUFSZ];

    while (!isspace(*debugtext)) ++debugtext;
    while (isspace(*debugtext)) ++debugtext;

    fp = fopen(debugtext, "r");
    if (fp == NULL) {
	perror(debugtext);
	sprintf(second_buf[num_buffer_lines++], "problem: %s", debugtext);
	return LOAD;
    }

    LOG("Typing...");
    int ch = -1;
    while (fgets(buf, DEBUG_BUFSZ, fp)) {
        LOG("%s", buf);

        stepping_struct_t s = {
            .step_type = LOADING,
            .step_text = buf
        };

        ch = debugger_go(s);
        if (ch != -1) {
            break;
        }
    }
    fclose(fp);

    if (ch == -1) {
        stepping_struct_t s = {
            .step_type = GOING,
        };
        debugger_go(s);
    }

    return LOAD;
}

{BOS}cput?r?a?c?e?{EOS} {
#if CPU_TRACING
    char *buf = NULL;
    asprintf(&buf, "%s/%s", getenv("HOME"), "cputrace.txt");
    cpu65_trace_toggle(buf);
    ASPRINTF_FREE(buf);
#else
    LOG("CPU tracing not enabled...");
#endif
}

{BOS}diskt?r?a?c?e?{EOS} {
#if DISK_TRACING
    char *buf = NULL;
    asprintf(&buf, "%s/%s", getenv("HOME"), "disktrace.txt");
    c_toggle_disk_trace_6(buf, NULL);
    ASPRINTF_FREE(buf);
#else
    LOG("Disk tracing not enabled...");
#endif
}

{BOS}la?n?g?{EOS} {
    /* display language card settings */
    show_lc_info();
    return LC;
}

{BOS}dri?v?e?{EOS} {
    /* show disk settings */
    show_disk_info();
    return DRIVE;
}

{BOS}vm?{EOS} {
    /* show other VM softswitch settings */
    show_misc_info();
    return VM;
}

{BOS}fre?s?h?{EOS} {
    /* refresh the screen */
    clear_debugger_screen();
}

{BOS}(fbsha1?){EOS} {
    fb_sha1();
    return FBSHA1;
}

{BOS}(\?|he?l?p?){EOS} {
    display_help();
    return HELP;
}

{BOS}bsave{WS}+{CHAR}+{WS}+{BANK}{ADDRS}{WS}+{HEX}+{EOS} {
    /* bsave <file> /<bank>/<addr> <len> */
    /* save memory dump to file */
    FILE *fp = NULL;
    char *ptr = NULL;
    char buf[DEBUG_BUFSZ];

    while (!isspace(*debugtext)) ++debugtext;
    while (isspace(*debugtext)) ++debugtext;

    /* copy file name */
    ptr = debugtext;
    while (!isspace(*debugtext)) ++debugtext;
    int len = MIN((int)(debugtext-ptr), DEBUG_BUFSZ-1);
    strncpy(buf, ptr, len);
    buf[len] = '\0';

    /* get bank info */
    while (*debugtext != '/') ++debugtext;
    ++debugtext;
    int bank = (int)strtol(debugtext, &debugtext, 10);
    ++debugtext;

    /* extract addrs and len */
    unsigned int addrs = (int)strtol(debugtext, &debugtext, 16);
    while (isspace(*debugtext)) ++debugtext;
    len = (int)strtol(debugtext, &debugtext, 16);

    if (addrs+len > 0x10000) {
        sprintf(second_buf[num_buffer_lines++], "buffer length overflow");
        return BSAVE;
    }

    fp = fopen(buf, "w");	/* try to open file for writing */
    if (fp == NULL) {
	perror(buf);
	sprintf(second_buf[num_buffer_lines++], "problem: %s", buf);
	return BSAVE;
    }

    do {
        size_t written = fwrite(apple_ii_64k[bank]+addrs, 1, len, fp);
        len -= written;
        addrs += written;
    } while(len);

    sprintf(second_buf[num_buffer_lines++], "bsaved: %s", buf);
    fclose(fp);
    return BSAVE;
}

{BOS}log{EOS} {
    /* log debugger output to file - not implemented */
    return LOG;
}

{BOS}save{EOS} {
    /* save apple2 state to a .img file - not implemented I'd like to
     * be compatible with the applePC emulator's .img format. anyone
     * have documentation on this?  -ASC
     **/
    return SAVE;
}


\n	/* ignore newlines */

.	/* ignore extraneous characters */


%%


int yywrap () {
    return 1;
}

/* initialize the buffer - needed each time through */
void init_lex (char *str, int size) {
    if (buffer) yy_delete_buffer(buffer);
    buffer = yy_scan_buffer(str, size);

    if (!buffer) {	/* oops */
	printf("lex buffer not big enough\n");
	exit(1);
    }	       

#define WONT_EXECUTE 0
    static volatile void *hack_warnings = &hack_warnings;
    if (hack_warnings == WONT_EXECUTE) {
        // flex defines these, but we don't use 'em ...
        char hack[2];
        yyunput('\0', hack);
        input();
    }
#undef WONT_EXECUTE
}

