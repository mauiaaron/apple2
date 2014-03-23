/*
 * Apple // emulator for Linux: Main debugger routines
 *
 * Copyright 1997, 1998, 2013 Aaron Culliney
 * Copyright 1998, 1999, 2000 Michael Deutschmann
 *
 * This software package is subject to the GNU General Public License
 * version 2 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * THERE ARE NO WARRANTIES WHATSOEVER.
 *
 * Originally added to emulator project in 1997 by Aaron Culliney
 *
 */

#include "common.h"

const struct opcode_struct *opcodes;

static stepping_struct_t stepping_struct;

volatile bool is_debugging = false;

#define BUF_X           DEBUGGER_BUF_X
#define BUF_Y           DEBUGGER_BUF_Y
#define SCREEN_X        81 // 80col + 1
#define SCREEN_Y        24
#define PROMPT_X        2
#define PROMPT_Y        BUF_Y - 1
#define PROMPT_END_X    BUF_X - 2
#define command_line    command_buf[PROMPT_Y]

char second_buf[BUF_Y][BUF_X];          /* scratch buffer for output */
int num_buffer_lines;                   /* num lines of output */
int arg1, arg2, arg3;                   /* command arguments */
int breakpoints[MAX_BRKPTS];            /* memory breakpoints */
int watchpoints[MAX_BRKPTS];            /* memory watchpoints */

/* debugger globals */
//1.  5.  10.  15.  20.  25.  30.  35.  40.  45.  50.  55.  60.  65.  70.  75.  80.",
static char screen[SCREEN_Y][SCREEN_X] =
{ "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||",
  "|                                      |                                       |",
  "|                                      |                                       |",
  "|                                      |                                       |",
  "|                                      |                                       |",
  "|                                      |                                       |",
  "|                                      |                                       |",
  "|                                      |                                       |",
  "|                                      |                                       |",
  "|                                      |                                       |",
  "|                                      |                                       |",
  "|                                      |                                       |",
  "|                                      |                                       |",
  "|                                      |                                       |",
  "|                                      |                                       |",
  "|                                      |                                       |",
  "|                                      |                                       |",
  "|                                      |                                       |",
  "|                                      |                                       |",
  "|                                      |                                       |",
  "|                                      |                                       |",
  "|                                      |                                       |",
  "|                                      |                                       |",
  "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||" };

static char command_buf[BUF_Y][BUF_X];  /* command line prompt */
char lexbuf[BUF_X+2];                   /* comman line to be flex'ed */

uint8_t current_opcode;

int op_breakpoints[256];                /* opcode breakpoints */

/* in debug.l */
extern int yylex();
extern void init_lex(char *buf, int size);

/* -------------------------------------------------------------------------
    c_get_current_rambank (addrs) - return the current ram bank for addrs.
        returns 0 = bank 0
                1 = bank 1
   ------------------------------------------------------------------------- */
int c_get_current_rambank(int addrs) {
    if ((addrs >= 0x200) && (addrs < 0xD000))
    {


        /* SLOTROM */
        if ((addrs >= 0xC100) && (addrs < 0xD000))
        {
            /* expansion rom */
            if ((addrs >= 0xC800) && (addrs < 0xD000))
            {
                return 1; /* always internal
                           * (the real rules are more complex, but
                           * this'll suffice since the slot 3 pseudo-card
                           * is the only one with c8 space) */

            }

            /* if SLOTCXROM, then internal rom (regardless of
               SLOTC3ROM setting). */
            if (softswitches & SS_CXROM)
            {
                return 1;
            }

            /* slot 3 rom */
            if ((addrs >= 0xC300) && (addrs < 0xC400))
            {
                return !!(softswitches & SS_C3ROM);
            }

            return 0;   /* peripheral rom */
        }

        /* text page 1 */
        if ((addrs >= 0x400) && (addrs < 0x800))
        {
            return !!(softswitches & SS_TEXTRD);
        }

        /* hires page 1 with 80STORE and HIRES on */
        if ((addrs >= 0x2000) && (addrs < 0x4000))
        {
            return !!(softswitches & SS_HGRRD);
        }

        /* otherwise return RAMRD flag */
        return !!(softswitches & SS_RAMRD);
    }

    /* executing in ALTZP space. */
    return !!(softswitches & SS_ALTZP);
}

/* -------------------------------------------------------------------------
    get_last_opcode () - returns the last executed opcode
   ------------------------------------------------------------------------- */
uint8_t get_last_opcode() {
    return cpu65_debug.opcode;
}

/* -------------------------------------------------------------------------
    get_current_opcode () - returns the opcode from the address that
        the PC is currently reading from.
   ------------------------------------------------------------------------- */
uint8_t get_current_opcode() {
    int bank = c_get_current_rambank(cpu65_current.pc);
    int lcbank = 0;

    /* main RAM */
    if (cpu65_current.pc < 0xD000)
    {
        return apple_ii_64k[bank][cpu65_current.pc];
    }

    /* LC RAM */
    if (cpu65_current.pc >= 0xE000)
    {
        if (softswitches & SS_LCRAM)
        {
            return language_card[bank][cpu65_current.pc-0xE000];
        }
        else
        {
            return apple_ii_64k[bank][cpu65_current.pc];
        }
    }

    /* LC BANK RAM */
    if (softswitches & SS_BANK2)
    {
        lcbank = 0x1000;
    }

    if (softswitches & SS_LCRAM)
    {
        return language_banks[bank][cpu65_current.pc-0xD000+lcbank];
    }
    else
    {
        return apple_ii_64k[bank][cpu65_current.pc];
    }
}

/* -------------------------------------------------------------------------
    dump_mem () - hexdump of memory to debug console
        we DO NOT wrap the display : 0xffff -> 0x0 (programs can't wrap)
   ------------------------------------------------------------------------- */

