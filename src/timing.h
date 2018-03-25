/*
 * Apple // emulator for *ix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 2013-2015 Aaron Culliney
 *
 */

/*
 * 65c02 CPU Timing Support.
 *
 */

#ifndef _TIMING_H_
#define _TIMING_H_

#include "common.h"

#if !defined(NANOSECONDS_PER_SECOND)
#define NANOSECONDS_PER_SECOND 1000000000UL
#endif

// At a rate of ~1000x/sec, the emulator will (1) determine the number X of 65c02 instructions to execute and then
// executes them, (2) perform post-instruction-churn bookkeeping, and (3) sleep for a calculated interval.
//
// * The actual wall clock time to perform the emulated churn and bookkeeping is used to determine the sleep interval
//
// * The speaker provides feedback to the calculation of X (the number of instructions to churn)
#define EXECUTION_CHURN_RATE   1000UL
#define EXECUTION_PERIOD_NSECS 1000000UL // NANOSECONDS_PER_SECOND / EXECUTION_CHURN_RATE

// timing values cribbed from AppleWin ... reference: Sather's _Understanding the Apple IIe_
// TODO: revisit this if/when attempting to actually sync up VBL/VSYNC to actual device vsync

// 14318181.81...
#define _M14     (157500000.0 / 11.0)
#define _M14_INT (157500000   / 11)

// 65 cycles per 912 14M clocks = 1020484.45 ...
#define CLK_6502     ((_M14     * 65.0) / 912.0)
#define CLK_6502_INT ((unsigned int)CLK_6502)

#define CPU_SCALE_SLOWEST 0.25
#define CPU_SCALE_FASTEST_PIVOT 4.0
#define CPU_SCALE_FASTEST (CPU_SCALE_FASTEST_PIVOT + 0.0625)
#ifdef INTERFACE_CLASSIC
#   define CPU_SCALE_STEP_DIV 0.01
#   define CPU_SCALE_STEP 0.05
#endif

extern unsigned long cycles_count_total;    // cumulative cycles count from machine reset
extern double cycles_persec_target;         // CLK_6502 * current CPU scale
extern int cycles_speaker_feedback;         // current -/+ speaker requested feedback

extern double cpu_scale_factor;             // scale factor #1
extern double cpu_altscale_factor;          // scale factor #2
extern bool is_fullspeed;                   // emulation in full native speed?
extern bool alt_speed_enabled;

extern READONLY pthread_t cpu_thread_id;

/*
 * calculate the difference between two timespec structures
 */
struct timespec timespec_diff(struct timespec start, struct timespec end, bool *negative);
struct timespec timespec_add(struct timespec start, unsigned long nsecs);

/*
 * True if current thread is the CPU thread.
 */
bool timing_isCPUThread(void);

/*
 * start CPU thread
 */
void timing_startCPU(void);

/*
 * stop CPU thread
 */
void timing_stopCPU(void);

/*
 * toggles CPU speed between configured values
 */
void timing_toggleCPUSpeed(void);

#if !MOBILE_DEVICE
/*
 * check whether automatic adjusting of CPU speed is configured.
 */
bool timing_shouldAutoAdjustSpeed(void);
#endif

/*
 * initialize timing
 */
void timing_initialize(void);

/*
 * Pause timing/CPU thread.
 *
 * This may block for a short amount of time to grab the appropriate mutex.  CPU thread is blocked upon function return,
 * until call to cpu_resume() is made.
 */
void cpu_pause(void);

/*
 * Resume timing/CPU thread
 */
void cpu_resume(void);

/*
 * Is the CPU paused?
 */
bool cpu_isPaused(void);

/*
 * checkpoints current cycle count and updates total (for timing-dependent I/O)
 */
void timing_checkpoint_cycles(void);

// ----------------------------------------------------------------------------

bool timing_saveState(StateHelper_s *helper);

bool timing_loadState(StateHelper_s *helper);


#endif // whole file
