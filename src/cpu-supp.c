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

#if CPU_TRACING
#   if VM_TRACING
#       define RESET_VM_TRACING 1
#   endif
#   undef VM_TRACING
#endif

#include "common.h"

cpu65_run_args_s run_args = { 0 };

static pthread_mutex_t irq_mutex = PTHREAD_MUTEX_INITIALIZER;

uint8_t cpu65_flags_encode[256] = { 0 };
uint8_t cpu65_flags_decode[256] = { 0 };

void *cpu65_vmem_r[sizeof(void*) * 256] = { 0 };
void *cpu65_vmem_w[sizeof(void*) * 256] = { 0 };

#if CPU_TRACING
static int8_t opargs[3] = { 0 };
static int8_t nargs = 0;
static uint16_t current_pc = 0x0;
static FILE *cpu_trace_fp = NULL;
#endif

// ----------------------------------------------------------------------------
// 65c02 Opcode Jump Table

extern void op_BRK(void), op_ORA_ind_x(void), op_UNK_65c02(void), op_TSB_zpage(void), op_ORA_zpage(void), op_ASL_zpage(void), op_RMB0_65c02(void), op_PHP(void), op_ORA_imm(void), op_ASL_acc(void), op_TSB_abs(void), op_ORA_abs(void), op_ASL_abs(void), op_BBR0_65c02(void), op_BPL(void), op_ORA_ind_y(void), op_ORA_ind_zpage(void), op_TRB_zpage(void), op_ORA_zpage_x(void), op_ASL_zpage_x(void), op_RMB1_65c02(void), op_CLC(void), op_ORA_abs_y(void), op_INA(void), op_TRB_abs(void), op_ORA_abs_x(void), op_ASL_abs_x(void), op_BBR1_65c02(void), op_JSR(void), op_AND_ind_x(void), op_BIT_zpage(void), op_AND_zpage(void), op_ROL_zpage(void), op_RMB2_65c02(void), op_PLP(void), op_AND_imm(void), op_ROL_acc(void), op_BIT_abs(void), op_AND_abs(void), op_ROL_abs(void), op_BBR2_65c02(void), op_BMI(void), op_AND_ind_y(void), op_AND_ind_zpage(void), op_BIT_zpage_x(void), op_AND_zpage_x(void), op_ROL_zpage_x(void), op_RMB3_65c02(void), op_SEC(void), op_AND_abs_y(void), op_DEA(void), op_BIT_abs_x(void), op_AND_abs_x(void), op_ROL_abs_x(void), op_BBR3_65c02(void), op_RTI(void), op_EOR_ind_x(void), op_EOR_zpage(void), op_LSR_zpage(void), op_RMB4_65c02(void), op_PHA(void), op_EOR_imm(void), op_LSR_acc(void), op_JMP_abs(void), op_EOR_abs(void), op_LSR_abs(void), op_BBR4_65c02(void), op_BVC(void), op_EOR_ind_y(void), op_EOR_ind_zpage(void), op_EOR_zpage_x(void), op_LSR_zpage_x(void), op_RMB5_65c02(void), op_CLI(void), op_EOR_abs_y(void), op_PHY(void), op_EOR_abs_x(void), op_LSR_abs_x(void), op_BBR5_65c02(void), op_RTS(void), op_ADC_ind_x(void), op_STZ_zpage(void), op_ADC_zpage(void), op_ROR_zpage(void), op_RMB6_65c02(void), op_PLA(void), op_ADC_imm(void), op_ROR_acc(void), op_JMP_ind(void), op_ADC_abs(void), op_ROR_abs(void), op_BBR6_65c02(void), op_BVS(void), op_ADC_ind_y(void), op_ADC_ind_zpage(void), op_STZ_zpage_x(void), op_ADC_zpage_x(void), op_ROR_zpage_x(void), op_RMB7_65c02(void), op_SEI(void), op_ADC_abs_y(void), op_PLY(void), op_JMP_abs_ind_x(void), op_ADC_abs_x(void), op_ROR_abs_x(void), op_BBR7_65c02(void), op_BRA(void), op_STA_ind_x(void), op_STY_zpage(void), op_STA_zpage(void), op_STX_zpage(void), op_SMB0_65c02(void), op_DEY(void), op_BIT_imm(void), op_TXA(void), op_STY_abs(void), op_STA_abs(void), op_STX_abs(void), op_BBS0_65c02(void), op_BCC(void), op_STA_ind_y(void), op_STA_ind_zpage(void), op_STY_zpage_x(void), op_STA_zpage_x(void), op_STX_zpage_y(void), op_SMB1_65c02(void), op_TYA(void), op_STA_abs_y(void), op_TXS(void), op_STZ_abs(void), op_STA_abs_x(void), op_STZ_abs_x(void), op_BBS1_65c02(void), op_LDY_imm(void), op_LDA_ind_x(void), op_LDX_imm(void), op_LDY_zpage(void), op_LDA_zpage(void), op_LDX_zpage(void), op_SMB2_65c02(void), op_TAY(void), op_LDA_imm(void), op_TAX(void), op_LDY_abs(void), op_LDA_abs(void), op_LDX_abs(void), op_BBS2_65c02(void), op_BCS(void), op_LDA_ind_y(void), op_LDA_ind_zpage(void), op_LDY_zpage_x(void), op_LDA_zpage_x(void), op_LDX_zpage_y(void), op_SMB3_65c02(void), op_CLV(void), op_LDA_abs_y(void), op_TSX(void), op_LDY_abs_x(void), op_LDA_abs_x(void), op_LDX_abs_y(void), op_BBS3_65c02(void), op_CPY_imm(void), op_CMP_ind_x(void), op_CPY_zpage(void), op_CMP_zpage(void), op_DEC_zpage(void), op_SMB4_65c02(void), op_INY(void), op_CMP_imm(void), op_DEX(void), op_WAI_65c02(void), op_CPY_abs(void), op_CMP_abs(void), op_DEC_abs(void), op_BBS4_65c02(void), op_BNE(void), op_CMP_ind_y(void), op_CMP_ind_zpage(void), op_CMP_zpage_x(void), op_DEC_zpage_x(void), op_SMB5_65c02(void), op_CLD(void), op_CMP_abs_y(void), op_PHX(void), op_STP_65c02(void), op_CMP_abs_x(void), op_DEC_abs_x(void), op_BBS5_65c02(void), op_CPX_imm(void), op_SBC_ind_x(void), op_CPX_zpage(void), op_SBC_zpage(void), op_INC_zpage(void), op_SMB6_65c02(void), op_INX(void), op_SBC_imm(void), op_NOP(void), op_CPX_abs(void), op_SBC_abs(void), op_INC_abs(void), op_BBS6_65c02(void), op_BEQ(void), op_SBC_ind_y(void), op_SBC_ind_zpage(void), op_SBC_zpage_x(void), op_INC_zpage_x(void), op_SMB7_65c02(void), op_SED(void), op_SBC_abs_y(void), op_PLX(void), op_SBC_abs_x(void), op_INC_abs_x(void), op_BBS7_65c02(void);