void dump_mem(int addrs, int len, int lc, int do_ascii, int rambank) {
    int i, j, mod, end;
    uint8_t op;
    int orig_addrs = addrs;             /* address for display */

    /* check which rambank */
    if (rambank == -1)
    {
        rambank = c_get_current_rambank(addrs);
    }

    if (!lc && (softswitches & SS_LCRAM) && (addrs >= 0xd000))
    {
        /* read lc anyway */
        lc = 1 + !!(softswitches & SS_BANK2);
    }

    if ((addrs < 0) || (addrs > 0xffff))
    {
        addrs = cpu65_current.pc;
        orig_addrs = addrs;
    }

    if (lc)
    {
        orig_addrs = addrs;
        if ((addrs >= 0xd000) && (addrs <= 0xffff))
        {
            addrs -= 0xd000;
        }

        if ((addrs < 0) || (addrs > 0x2fff))
        {
            addrs = 0;
        }
    }

    if ((len < 1) || (len > 256))
    {
        len = 256;
    }

    if (do_ascii && (len > 128))
    {
        len = 128;
    }


    /* save hexdump in second_buf */
    end = (lc) ? 0x3000 : 0x10000;
    for (i = num_buffer_lines-1, j = 0; ((j < len) && (addrs+j < end)); j++)
    {

        mod = j % (16 >> do_ascii);

        if (lc)
        {
            op = (addrs+j >= 0x1000) ? language_card[rambank][(addrs+j)-0x1000] : (lc == 1) ? language_banks[rambank][addrs+j] : language_banks[rambank][0x1000+addrs+j];
        }
        else
        {
            op = apple_ii_64k[rambank][addrs+j];
        }

        if (!mod)
        {
            if (++i)
            {
                for (mod=0; mod<BUF_X; mod++)
                {
                    if (second_buf[i-1][mod] == '\0')
                    {
                        second_buf[i-1][mod] = ' ';
                    }
                }
            }

            memset(second_buf[i], ' ', BUF_X); mod = 0;
            sprintf(second_buf[i], "%04X:%02X", orig_addrs+j, op);
            if (do_ascii)
            {
                sprintf(second_buf[i]+23, "%c", ((op&0x7f) > 31) ? (op&0x7f) : '.');
            }

            continue;
        }

        sprintf(second_buf[i]+5+mod*2, "%02X", op);
        if (do_ascii)
        {
            sprintf(second_buf[i]+23+mod, "%c", ((op&0x7f) > 31) ? (op&0x7f) : '.');
        }
    }

    for (mod=0; mod<BUF_X; mod++)
    {
        if (second_buf[i][mod] == '\0')
        {
            second_buf[i][mod] = ' ';
        }
    }

    num_buffer_lines = i + 1;
}


/* -------------------------------------------------------------------------
    search_mem () - search memory for bytes
   ------------------------------------------------------------------------- */
void search_mem(char *hexstr, int lc, int rambank) {
    int i = 0, j = 0, end, op;
    static char scratch[3];
    uint8_t byte;

    end = (lc) ? 0x3000 : 0x10000;

    /* check which rambank for cpu65_current.pc */
    if (rambank == -1)
    {
        rambank = c_get_current_rambank(cpu65_current.pc);
    }

    /* iterate over memory */
    for (i = 0; i < end; i++)
    {
        strncpy(scratch, hexstr+j, 2);          /* extract a byte */
        byte = (uint8_t) strtol(scratch, (char**)NULL, 16);

        if (lc)
        {
            op = (i >= 0x1000) ? language_card[rambank][i-0x1000] : (lc == 1) ? language_banks[rambank][i] : language_banks[rambank][0x1000+i];
        }
        else
        {
            op = apple_ii_64k[rambank][i];
        }

        if (byte == op)                 /* matched byte? */
        {
            ++j;                                /* increment */
            if (!isxdigit(*(hexstr+j)))         /* end of bytes? */
            {   /* then we found a match */
                sprintf(second_buf[num_buffer_lines], "%04X: %s", i-(j>>1), hexstr);
                num_buffer_lines = (num_buffer_lines + 1) % (BUF_Y-2);
                j = 0; continue;
            }

            ++j;
            if (!isxdigit(*(hexstr+j)))         /* end of bytes? */
            {   /* then we found a match */
                sprintf(second_buf[num_buffer_lines], "%04X: %s", i-(j>>1)+1, hexstr);
                num_buffer_lines = (num_buffer_lines + 1) % (BUF_Y-2);
                j = 0; continue;
            }

            continue;
        }

        j = 0;
    }
}


/* -------------------------------------------------------------------------
    set_mem () - write to memory.  we use the do_write_memory routine
    to "safely" set memory...
   ------------------------------------------------------------------------- */
void set_mem(int addrs, char *hexstr) {
    static char scratch[3];
    uint8_t data;

    if ((addrs < 0) || (addrs > 0xffff))
    {
        sprintf(second_buf[num_buffer_lines++], "invalid address");
        return;
    }

    while (*hexstr)
    {
        strncpy(scratch, hexstr, 2);
        data = (uint8_t) strtol(scratch, (char**)NULL, 16);

        /* call the set_memory routine, which knows how to route the
           request */
        cpu65_direct_write(addrs,data);

        ++hexstr;
        if (!*hexstr)
        {
            break;
        }

        ++hexstr;
        if (++addrs > 0xffff)
        {
            return;                    /* don't overwrite memory */
        }
    }
}


/* -------------------------------------------------------------------------
    set_lc_mem () - specifically write to apple II language card RAM memory
   ------------------------------------------------------------------------- */
void set_lc_mem(int addrs, int lcbank, char *hexstr) {
    static char scratch[3];
    uint8_t data;

    if ((addrs >= 0xd000) && (addrs <= 0xffff))
    {
        addrs -= 0xd000;
    }

    if ((addrs < 0) || (addrs > 0x2fff))
    {
        sprintf(second_buf[num_buffer_lines++], "invalid LC address");
        return;
    }

    while (*hexstr)
    {
        strncpy(scratch, hexstr, 2);
        data = (uint8_t) strtol(scratch, (char**)NULL, 16);

        /* ??? no way to write to aux LC banks */

        if (addrs >= 0x1000)
        {
            language_card[0][addrs - 0x1000] = data;
        }
        else if (lcbank)
        {
            language_banks[0][addrs] = data;
        }
        else
        {
            language_banks[0][addrs + 0x1000] = data;
        }

        ++hexstr;
        if (!*hexstr)
        {
            break;
        }

        ++hexstr;
        if (++addrs > 0x2fff)
        {
            return;
        }
    }
}

