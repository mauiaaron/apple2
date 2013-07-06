/*
 * Apple // emulator for Linux: Opcode tables for debugger
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

#include "debug.h"

const char * const disasm_templates[15] =
{
    "",
    "A",
    "#$%02X",
    "$%02X",
    "$%02X,X",
    "$%02X,Y",
    "$%02X%02X",
    "$%02X%02X,X",
    "$%02X%02X,Y",
    "($%02X)",
    "($%02X,X)",
    "($%02X),Y",
    "($%02X%02X)",
    "($%02X%02X),X",
    "$%04X (%c%02X)"
};

const struct opcode_struct opcodes_6502[256] =
{
    { "BRK", addr_implied },
    { "ORA", addr_indirect_x },
    { "???", addr_implied },
    { "???", addr_implied },
    { "???", addr_implied },
    { "ORA", addr_zeropage },
    { "ASL", addr_zeropage },
    { "???", addr_implied },
    { "PHP", addr_implied },
    { "ORA", addr_immediate },
    { "ASL", addr_accumulator },
    { "???", addr_implied },
    { "???", addr_implied },
    { "ORA", addr_absolute },
    { "ASL", addr_absolute },
    { "???", addr_implied },
    { "BPL", addr_relative },
    { "ORA", addr_indirect_y },
    { "???", addr_implied },
    { "???", addr_implied },
    { "???", addr_implied },
    { "ORA", addr_zeropage_x },
    { "ASL", addr_zeropage_x },
    { "???", addr_implied },
    { "CLC", addr_implied },
    { "ORA", addr_absolute_y },
    { "???", addr_implied },
    { "???", addr_implied },
    { "???", addr_implied },
    { "ORA", addr_absolute_x },
    { "ASL", addr_absolute_x },
    { "???", addr_implied },
    { "JSR", addr_absolute },
    { "AND", addr_indirect_x },
    { "???", addr_implied },
    { "???", addr_implied },
    { "BIT", addr_zeropage },
    { "AND", addr_zeropage },
    { "ROL", addr_zeropage },
    { "???", addr_implied },
    { "PLP", addr_implied },
    { "AND", addr_immediate },
    { "ROL", addr_accumulator },
    { "???", addr_implied },
    { "BIT", addr_absolute },
    { "AND", addr_absolute },
    { "ROL", addr_absolute },
    { "???", addr_implied },
    { "BMI", addr_relative },
    { "AND", addr_indirect_y },
    { "???", addr_implied },
    { "???", addr_implied },
    { "???", addr_implied },
    { "AND", addr_zeropage_x },
    { "ROL", addr_zeropage_x },
    { "???", addr_implied },
    { "SEC", addr_implied },
    { "AND", addr_absolute_y },
    { "???", addr_implied },
    { "???", addr_implied },
    { "???", addr_implied },
    { "AND", addr_absolute_x },
    { "ROL", addr_absolute_x },
    { "???", addr_implied },
    { "RTI", addr_implied },
    { "EOR", addr_indirect_x },
    { "???", addr_implied },
    { "???", addr_implied },
    { "???", addr_implied },
    { "EOR", addr_zeropage },
    { "LSR", addr_zeropage },
    { "???", addr_implied },
    { "PHA", addr_implied },
    { "EOR", addr_immediate },
    { "LSR", addr_accumulator },
    { "???", addr_implied },
    { "JMP", addr_absolute },
    { "EOR", addr_absolute },
    { "LSR", addr_absolute },
    { "???", addr_implied },
    { "BVC", addr_relative },
    { "EOR", addr_indirect_y },
    { "???", addr_implied },
    { "???", addr_implied },
    { "???", addr_implied },
    { "EOR", addr_zeropage_x },
    { "LSR", addr_zeropage_x },
    { "???", addr_implied },
    { "CLI", addr_implied },
    { "EOR", addr_absolute_y },
    { "???", addr_implied },
    { "???", addr_implied },
    { "???", addr_implied },
    { "EOR", addr_absolute_x },
    { "LSR", addr_absolute_x },
    { "???", addr_implied },
    { "RTS", addr_implied },
    { "ADC", addr_indirect_x },
    { "???", addr_implied },
    { "???", addr_implied },
    { "???", addr_implied },
    { "ADC", addr_zeropage },
    { "ROR", addr_zeropage },
    { "???", addr_implied },
    { "PLA", addr_implied },
    { "ADC", addr_immediate },
    { "ROR", addr_accumulator },
    { "???", addr_implied },
    { "JMP", addr_j_indirect },
    { "ADC", addr_absolute },
    { "ROR", addr_absolute },
    { "???", addr_implied },
    { "BVS", addr_relative },
    { "ADC", addr_indirect_y },
    { "???", addr_implied },
    { "???", addr_implied },
    { "???", addr_implied },
    { "ADC", addr_zeropage_x },
    { "ROR", addr_zeropage_x },
    { "???", addr_implied },
    { "SEI", addr_implied },
    { "ADC", addr_absolute_y },
    { "???", addr_implied },
    { "???", addr_implied },
    { "???", addr_implied },
    { "ADC", addr_absolute_x },
    { "ROR", addr_absolute_x },
    { "???", addr_implied },
    { "???", addr_implied },
    { "STA", addr_indirect_x },
    { "???", addr_implied },
    { "???", addr_implied },
    { "STY", addr_zeropage },
    { "STA", addr_zeropage },
    { "STX", addr_zeropage },
    { "???", addr_implied },
    { "DEY", addr_implied },
    { "???", addr_implied },
    { "TXA", addr_implied },
    { "???", addr_implied },
    { "STY", addr_absolute },
    { "STA", addr_absolute },
    { "STX", addr_absolute },
    { "???", addr_implied },
    { "BCC", addr_relative },
    { "STA", addr_indirect_y },
    { "???", addr_implied },
    { "???", addr_implied },
    { "STY", addr_zeropage_x },
    { "STA", addr_zeropage_x },
    { "STX", addr_zeropage_y },
    { "???", addr_implied },
    { "TYA", addr_implied },
    { "STA", addr_absolute_y },
    { "TXS", addr_implied },
    { "???", addr_implied },
    { "???", addr_implied },
    { "STA", addr_absolute_x },
    { "???", addr_implied },
    { "???", addr_implied },
    { "LDY", addr_immediate },
    { "LDA", addr_indirect_x },
    { "LDX", addr_immediate },
    { "???", addr_implied },
    { "LDY", addr_zeropage },
    { "LDA", addr_zeropage },
    { "LDX", addr_zeropage },
    { "???", addr_implied },
    { "TAY", addr_implied },
    { "LDA", addr_immediate },
    { "TAX", addr_implied },
    { "???", addr_implied },
    { "LDY", addr_absolute },
    { "LDA", addr_absolute },
    { "LDX", addr_absolute },
    { "???", addr_implied },
    { "BCS", addr_relative },
    { "LDA", addr_indirect_y },
    { "???", addr_implied },
    { "???", addr_implied },
    { "LDY", addr_zeropage_x },
    { "LDA", addr_zeropage_x },
    { "LDX", addr_zeropage_y },
    { "???", addr_implied },
    { "CLV", addr_implied },
    { "LDA", addr_absolute_y },
    { "TSX", addr_implied },
    { "???", addr_implied },
    { "LDY", addr_absolute_x },
    { "LDA", addr_absolute_x },
    { "LDX", addr_absolute_y },
    { "???", addr_implied },
    { "CPY", addr_immediate },
    { "CMP", addr_indirect_x },
    { "???", addr_implied },
    { "???", addr_implied },
    { "CPY", addr_zeropage },
    { "CMP", addr_zeropage },
    { "DEC", addr_zeropage },
    { "???", addr_implied },
    { "INY", addr_implied },
    { "CMP", addr_immediate },
    { "DEX", addr_implied },
    { "???", addr_implied },
    { "CPY", addr_absolute },
    { "CMP", addr_absolute },
    { "DEC", addr_absolute },
    { "???", addr_implied },
    { "BNE", addr_relative },
    { "CMP", addr_indirect_y },
    { "???", addr_implied },
    { "???", addr_implied },
    { "???", addr_implied },
    { "CMP", addr_zeropage_x },
    { "DEC", addr_zeropage_x },
    { "???", addr_implied },
    { "CLD", addr_implied },
    { "CMP", addr_absolute_y },
    { "???", addr_implied },
    { "???", addr_implied },
    { "???", addr_implied },
    { "CMP", addr_absolute_x },
    { "DEC", addr_absolute_x },
    { "???", addr_implied },
    { "CPX", addr_immediate },
    { "SBC", addr_indirect_x },
    { "???", addr_implied },
    { "???", addr_implied },
    { "CPX", addr_zeropage },
    { "SBC", addr_zeropage },
    { "INC", addr_zeropage },
    { "???", addr_implied },
    { "INX", addr_implied },
    { "SBC", addr_immediate },
    { "NOP", addr_implied },
    { "???", addr_implied },
    { "CPX", addr_absolute },
    { "SBC", addr_absolute },
    { "INC", addr_absolute },
    { "???", addr_implied },
    { "BEQ", addr_relative },
    { "SBC", addr_indirect_y },
    { "???", addr_implied },
    { "???", addr_implied },
    { "???", addr_implied },
    { "SBC", addr_zeropage_x },
    { "INC", addr_zeropage_x },
    { "???", addr_implied },
    { "SED", addr_implied },
    { "SBC", addr_absolute_y },
    { "???", addr_implied },
    { "???", addr_implied },
    { "???", addr_implied },
    { "SBC", addr_absolute_x },
    { "INC", addr_absolute_x },
    { "???", addr_implied },
};

#ifdef APPLE_IIE

const struct opcode_struct opcodes_65c02[256] =
{
    { "BRK", addr_implied },
    { "ORA", addr_indirect_x },
    { "???", addr_implied },
    { "???", addr_implied },
    { "TSB", addr_zeropage },
    { "ORA", addr_zeropage },
    { "ASL", addr_zeropage },
    { "???", addr_implied },
    { "PHP", addr_implied },
    { "ORA", addr_immediate },
    { "ASL", addr_accumulator },
    { "???", addr_implied },
    { "TSB", addr_absolute },
    { "ORA", addr_absolute },
    { "ASL", addr_absolute },
    { "???", addr_implied },
    { "BPL", addr_relative },
    { "ORA", addr_indirect_y },
    { "ORA", addr_indirect },
    { "???", addr_implied },
    { "TRB", addr_zeropage },
    { "ORA", addr_zeropage_x },
    { "ASL", addr_zeropage_x },
    { "???", addr_implied },
    { "CLC", addr_implied },
    { "ORA", addr_absolute_y },
    { "INC", addr_accumulator },
    { "???", addr_implied },
    { "TRB", addr_absolute },
    { "ORA", addr_absolute_x },
    { "ASL", addr_absolute_x },
    { "???", addr_implied },
    { "JSR", addr_absolute },
    { "AND", addr_indirect_x },
    { "???", addr_implied },
    { "???", addr_implied },
    { "BIT", addr_zeropage },
    { "AND", addr_zeropage },
    { "ROL", addr_zeropage },
    { "???", addr_implied },
    { "PLP", addr_implied },
    { "AND", addr_immediate },
    { "ROL", addr_accumulator },
    { "???", addr_implied },
    { "BIT", addr_absolute },
    { "AND", addr_absolute },
    { "ROL", addr_absolute },
    { "???", addr_implied },
    { "BMI", addr_relative },
    { "AND", addr_indirect_y },
    { "AND", addr_indirect },
    { "???", addr_implied },
    { "BIT", addr_zeropage_x },
    { "AND", addr_zeropage_x },
    { "ROL", addr_zeropage_x },
    { "???", addr_implied },
    { "SEC", addr_implied },
    { "AND", addr_absolute_y },
    { "DEC", addr_accumulator },
    { "???", addr_implied },
    { "BIT", addr_absolute_x },
    { "AND", addr_absolute_x },
    { "ROL", addr_absolute_x },
    { "???", addr_implied },
    { "RTI", addr_implied },
    { "EOR", addr_indirect_x },
    { "???", addr_implied },
    { "???", addr_implied },
    { "???", addr_implied },
    { "EOR", addr_zeropage },
    { "LSR", addr_zeropage },
    { "???", addr_implied },
    { "PHA", addr_implied },
    { "EOR", addr_immediate },
    { "LSR", addr_accumulator },
    { "???", addr_implied },
    { "JMP", addr_absolute },
    { "EOR", addr_absolute },
    { "LSR", addr_absolute },
    { "???", addr_implied },
    { "BVC", addr_relative },
    { "EOR", addr_indirect_y },
    { "EOR", addr_indirect },
    { "???", addr_implied },
    { "???", addr_implied },
    { "EOR", addr_zeropage_x },
    { "LSR", addr_zeropage_x },
    { "???", addr_implied },
    { "CLI", addr_implied },
    { "EOR", addr_absolute_y },
    { "PHY", addr_implied },
    { "???", addr_implied },
    { "???", addr_implied },
    { "EOR", addr_absolute_x },
    { "LSR", addr_absolute_x },
    { "???", addr_implied },
    { "RTS", addr_implied },
    { "ADC", addr_indirect_x },
    { "???", addr_implied },
    { "???", addr_implied },
    { "STZ", addr_zeropage },
    { "ADC", addr_zeropage },
    { "ROR", addr_zeropage },
    { "???", addr_implied },
    { "PLA", addr_implied },
    { "ADC", addr_immediate },
    { "ROR", addr_accumulator },
    { "???", addr_implied },
    { "JMP", addr_j_indirect },
    { "ADC", addr_absolute },
    { "ROR", addr_absolute },
    { "???", addr_implied },
    { "BVS", addr_relative },
    { "ADC", addr_indirect_y },
    { "ADC", addr_indirect },
    { "???", addr_implied },
    { "STZ", addr_zeropage_x },
    { "ADC", addr_zeropage_x },
    { "ROR", addr_zeropage_x },
    { "???", addr_implied },
    { "SEI", addr_implied },
    { "ADC", addr_absolute_y },
    { "PLY", addr_implied },
    { "???", addr_implied },
    { "JMP", addr_j_indirect_x },
    { "ADC", addr_absolute_x },
    { "ROR", addr_absolute_x },
    { "???", addr_implied },
    { "BRA", addr_relative },
    { "STA", addr_indirect_x },
    { "???", addr_implied },
    { "???", addr_implied },
    { "STY", addr_zeropage },
    { "STA", addr_zeropage },
    { "STX", addr_zeropage },
    { "???", addr_implied },
    { "DEY", addr_implied },
    { "BIT", addr_immediate },
    { "TXA", addr_implied },
    { "???", addr_implied },
    { "STY", addr_absolute },
    { "STA", addr_absolute },
    { "STX", addr_absolute },
    { "???", addr_implied },
    { "BCC", addr_relative },
    { "STA", addr_indirect_y },
    { "STA", addr_indirect },
    { "???", addr_implied },
    { "STY", addr_zeropage_x },
    { "STA", addr_zeropage_x },
    { "STX", addr_zeropage_y },
    { "???", addr_implied },
    { "TYA", addr_implied },
    { "STA", addr_absolute_y },
    { "TXS", addr_implied },
    { "???", addr_implied },
    { "STZ", addr_absolute },
    { "STA", addr_absolute_x },
    { "STZ", addr_absolute_x },
    { "???", addr_implied },
    { "LDY", addr_immediate },
    { "LDA", addr_indirect_x },
    { "LDX", addr_immediate },
    { "???", addr_implied },
    { "LDY", addr_zeropage },
    { "LDA", addr_zeropage },
    { "LDX", addr_zeropage },
    { "???", addr_implied },
    { "TAY", addr_implied },
    { "LDA", addr_immediate },
    { "TAX", addr_implied },
    { "???", addr_implied },
    { "LDY", addr_absolute },
    { "LDA", addr_absolute },
    { "LDX", addr_absolute },
    { "???", addr_implied },
    { "BCS", addr_relative },
    { "LDA", addr_indirect_y },
    { "LDA", addr_indirect },
    { "???", addr_implied },
    { "LDY", addr_zeropage_x },
    { "LDA", addr_zeropage_x },
    { "LDX", addr_zeropage_y },
    { "???", addr_implied },
    { "CLV", addr_implied },
    { "LDA", addr_absolute_y },
    { "TSX", addr_implied },
    { "???", addr_implied },
    { "LDY", addr_absolute_x },
    { "LDA", addr_absolute_x },
    { "LDX", addr_absolute_y },
    { "???", addr_implied },
    { "CPY", addr_immediate },
    { "CMP", addr_indirect_x },
    { "???", addr_implied },
    { "???", addr_implied },
    { "CPY", addr_zeropage },
    { "CMP", addr_zeropage },
    { "DEC", addr_zeropage },
    { "???", addr_implied },
    { "INY", addr_implied },
    { "CMP", addr_immediate },
    { "DEX", addr_implied },
    { "???", addr_implied },
    { "CPY", addr_absolute },
    { "CMP", addr_absolute },
    { "DEC", addr_absolute },
    { "???", addr_implied },
    { "BNE", addr_relative },
    { "CMP", addr_indirect_y },
    { "CMP", addr_indirect },
    { "???", addr_implied },
    { "???", addr_implied },
    { "CMP", addr_zeropage_x },
    { "DEC", addr_zeropage_x },
    { "???", addr_implied },
    { "CLD", addr_implied },
    { "CMP", addr_absolute_y },
    { "PHX", addr_implied },
    { "???", addr_implied },
    { "???", addr_implied },
    { "CMP", addr_absolute_x },
    { "DEC", addr_absolute_x },
    { "???", addr_implied },
    { "CPX", addr_immediate },
    { "SBC", addr_indirect_x },
    { "???", addr_implied },
    { "???", addr_implied },
    { "CPX", addr_zeropage },
    { "SBC", addr_zeropage },
    { "INC", addr_zeropage },
    { "???", addr_implied },
    { "INX", addr_implied },
    { "SBC", addr_immediate },
    { "NOP", addr_implied },
    { "???", addr_implied },
    { "CPX", addr_absolute },
    { "SBC", addr_absolute },
    { "INC", addr_absolute },
    { "???", addr_implied },
    { "BEQ", addr_relative },
    { "SBC", addr_indirect_y },
    { "SBC", addr_indirect },
    { "???", addr_implied },
    { "???", addr_implied },
    { "SBC", addr_zeropage_x },
    { "INC", addr_zeropage_x },
    { "???", addr_implied },
    { "SED", addr_implied },
    { "SBC", addr_absolute_y },
    { "PLX", addr_implied },
    { "???", addr_implied },
    { "???", addr_implied },
    { "SBC", addr_absolute_x },
    { "INC", addr_absolute_x },
    { "???", addr_implied },
};

#endif /* APPLE_IIE */