void *cpu65__opcodes[256] = {
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
    7, // op_DEC_abs_x
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
    7, // op_INC_abs_x
    5  // op_BBS7_65c02
};

// NOTE: currently this is a conversion table between i386 flags <-> 6502 P register
static void init_flags_conversion_tables(void) {
    for (unsigned int i = 0; i < 256; i++) {
        uint8_t val = 0;

        if (i & C_Flag) {
            val |= C_Flag_6502;
        }

        if (i & X_Flag) {
            val |= X_Flag_6502;
        }

        if (i & I_Flag) {
            val |= I_Flag_6502;
        }

        if (i & V_Flag) {
            val |= V_Flag_6502;
        }

        if (i & B_Flag) {
            val |= B_Flag_6502;
        }

        if (i & D_Flag) {
            val |= D_Flag_6502;
        }

        if (i & Z_Flag) {
            val |= Z_Flag_6502;
        }

        if (i & N_Flag) {
            val |= N_Flag_6502;
        }

        cpu65_flags_encode[ i ] = val;
        cpu65_flags_decode[ val ] = (uint8_t)i;
    }
}

static __attribute__((constructor)) void __init_cpu65(void) {
    // emulator_registerStartupCallback(CTOR_PRIORITY_LATE, &_init_cpu65); -- 2018/01/15 NOTE : too late for testcpu.c

    run_args.cpu65_vmem_r = &cpu65_vmem_r[0];
    run_args.cpu65_vmem_w = &cpu65_vmem_w[0];
    run_args.cpu65_flags_encode = &cpu65_flags_encode[0];
    run_args.cpu65_flags_decode = &cpu65_flags_decode[0];
    run_args.cpu65__opcodes = &cpu65__opcodes[0];
    run_args.cpu65__opcycles = &cpu65__opcycles[0];

    run_args.interrupt_vector = 0xFFFE;
    run_args.reset_vector = 0xFFFC;

#if CPU_TRACING
    extern void cpu65_trace_prologue(uint16_t, uint8_t);
    run_args.cpu65_trace_prologue = cpu65_trace_prologue;
    extern void cpu65_trace_arg(uint16_t, uint8_t);
    run_args.cpu65_trace_arg = cpu65_trace_arg;
    extern void cpu65_trace_epilogue(uint16_t, uint8_t);
    run_args.cpu65_trace_epilogue = cpu65_trace_epilogue;
    extern void cpu65_trace_irq(uint16_t, uint8_t);
    run_args.cpu65_trace_irq = cpu65_trace_irq;
#endif

#ifndef NDEBUG
    extern uint8_t (*debug_illegal_bcd)(uint16_t);
    run_args.debug_illegal_bcd = debug_illegal_bcd;
#endif
}