/* -------------------------------------------------------------------------
    bload () - bload file data into emulator.  this is essentially the
    same as the set_mem routine.  we use the do_write_memory routine to
    "safely" set memory...
   ------------------------------------------------------------------------- */
void bload(FILE *f, char *name, int addrs) {
    uint8_t *hexstr = NULL;
    int len = -1;
    uint8_t data;

    if ((addrs < 0) || (addrs > 0xffff))
    {
        sprintf(second_buf[num_buffer_lines++], "invalid address");
        return;
    }

    while ((len = fread(temp, 1, TEMPSIZE, f)))
    {
        hexstr = (uint8_t*)temp;
        for (; len > 0; len--)
        {
            data = *hexstr;

            /* call the set_memory routine, which knows how to route
               the request */
            cpu65_direct_write(addrs,data);

            ++hexstr;
            if (++addrs > 0xffff)
            {
                return;                    /* don't overwrite memory */
            }
        }
    }

    sprintf(second_buf[num_buffer_lines++], "bloaded: %s", name);
}


/* -------------------------------------------------------------------------
    disasm () - disassemble instructions
        we DO NOT wrap the display : 0xffff -> 0x0
   ------------------------------------------------------------------------- */

void disasm(int addrs, int len, int lc, int rambank) {
    static char fmt[64];
    uint8_t op;
    char arg1, arg2;
    int i, j, k, end, orig_addrs = addrs;

    /* check which rambank for cpu65_current.pc */
    if (rambank == -1)
    {
        rambank = c_get_current_rambank(addrs);
    }

    if (!lc && (softswitches & SS_LCRAM) && (addrs >= 0xd000))
    {
        /* read lc anyway */
        lc = 1 + !!(softswitches & SS_BANK2);
    }

    /* handle invalid address request */
    if ((addrs < 0) || (addrs > 0xffff))
    {
        addrs = cpu65_current.pc;
        orig_addrs = addrs;
    }

    /* disassembling from language card */
    if (lc)
    {
        if ((addrs >= 0xd000) && (addrs <= 0xffff))
        {
            addrs -= 0xd000;
        }

        if ((addrs < 0) || (addrs > 0x2fff))
        {
            addrs = 0;
        }
    }

    if (len > BUF_Y - 2)
    {
        len = BUF_Y - 2 - num_buffer_lines;
    }

    /* save hexdump in second_buf */
    end = (lc) ? 0x3000 : 0x10000;
    for (i = num_buffer_lines, j = addrs, k=orig_addrs; ((i<len) && (j<end)); i++, j++)
    {

        if (lc)
        {
            op = (j >= 0x1000) ? language_card[rambank][j-0x1000] : (lc == 1) ? language_banks[rambank][j] : language_banks[rambank][0x1000+j];
        }
        else
        {
            op = apple_ii_64k[rambank][j];
        }

        switch (opcodes[op].mode)
        {
        case addr_implied:
        case addr_accumulator:                  /* no arg */
            sprintf(second_buf[i], "/%02X/%04X: %02X      %s %s", rambank, k++, op, opcodes[op].mnemonic, disasm_templates[opcodes[op].mode]);
            break;

        case addr_immediate:
        case addr_zeropage:
        case addr_zeropage_x:
        case addr_zeropage_y:
        case addr_indirect:
        case addr_indirect_x:
        case addr_indirect_y:                   /* byte arg */
            if (k == 0xffff)
            {
                num_buffer_lines = i;
                return;
            }

            if (lc)
            {
                arg1 = (j >= 0x1000) ? language_card[rambank][++j-0x1000] : (lc == 1) ? language_banks[rambank][++j] : language_banks[rambank][++j+0x1000];
            }
            else
            {
                arg1 = apple_ii_64k[rambank][++j];
            }

            sprintf(fmt, "/%02X/%04X: %02X%02X    %s %s", rambank, k, op, (uint8_t)arg1, opcodes[op].mnemonic, disasm_templates[opcodes[op].mode]);

            sprintf(second_buf[i], fmt, (uint8_t)arg1);
            k+=2;
            break;

        case addr_absolute:
        case addr_absolute_x:
        case addr_absolute_y:
        case addr_j_indirect:
        case addr_j_indirect_x:                 /* word arg */
            if (k >= 0xfffe)
            {
                num_buffer_lines = i;
                return;
            }

            if (lc)
            {
                arg1 = (j >= 0x1000) ? language_card[rambank][++j-0x1000] : (lc == 1) ? language_banks[rambank][++j] : language_banks[rambank][++j+0x1000];
                arg2 = (j >= 0x1000) ? language_card[rambank][++j-0x1000] : (lc == 1) ? language_banks[rambank][++j] : language_banks[rambank][++j+0x1000];
            }
            else
            {
                arg1 = apple_ii_64k[rambank][++j];
                arg2 = apple_ii_64k[rambank][++j];
            }

            sprintf(fmt, "/%02X/%04X: %02X%02X%02X  %s %s", rambank, k, op, (uint8_t)arg1, (uint8_t)arg2, opcodes[op].mnemonic, disasm_templates[opcodes[op].mode]);
            sprintf(second_buf[i], fmt, (uint8_t)arg2, (uint8_t)arg1);
            k+=3;
            break;

        case addr_relative:                     /* offset */
            if (k == 0xffff)
            {
                num_buffer_lines = i;
                return;
            }

            if (lc)
            {
                arg1 = (j >= 0x1000) ? language_card[rambank][++j-0x1000] : (lc == 1) ? language_banks[rambank][++j] : language_banks[rambank][++j+0x1000];
            }
            else
            {
                arg1 = apple_ii_64k[rambank][++j];
            }

            sprintf(fmt, "/%02X/%04X: %02X%02X    %s %s", rambank, k, op, (uint8_t)arg1, opcodes[op].mnemonic, disasm_templates[opcodes[op].mode]);
            if (arg1 < 0)
            {
                sprintf(second_buf[i], fmt, k + arg1 + 2, '-', (uint8_t)(-arg1));
            }
            else
            {
                sprintf(second_buf[i], fmt, k + arg1 + 2, '+', (uint8_t)arg1);
            }

            k+=2;
            break;

        default:                                /* shouldn't happen */
            sprintf(second_buf[i], "args to opcode incorrect!");
            break;
        }
    }

    num_buffer_lines = i;
}

