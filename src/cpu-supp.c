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

#include "common.h"

uint16_t cpu65_pc;
uint8_t  cpu65_a;
uint8_t  cpu65_f;
uint8_t  cpu65_x;
uint8_t  cpu65_y;
uint8_t  cpu65_sp;

uint16_t cpu65_ea;
uint8_t  cpu65_d;
uint8_t  cpu65_rw;
uint8_t  cpu65_opcode;
uint8_t  cpu65_opcycles;

int16_t cpu65_cycle_count = 0;
int16_t cpu65_cycles_to_execute = 0;
uint8_t cpu65__signal = 0;

static pthread_mutex_t irq_mutex = PTHREAD_MUTEX_INITIALIZER;

uint8_t cpu65_flags_encode[256] = { 0 };
uint8_t cpu65_flags_decode[256] = { 0 };

void *cpu65_vmem_r[0x10000] = { 0 };
void *cpu65_vmem_w[0x10000] = { 0 };

#if CPU_TRACING
static int8_t opargs[3] = { 0 };
static int8_t nargs = 0;
static uint16_t current_pc = 0x0;
static FILE *cpu_trace_fp = NULL;
#endif

// ----------------------------------------------------------------------------
// 65c02 Opcode Jump Table

extern void op_BRK(), op_ORA_ind_x(), op_UNK_65c02(), op_TSB_zpage(), op_ORA_zpage(), op_ASL_zpage(), op_RMB0_65c02(), op_PHP(), op_ORA_imm(), op_ASL_acc(), op_TSB_abs(), op_ORA_abs(), op_ASL_abs(), op_BBR0_65c02(), op_BPL(), op_ORA_ind_y(), op_ORA_ind_zpage(), op_TRB_zpage(), op_ORA_zpage_x(), op_ASL_zpage_x(), op_RMB1_65c02(), op_CLC(), op_ORA_abs_y(), op_INA(), op_TRB_abs(), op_ORA_abs_x(), op_ASL_abs_x(), op_BBR1_65c02(), op_JSR(), op_AND_ind_x(), op_BIT_zpage(), op_AND_zpage(), op_ROL_zpage(), op_RMB2_65c02(), op_PLP(), op_AND_imm(), op_ROL_acc(), op_BIT_abs(), op_AND_abs(), op_ROL_abs(), op_BBR2_65c02(), op_BMI(), op_AND_ind_y(), op_AND_ind_zpage(), op_BIT_zpage_x(), op_AND_zpage_x(), op_ROL_zpage_x(), op_RMB3_65c02(), op_SEC(), op_AND_abs_y(), op_DEA(), op_BIT_abs_x(), op_AND_abs_x(), op_ROL_abs_x(), op_BBR3_65c02(), op_RTI(), op_EOR_ind_x(), op_EOR_zpage(), op_LSR_zpage(), op_RMB4_65c02(), op_PHA(), op_EOR_imm(), op_LSR_acc(), op_JMP_abs(), op_EOR_abs(), op_LSR_abs(), op_BBR4_65c02(), op_BVC(), op_EOR_ind_y(), op_EOR_ind_zpage(), op_EOR_zpage_x(), op_LSR_zpage_x(), op_RMB5_65c02(), op_CLI(), op_EOR_abs_y(), op_PHY(), op_EOR_abs_x(), op_LSR_abs_x(), op_BBR5_65c02(), op_RTS(), op_ADC_ind_x(), op_STZ_zpage(), op_ADC_zpage(), op_ROR_zpage(), op_RMB6_65c02(), op_PLA(), op_ADC_imm(), op_ROR_acc(), op_JMP_ind(), op_ADC_abs(), op_ROR_abs(), op_BBR6_65c02(), op_BVS(), op_ADC_ind_y(), op_ADC_ind_zpage(), op_STZ_zpage_x(), op_ADC_zpage_x(), op_ROR_zpage_x(), op_RMB7_65c02(), op_SEI(), op_ADC_abs_y(), op_PLY(), op_JMP_abs_ind_x(), op_ADC_abs_x(), op_ROR_abs_x(), op_BBR7_65c02(), op_BRA(), op_STA_ind_x(), op_STY_zpage(), op_STA_zpage(), op_STX_zpage(), op_SMB0_65c02(), op_DEY(), op_BIT_imm(), op_TXA(), op_STY_abs(), op_STA_abs(), op_STX_abs(), op_BBS0_65c02(), op_BCC(), op_STA_ind_y(), op_STA_ind_zpage(), op_STY_zpage_x(), op_STA_zpage_x(), op_STX_zpage_y(), op_SMB1_65c02(), op_TYA(), op_STA_abs_y(), op_TXS(), op_STZ_abs(), op_STA_abs_x(), op_STZ_abs_x(), op_BBS1_65c02(), op_LDY_imm(), op_LDA_ind_x(), op_LDX_imm(), op_LDY_zpage(), op_LDA_zpage(), op_LDX_zpage(), op_SMB2_65c02(), op_TAY(), op_LDA_imm(), op_TAX(), op_LDY_abs(), op_LDA_abs(), op_LDX_abs(), op_BBS2_65c02(), op_BCS(), op_LDA_ind_y(), op_LDA_ind_zpage(), op_LDY_zpage_x(), op_LDA_zpage_x(), op_LDX_zpage_y(), op_SMB3_65c02(), op_CLV(), op_LDA_abs_y(), op_TSX(), op_LDY_abs_x(), op_LDA_abs_x(), op_LDX_abs_y(), op_BBS3_65c02(), op_CPY_imm(), op_CMP_ind_x(), op_CPY_zpage(), op_CMP_zpage(), op_DEC_zpage(), op_SMB4_65c02(), op_INY(), op_CMP_imm(), op_DEX(), op_WAI_65c02(), op_CPY_abs(), op_CMP_abs(), op_DEC_abs(), op_BBS4_65c02(), op_BNE(), op_CMP_ind_y(), op_CMP_ind_zpage(), op_CMP_zpage_x(), op_DEC_zpage_x(), op_SMB5_65c02(), op_CLD(), op_CMP_abs_y(), op_PHX(), op_STP_65c02(), op_CMP_abs_x(), op_DEC_abs_x(), op_BBS5_65c02(), op_CPX_imm(), op_SBC_ind_x(), op_CPX_zpage(), op_SBC_zpage(), op_INC_zpage(), op_SMB6_65c02(), op_INX(), op_SBC_imm(), op_NOP(), op_CPX_abs(), op_SBC_abs(), op_INC_abs(), op_BBS6_65c02(), op_BEQ(), op_SBC_ind_y(), op_SBC_ind_zpage(), op_SBC_zpage_x(), op_INC_zpage_x(), op_SMB7_65c02(), op_SED(), op_SBC_abs_y(), op_PLX(), op_SBC_abs_x(), op_INC_abs_x(), op_BBS7_65c02();

