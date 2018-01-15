/*
 * Apple // emulator for *ix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 2017+ Aaron Culliney
 *
 */

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

#include "glue.h"

int main (int argc, char **argv) {

    printf("/* This file is auto-generated for a specific architecture ABI */\n");

    OUTPUT_CPU_IRQCHECK();

    OUTPUT_CPU65_TRACE_PROLOGUE();
    OUTPUT_CPU65_TRACE_ARG();
    OUTPUT_CPU65_TRACE_ARG1();
    OUTPUT_CPU65_TRACE_ARG2();
    OUTPUT_CPU65_TRACE_EPILOGUE();
    OUTPUT_CPU65_TRACE_IRQ();

    OUTPUT_DEBUG_ILLEGAL_BCD();

    OUTPUT_CPU65_VMEM_R();
    OUTPUT_CPU65_VMEM_W();
    OUTPUT_CPU65_FLAGS_ENCODE();
    OUTPUT_CPU65_FLAGS_DECODE();
    OUTPUT_CPU65__OPCODES();
    OUTPUT_CPU65__OPCYCLES();

    OUTPUT_BASE_RAMRD();
    OUTPUT_BASE_RAMWRT();
    OUTPUT_BASE_TEXTRD();
    OUTPUT_BASE_TEXTWRT();
    OUTPUT_BASE_HGRRD();
    OUTPUT_BASE_HGRWRT();

    OUTPUT_BASE_STACKZP();
    OUTPUT_BASE_D000_RD();
    OUTPUT_BASE_E000_RD();
    OUTPUT_BASE_D000_WRT();
    OUTPUT_BASE_E000_WRT();

    OUTPUT_BASE_C3ROM();
    OUTPUT_BASE_C4ROM();
    OUTPUT_BASE_C5ROM();
    OUTPUT_BASE_CXROM();

    OUTPUT_SOFTSWITCHES();

    OUTPUT_GC_CYCLES_TIMER_0();
    OUTPUT_GC_CYCLES_TIMER_1();

    OUTPUT_CPU65_CYCLES_TO_EXECUTE();
    OUTPUT_CPU65_CYCLE_COUNT();
    OUTPUT_IRQ_CHECK_TIMEOUT();

    OUTPUT_INTERRUPT_VECTOR();
    OUTPUT_RESET_VECTOR();

    OUTPUT_CPU65_PC();
    OUTPUT_CPU65_EA();

    OUTPUT_CPU65_A();
    OUTPUT_CPU65_F();
    OUTPUT_CPU65_X();
    OUTPUT_CPU65_Y();
    OUTPUT_CPU65_SP();

    OUTPUT_CPU65_D();
    OUTPUT_CPU65_RW();
    OUTPUT_CPU65_OPCODE();
    OUTPUT_CPU65_OPCYCLES();

    OUTPUT_CPU65__SIGNAL();

    OUTPUT_JOY_BUTTON0();
    OUTPUT_JOY_BUTTON1();

    OUTPUT_EMUL_REINITIALIZE();

    fflush(stdout);

    return 0;
}