/* -------------------------------------------------------------------------
    show_regs () - shows 6502 registers
   ------------------------------------------------------------------------- */

void show_regs() {
    sprintf(second_buf[num_buffer_lines++], "PC = %04X EA = %04X SP = %04X", cpu65_current.pc, cpu65_debug.ea, cpu65_current.sp + 0x0100);
    sprintf(second_buf[num_buffer_lines++], "X = %02X Y = %02X A = %02X F = %02X", cpu65_current.x, cpu65_current.y, cpu65_current.a, cpu65_current.f);

    memset(second_buf[num_buffer_lines], ' ', BUF_X);
    if (cpu65_current.f & C_Flag_6502)
    {
        second_buf[num_buffer_lines][0]='C';
    }

    if (cpu65_current.f & X_Flag_6502)
    {
        second_buf[num_buffer_lines][1]='X';
    }

    if (cpu65_current.f & I_Flag_6502)
    {
        second_buf[num_buffer_lines][2]='I';
    }

    if (cpu65_current.f & V_Flag_6502)
    {
        second_buf[num_buffer_lines][3]='V';
    }

    if (cpu65_current.f & B_Flag_6502)
    {
        second_buf[num_buffer_lines][4]='B';
    }

    if (cpu65_current.f & D_Flag_6502)
    {
        second_buf[num_buffer_lines][5]='D';
    }

    if (cpu65_current.f & Z_Flag_6502)
    {
        second_buf[num_buffer_lines][6]='Z';
    }

    if (cpu65_current.f & N_Flag_6502)
    {
        second_buf[num_buffer_lines][7]='N';
    }

    ++num_buffer_lines;
}

/* -------------------------------------------------------------------------
    will_branch () = will instruction branch?
                -1 - n/a
                0  - no it won't
                >0  - yes it will
   ------------------------------------------------------------------------- */
static int will_branch() {

    uint8_t op = get_current_opcode();

    switch (op)
    {
    case 0x10:                          /* BPL */
        return (int) !(cpu65_current.f & N_Flag_6502);
    case 0x30:                          /* BMI */
        return (int) (cpu65_current.f & N_Flag_6502);
    case 0x50:                          /* BVC */
        return (int) !(cpu65_current.f & V_Flag_6502);
    case 0x70:                          /* BVS */
        return (int) (cpu65_current.f & V_Flag_6502);
    case 0x80:                          /* BRA */
        return 1;
    case 0x90:                          /* BCC */
        return (int) !(cpu65_current.f & C_Flag_6502);
    case 0xb0:                          /* BCS */
        return (int) (cpu65_current.f & C_Flag_6502);
    case 0xd0:                          /* BNE */
        return (int) !(cpu65_current.f & Z_Flag_6502);
    case 0xf0:                          /* BEQ */
        return (int) (cpu65_current.f & Z_Flag_6502);
    }

    return -1;
}


/* -------------------------------------------------------------------------
    set_halt () = set a breakpoint or watchpoint in memory
        type = points to "watchpoints" or "breakpoints" array
   ------------------------------------------------------------------------- */
void set_halt(int *type, int addrs) {
    int i;

    for (i = 0; i < MAX_BRKPTS; i++)
    {
        if (type[i] == -1)
        {
            type[i] = addrs;
            sprintf(second_buf[num_buffer_lines++], "set at %04X", addrs);
            return;
        }
    }

    sprintf(second_buf[num_buffer_lines++], "too many!");
}

/* -------------------------------------------------------------------------
    clear_halt () = unset a critical breakpoint or watchpoint in memory
        type = points to "watchpoints" or "breakpoints" array
        pt = (pt - 1) into type.  0 indicates clear all.
   ------------------------------------------------------------------------- */
void clear_halt(int *type, int pt) {
    int i;

    if (!pt)            /* unset all */
    {
        for (i = 0; i < MAX_BRKPTS; i++)
        {
            type[i] = -1;
        }

        return;
    }

    type[pt-1] = -1;    /* unset single */
}

/* -------------------------------------------------------------------------
    set_halt_opcode () = set a breakpoint on a particular opcode.
   ------------------------------------------------------------------------- */
void set_halt_opcode(uint8_t opcode) {
    op_breakpoints[opcode] = 1;
}

/* -------------------------------------------------------------------------
    clear_halt_opcode () = unset an opcode breakpoint.
   ------------------------------------------------------------------------- */
void clear_halt_opcode(uint8_t opcode) {
    op_breakpoints[opcode] = 0;
}

/* -------------------------------------------------------------------------
    set_halt_65c02 () = set a breakpoint on all 65c02 instructions.
    assumes that you are in //e mode...
   ------------------------------------------------------------------------- */