void *const cpu65__opcodes[256] = {
    op_BRK,            // 00
    op_ORA_ind_x,
    op_UNK_65c02,
    op_UNK_65c02,
    op_TSB_zpage,
    op_ORA_zpage,
    op_ASL_zpage,
    op_RMB0_65c02,
    op_PHP,            // 08
    op_ORA_imm,
    op_ASL_acc,
    op_UNK_65c02,
    op_TSB_abs,
    op_ORA_abs,
    op_ASL_abs,
    op_BBR0_65c02,
    op_BPL,            // 10
    op_ORA_ind_y,
    op_ORA_ind_zpage,
    op_UNK_65c02,
    op_TRB_zpage,
    op_ORA_zpage_x,
    op_ASL_zpage_x,
    op_RMB1_65c02,
    op_CLC,            // 18
    op_ORA_abs_y,
    op_INA,
    op_UNK_65c02,
    op_TRB_abs,
    op_ORA_abs_x,
    op_ASL_abs_x,
    op_BBR1_65c02,
    op_JSR,            // 20
    op_AND_ind_x,
    op_UNK_65c02,
    op_UNK_65c02,
    op_BIT_zpage,
    op_AND_zpage,
    op_ROL_zpage,
    op_RMB2_65c02,
    op_PLP,            // 28
    op_AND_imm,
    op_ROL_acc,
    op_UNK_65c02,
    op_BIT_abs,
    op_AND_abs,
    op_ROL_abs,
    op_BBR2_65c02,
    op_BMI,            // 30
    op_AND_ind_y,
    op_AND_ind_zpage,
    op_UNK_65c02,
    op_BIT_zpage_x,
    op_AND_zpage_x,
    op_ROL_zpage_x,
    op_RMB3_65c02,
    op_SEC,            // 38
    op_AND_abs_y,
    op_DEA,
    op_UNK_65c02,
    op_BIT_abs_x,
    op_AND_abs_x,
    op_ROL_abs_x,
    op_BBR3_65c02,
    op_RTI,            // 40
    op_EOR_ind_x,
    op_UNK_65c02,
    op_UNK_65c02,
    op_UNK_65c02,
    op_EOR_zpage,
    op_LSR_zpage,
    op_RMB4_65c02,
    op_PHA,            // 48
    op_EOR_imm,
    op_LSR_acc,
    op_UNK_65c02,
    op_JMP_abs,
    op_EOR_abs,
    op_LSR_abs,
    op_BBR4_65c02,
    op_BVC,            // 50
    op_EOR_ind_y,
    op_EOR_ind_zpage,
    op_UNK_65c02,
    op_UNK_65c02,
    op_EOR_zpage_x,
    op_LSR_zpage_x,
    op_RMB5_65c02,
    op_CLI,            // 58
    op_EOR_abs_y,
    op_PHY,
    op_UNK_65c02,
    op_UNK_65c02,
    op_EOR_abs_x,
    op_LSR_abs_x,
    op_BBR5_65c02,
    op_RTS,            // 60
    op_ADC_ind_x,
    op_UNK_65c02,
    op_UNK_65c02,
    op_STZ_zpage,
    op_ADC_zpage,
    op_ROR_zpage,
    op_RMB6_65c02,
    op_PLA,            // 68
    op_ADC_imm,
    op_ROR_acc,
    op_UNK_65c02,
    op_JMP_ind,
    op_ADC_abs,
    op_ROR_abs,
    op_BBR6_65c02,
    op_BVS,            // 70
    op_ADC_ind_y,
    op_ADC_ind_zpage,
    op_UNK_65c02,
    op_STZ_zpage_x,
    op_ADC_zpage_x,
    op_ROR_zpage_x,
    op_RMB7_65c02,
    op_SEI,            // 78
    op_ADC_abs_y,
    op_PLY,
    op_UNK_65c02,
    op_JMP_abs_ind_x,
    op_ADC_abs_x,
    op_ROR_abs_x,
    op_BBR7_65c02,
    op_BRA,            // 80
    op_STA_ind_x,
    op_UNK_65c02,
    op_UNK_65c02,
    op_STY_zpage,
    op_STA_zpage,
    op_STX_zpage,
    op_SMB0_65c02,
    op_DEY,            // 88
    op_BIT_imm,
    op_TXA,
    op_UNK_65c02,
    op_STY_abs,
    op_STA_abs,
    op_STX_abs,
    op_BBS0_65c02,
    op_BCC,            // 90
    op_STA_ind_y,
    op_STA_ind_zpage,
    op_UNK_65c02,
    op_STY_zpage_x,
    op_STA_zpage_x,
    op_STX_zpage_y,
    op_SMB1_65c02,
    op_TYA,            // 98
    op_STA_abs_y,
    op_TXS,
    op_UNK_65c02,
    op_STZ_abs,
    op_STA_abs_x,
    op_STZ_abs_x,
    op_BBS1_65c02,
    op_LDY_imm,        // A0
    op_LDA_ind_x,
    op_LDX_imm,
    op_UNK_65c02,
    op_LDY_zpage,
    op_LDA_zpage,
    op_LDX_zpage,
    op_SMB2_65c02,
    op_TAY,            // A8
    op_LDA_imm,
    op_TAX,
    op_UNK_65c02,
    op_LDY_abs,
    op_LDA_abs,
    op_LDX_abs,
    op_BBS2_65c02,
    op_BCS,            // B0
    op_LDA_ind_y,
    op_LDA_ind_zpage,
    op_UNK_65c02,
    op_LDY_zpage_x,
    op_LDA_zpage_x,
    op_LDX_zpage_y,
    op_SMB3_65c02,
    op_CLV,            // B8
    op_LDA_abs_y,
    op_TSX,
    op_UNK_65c02,
    op_LDY_abs_x,
    op_LDA_abs_x,
    op_LDX_abs_y,
    op_BBS3_65c02,
    op_CPY_imm,        // C0
    op_CMP_ind_x,
    op_UNK_65c02,
    op_UNK_65c02,
    op_CPY_zpage,
    op_CMP_zpage,
    op_DEC_zpage,
    op_SMB4_65c02,
    op_INY,            // C8
    op_CMP_imm,
    op_DEX,
    op_WAI_65c02,
    op_CPY_abs,
    op_CMP_abs,
    op_DEC_abs,
    op_BBS4_65c02,
    op_BNE,            // D0
    op_CMP_ind_y,
    op_CMP_ind_zpage,
    op_UNK_65c02,
    op_UNK_65c02,
    op_CMP_zpage_x,
    op_DEC_zpage_x,
    op_SMB5_65c02,
    op_CLD,            // D8
    op_CMP_abs_y,
    op_PHX,
    op_STP_65c02,
    op_UNK_65c02,
    op_CMP_abs_x,
    op_DEC_abs_x,
    op_BBS5_65c02,
    op_CPX_imm,        // E0
    op_SBC_ind_x,
    op_UNK_65c02,
    op_UNK_65c02,
    op_CPX_zpage,
    op_SBC_zpage,
    op_INC_zpage,
    op_SMB6_65c02,
    op_INX,            // E8
    op_SBC_imm,
    op_NOP,
    op_UNK_65c02,
    op_CPX_abs,
    op_SBC_abs,
    op_INC_abs,
    op_BBS6_65c02,
    op_BEQ,            // F0
    op_SBC_ind_y,
    op_SBC_ind_zpage,
    op_UNK_65c02,
    op_UNK_65c02,
    op_SBC_zpage_x,
    op_INC_zpage_x,
    op_SMB7_65c02,
    op_SED,            // F8
    op_SBC_abs_y,
    op_PLX,
    op_UNK_65c02,
    op_UNK_65c02,
    op_SBC_abs_x,
    op_INC_abs_x,
    op_BBS7_65c02
};

