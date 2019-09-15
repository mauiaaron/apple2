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

#include "common.h"

const char * const disasm_templates[NUM_ADDRESSING_MODES] = {
    "",                     // addr_implied
    "A",                    // addr_accumulator
    "#$%02X",               // addr_immediate
    "$%02X",                // addr_zeropage
    "$%02X,X",              // addr_zeropage_x
    "$%02X,Y",              // addr_zeropage_y
    "$%02X%02X",            // addr_absolute
    "$%02X%02X,X",          // addr_absolute_x
    "$%02X%02X,Y",          // addr_absolute_y
    "($%02X)",              // addr_indirect
    "($%02X,X)",            // addr_indirect_x
    "($%02X),Y",            // addr_indirect_y
    "($%02X%02X)",          // addr_j_indirect - non-zeropage indirects, used in JMP only
    "($%02X%02X),X",        // addr_j_indirect_x
    "$%04X (%c%02X)"        // addr_relative
};

const struct opcode_struct_s opcodes_65c02[256] = {
    { "BRK", addr_implied },        // 0x00
    { "ORA", addr_indirect_x },
    { "???", addr_implied },
    { "???", addr_implied },
    { "TSB", addr_zeropage },
    { "ORA", addr_zeropage },
    { "ASL", addr_zeropage },
    { "???", addr_implied },
    { "PHP", addr_implied },        // 0x08
    { "ORA", addr_immediate },
    { "ASL", addr_accumulator },
    { "???", addr_implied },
    { "TSB", addr_absolute },
    { "ORA", addr_absolute },
    { "ASL", addr_absolute },
    { "???", addr_implied },
    { "BPL", addr_relative },       // 0x10
    { "ORA", addr_indirect_y },
    { "ORA", addr_indirect },
    { "???", addr_implied },
    { "TRB", addr_zeropage },
    { "ORA", addr_zeropage_x },
    { "ASL", addr_zeropage_x },
    { "???", addr_implied },
    { "CLC", addr_implied },        // 0x18
    { "ORA", addr_absolute_y },
    { "INC", addr_accumulator },
    { "???", addr_implied },
    { "TRB", addr_absolute },
    { "ORA", addr_absolute_x },
    { "ASL", addr_absolute_x },
    { "???", addr_implied },
    { "JSR", addr_absolute },       // 0x20
    { "AND", addr_indirect_x },
    { "???", addr_implied },
    { "???", addr_implied },
    { "BIT", addr_zeropage },
    { "AND", addr_zeropage },
    { "ROL", addr_zeropage },
    { "???", addr_implied },
    { "PLP", addr_implied },        // 0x28
    { "AND", addr_immediate },
    { "ROL", addr_accumulator },
    { "???", addr_implied },
    { "BIT", addr_absolute },
    { "AND", addr_absolute },
    { "ROL", addr_absolute },
    { "???", addr_implied },
    { "BMI", addr_relative },       // 0x30
    { "AND", addr_indirect_y },
    { "AND", addr_indirect },
    { "???", addr_implied },
    { "BIT", addr_zeropage_x },
    { "AND", addr_zeropage_x },
    { "ROL", addr_zeropage_x },
    { "???", addr_implied },
    { "SEC", addr_implied },        // 0x38
    { "AND", addr_absolute_y },
    { "DEC", addr_accumulator },
    { "???", addr_implied },
    { "BIT", addr_absolute_x },
    { "AND", addr_absolute_x },
    { "ROL", addr_absolute_x },
    { "???", addr_implied },
    { "RTI", addr_implied },        // 0x40
    { "EOR", addr_indirect_x },
    { "???", addr_implied },
    { "???", addr_implied },
    { "???", addr_implied },
    { "EOR", addr_zeropage },
    { "LSR", addr_zeropage },
    { "???", addr_implied },
    { "PHA", addr_implied },        // 0x48
    { "EOR", addr_immediate },
    { "LSR", addr_accumulator },
    { "???", addr_implied },
    { "JMP", addr_absolute },
    { "EOR", addr_absolute },
    { "LSR", addr_absolute },
    { "???", addr_implied },
    { "BVC", addr_relative },       // 0x50
    { "EOR", addr_indirect_y },
    { "EOR", addr_indirect },
    { "???", addr_implied },
    { "???", addr_implied },
    { "EOR", addr_zeropage_x },
    { "LSR", addr_zeropage_x },
    { "???", addr_implied },
    { "CLI", addr_implied },        // 0x58
    { "EOR", addr_absolute_y },
    { "PHY", addr_implied },
    { "???", addr_implied },
    { "???", addr_implied },
    { "EOR", addr_absolute_x },
    { "LSR", addr_absolute_x },
    { "???", addr_implied },
    { "RTS", addr_implied },        // 0x60
    { "ADC", addr_indirect_x },
    { "???", addr_implied },
    { "???", addr_implied },
    { "STZ", addr_zeropage },
    { "ADC", addr_zeropage },
    { "ROR", addr_zeropage },
    { "???", addr_implied },
    { "PLA", addr_implied },        // 0x68
    { "ADC", addr_immediate },
    { "ROR", addr_accumulator },
    { "???", addr_implied },
    { "JMP", addr_j_indirect },
    { "ADC", addr_absolute },
    { "ROR", addr_absolute },
    { "???", addr_implied },
    { "BVS", addr_relative },       // 0x70
    { "ADC", addr_indirect_y },
    { "ADC", addr_indirect },
    { "???", addr_implied },
    { "STZ", addr_zeropage_x },
    { "ADC", addr_zeropage_x },
    { "ROR", addr_zeropage_x },
    { "???", addr_implied },
    { "SEI", addr_implied },        // 0x78
    { "ADC", addr_absolute_y },
    { "PLY", addr_implied },
    { "???", addr_implied },
    { "JMP", addr_j_indirect_x },
    { "ADC", addr_absolute_x },
    { "ROR", addr_absolute_x },
    { "???", addr_implied },
    { "BRA", addr_relative },       // 0x80
    { "STA", addr_indirect_x },
    { "???", addr_implied },
    { "???", addr_implied },
    { "STY", addr_zeropage },
    { "STA", addr_zeropage },
    { "STX", addr_zeropage },
    { "???", addr_implied },
    { "DEY", addr_implied },        // 0x88
    { "BIT", addr_immediate },
    { "TXA", addr_implied },
    { "???", addr_implied },
    { "STY", addr_absolute },
    { "STA", addr_absolute },
    { "STX", addr_absolute },
    { "???", addr_implied },
    { "BCC", addr_relative },       // 0x90
    { "STA", addr_indirect_y },
    { "STA", addr_indirect },
    { "???", addr_implied },
    { "STY", addr_zeropage_x },
    { "STA", addr_zeropage_x },
    { "STX", addr_zeropage_y },
    { "???", addr_implied },
    { "TYA", addr_implied },        // 0x98
    { "STA", addr_absolute_y },
    { "TXS", addr_implied },
    { "???", addr_implied },
    { "STZ", addr_absolute },
    { "STA", addr_absolute_x },
    { "STZ", addr_absolute_x },
    { "???", addr_implied },
    { "LDY", addr_immediate },      // 0xA0
    { "LDA", addr_indirect_x },
    { "LDX", addr_immediate },
    { "???", addr_implied },
    { "LDY", addr_zeropage },
    { "LDA", addr_zeropage },
    { "LDX", addr_zeropage },
    { "???", addr_implied },
    { "TAY", addr_implied },        // 0xA8
    { "LDA", addr_immediate },
    { "TAX", addr_implied },
    { "???", addr_implied },
    { "LDY", addr_absolute },
    { "LDA", addr_absolute },
    { "LDX", addr_absolute },
    { "???", addr_implied },
    { "BCS", addr_relative },       // 0xB0
    { "LDA", addr_indirect_y },
    { "LDA", addr_indirect },
    { "???", addr_implied },
    { "LDY", addr_zeropage_x },
    { "LDA", addr_zeropage_x },
    { "LDX", addr_zeropage_y },
    { "???", addr_implied },
    { "CLV", addr_implied },        // 0xB8
    { "LDA", addr_absolute_y },
    { "TSX", addr_implied },
    { "???", addr_implied },
    { "LDY", addr_absolute_x },
    { "LDA", addr_absolute_x },
    { "LDX", addr_absolute_y },
    { "???", addr_implied },
    { "CPY", addr_immediate },      // 0xC0
    { "CMP", addr_indirect_x },
    { "???", addr_implied },
    { "???", addr_implied },
    { "CPY", addr_zeropage },
    { "CMP", addr_zeropage },
    { "DEC", addr_zeropage },
    { "???", addr_implied },
    { "INY", addr_implied },        // 0xC8
    { "CMP", addr_immediate },
    { "DEX", addr_implied },
    { "???", addr_implied },
    { "CPY", addr_absolute },
    { "CMP", addr_absolute },
    { "DEC", addr_absolute },
    { "???", addr_implied },
    { "BNE", addr_relative },       // 0xD0
    { "CMP", addr_indirect_y },
    { "CMP", addr_indirect },
    { "???", addr_implied },
    { "???", addr_implied },
    { "CMP", addr_zeropage_x },
    { "DEC", addr_zeropage_x },
    { "???", addr_implied },
    { "CLD", addr_implied },        // 0xD8
    { "CMP", addr_absolute_y },
    { "PHX", addr_implied },
    { "???", addr_implied },
    { "???", addr_implied },
    { "CMP", addr_absolute_x },
    { "DEC", addr_absolute_x },
    { "???", addr_implied },
    { "CPX", addr_immediate },      // 0xE0
    { "SBC", addr_indirect_x },
    { "???", addr_implied },
    { "???", addr_implied },
    { "CPX", addr_zeropage },
    { "SBC", addr_zeropage },
    { "INC", addr_zeropage },
    { "???", addr_implied },
    { "INX", addr_implied },        // 0xE8
    { "SBC", addr_immediate },
    { "NOP", addr_implied },
    { "???", addr_implied },
    { "CPX", addr_absolute },
    { "SBC", addr_absolute },
    { "INC", addr_absolute },
    { "???", addr_implied },
    { "BEQ", addr_relative },       // 0xF0
    { "SBC", addr_indirect_y },
    { "SBC", addr_indirect },
    { "???", addr_implied },
    { "???", addr_implied },
    { "SBC", addr_zeropage_x },
    { "INC", addr_zeropage_x },
    { "???", addr_implied },
    { "SED", addr_implied },        // 0xF8
    { "SBC", addr_absolute_y },
    { "PLX", addr_implied },
    { "???", addr_implied },
    { "???", addr_implied },
    { "SBC", addr_absolute_x },
    { "INC", addr_absolute_x },
    { "???", addr_implied },
};