void set_halt_65c02() {
    set_halt_opcode((uint8_t)0x02); set_halt_opcode((uint8_t)0x04);
    set_halt_opcode((uint8_t)0x0C); set_halt_opcode((uint8_t)0x12);
    set_halt_opcode((uint8_t)0x14); set_halt_opcode((uint8_t)0x1A);
    set_halt_opcode((uint8_t)0x1C); set_halt_opcode((uint8_t)0x32);
    set_halt_opcode((uint8_t)0x34); set_halt_opcode((uint8_t)0x3A);
    set_halt_opcode((uint8_t)0x3C); set_halt_opcode((uint8_t)0x52);
    set_halt_opcode((uint8_t)0x5A); set_halt_opcode((uint8_t)0x64);
    set_halt_opcode((uint8_t)0x72); set_halt_opcode((uint8_t)0x74);
    set_halt_opcode((uint8_t)0x7A); set_halt_opcode((uint8_t)0x7C);
    set_halt_opcode((uint8_t)0x80); set_halt_opcode((uint8_t)0x89);
    set_halt_opcode((uint8_t)0x92); set_halt_opcode((uint8_t)0x9C);
    set_halt_opcode((uint8_t)0x9E); set_halt_opcode((uint8_t)0xB2);
    set_halt_opcode((uint8_t)0xD2); set_halt_opcode((uint8_t)0xDA);
    set_halt_opcode((uint8_t)0xF2); set_halt_opcode((uint8_t)0xFA);
}

/* -------------------------------------------------------------------------
    clear_halt_65c02 () = clear all 65c02 instructions
   ------------------------------------------------------------------------- */
void clear_halt_65c02() {
    clear_halt_opcode((uint8_t)0x02); clear_halt_opcode((uint8_t)0x04);
    clear_halt_opcode((uint8_t)0x0C); clear_halt_opcode((uint8_t)0x12);
    clear_halt_opcode((uint8_t)0x14); clear_halt_opcode((uint8_t)0x1A);
    clear_halt_opcode((uint8_t)0x1C); clear_halt_opcode((uint8_t)0x32);
    clear_halt_opcode((uint8_t)0x34); clear_halt_opcode((uint8_t)0x3A);
    clear_halt_opcode((uint8_t)0x3C); clear_halt_opcode((uint8_t)0x52);
    clear_halt_opcode((uint8_t)0x5A); clear_halt_opcode((uint8_t)0x64);
    clear_halt_opcode((uint8_t)0x72); clear_halt_opcode((uint8_t)0x74);
    clear_halt_opcode((uint8_t)0x7A); clear_halt_opcode((uint8_t)0x7C);
    clear_halt_opcode((uint8_t)0x80); clear_halt_opcode((uint8_t)0x89);
    clear_halt_opcode((uint8_t)0x92); clear_halt_opcode((uint8_t)0x9C);
    clear_halt_opcode((uint8_t)0x9E); clear_halt_opcode((uint8_t)0xB2);
    clear_halt_opcode((uint8_t)0xD2); clear_halt_opcode((uint8_t)0xDA);
    clear_halt_opcode((uint8_t)0xF2); clear_halt_opcode((uint8_t)0xFA);
}

/* -------------------------------------------------------------------------
    at_haltpt () - tests if at haltpt
        returns 0 = no breaks or watches
                1 = one break or watchpoint fired
                n = two or more breaks and/or watches fired
   ------------------------------------------------------------------------- */
int at_haltpt() {
    int i;

    /* check op_breakpoints */
    uint8_t op = get_last_opcode();
    if (op_breakpoints[op])
    {
        sprintf(second_buf[num_buffer_lines++], "stopped at %04X bank %d instruction %02X", cpu65_current.pc, c_get_current_rambank(cpu65_current.pc), op);
    }

    for (i = 0; i < MAX_BRKPTS; i++)
    {

        if (cpu65_current.pc == breakpoints[i])
        {
            sprintf(second_buf[num_buffer_lines++], "stopped at %04X bank %d", breakpoints[i], c_get_current_rambank(cpu65_current.pc));
        }
    }

    if (cpu65_debug.rw)   /* only check watchpoints if read/write occured */
    {
        for (i = 0; i < MAX_BRKPTS; i++)
        {
            if (cpu65_debug.ea == watchpoints[i])
            {
                if (cpu65_debug.rw & 2)
                {
                    sprintf(second_buf[num_buffer_lines++], "wrote: %04X: %02X", watchpoints[i], cpu65_debug.d);
                }
                else
                {
                    sprintf(second_buf[num_buffer_lines++], "read: %04X", watchpoints[i]);
                }

                cpu65_debug.rw = 0; /* only allow WP to trip once */
            }
        }
    }

    return num_buffer_lines;    /* 0 indicates nothing happened */
}

/* -------------------------------------------------------------------------
    show_breakpts () - show breakpoints and watchpoints
   ------------------------------------------------------------------------- */
void show_breakpts() {
    int i=num_buffer_lines, k;

    for (k = 0; k < MAX_BRKPTS; k++)
    {
        if ((breakpoints[k] >= 0) && (watchpoints[k] >= 0))
        {
            sprintf(second_buf[i++], "break %02d at %04X  watch %02d at %04X", k+1, breakpoints[k], k+1, watchpoints[k]);
        }
        else if (breakpoints[k] >= 0)
        {
            sprintf(second_buf[i++], "break %02d at %04X", k+1, breakpoints[k]);
        }
        else if (watchpoints[k] >= 0)
        {
            memset(second_buf[i], ' ', BUF_X);
            sprintf(second_buf[i++]+16, "  watch %02d at %04X", k+1, watchpoints[k]);
        }
    }

    num_buffer_lines = i;
}

/* -------------------------------------------------------------------------
    show_opcode_breakpts () - show opcode breakpoints
   ------------------------------------------------------------------------- */