// ----------------------------------------------------------------------------
// Base values for opcode cycle counts

uint8_t cpu65__opcycles[256] = {
    7, // op_BRK             00
    6, // op_ORA_ind_x
    7, // op_UNK_65c02
    7, // op_UNK_65c02
    5, // op_TSB_zpage
    3, // op_ORA_zpage
    5, // op_ASL_zpage
    5, // op_RMB0_65c02
    3, // op_PHP             08
    2, // op_ORA_imm
    2, // op_ASL_acc
    7, // op_UNK_65c02
    6, // op_TSB_abs
    4, // op_ORA_abs
    6, // op_ASL_abs
    5, // op_BBR0_65c02
    2, // op_BPL             10
    5, // op_ORA_ind_y
    5, // op_ORA_ind_zpage
    7, // op_UNK_65c02
    5, // op_TRB_zpage
    4, // op_ORA_zpage_x
    6, // op_ASL_zpage_x
    5, // op_RMB1_65c02
    2, // op_CLC             18
    4, // op_ORA_abs_y
    2, // op_INA
    7, // op_UNK_65c02
    6, // op_TRB_abs
    4, // op_ORA_abs_x
    6, // op_ASL_abs_x
    5, // op_BBR1_65c02
    6, // op_JSR             20
    6, // op_AND_ind_x
    7, // op_UNK_65c02
    7, // op_UNK_65c02
    3, // op_BIT_zpage
    3, // op_AND_zpage
    5, // op_ROL_zpage
    5, // op_RMB2_65c02
    4, // op_PLP             28
    2, // op_AND_imm
    2, // op_ROL_acc
    7, // op_UNK_65c02
    4, // op_BIT_abs
    4, // op_AND_abs
    6, // op_ROL_abs
    5, // op_BBR2_65c02
    2, // op_BMI             30
    5, // op_AND_ind_y
    5, // op_AND_ind_zpage
    7, // op_UNK_65c02
    4, // op_BIT_zpage_x
    4, // op_AND_zpage_x
    6, // op_ROL_zpage_x
    5, // op_RMB3_65c02
    2, // op_SEC             38
    4, // op_AND_abs_y
    2, // op_DEA
    7, // op_UNK_65c02
    4, // op_BIT_abs_x
    4, // op_AND_abs_x
    6, // op_ROL_abs_x
    5, // op_BBR3_65c02
    6, // op_RTI             40
    6, // op_EOR_ind_x
    7, // op_UNK_65c02
    7, // op_UNK_65c02
    7, // op_UNK_65c02
    3, // op_EOR_zpage
    5, // op_LSR_zpage
    5, // op_RMB4_65c02
    3, // op_PHA             48
    2, // op_EOR_imm
    2, // op_LSR_acc
    7, // op_UNK_65c02
    3, // op_JMP_abs
    4, // op_EOR_abs
    6, // op_LSR_abs
    5, // op_BBR4_65c02
    2, // op_BVC             50
    5, // op_EOR_ind_y
    5, // op_EOR_ind_zpage
    7, // op_UNK_65c02
    7, // op_UNK_65c02
    4, // op_EOR_zpage_x
    6, // op_LSR_zpage_x
    5, // op_RMB5_65c02
    2, // op_CLI             58
    4, // op_EOR_abs_y
    3, // op_PHY
    7, // op_UNK_65c02
    7, // op_UNK_65c02
    4, // op_EOR_abs_x
    6, // op_LSR_abs_x
    5, // op_BBR5_65c02
    6, // op_RTS             60
    6, // op_ADC_ind_x
    7, // op_UNK_65c02
    7, // op_UNK_65c02
    3, // op_STZ_zpage
    3, // op_ADC_zpage
    5, // op_ROR_zpage
    5, // op_RMB6_65c02
    4, // op_PLA             68
    2, // op_ADC_imm
    2, // op_ROR_acc
    7, // op_UNK_65c02
    6, // op_JMP_ind
    4, // op_ADC_abs
    6, // op_ROR_abs
    5, // op_BBR6_65c02
    2, // op_BVS             70
    5, // op_ADC_ind_y
    5, // op_ADC_ind_zpage
    7, // op_UNK_65c02
    4, // op_STZ_zpage_x
    4, // op_ADC_zpage_x
    6, // op_ROR_zpage_x
    5, // op_RMB7_65c02
    2, // op_SEI             78
    4, // op_ADC_abs_y
    4, // op_PLY
    7, // op_UNK_65c02
    6, // op_JMP_abs_ind_x
    4, // op_ADC_abs_x
    6, // op_ROR_abs_x
    5, // op_BBR7_65c02
    2, // op_BRA             80
    6, // op_STA_ind_x
    7, // op_UNK_65c02
    7, // op_UNK_65c02
    3, // op_STY_zpage
    3, // op_STA_zpage
    3, // op_STX_zpage
    5, // op_SMB0_65c02
    2, // op_DEY             88
    2, // op_BIT_imm
    2, // op_TXA
    7, // op_UNK_65c02
    4, // op_STY_abs
    4, // op_STA_abs
    4, // op_STX_abs
    5, // op_BBS0_65c02
    2, // op_BCC             90
    6, // op_STA_ind_y
    5, // op_STA_ind_zpage
    7, // op_UNK_65c02
    4, // op_STY_zpage_x
    4, // op_STA_zpage_x
    4, // op_STX_zpage_y
    5, // op_SMB1_65c02
    2, // op_TYA             98
    5, // op_STA_abs_y
    2, // op_TXS
    7, // op_UNK_65c02
    4, // op_STZ_abs
    5, // op_STA_abs_x
    5, // op_STZ_abs_x
    5, // op_BBS1_65c02
    2, // op_LDY_imm         A0
    6, // op_LDA_ind_x
    2, // op_LDX_imm
    7, // op_UNK_65c02
    3, // op_LDY_zpage
    3, // op_LDA_zpage
    3, // op_LDX_zpage
    5, // op_SMB2_65c02
    2, // op_TAY             A8
    2, // op_LDA_imm
    2, // op_TAX
    7, // op_UNK_65c02
    4, // op_LDY_abs
    4, // op_LDA_abs
    4, // op_LDX_abs
    5, // op_BBS2_65c02
    2, // op_BCS             B0
    5, // op_LDA_ind_y
    5, // op_LDA_ind_zpage
    7, // op_UNK_65c02
    4, // op_LDY_zpage_x
    4, // op_LDA_zpage_x
    4, // op_LDX_zpage_y
    5, // op_SMB3_65c02
    2, // op_CLV             B8
    4, // op_LDA_abs_y
    2, // op_TSX
    7, // op_UNK_65c02
    4, // op_LDY_abs_x
    4, // op_LDA_abs_x
    4, // op_LDX_abs_y
    5, // op_BBS3_65c02
    2, // op_CPY_imm         C0
    6, // op_CMP_ind_x
    7, // op_UNK_65c02
    7, // op_UNK_65c02
    3, // op_CPY_zpage
    3, // op_CMP_zpage
    5, // op_DEC_zpage
    5, // op_SMB4_65c02
    2, // op_INY             C8
    2, // op_CMP_imm
    2, // op_DEX
    7, // op_WAI_65c02
    4, // op_CPY_abs
    4, // op_CMP_abs
    6, // op_DEC_abs
    5, // op_BBS4_65c02
    2, // op_BNE             D0
    5, // op_CMP_ind_y
    5, // op_CMP_ind_zpage
    7, // op_UNK_65c02
    7, // op_UNK_65c02
    4, // op_CMP_zpage_x
    6, // op_DEC_zpage_x
    5, // op_SMB5_65c02
    2, // op_CLD             D8
    4, // op_CMP_abs_y
    3, // op_PHX
    7, // op_STP_65c02
    7, // op_UNK_65c02
    4, // op_CMP_abs_x
    6, // op_DEC_abs_x
    5, // op_BBS5_65c02
    2, // op_CPX_imm         E0
    6, // op_SBC_ind_x
    7, // op_UNK_65c02
    7, // op_UNK_65c02
    3, // op_CPX_zpage
    3, // op_SBC_zpage
    5, // op_INC_zpage
    5, // op_SMB6_65c02
    2, // op_INX             E8
    2, // op_SBC_imm
    2, // op_NOP
    7, // op_UNK_65c02
    4, // op_CPX_abs
    4, // op_SBC_abs
    6, // op_INC_abs
    5, // op_BBS6_65c02
    2, // op_BEQ             F0
    5, // op_SBC_ind_y
    5, // op_SBC_ind_zpage
    7, // op_UNK_65c02
    7, // op_UNK_65c02
    4, // op_SBC_zpage_x
    6, // op_INC_zpage_x
    5, // op_SMB7_65c02
    2, // op_SED             F8
    4, // op_SBC_abs_y
    4, // op_PLX
    7, // op_UNK_65c02
    7, // op_UNK_65c02
    4, // op_SBC_abs_x
    6, // op_INC_abs_x
    5  // op_BBS7_65c02
};