const struct opcode_struct opcodes_undoc[256] =
{
    { "BRK", addr_implied },
    { "ORA", addr_indirect_x },
    { "hang", addr_implied },
    { "lor", addr_indirect_x },
    { "nop", addr_zeropage },
    { "ORA", addr_zeropage },
    { "ASL", addr_zeropage },
    { "lor", addr_zeropage },
    { "PHP", addr_implied },
    { "ORA", addr_immediate },
    { "ASL", addr_accumulator },
    { "ana", addr_immediate },
    { "nop", addr_absolute },
    { "ORA", addr_absolute },
    { "ASL", addr_absolute },
    { "lor", addr_absolute },
    { "BPL", addr_relative },
    { "ORA", addr_indirect_y },
    { "hang", addr_implied },
    { "lor", addr_indirect_y },
    { "nop", addr_zeropage_x },
    { "ORA", addr_zeropage_x },
    { "ASL", addr_zeropage_x },
    { "lor", addr_zeropage_x },
    { "CLC", addr_implied },
    { "ORA", addr_absolute_y },
    { "nop", addr_implied },
    { "lor", addr_absolute_y },
    { "nop", addr_absolute_x },
    { "ORA", addr_absolute_x },
    { "ASL", addr_absolute_x },
    { "lor", addr_absolute },
    { "JSR", addr_absolute },
    { "AND", addr_indirect_x },
    { "hang", addr_implied },
    { "lan", addr_indirect_x },
    { "BIT", addr_zeropage },
    { "AND", addr_zeropage },
    { "ROL", addr_zeropage },
    { "lan", addr_zeropage },
    { "PLP", addr_implied },
    { "AND", addr_immediate },
    { "ROL", addr_accumulator },
    { "anb", addr_immediate },
    { "BIT", addr_absolute },
    { "AND", addr_absolute },
    { "ROL", addr_absolute },
    { "lan", addr_absolute },
    { "BMI", addr_relative },
    { "AND", addr_indirect_y },
    { "hang", addr_implied },
    { "lan", addr_indirect_y },
    { "nop", addr_zeropage_x },
    { "AND", addr_zeropage_x },
    { "ROL", addr_zeropage_x },
    { "lan", addr_zeropage_x },
    { "SEC", addr_implied },
    { "AND", addr_absolute_y },
    { "nop", addr_implied },
    { "lan", addr_absolute_y },
    { "nop", addr_absolute_x },
    { "AND", addr_absolute_x },
    { "ROL", addr_absolute_x },
    { "lan", addr_absolute_x },
    { "RTI", addr_implied },
    { "EOR", addr_indirect_x },
    { "hang", addr_implied },
    { "reo", addr_indirect_x },
    { "nop", addr_zeropage },
    { "EOR", addr_zeropage },
    { "LSR", addr_zeropage },
    { "reo", addr_zeropage },
    { "PHA", addr_implied },
    { "EOR", addr_immediate },
    { "LSR", addr_accumulator },
    { "ram", addr_immediate },
    { "JMP", addr_absolute },
    { "EOR", addr_absolute },
    { "LSR", addr_absolute },
    { "reo", addr_absolute },
    { "BVC", addr_relative },
    { "EOR", addr_indirect_y },
    { "hang", addr_implied },
    { "reo", addr_indirect_y },
    { "nop", addr_zeropage_x },
    { "EOR", addr_zeropage_x },
    { "LSR", addr_zeropage_x },
    { "reo", addr_zeropage_x },
    { "CLI", addr_implied },
    { "EOR", addr_absolute_y },
    { "nop", addr_implied },
    { "reo", addr_absolute_y },
    { "nop", addr_absolute_x },
    { "EOR", addr_absolute_x },
    { "LSR", addr_absolute_x },
    { "reo", addr_absolute_x },
    { "RTS", addr_implied },
    { "ADC", addr_indirect_x },
    { "hang", addr_implied },
    { "rad", addr_indirect_x },
    { "nop", addr_zeropage },
    { "ADC", addr_zeropage },
    { "ROR", addr_zeropage },
    { "rad", addr_zeropage },
    { "PLA", addr_implied },
    { "ADC", addr_immediate },
    { "ROR", addr_accumulator },
    { "rbm", addr_immediate },
    { "JMP", addr_j_indirect },
    { "ADC", addr_absolute },
    { "ROR", addr_absolute },
    { "rad", addr_absolute },
    { "BVS", addr_relative },
    { "ADC", addr_indirect_y },
    { "hang", addr_implied },
    { "rad", addr_indirect_y },
    { "nop", addr_zeropage_x },
    { "ADC", addr_zeropage_x },
    { "ROR", addr_zeropage_x },
    { "rad", addr_zeropage_x },
    { "SEI", addr_implied },
    { "ADC", addr_absolute_y },
    { "nop", addr_implied },
    { "rad", addr_absolute_y },
    { "nop", addr_absolute_x },
    { "ADC", addr_absolute_x },
    { "ROR", addr_absolute_x },
    { "rad", addr_absolute_x },
    { "nop", addr_immediate },
    { "STA", addr_indirect_x },
    { "nop", addr_immediate },
    { "aax", addr_indirect_x },
    { "STY", addr_zeropage },
    { "STA", addr_zeropage },
    { "STX", addr_zeropage },
    { "aax", addr_zeropage },
    { "DEY", addr_implied },
    { "nop", addr_immediate },
    { "TXA", addr_implied },
    { "xma", addr_immediate },
    { "STY", addr_absolute },
    { "STA", addr_absolute },
    { "STX", addr_absolute },
    { "aax", addr_absolute },
    { "BCC", addr_relative },
    { "STA", addr_indirect_y },
    { "hang", addr_implied },
    { "aax", addr_indirect_y },
    { "STY", addr_zeropage_x },
    { "STA", addr_zeropage_x },
    { "STX", addr_zeropage_y },
    { "aax", addr_zeropage_y },
    { "TYA", addr_implied },
    { "STA", addr_absolute_y },
    { "TXS", addr_implied },
    { "axs", addr_absolute_y },
    { "tey", addr_absolute_x },
    { "STA", addr_absolute_x },
    { "tex", addr_absolute_y },
    { "tea", addr_absolute_y },
    { "LDY", addr_immediate },
    { "LDA", addr_indirect_x },
    { "LDX", addr_immediate },
    { "lax", addr_indirect_x },
    { "LDY", addr_zeropage },
    { "LDA", addr_zeropage },
    { "LDX", addr_zeropage },
    { "lax", addr_zeropage },
    { "TAY", addr_implied },
    { "LDA", addr_immediate },
    { "TAX", addr_implied },
    { "ama", addr_immediate },
    { "LDY", addr_absolute },
    { "LDA", addr_absolute },
    { "LDX", addr_absolute },
    { "lax", addr_absolute },
    { "BCS", addr_relative },
    { "LDA", addr_indirect_y },
    { "hang", addr_implied },
    { "lax", addr_indirect_y },
    { "LDY", addr_zeropage_x },
    { "LDA", addr_zeropage_x },
    { "LDX", addr_zeropage_y },
    { "laz", addr_zeropage_y },
    { "CLV", addr_implied },
    { "LDA", addr_absolute_y },
    { "TSX", addr_implied },
    { "las", addr_absolute_y },
    { "LDY", addr_absolute_x },
    { "LDA", addr_absolute_x },
    { "LDX", addr_absolute_y },
    { "lax", addr_absolute_y },
    { "CPY", addr_immediate },
    { "CMP", addr_indirect_x },
    { "nop", addr_immediate },
    { "dcp", addr_indirect_x },
    { "CPY", addr_zeropage },
    { "CMP", addr_zeropage },
    { "DEC", addr_zeropage },
    { "dcp", addr_zeropage },
    { "INY", addr_implied },
    { "CMP", addr_immediate },
    { "DEX", addr_implied },
    { "axm", addr_immediate },
    { "CPY", addr_absolute },
    { "CMP", addr_absolute },
    { "DEC", addr_absolute },
    { "dcp", addr_absolute },
    { "BNE", addr_relative },
    { "CMP", addr_indirect_y },
    { "hang", addr_implied },
    { "dcp", addr_indirect_y },
    { "nop", addr_zeropage_x },
    { "CMP", addr_zeropage_x },
    { "DEC", addr_zeropage_x },
    { "dcp", addr_zeropage_x },
    { "CLD", addr_implied },
    { "CMP", addr_absolute_y },
    { "nop", addr_implied },
    { "dcp", addr_absolute_y },
    { "nop", addr_absolute_x },
    { "CMP", addr_absolute_x },
    { "DEC", addr_absolute_x },
    { "dcp", addr_absolute_x },
    { "CPX", addr_immediate },
    { "SBC", addr_indirect_x },
    { "nop", addr_immediate },
    { "isb", addr_indirect_x },
    { "CPX", addr_zeropage },
    { "SBC", addr_zeropage },
    { "INC", addr_zeropage },
    { "isb", addr_zeropage },
    { "INX", addr_implied },
    { "SBC", addr_immediate },
    { "NOP", addr_implied },
    { "zbc", addr_immediate },
    { "CPX", addr_absolute },
    { "SBC", addr_absolute },
    { "INC", addr_absolute },
    { "isb", addr_absolute },
    { "BEQ", addr_relative },
    { "SBC", addr_indirect_y },
    { "hang", addr_implied },
    { "isb", addr_indirect_y },
    { "nop", addr_zeropage_x },
    { "SBC", addr_zeropage_x },
    { "INC", addr_zeropage_x },
    { "isb", addr_zeropage_x },
    { "SED", addr_implied },
    { "SBC", addr_absolute_y },
    { "nop", addr_implied },
    { "isb", addr_absolute_y },
    { "nop", addr_absolute_x },
    { "SBC", addr_absolute_x },
    { "INC", addr_absolute_x },
    { "isb", addr_absolute_x },
};
