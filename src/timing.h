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

#if !defined(NANOSECONDS_PER_SECOND)
#define NANOSECONDS_PER_SECOND 1000000000
#endif

// timing values cribbed from AppleWin ... reference: Sather's _Understanding the Apple IIe_
// TODO: revisit this if/when attempting to actually sync up VBL/VSYNC to actual device vsync

// 14318181.81...
#define _M14     (157500000.0 / 11.0)
#define _M14_INT (157500000   / 11)

// 65 cycles per 912 14M clocks = 1020484.45...
#define CLK_6502     ((_M14     * 65.0) / 912.0)
#define CLK_6502_INT ((_M14_INT * 65)   / 912)

#define CPU_SCALE_SLOWEST 0.25
#define CPU_SCALE_FASTEST 4.05
#ifdef INTERFACE_CLASSIC
#   define CPU_SCALE_STEP_DIV 0.01
#   define CPU_SCALE_STEP 0.05
#endif

extern unsigned long long cycles_count_total;// cumulative cycles count from machine reset
extern double cycles_persec_target;         // CLK_6502 * current CPU scale
extern int cycles_speaker_feedback;         // current -/+ speaker requested feedback

extern double cpu_scale_factor;             // scale factor #1
extern double cpu_altscale_factor;          // scale factor #2
extern bool is_fullspeed;                   // emulation in full native speed?

extern pthread_t cpu_thread_id;

/*
 * calculate the difference between two timespec structures
 */
struct timespec timespec_diff(struct timespec start, struct timespec end, bool *negative);

/*
 * toggles CPU speed between configured values
 */
void timing_toggle_cpu_speed(void);

/*
 * (dis)allow automatic adjusting of CPU speed between presently configured and max (when loading disk images).
 */
void timing_set_auto_adjust_speed(bool auto_adjust);

/*
 * check whether automatic adjusting of CPU speed is configured.
 */
bool timing_should_auto_adjust_speed(void);

/*
 * initialize timing
 */
void timing_initialize(void);

/*
 * timing/CPU thread entry point
 */
void *cpu_thread(void *ignored);

/*
 * checkpoints current cycle count and updates total (for timing-dependent I/O)
 */
void timing_checkpoint_cycles(void);

#endif // whole file