// NOTE: currently this is a conversion table between i386 flags <-> 6502 P register
static void initialize_code_tables()
{
    for (unsigned i = 0; i < 256; i++)
    {
        unsigned char val = 0;

        if (i & C_Flag)
        {
            val |= C_Flag_6502;
        }

        if (i & X_Flag)
        {
            val |= X_Flag_6502;
        }

        if (i & I_Flag)
        {
            val |= I_Flag_6502;
        }

        if (i & V_Flag)
        {
            val |= V_Flag_6502;
        }

        if (i & B_Flag)
        {
            val |= B_Flag_6502;
        }

        if (i & D_Flag)
        {
            val |= D_Flag_6502;
        }

        if (i & Z_Flag)
        {
            val |= Z_Flag_6502;
        }

        if (i & N_Flag)
        {
            val |= N_Flag_6502;
        }

        cpu65_flags_encode[ i ] = val/* | 0x20 WTF?*/;
        cpu65_flags_decode[ val ] = i;
    }
}

void cpu65_init()
{
    initialize_code_tables();
    cpu65__signal = 0;
}

void cpu65_interrupt(int reason)
{
    pthread_mutex_lock(&irq_mutex);
    cpu65__signal |= reason;
    pthread_mutex_unlock(&irq_mutex);
}