void cpu65_init(void) {
    init_flags_conversion_tables();
    run_args.cpu65__signal = 0;
    run_args.cpu65_pc = 0x0;
    run_args.cpu65_ea = 0x0;
    run_args.cpu65_a = 0xFF;
    run_args.cpu65_x = 0xFF;
    run_args.cpu65_y = 0xFF;
    run_args.cpu65_f = (C_Flag_6502|X_Flag_6502|I_Flag_6502|V_Flag_6502|B_Flag_6502|Z_Flag_6502|N_Flag_6502);
    run_args.cpu65_sp = 0xFC;
}

void cpu65_interrupt(int reason) {
    pthread_mutex_lock(&irq_mutex);
    run_args.cpu65__signal |= reason;
    pthread_mutex_unlock(&irq_mutex);
}

void cpu65_uninterrupt(int reason) {
    pthread_mutex_lock(&irq_mutex);
    run_args.cpu65__signal &= ~reason;
    pthread_mutex_unlock(&irq_mutex);
}

void cpu65_reboot(void) {
    run_args.joy_button0 = 0xff; // OpenApple -- should be balanced by c_joystick_reset() triggers on CPU thread
    cpu65_interrupt(ResetSig);
}

bool cpu65_saveState(StateHelper_s *helper) {
    bool saved = false;
    int fd = helper->fd;

    do {
        uint8_t serialized[4] = { 0 };

        // save CPU state
        serialized[0] = ((run_args.cpu65_pc & 0xFF00) >> 8);
        serialized[1] = ((run_args.cpu65_pc & 0xFF  ) >> 0);
        if (!helper->save(fd, serialized, sizeof(run_args.cpu65_pc))) {
            break;
        }

        serialized[0] = ((run_args.cpu65_ea & 0xFF00) >> 8);
        serialized[1] = ((run_args.cpu65_ea & 0xFF  ) >> 0);
        if (!helper->save(fd, serialized, sizeof(run_args.cpu65_ea))) {
            break;
        }

        if (!helper->save(fd, &run_args.cpu65_a, sizeof(run_args.cpu65_a))) {
            break;
        }
        if (!helper->save(fd, &run_args.cpu65_f, sizeof(run_args.cpu65_f))) {
            break;
        }
        if (!helper->save(fd, &run_args.cpu65_x, sizeof(run_args.cpu65_x))) {
            break;
        }
        if (!helper->save(fd, &run_args.cpu65_y, sizeof(run_args.cpu65_y))) {
            break;
        }
        if (!helper->save(fd, &run_args.cpu65_sp, sizeof(run_args.cpu65_sp))) {
            break;
        }

        saved = true;
    } while (0);

    return saved;
}