const struct opcode_struct_s *opcodes = opcodes_65c02;

#if CPU_TRACING
const uint8_t opcodes_65c02_numargs[256] = {
    0, 1, 0, 0, 1, 1, 1, 0,   0, 1, 0, 0, 2, 2, 2, 0, // 0x00-0x0F
    1, 1, 1, 0, 1, 1, 1, 0,   0, 2, 0, 0, 2, 2, 2, 0, // 0x10-0x1F
    2, 1, 0, 0, 1, 1, 1, 0,   0, 1, 0, 0, 2, 2, 2, 0, // 0x20-0x2F
    1, 1, 1, 0, 1, 1, 1, 0,   0, 2, 0, 0, 2, 2, 2, 0, // 0x30-0x3F
    0, 1, 0, 0, 0, 1, 1, 0,   0, 1, 0, 0, 2, 2, 2, 0, // 0x40-0x4F
    1, 1, 1, 0, 0, 1, 1, 0,   0, 2, 0, 0, 0, 2, 2, 0, // 0x50-0x5F
    0, 1, 0, 0, 1, 1, 1, 0,   0, 1, 0, 0, 2, 2, 2, 0, // 0x60-0x6F
    1, 1, 1, 0, 1, 1, 1, 0,   0, 2, 0, 0, 2, 2, 2, 0, // 0x70-0x7F
    1, 1, 0, 0, 1, 1, 1, 0,   0, 1, 0, 0, 2, 2, 2, 0, // 0x80-0x8F
    1, 1, 1, 0, 1, 1, 1, 0,   0, 2, 0, 0, 2, 2, 2, 0, // 0x90-0x9F
    1, 1, 1, 0, 1, 1, 1, 0,   0, 1, 0, 0, 2, 2, 2, 0, // 0xA0-0xAF
    1, 1, 1, 0, 1, 1, 1, 0,   0, 2, 0, 0, 2, 2, 2, 0, // 0xB0-0xBF
    1, 1, 0, 0, 1, 1, 1, 0,   0, 1, 0, 0, 2, 2, 2, 0, // 0xC0-0xCF
    1, 1, 1, 0, 0, 1, 1, 0,   0, 2, 0, 0, 0, 2, 2, 0, // 0xD0-0xDF
    1, 1, 0, 0, 1, 1, 1, 0,   0, 1, 0, 0, 2, 2, 2, 0, // 0xE0-0xEF
    1, 1, 1, 0, 0, 1, 1, 0,   0, 2, 0, 0, 0, 2, 2, 0, // 0xF0-0xFF
};
#endif