void cpu65_uninterrupt(int reason)
{
    pthread_mutex_lock(&irq_mutex);
    cpu65__signal &= ~reason;
    pthread_mutex_unlock(&irq_mutex);
}

void cpu65_reboot(void) {
    timing_initialize();
    video_set(0);
    joy_button0 = 0xff; // OpenApple
    cpu65_interrupt(ResetSig);
    c_initialize_sound_hooks();
}

#if CPU_TRACING

/* -------------------------------------------------------------------------
    CPU Tracing routines
   ------------------------------------------------------------------------- */

void cpu65_trace_begin(const char *trace_file) {
    if (trace_file) {
        cpu_trace_fp = fopen(trace_file, "w");
    }
}

void cpu65_trace_end(void) {
    if (cpu_trace_fp) {
        fflush(cpu_trace_fp);
        fclose(cpu_trace_fp);
        cpu_trace_fp = NULL;
    }
}

void cpu65_trace_toggle(const char *trace_file) {
    if (cpu_trace_fp) {
        cpu65_trace_end();
    } else {
        cpu65_trace_begin(trace_file);
    }
}

GLUE_C_WRITE(cpu65_trace_prologue)
{
    nargs = 0;
    current_pc = cpu65_pc;
}

GLUE_C_WRITE(cpu65_trace_arg)
{
    assert(nargs <= 2);
    opargs[nargs++] = b;
}