bool cpu65_loadState(StateHelper_s *helper) {
    bool loaded = false;
    int fd = helper->fd;

    do {

        uint8_t serialized[4] = { 0 };

        // load CPU state
        if (!helper->load(fd, serialized, sizeof(uint16_t))) {
            break;
        }
        run_args.cpu65_pc  = (serialized[0] << 8);
        run_args.cpu65_pc |=  serialized[1];

        if (!helper->load(fd, serialized, sizeof(uint16_t))) {
            break;
        }
        run_args.cpu65_ea  = (serialized[0] << 8);
        run_args.cpu65_ea |=  serialized[1];

        if (!helper->load(fd, &run_args.cpu65_a, sizeof(run_args.cpu65_a))) {
            break;
        }
        if (!helper->load(fd, &run_args.cpu65_f, sizeof(run_args.cpu65_f))) {
            break;
        }
        if (!helper->load(fd, &run_args.cpu65_x, sizeof(run_args.cpu65_x))) {
            break;
        }
        if (!helper->load(fd, &run_args.cpu65_y, sizeof(run_args.cpu65_y))) {
            break;
        }
        if (!helper->load(fd, &run_args.cpu65_sp, sizeof(run_args.cpu65_sp))) {
            break;
        }

        loaded = true;
    } while (0);

    return loaded;
}

#if CPU_TRACING
extern const struct opcode_struct_s opcodes_65c02[256];
extern const uint8_t opcodes_65c02_numargs[256];

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
    current_pc = run_args.cpu65_pc;
}

GLUE_C_WRITE(cpu65_trace_arg)
{
    assert(nargs <= 2);
    opargs[nargs++] = b;
}