void show_opcode_breakpts() {
    int i=num_buffer_lines, k;

    sprintf(second_buf[i++], "    0 1 2 3 4 5 6 7 8 9 A B C D E F");
    sprintf(second_buf[i++], "   |-------------------------------|");
    for (k = 0; k < 0x10; k++)
    {
        sprintf(second_buf[i++],
                "  %X|%s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s|", k,
                op_breakpoints[k     ] ? "x" : " ",
                op_breakpoints[k+0x10] ? "x" : " ",
                op_breakpoints[k+0x20] ? "x" : " ",
                op_breakpoints[k+0x30] ? "x" : " ",
                op_breakpoints[k+0x40] ? "x" : " ",
                op_breakpoints[k+0x50] ? "x" : " ",
                op_breakpoints[k+0x60] ? "x" : " ",
                op_breakpoints[k+0x70] ? "x" : " ",
                op_breakpoints[k+0x80] ? "x" : " ",
                op_breakpoints[k+0x90] ? "x" : " ",
                op_breakpoints[k+0xA0] ? "x" : " ",
                op_breakpoints[k+0xB0] ? "x" : " ",
                op_breakpoints[k+0xC0] ? "x" : " ",
                op_breakpoints[k+0xD0] ? "x" : " ",
                op_breakpoints[k+0xE0] ? "x" : " ",
                op_breakpoints[k+0xF0] ? "x" : " ");
    }

    sprintf(second_buf[i++], "   |-------------------------------|");

    num_buffer_lines = i;
}

/* -------------------------------------------------------------------------
    show_lc_info () - show language card info
   ------------------------------------------------------------------------- */
void show_lc_info() {
    int i = num_buffer_lines;
    sprintf(second_buf[i++], "lc bank = %d", 1 + !!(softswitches && SS_BANK2));
    (softswitches & SS_LCWRT) ? sprintf(second_buf[i++], "write LC") : sprintf(second_buf[i++], "LC write protected");
    (softswitches & SS_LCRAM) ? sprintf(second_buf[i++], "read LC")  : sprintf(second_buf[i++], "read ROM");
    sprintf(second_buf[i++], "second = %d", !!(softswitches && SS_LCSEC));
    num_buffer_lines = i;
}

void show_misc_info() {
    int i = num_buffer_lines;
    sprintf(second_buf[i++], "TEXT (%04X): %s", SW_TEXT + !!(softswitches & SS_TEXT), (softswitches & SS_TEXT) ? "on" : "off");
    sprintf(second_buf[i++], "MIXED (%04X): %s", SW_MIXED + !!(softswitches & SS_MIXED), (softswitches & SS_MIXED) ? "on" : "off");
    sprintf(second_buf[i++], "PAGE2 (%04X): %s", SW_PAGE2 + !!(softswitches & SS_PAGE2), (softswitches & SS_PAGE2) ? "on" : "off");
    sprintf(second_buf[i++], "HIRES (%04X): %s", SW_HIRES + !!(softswitches & SS_HIRES), (softswitches & SS_HIRES) ? "on" : "off");
    sprintf(second_buf[i++], "80STORE (%04X): %s", SW_80STORE + !!(softswitches & SS_80STORE), (softswitches & SS_80STORE) ? "on" : "off");
    sprintf(second_buf[i++], "RAMRD (%04X): %s", SW_RAMRD + !!(softswitches & SS_RAMRD), (softswitches & SS_RAMRD) ? "on" : "off");
    sprintf(second_buf[i++], "RAMWRT (%04X): %s", SW_RAMWRT + !!(softswitches & SS_RAMWRT), (softswitches & SS_RAMWRT) ? "on" : "off");
    sprintf(second_buf[i++], "ALTZP (%04X): %s", SW_ALTZP + !!(softswitches & SS_ALTZP), (softswitches & SS_ALTZP) ? "on" : "off");
    sprintf(second_buf[i++], "80COL (%04X): %s", SW_80COL + !!(softswitches & SS_80COL), (softswitches & SS_80COL) ? "on" : "off");
    sprintf(second_buf[i++], "ALTCHAR (%04X): %s", SW_ALTCHAR + !!(softswitches & SS_ALTCHAR), (softswitches & SS_ALTCHAR) ? "on" : "off");
    sprintf(second_buf[i++], "SLOTC3ROM (%04X): %s", SW_SLOTC3ROM -/*anomaly*/ !!(softswitches & SS_C3ROM), (softswitches & SS_C3ROM) ? "on" : "off");
    sprintf(second_buf[i++], "SLOTCXROM (%04X): %s", SW_SLOTCXROM + !!(softswitches & SS_CXROM), (softswitches & SS_CXROM) ? "on" : "off");
    sprintf(second_buf[i++], "DHIRES (%04X): %s", SW_DHIRES + !!(softswitches && SS_DHIRES), (softswitches & SS_DHIRES) ? "on" : "off");
    sprintf(second_buf[i++], "IOUDIS (%04X): %s", SW_IOUDIS + !!(softswitches && SS_IOUDIS), (softswitches && SS_IOUDIS) ? "on" : "off");
/*     sprintf(second_buf[i++], "RDVBLBAR: %s", (SLOTCXROM & 0x80) */
/*          ? "on" : "off"); */

    num_buffer_lines = i;
}

/* -------------------------------------------------------------------------
    show_disk_info () - disk II info
   ------------------------------------------------------------------------- */