GLUE_C_WRITE(cpu65_trace_arg1)
{
    assert(nargs <= 2);
    opargs[2] = b;
    ++nargs;
}

GLUE_C_WRITE(cpu65_trace_arg2)
{
    assert(nargs <= 2);
    opargs[1] = b;
    ++nargs;
}

GLUE_C_WRITE(cpu65_trace_epilogue)
{
    char fmt[64];
    char buf[64];

    int8_t arg1 = opargs[1];
    int8_t arg2 = opargs[2];

    if (!cpu_trace_fp) {
        return;
    }

    assert(nargs > 0);
    assert(nargs <= 3);

#warning FIXME TODO ... need to refactor this and the debugger routines to use the same codepaths ...
    switch (opcodes_65c02[cpu65_opcode].mode) {
        case addr_implied:
        case addr_accumulator:                  /* no arg */
            sprintf(buf, "%04X:%02X %s %s", current_pc, cpu65_opcode, opcodes_65c02[cpu65_opcode].mnemonic, disasm_templates[opcodes_65c02[cpu65_opcode].mode]);
            break;

        case addr_immediate:
        case addr_zeropage:
        case addr_zeropage_x:
        case addr_zeropage_y:
        case addr_indirect:
        case addr_indirect_x:
        case addr_indirect_y:                   /* byte arg */
            sprintf(fmt, "%04X:%02X%02X %s %s", current_pc, cpu65_opcode, (uint8_t)arg1, opcodes_65c02[cpu65_opcode].mnemonic, disasm_templates[opcodes_65c02[cpu65_opcode].mode]);
            sprintf(buf, fmt, (uint8_t)arg1);
            break;

        case addr_absolute:
        case addr_absolute_x:
        case addr_absolute_y:
        case addr_j_indirect:
        case addr_j_indirect_x:                 /* word arg */
            sprintf(fmt, "%04X:%02X%02X%02X %s %s", current_pc, cpu65_opcode, (uint8_t)arg1, (uint8_t)arg2, opcodes_65c02[cpu65_opcode].mnemonic, disasm_templates[opcodes_65c02[cpu65_opcode].mode]);
            sprintf(buf, fmt, (uint8_t)arg1, (uint8_t)arg2);
            break;

        case addr_relative:                     /* offset */
            sprintf(fmt, "%04X:%02X%02X %s %s", current_pc, cpu65_opcode, (uint8_t)arg1, opcodes_65c02[cpu65_opcode].mnemonic, disasm_templates[opcodes_65c02[cpu65_opcode].mode]);
            if (arg1 < 0) {
                sprintf(buf, fmt, current_pc + arg1 + 2, '-', (uint8_t)(-arg1));
            } else {
                sprintf(buf, fmt, current_pc + arg1 + 2, '+', (uint8_t)arg1);
            }
            break;

        default:                                /* shouldn't happen */
            sprintf(buf, "invalid opcode mode");
            break;
    }

    char regs_buf[64];
    sprintf(regs_buf, "EA:%04X SP:%02X X:%02X Y:%02X A:%02X", cpu65_ea, cpu65_sp, cpu65_x, cpu65_y, cpu65_a);

#define FLAGS_BUFSZ 9
    char flags_buf[FLAGS_BUFSZ];
    memset(flags_buf, '-', FLAGS_BUFSZ);
    if (cpu65_f & C_Flag_6502) {
        flags_buf[0]='C';
    }
    if (cpu65_f & X_Flag_6502) {
        flags_buf[1]='X';
    }
    if (cpu65_f & I_Flag_6502) {
        flags_buf[2]='I';
    }
    if (cpu65_f & V_Flag_6502) {
        flags_buf[3]='V';
    }
    if (cpu65_f & B_Flag_6502) {
        flags_buf[4]='B';
    }
    if (cpu65_f & D_Flag_6502) {
        flags_buf[5]='D';
    }
    if (cpu65_f & Z_Flag_6502) {
        flags_buf[6]='Z';
    }
    if (cpu65_f & N_Flag_6502) {
        flags_buf[7]='N';
    }
    flags_buf[8] = '\0';

    fprintf(cpu_trace_fp, "%s %s %s\n", buf, regs_buf, flags_buf);
    fflush(cpu_trace_fp);
}

#endif // CPU_TRACING