#if 0
const struct opcode_struct_s opcodes_6502[256] = {
    { "BRK", addr_implied },        // 0x00
    { "ORA", addr_indirect_x },
    { "???", addr_implied },
    { "???", addr_implied },
    { "???", addr_implied },
    { "ORA", addr_zeropage },
    { "ASL", addr_zeropage },
    { "???", addr_implied },
    { "PHP", addr_implied },        // 0x08
    { "ORA", addr_immediate },
    { "ASL", addr_accumulator },
    { "???", addr_implied },
    { "???", addr_implied },
    { "ORA", addr_absolute },
    { "ASL", addr_absolute },
    { "???", addr_implied },
    { "BPL", addr_relative },       // 0x10
    { "ORA", addr_indirect_y },
    { "???", addr_implied },
    { "???", addr_implied },
    { "???", addr_implied },
    { "ORA", addr_zeropage_x },
    { "ASL", addr_zeropage_x },
    { "???", addr_implied },
    { "CLC", addr_implied },        // 0x18
    { "ORA", addr_absolute_y },
    { "???", addr_implied },
    { "???", addr_implied },
    { "???", addr_implied },
    { "ORA", addr_absolute_x },
    { "ASL", addr_absolute_x },
    { "???", addr_implied },
    { "JSR", addr_absolute },       // 0x20
    { "AND", addr_indirect_x },
    { "???", addr_implied },
    { "???", addr_implied },
    { "BIT", addr_zeropage },
    { "AND", addr_zeropage },
    { "ROL", addr_zeropage },
    { "???", addr_implied },
    { "PLP", addr_implied },        // 0x28
    { "AND", addr_immediate },
    { "ROL", addr_accumulator },
    { "???", addr_implied },
    { "BIT", addr_absolute },
    { "AND", addr_absolute },
    { "ROL", addr_absolute },
    { "???", addr_implied },
    { "BMI", addr_relative },       // 0x30
    { "AND", addr_indirect_y },
    { "???", addr_implied },
    { "???", addr_implied },
    { "???", addr_implied },
    { "AND", addr_zeropage_x },
    { "ROL", addr_zeropage_x },
    { "???", addr_implied },
    { "SEC", addr_implied },        // 0x38
    { "AND", addr_absolute_y },
    { "???", addr_implied },
    { "???", addr_implied },
    { "???", addr_implied },
    { "AND", addr_absolute_x },
    { "ROL", addr_absolute_x },
    { "???", addr_implied },
    { "RTI", addr_implied },        // 0x40
    { "EOR", addr_indirect_x },
    { "???", addr_implied },
    { "???", addr_implied },
    { "???", addr_implied },
    { "EOR", addr_zeropage },
    { "LSR", addr_zeropage },
    { "???", addr_implied },
    { "PHA", addr_implied },        // 0x48
    { "EOR", addr_immediate },
    { "LSR", addr_accumulator },
    { "???", addr_implied },
    { "JMP", addr_absolute },
    { "EOR", addr_absolute },
    { "LSR", addr_absolute },
    { "???", addr_implied },
    { "BVC", addr_relative },       // 0x50
    { "EOR", addr_indirect_y },
    { "???", addr_implied },
    { "???", addr_implied },
    { "???", addr_implied },
    { "EOR", addr_zeropage_x },
    { "LSR", addr_zeropage_x },
    { "???", addr_implied },
    { "CLI", addr_implied },        // 0x58
    { "EOR", addr_absolute_y },
    { "???", addr_implied },
    { "???", addr_implied },
    { "???", addr_implied },
    { "EOR", addr_absolute_x },
    { "LSR", addr_absolute_x },
    { "???", addr_implied },
    { "RTS", addr_implied },        // 0x60
    { "ADC", addr_indirect_x },
    { "???", addr_implied },
    { "???", addr_implied },
    { "???", addr_implied },
    { "ADC", addr_zeropage },
    { "ROR", addr_zeropage },
    { "???", addr_implied },
    { "PLA", addr_implied },        // 0x68
    { "ADC", addr_immediate },
    { "ROR", addr_accumulator },
    { "???", addr_implied },
    { "JMP", addr_j_indirect },
    { "ADC", addr_absolute },
    { "ROR", addr_absolute },
    { "???", addr_implied },
    { "BVS", addr_relative },       // 0x70
    { "ADC", addr_indirect_y },
    { "???", addr_implied },
    { "???", addr_implied },
    { "???", addr_implied },
    { "ADC", addr_zeropage_x },
    { "ROR", addr_zeropage_x },
    { "???", addr_implied },
    { "SEI", addr_implied },        // 0x78
    { "ADC", addr_absolute_y },
    { "???", addr_implied },
    { "???", addr_implied },
    { "???", addr_implied },
    { "ADC", addr_absolute_x },
    { "ROR", addr_absolute_x },
    { "???", addr_implied },
    { "???", addr_implied },        // 0x80
    { "STA", addr_indirect_x },
    { "???", addr_implied },
    { "???", addr_implied },
    { "STY", addr_zeropage },
    { "STA", addr_zeropage },
    { "STX", addr_zeropage },
    { "???", addr_implied },
    { "DEY", addr_implied },        // 0x88
    { "???", addr_implied },
    { "TXA", addr_implied },
    { "???", addr_implied },
    { "STY", addr_absolute },
    { "STA", addr_absolute },
    { "STX", addr_absolute },
    { "???", addr_implied },
    { "BCC", addr_relative },       // 0x90
    { "STA", addr_indirect_y },
    { "???", addr_implied },
    { "???", addr_implied },
    { "STY", addr_zeropage_x },
    { "STA", addr_zeropage_x },
    { "STX", addr_zeropage_y },
    { "???", addr_implied },
    { "TYA", addr_implied },        // 0x98
    { "STA", addr_absolute_y },
    { "TXS", addr_implied },
    { "???", addr_implied },
    { "???", addr_implied },
    { "STA", addr_absolute_x },
    { "???", addr_implied },
    { "???", addr_implied },
    { "LDY", addr_immediate },      // 0xA0
    { "LDA", addr_indirect_x },
    { "LDX", addr_immediate },
    { "???", addr_implied },
    { "LDY", addr_zeropage },
    { "LDA", addr_zeropage },
    { "LDX", addr_zeropage },
    { "???", addr_implied },
    { "TAY", addr_implied },        // 0xA8
    { "LDA", addr_immediate },
    { "TAX", addr_implied },
    { "???", addr_implied },
    { "LDY", addr_absolute },
    { "LDA", addr_absolute },
    { "LDX", addr_absolute },
    { "???", addr_implied },
    { "BCS", addr_relative },       // 0xB0
    { "LDA", addr_indirect_y },
    { "???", addr_implied },
    { "???", addr_implied },
    { "LDY", addr_zeropage_x },
    { "LDA", addr_zeropage_x },
    { "LDX", addr_zeropage_y },
    { "???", addr_implied },
    { "CLV", addr_implied },        // 0xB8
    { "LDA", addr_absolute_y },
    { "TSX", addr_implied },
    { "???", addr_implied },
    { "LDY", addr_absolute_x },
    { "LDA", addr_absolute_x },
    { "LDX", addr_absolute_y },
    { "???", addr_implied },
    { "CPY", addr_immediate },      // 0xC0
    { "CMP", addr_indirect_x },
    { "???", addr_implied },
    { "???", addr_implied },
    { "CPY", addr_zeropage },
    { "CMP", addr_zeropage },
    { "DEC", addr_zeropage },
    { "???", addr_implied },
    { "INY", addr_implied },        // 0xC8
    { "CMP", addr_immediate },
    { "DEX", addr_implied },
    { "???", addr_implied },
    { "CPY", addr_absolute },
    { "CMP", addr_absolute },
    { "DEC", addr_absolute },
    { "???", addr_implied },
    { "BNE", addr_relative },       // 0xD0
    { "CMP", addr_indirect_y },
    { "???", addr_implied },
    { "???", addr_implied },
    { "???", addr_implied },
    { "CMP", addr_zeropage_x },
    { "DEC", addr_zeropage_x },
    { "???", addr_implied },
    { "CLD", addr_implied },        // 0xD8
    { "CMP", addr_absolute_y },
    { "???", addr_implied },
    { "???", addr_implied },
    { "???", addr_implied },
    { "CMP", addr_absolute_x },
    { "DEC", addr_absolute_x },
    { "???", addr_implied },
    { "CPX", addr_immediate },      // 0xE0
    { "SBC", addr_indirect_x },
    { "???", addr_implied },
    { "???", addr_implied },
    { "CPX", addr_zeropage },
    { "SBC", addr_zeropage },
    { "INC", addr_zeropage },
    { "???", addr_implied },
    { "INX", addr_implied },        // 0xE8
    { "SBC", addr_immediate },
    { "NOP", addr_implied },
    { "???", addr_implied },
    { "CPX", addr_absolute },
    { "SBC", addr_absolute },
    { "INC", addr_absolute },
    { "???", addr_implied },
    { "BEQ", addr_relative },       // 0xF0
    { "SBC", addr_indirect_y },
    { "???", addr_implied },
    { "???", addr_implied },
    { "???", addr_implied },
    { "SBC", addr_zeropage_x },
    { "INC", addr_zeropage_x },
    { "???", addr_implied },
    { "SED", addr_implied },        // 0xF8
    { "SBC", addr_absolute_y },
    { "???", addr_implied },
    { "???", addr_implied },
    { "???", addr_implied },
    { "SBC", addr_absolute_x },
    { "INC", addr_absolute_x },
    { "???", addr_implied },
};