void show_disk_info() {
    static char tmp[32];
    int i = num_buffer_lines, len = 0, off = 19;

    /* generic information */
    sprintf(second_buf[i++], "drive %s", (disk6.drive) ? "B" : "A");
    sprintf(second_buf[i++], "motor %s", (disk6.motor) ? "off" : "on");
    sprintf(second_buf[i++], "%s", (disk6.ddrw) ? "write" : "read");
    sprintf(second_buf[i++], "byte = %02X", disk6.disk_byte);
    if (!disk6.disk[disk6.drive].nibblized)
    {
        sprintf(second_buf[i++], "volume = %d", disk6.volume);
        sprintf(second_buf[i++], "checksum = %d", disk6.checksum);
    }

    sprintf(second_buf[i++], "-------------------------------------");

    /* drive / image specific information */
    memset(second_buf[i], ' ', BUF_X);
    if ((len = strlen(disk6.disk[0].file_name)))
    {
        while ((--len) && (disk6.disk[0].file_name[len] != '/'))
        {
        }

        strncpy(tmp, disk6.disk[0].file_name + len + 1, 31);
        *(second_buf[i] + sprintf(second_buf[i], "%s", tmp)) = ' ';
    }

    if ((len = strlen(disk6.disk[1].file_name)))
    {
        while ((--len) && (disk6.disk[1].file_name[len] != '/'))
        {
        }

        strncpy(tmp, disk6.disk[1].file_name + len + 1, 31);
        sprintf(second_buf[i]+off, "%s", tmp);
    }

    memset(second_buf[++i], ' ', BUF_X);
    *(second_buf[i] + sprintf(second_buf[i], "%s %d bytes", (disk6.disk[0].nibblized) ? ".nib" : ".dsk", (int)disk6.disk[0].file_size)) = ' ';
    sprintf(second_buf[i++]+off, "%s %d bytes", (disk6.disk[1].nibblized) ? ".nib" : ".dsk", (int)disk6.disk[1].file_size);

    memset(second_buf[i], ' ', BUF_X);
    *(second_buf[i] + sprintf(second_buf[i], "write %s", (disk6.disk[0].is_protected) ? "protected" : "enabled")) = ' ';
    sprintf(second_buf[i++]+off, "write %s", (disk6.disk[1].is_protected) ? "protected" : "enabled");

    memset(second_buf[i], ' ', BUF_X);
    *(second_buf[i] + sprintf(second_buf[i], "phase %d %s", disk6.disk[0].phase, (disk6.disk[0].phase_change) ? "(new)" : "")) = ' ';
    sprintf(second_buf[i++]+off, "phase %d %s", disk6.disk[1].phase, (disk6.disk[1].phase_change) ? "(new)" : "");

    memset(second_buf[i], ' ', BUF_X);
    if (!disk6.disk[0].nibblized)
    {
        *(second_buf[i] + sprintf(second_buf[i], "sector %d", disk6.disk[0].sector)) = ' ';
        if (disk6.disk[1].nibblized)
        {
            ++i;
        }
    }

    if (!disk6.disk[1].nibblized)
    {
        sprintf(second_buf[i++]+off, "sector %d", disk6.disk[1].sector);
    }

    num_buffer_lines = i;
}

/* -------------------------------------------------------------------------
    clear_debugger_screen () - clears the screen of graphics artifacts.
   ------------------------------------------------------------------------- */
void clear_debugger_screen() {
    int i;
    video_setpage( 0 );
    for (i = 0; i < 24; i++)
    {
        c_interface_print(0, i, 2, screen[ i ] );
    }
}

/* -------------------------------------------------------------------------
    end_cpu_step () - finish a stepping command
        display the next instruction, and tell whether it will branch
   ------------------------------------------------------------------------- */
static void end_cpu_step() {

    clear_debugger_screen();
    disasm(cpu65_current.pc, 1, 0, -1);
    int branch = will_branch();
    if (branch == -1)
    {
        return;
    }

    sprintf(second_buf[num_buffer_lines++], "%s", (branch) ? "will branch" : "will not branch");
}

/* -------------------------------------------------------------------------
    begin_cpu_step() - step the CPU
        set the CPU into stepping mode and yield to CPU thread
   ------------------------------------------------------------------------- */
static void begin_cpu_step()
{
    int err = 0;

    do {
        if ((err = pthread_cond_signal(&interface_cond))) {
            ERRLOG("pthread_cond_signal : %d", err);
        }
        if ((err = pthread_cond_wait(&interface_cond, &interface_mutex))) {
            ERRLOG("pthread_cond_wait : %d", err);
        }

        if (c_mygetch(0) != -1) {
            break;
        }
    } while (!stepping_struct.should_break);

    if ((err = pthread_cond_signal(&interface_cond))) {
        ERRLOG("pthread_cond_signal : %d", err);
    }

    end_cpu_step();
}

/* -------------------------------------------------------------------------
    c_debugger_should_break()
   ------------------------------------------------------------------------- */
bool c_debugger_should_break() {

    // WARNING : this state management function should be called from CPU thread only!

    if (at_haltpt()) {
        stepping_struct.should_break = true;
    } else {
        uint8_t op = get_last_opcode();

        switch (stepping_struct.step_type) {
            case STEPPING:
            {
                --stepping_struct.step_count;
                if (stepping_struct.step_count == 0) {
                    stepping_struct.should_break = true;
                }
            }
            break;

            case NEXTING:
            {
                if (stepping_struct.step_count > 0) {
                    --stepping_struct.step_count;
                }

                if (op == 0x20) {
                    ++stepping_struct.step_frame; // JSR
                }
                if (op == 0x60) {
                    --stepping_struct.step_frame; // RTS
                }

                if ((stepping_struct.step_frame == 0) && (stepping_struct.step_count == 0)) {
                    stepping_struct.should_break = true;
                }
            }
            break;

            case UNTILING:
            {
                if (stepping_struct.step_pc == cpu65_current.pc) {
                    stepping_struct.should_break = true;
                }
            }
            break;

            case FINISHING:
            {
                if (op == 0x20) {
                    ++stepping_struct.step_frame; // JSR
                }
                if (op == 0x60) {
                    --stepping_struct.step_frame; // RTS
                }

                if (stepping_struct.step_frame == 0) {
                    stepping_struct.should_break = true;
                }
            }
            break;

            case GOING:
            break;
        }
    }

    return stepping_struct.should_break;
}

/* -------------------------------------------------------------------------
    c_debugger_begin_stepping () - step into or step over commands
   ------------------------------------------------------------------------- */
void c_debugger_begin_stepping(stepping_struct_t s) {
    stepping_struct = s;
    begin_cpu_step();
}

/* -------------------------------------------------------------------------
    display_help ()
        show quick reference command usage
   ------------------------------------------------------------------------- */
