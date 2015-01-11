/*
 * Apple // emulator for *nix
 *
 * This software package is subject to the GNU General Public License
 * version 2 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * THERE ARE NO WARRANTIES WHATSOEVER.
 *
 */

/*
 * 65c02 CPU Timing Support.
 *
 * Copyleft 2013 Aaron Culliney
 *
 */

#ifndef _TIMING_H_
#define _TIMING_H_

#include "common.h"

#define NANOSECONDS 1000000000

// timing values cribbed from AppleWin ... should double-check _Understanding the Apple IIe_

// 14318181.81...
#define _M14 (157500000.0 / 11.0)

// 65 cycles per 912 14M clocks = 1020484.45...
#define CLK_6502 ((_M14 * 65.0) / 912.0)

#define CPU_SCALE_SLOWEST 0.25
#define CPU_SCALE_FASTEST 4.05
#define CPU_SCALE_STEP_DIV 0.01
#define CPU_SCALE_STEP 0.05

#define SPKR_SAMPLE_RATE 22050

extern double g_fCurrentCLK6502;
extern bool g_bFullSpeed;
extern uint64_t g_nCumulativeCycles;
extern int g_nCpuCyclesFeedback;

extern double cpu_scale_factor;
extern double cpu_altscale_factor;

extern pthread_t cpu_thread_id;

extern int gc_cycles_timer_0;
extern int gc_cycles_timer_1;

struct timespec timespec_diff(struct timespec start, struct timespec end, bool *negative);

void timing_toggle_cpu_speed(void);

void timing_initialize(void);

void *cpu_thread(void *ignored);

void CpuCalcCycles(const unsigned long nExecutedCycles);

#endif // whole file