const struct opcode_struct_s opcodes_undoc[256] = {
    { "BRK", addr_implied },        // 0x00
    { "ORA", addr_indirect_x },
    { "hang", addr_implied },
    { "lor", addr_indirect_x },
    { "nop", addr_zeropage },
    { "ORA", addr_zeropage },
    { "ASL", addr_zeropage },
    { "lor", addr_zeropage },
    { "PHP", addr_implied },        // 0x08
    { "ORA", addr_immediate },
    { "ASL", addr_accumulator },
    { "ana", addr_immediate },
    { "nop", addr_absolute },
    { "ORA", addr_absolute },
    { "ASL", addr_absolute },
    { "lor", addr_absolute },
    { "BPL", addr_relative },       // 0x10
    { "ORA", addr_indirect_y },
    { "hang", addr_implied },
    { "lor", addr_indirect_y },
    { "nop", addr_zeropage_x },
    { "ORA", addr_zeropage_x },
    { "ASL", addr_zeropage_x },
    { "lor", addr_zeropage_x },
    { "CLC", addr_implied },        // 0x18
    { "ORA", addr_absolute_y },
    { "nop", addr_implied },
    { "lor", addr_absolute_y },
    { "nop", addr_absolute_x },
    { "ORA", addr_absolute_x },
    { "ASL", addr_absolute_x },
    { "lor", addr_absolute },
    { "JSR", addr_absolute },       // 0x20
    { "AND", addr_indirect_x },
    { "hang", addr_implied },
    { "lan", addr_indirect_x },
    { "BIT", addr_zeropage },
    { "AND", addr_zeropage },
    { "ROL", addr_zeropage },
    { "lan", addr_zeropage },
    { "PLP", addr_implied },        // 0x28
    { "AND", addr_immediate },
    { "ROL", addr_accumulator },
    { "anb", addr_immediate },
    { "BIT", addr_absolute },
    { "AND", addr_absolute },
    { "ROL", addr_absolute },
    { "lan", addr_absolute },
    { "BMI", addr_relative },       // 0x30
    { "AND", addr_indirect_y },
    { "hang", addr_implied },
    { "lan", addr_indirect_y },
    { "nop", addr_zeropage_x },
    { "AND", addr_zeropage_x },
    { "ROL", addr_zeropage_x },
    { "lan", addr_zeropage_x },
    { "SEC", addr_implied },        // 0x38
    { "AND", addr_absolute_y },
    { "nop", addr_implied },
    { "lan", addr_absolute_y },
    { "nop", addr_absolute_x },
    { "AND", addr_absolute_x },
    { "ROL", addr_absolute_x },
    { "lan", addr_absolute_x },
    { "RTI", addr_implied },        // 0x40
    { "EOR", addr_indirect_x },
    { "hang", addr_implied },
    { "reo", addr_indirect_x },
    { "nop", addr_zeropage },
    { "EOR", addr_zeropage },
    { "LSR", addr_zeropage },
    { "reo", addr_zeropage },
    { "PHA", addr_implied },        // 0x48
    { "EOR", addr_immediate },
    { "LSR", addr_accumulator },
    { "ram", addr_immediate },
    { "JMP", addr_absolute },
    { "EOR", addr_absolute },
    { "LSR", addr_absolute },
    { "reo", addr_absolute },
    { "BVC", addr_relative },       // 0x50
    { "EOR", addr_indirect_y },
    { "hang", addr_implied },
    { "reo", addr_indirect_y },
    { "nop", addr_zeropage_x },
    { "EOR", addr_zeropage_x },
    { "LSR", addr_zeropage_x },
    { "reo", addr_zeropage_x },
    { "CLI", addr_implied },        // 0x58
    { "EOR", addr_absolute_y },
    { "nop", addr_implied },
    { "reo", addr_absolute_y },
    { "nop", addr_absolute_x },
    { "EOR", addr_absolute_x },
    { "LSR", addr_absolute_x },
    { "reo", addr_absolute_x },
    { "RTS", addr_implied },        // 0x60
    { "ADC", addr_indirect_x },
    { "hang", addr_implied },
    { "rad", addr_indirect_x },
    { "nop", addr_zeropage },
    { "ADC", addr_zeropage },
    { "ROR", addr_zeropage },
    { "rad", addr_zeropage },
    { "PLA", addr_implied },        // 0x68
    { "ADC", addr_immediate },
    { "ROR", addr_accumulator },
    { "rbm", addr_immediate },
    { "JMP", addr_j_indirect },
    { "ADC", addr_absolute },
    { "ROR", addr_absolute },
    { "rad", addr_absolute },
    { "BVS", addr_relative },       // 0x70
    { "ADC", addr_indirect_y },
    { "hang", addr_implied },
    { "rad", addr_indirect_y },
    { "nop", addr_zeropage_x },
    { "ADC", addr_zeropage_x },
    { "ROR", addr_zeropage_x },
    { "rad", addr_zeropage_x },
    { "SEI", addr_implied },        // 0x78
    { "ADC", addr_absolute_y },
    { "nop", addr_implied },
    { "rad", addr_absolute_y },
    { "nop", addr_absolute_x },
    { "ADC", addr_absolute_x },
    { "ROR", addr_absolute_x },
    { "rad", addr_absolute_x },
    { "nop", addr_immediate },      // 0x80
    { "STA", addr_indirect_x },
    { "nop", addr_immediate },
    { "aax", addr_indirect_x },
    { "STY", addr_zeropage },
    { "STA", addr_zeropage },
    { "STX", addr_zeropage },
    { "aax", addr_zeropage },
    { "DEY", addr_implied },        // 0x88
    { "nop", addr_immediate },
    { "TXA", addr_implied },
    { "xma", addr_immediate },
    { "STY", addr_absolute },
    { "STA", addr_absolute },
    { "STX", addr_absolute },
    { "aax", addr_absolute },
    { "BCC", addr_relative },       // 0x90
    { "STA", addr_indirect_y },
    { "hang", addr_implied },
    { "aax", addr_indirect_y },
    { "STY", addr_zeropage_x },
    { "STA", addr_zeropage_x },
    { "STX", addr_zeropage_y },
    { "aax", addr_zeropage_y },
    { "TYA", addr_implied },        // 0x98
    { "STA", addr_absolute_y },
    { "TXS", addr_implied },
    { "axs", addr_absolute_y },
    { "tey", addr_absolute_x },
    { "STA", addr_absolute_x },
    { "tex", addr_absolute_y },
    { "tea", addr_absolute_y },
    { "LDY", addr_immediate },      // 0xA0
    { "LDA", addr_indirect_x },
    { "LDX", addr_immediate },
    { "lax", addr_indirect_x },
    { "LDY", addr_zeropage },
    { "LDA", addr_zeropage },
    { "LDX", addr_zeropage },
    { "lax", addr_zeropage },
    { "TAY", addr_implied },        // 0xA8
    { "LDA", addr_immediate },
    { "TAX", addr_implied },
    { "ama", addr_immediate },
    { "LDY", addr_absolute },
    { "LDA", addr_absolute },
    { "LDX", addr_absolute },
    { "lax", addr_absolute },
    { "BCS", addr_relative },       // 0xB0
    { "LDA", addr_indirect_y },
    { "hang", addr_implied },
    { "lax", addr_indirect_y },
    { "LDY", addr_zeropage_x },
    { "LDA", addr_zeropage_x },
    { "LDX", addr_zeropage_y },
    { "laz", addr_zeropage_y },
    { "CLV", addr_implied },        // 0xB8
    { "LDA", addr_absolute_y },
    { "TSX", addr_implied },
    { "las", addr_absolute_y },
    { "LDY", addr_absolute_x },
    { "LDA", addr_absolute_x },
    { "LDX", addr_absolute_y },
    { "lax", addr_absolute_y },
    { "CPY", addr_immediate },      // 0xC0
    { "CMP", addr_indirect_x },
    { "nop", addr_immediate },
    { "dcp", addr_indirect_x },
    { "CPY", addr_zeropage },
    { "CMP", addr_zeropage },
    { "DEC", addr_zeropage },
    { "dcp", addr_zeropage },
    { "INY", addr_implied },        // 0xC8
    { "CMP", addr_immediate },
    { "DEX", addr_implied },
    { "axm", addr_immediate },
    { "CPY", addr_absolute },
    { "CMP", addr_absolute },
    { "DEC", addr_absolute },
    { "dcp", addr_absolute },
    { "BNE", addr_relative },       // 0xD0
    { "CMP", addr_indirect_y },
    { "hang", addr_implied },
    { "dcp", addr_indirect_y },
    { "nop", addr_zeropage_x },
    { "CMP", addr_zeropage_x },
    { "DEC", addr_zeropage_x },
    { "dcp", addr_zeropage_x },
    { "CLD", addr_implied },        // 0xD8
    { "CMP", addr_absolute_y },
    { "nop", addr_implied },
    { "dcp", addr_absolute_y },
    { "nop", addr_absolute_x },
    { "CMP", addr_absolute_x },
    { "DEC", addr_absolute_x },
    { "dcp", addr_absolute_x },
    { "CPX", addr_immediate },      // 0xE0
    { "SBC", addr_indirect_x },
    { "nop", addr_immediate },
    { "isb", addr_indirect_x },
    { "CPX", addr_zeropage },
    { "SBC", addr_zeropage },
    { "INC", addr_zeropage },
    { "isb", addr_zeropage },
    { "INX", addr_implied },        // 0xE8
    { "SBC", addr_immediate },
    { "NOP", addr_implied },
    { "zbc", addr_immediate },
    { "CPX", addr_absolute },
    { "SBC", addr_absolute },
    { "INC", addr_absolute },
    { "isb", addr_absolute },
    { "BEQ", addr_relative },       // 0xF0
    { "SBC", addr_indirect_y },
    { "hang", addr_implied },
    { "isb", addr_indirect_y },
    { "nop", addr_zeropage_x },
    { "SBC", addr_zeropage_x },
    { "INC", addr_zeropage_x },
    { "isb", addr_zeropage_x },
    { "SED", addr_implied },        // 0xF8
    { "SBC", addr_absolute_y },
    { "nop", addr_implied },
    { "isb", addr_absolute_y },
    { "nop", addr_absolute_x },
    { "SBC", addr_absolute_x },
    { "INC", addr_absolute_x },
    { "isb", addr_absolute_x },
};

#endif