GLUE_C_WRITE(cpu65_trace_epilogue)
{
    int8_t arg1 = opargs[1];
    int8_t arg2 = opargs[2];

    if (!cpu_trace_fp) {
        return;
    }

    assert(nargs > 0);
    assert(nargs <= 3);
    if (nargs != opcodes_65c02_numargs[run_args.cpu65_opcode]+1) {
        assert(false && "OOPS, most likely some cpu.S routine is not properly setting the arg value");
    }

    switch (opcodes_65c02[run_args.cpu65_opcode].mode) {
        case addr_implied:
        case addr_accumulator:
            fprintf(cpu_trace_fp, "%04X:%02X    ", current_pc, run_args.cpu65_opcode);
            break;
        case addr_immediate:
        case addr_zeropage:
        case addr_zeropage_x:
        case addr_zeropage_y:
        case addr_indirect:
        case addr_indirect_x:
        case addr_indirect_y:
        case addr_relative:
            fprintf(cpu_trace_fp, "%04X:%02X%02X  ", current_pc, run_args.cpu65_opcode, (uint8_t)arg1);
            break;
        case addr_absolute:
        case addr_absolute_x:
        case addr_absolute_y:
        case addr_j_indirect:
        case addr_j_indirect_x:
            fprintf(cpu_trace_fp, "%04X:%02X%02X%02X", current_pc, run_args.cpu65_opcode, (uint8_t)arg1, (uint8_t)arg2);
            break;
        default:
            fprintf(cpu_trace_fp, "invalid opcode mode");
            break;
    }

    fprintf(cpu_trace_fp, " SP:%02X X:%02X Y:%02X A:%02X", run_args.cpu65_sp, run_args.cpu65_x, run_args.cpu65_y, run_args.cpu65_a);

#define FLAGS_BUFSZ 9
    char flags_buf[FLAGS_BUFSZ];
    memset(flags_buf, '-', FLAGS_BUFSZ);
    if (run_args.cpu65_f & C_Flag_6502) {
        flags_buf[0]='C';
    }
    if (run_args.cpu65_f & X_Flag_6502) {
        flags_buf[1]='X';
    }
    if (run_args.cpu65_f & I_Flag_6502) {
        flags_buf[2]='I';
    }
    if (run_args.cpu65_f & V_Flag_6502) {
        flags_buf[3]='V';
    }
    if (run_args.cpu65_f & B_Flag_6502) {
        flags_buf[4]='B';
    }
    if (run_args.cpu65_f & D_Flag_6502) {
        flags_buf[5]='D';
    }
    if (run_args.cpu65_f & Z_Flag_6502) {
        flags_buf[6]='Z';
    }
    if (run_args.cpu65_f & N_Flag_6502) {
        flags_buf[7]='N';
    }
    flags_buf[8] = '\0';

    char fmt[64];
    if (UNLIKELY(run_args.cpu65_opcycles >= 10)) {
        // occurs rarely for interrupt + opcode
        snprintf(fmt, 64, "%s", " %s CY:%u");
    } else {
        snprintf(fmt, 64, "%s", " %s CYC:%u");
    }
    fprintf(cpu_trace_fp, fmt, flags_buf, run_args.cpu65_opcycles);

    uint16_t vidAddr = video_scannerAddress(NULL);
    uint8_t vidData = apple_ii_64k[0][vidAddr];
    fprintf(cpu_trace_fp, " VID:%04X:%02X", vidAddr, vidData);

#if CPU_TRACING_SHOW_EA
    fprintf(cpu_trace_fp, " EA:%04X", run_args.cpu65_ea);
#endif

    fprintf(cpu_trace_fp, " CY+%lu", (cycles_count_total + run_args.cpu65_opcycles));

    sprintf(fmt, " %s %s", opcodes_65c02[run_args.cpu65_opcode].mnemonic, disasm_templates[opcodes_65c02[run_args.cpu65_opcode].mode]);

    switch (opcodes_65c02[run_args.cpu65_opcode].mode) {
        case addr_implied:
        case addr_accumulator:
            fprintf(cpu_trace_fp, "%s", fmt);
            break;
        case addr_immediate:
        case addr_zeropage:
        case addr_zeropage_x:
        case addr_zeropage_y:
        case addr_indirect:
        case addr_indirect_x:
        case addr_indirect_y:
            fprintf(cpu_trace_fp, fmt, (uint8_t)arg1);
            break;
        case addr_absolute:
        case addr_absolute_x:
        case addr_absolute_y:
        case addr_j_indirect:
        case addr_j_indirect_x:
            fprintf(cpu_trace_fp, fmt, (uint8_t)arg2, (uint8_t)arg1);
            break;
        case addr_relative:
            if (arg1 < 0) {
                fprintf(cpu_trace_fp, fmt, current_pc + arg1 + 2, '-', (uint8_t)(-arg1));
            } else {
                fprintf(cpu_trace_fp, fmt, current_pc + arg1 + 2, '+', (uint8_t)arg1);
            }
            break;
        default:
            break;
    }

    fprintf(cpu_trace_fp, "%s", "\n");
}

GLUE_C_WRITE(cpu65_trace_irq)
{
    if (cpu_trace_fp) {
        fprintf(cpu_trace_fp, "IRQ:%02X\n", run_args.cpu65__signal);
    }
}

void cpu65_trace_checkpoint(void) {
    if (cpu_trace_fp) {
        //fprintf(cpu_trace_fp, "---TOTAL CYC:%lu\n",cycles_count_total);
        fflush(cpu_trace_fp);
    }
}

#   if RESET_VM_TRACING
#       define VM_TRACING 1
#   endif
#   undef RESET_VM_TRACING

#endif // CPU_TRACING