void display_help() {
    /*                      "|||||||||||||||||||||||||||||||||||||" */
    int i = num_buffer_lines;
    sprintf(second_buf[i++], "d{is} {lc1|lc2} {/bank/addr} {+}{len}");
    sprintf(second_buf[i++], "m{em} {lc1|lc2} {/bank/addr} {+}{len}");
    sprintf(second_buf[i++], "a{sc} {lc1|lc2} {/bank/addr} {+}{len}");
    sprintf(second_buf[i++], "r{egs}                               ");
    sprintf(second_buf[i++], "<addr> {lc1|lc2} : <byteseq>         ");
    sprintf(second_buf[i++], "(s{tep} | n{ext}) {len}              ");
    sprintf(second_buf[i++], "f{inish}                             ");
    sprintf(second_buf[i++], "u{ntil}                              ");
    sprintf(second_buf[i++], "g{o} {addr}                          ");
    sprintf(second_buf[i++], "sea{rch} {lc1|lc2} {bank} <bytes>    ");
    sprintf(second_buf[i++], "(b{reak} | w{atch}) {addr}           ");
    sprintf(second_buf[i++], "b{reak} op <byte>                    ");
    sprintf(second_buf[i++], "(c{lear} | i{gnore}) {num}           ");
    sprintf(second_buf[i++], "c{lear} op <byte>                    ");
    sprintf(second_buf[i++], "(sta{tus} | op{codes})               ");
    sprintf(second_buf[i++], "(l{ang} | d{rive} | vm)              ");
    sprintf(second_buf[i++], "bsave <filename> </bank/addr> <len>  ");
    sprintf(second_buf[i++], "bload <filename> <addr>              ");
    sprintf(second_buf[i++], "fr{esh}                              ");
    sprintf(second_buf[i++], "(h{elp} | ?)                         ");
    num_buffer_lines = i;
}


/* -------------------------------------------------------------------------
    do_debug_command ()
        perform a debugger command
   ------------------------------------------------------------------------- */

void do_debug_command() {
    int i = 0, j = 0, k = 0;

    /* reset key local vars */
    num_buffer_lines = 0;

    /* call lex to perform the command.*/
    strncpy(lexbuf, command_line + PROMPT_X, BUF_X);
    init_lex(lexbuf, BUF_X+2);
    yylex();

    /* set up to copy results into main buffer */
    if (num_buffer_lines >= PROMPT_Y)
    {
        k = BUF_Y - PROMPT_Y;
    }
    else
    {
        /* scroll buffer */
        for (i = 0, j = 0; i < PROMPT_Y - num_buffer_lines; i++, j = 0)
        {
            memcpy(command_buf[i], command_buf[num_buffer_lines+1+i], BUF_X);
            while ((j < BUF_X) && (command_buf[i][j] != '\0'))
            {
                j++;
            }

            memset(command_buf[i] + j, ' ', BUF_X - j - 1);
            command_buf[i][BUF_X - 1] = '\0';
        }
    }

    /* copy the debug results into debug console window. change '\0's
       to ' 's and cap with a single '\0' */
    while (i < PROMPT_Y)
    {
        j = 0;
        memcpy(command_buf[i], second_buf[k++], BUF_X);
        while ((j < BUF_X) && (command_buf[i][j] != '\0'))
        {
            ++j;
        }

        memset(command_buf[i] + j, ' ', BUF_X - j/* - 1*/);
        command_buf[i++][BUF_X - 1] = '\0';
    }

    /* new prompt */
    memset(command_line, ' ', BUF_X);
    command_line[0] = '>';
    command_line[BUF_X - 1] = '\0';

    /* display the new information */
    for (i=0; i<BUF_Y; i++)
    {
        c_interface_print(1, 1+i, 0, command_buf[i]);
    }

    /* reset before we return */
    num_buffer_lines = 0;
}

/* -------------------------------------------------------------------------
    c_interface_debugging()
        main debugging console
   ------------------------------------------------------------------------- */

#ifdef INTERFACE_CLASSIC
void c_interface_debugging() {

    static char lex_initted = 0;

    int err = 0;
    int i;
    int ch;
    int command_pos = PROMPT_X;

    opcodes = (apple_mode == 0) ? opcodes_6502 :
              (apple_mode == 1) ? opcodes_undoc :
              opcodes_65c02;

    /* initialize the buffers */
    for (i=0; i<BUF_Y; i++)
    {
        memset(command_buf[i], '\0', BUF_X);
        memset(second_buf [i], '\0', BUF_X);
    }

    memset(command_line, ' ', BUF_X);
    command_line[0] = '>';
    command_line[BUF_X - 1] = '\0';

    /* initialize the lexical scanner */
    if (!lex_initted)
    {
        memset(lexbuf, '\0', BUF_X+2);
        /*init_lex(lexbuf, BUF_X+2);*/
        lex_initted = 1;
    }

    video_setpage( 0 );
    c_interface_translate_screen( screen );
    for (i = 0; i < 24; i++)
    {
        c_interface_print(0, i, 2, screen[ i ] );
    }

    is_debugging = true;

    for (;;)
    {
        /* print command line */
        c_interface_print(1, 1+PROMPT_Y, 0, command_line);

        /* highlight cursor */
        video_plotchar(1+command_pos, 1+PROMPT_Y, 1, command_line[command_pos]);

        while ((ch = c_mygetch(1)) == -1)
        {
        }

        if (ch == kESC)
        {
            break;
        }
        else
        {
            /* backspace */
            if ((ch == 127 || ch == 8) && (command_pos > PROMPT_X))
            {
                command_line[--command_pos] = ' ';
            }
            /* return */
            else if (ch == 13)
            {
                command_line[command_pos] = '\0';
                do_debug_command();
                command_pos = PROMPT_X;
            }
            /* normal character */
            else if ((ch >= ' ') && (ch < 127) && (command_pos < PROMPT_END_X))
            {
                command_line[command_pos++] = ch;
            }
        }
    }

    c_interface_exit(-1);
    is_debugging = false;
    if ((err = pthread_cond_signal(&interface_cond)))
    {
        ERRLOG("pthread_cond_signal : %d", err);
    }
    return;
}
#endif

