/*
 * Apple // emulator for Linux
 *
 * CPU Timing Support.
 *
 * Mostly this adds support for specifically throttling the emulator speed to
 * match a 1.02MHz Apple //e.
 *
 * Added 2013 by Aaron Culliney
 *
 */

#include "timing.h"
#include "misc.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <limits.h>

#define CALIBRATE_HZ 120

static unsigned long cpu_target_hz       = APPLE2_HZ;                   // target clock speed
static unsigned long calibrate_interval  = NANOSECONDS / CALIBRATE_HZ;  // calibration interval for drifting
static unsigned long cycle_nanoseconds   = NANOSECONDS / APPLE2_HZ;     // nanosecs per cycle
static unsigned int  cycle_nanoseconds_count;

static struct timespec deltat, t0, ti, tj;

static unsigned long cycle_count=0;                 // CPU cycle counter
static int spinloop_count=0;               // spin loop counter

static long sleep_adjust=0;
static long sleep_adjust_inc=0;

extern pthread_mutex_t mutex;
extern pthread_cond_t cond;

// -----------------------------------------------------------------------------

// assuming end > start, returns end - start
static inline struct timespec timespec_diff(struct timespec start, struct timespec end) {
    struct timespec t;

    // assuming time_t is signed ...
    if (end.tv_nsec < start.tv_nsec)
    {
        t.tv_sec  = end.tv_sec - start.tv_sec - 1;
        t.tv_nsec = NANOSECONDS + end.tv_nsec - start.tv_nsec;
    }
    else
    {
        t.tv_sec  = end.tv_sec  - start.tv_sec;
        t.tv_nsec = end.tv_nsec - start.tv_nsec;
    }

    return t;
}

static inline long timespec_nsecs(struct timespec t) {
    return t.tv_sec*NANOSECONDS + t.tv_nsec;
}

// spin loop to throttle to target CPU Hz
static inline void _spin_loop(unsigned long c)
{
    static volatile unsigned int spinney=0; // volatile to prevent being optimized away
    for (unsigned long i=0; i<c; i++)
    {
        ++spinney;
    }
}

static void _determine_initial_spinloop_counter()
{
    struct timespec s0, s1;

    // time the spinloop to determine a good starting value for the spin counter

    unsigned long avg_spin_nsecs = 0;
    unsigned int const samples = 5;
    unsigned int i=0;
    spinloop_count = 500000000;
    do
    {
        clock_gettime(CLOCK_MONOTONIC, &s0);
        _spin_loop(spinloop_count);
        clock_gettime(CLOCK_MONOTONIC, &s1);
        deltat = timespec_diff(s0, s1);

        if (deltat.tv_sec > 0)
        {
            printf("oops long wait (>= %lu sec) adjusting loop count (%d -> %d)\n", deltat.tv_sec, spinloop_count, spinloop_count>>1);
            spinloop_count >>= 1;
            i = 0;
            avg_spin_nsecs = 0;
            continue;
        }

        printf("spinloop = %lu nsec\n", deltat.tv_nsec);
        avg_spin_nsecs += deltat.tv_nsec;
        ++i;
    } while (i<samples);

    avg_spin_nsecs = (avg_spin_nsecs / samples);
    printf("average  = %lu nsec\n", avg_spin_nsecs);

    spinloop_count = cycle_nanoseconds * spinloop_count / avg_spin_nsecs;

    cycle_nanoseconds_count = cycle_nanoseconds / spinloop_count;

    printf("counter for a single cycle = %d\n", spinloop_count);
}

void timing_initialize() {

    // should do this only on startup
    _determine_initial_spinloop_counter();

    clock_gettime(CLOCK_MONOTONIC, &t0);
    ti=t0;
}

void timing_set_cpu_scale(unsigned int scale)
{
    // ...
}

/*
 * Throttles 6502 CPU down to the target CPU frequency (default is speed of original Apple //e).
 *
 * This uses an adaptive spin loop to stay closer to the target CPU frequency.
 *
 */
void timing_throttle()
{
    static unsigned int drift_interval_counter=0;           // in nsecs since last
    static unsigned int instruction_interval_counter=0;     // instruction count since last
    static unsigned int spin_adjust_interval=INT_MAX;
    static int8_t spin_adjust_count=0;                      // +/- 1

    ++instruction_interval_counter;

    unsigned int opcycles = cpu65__opcycles[cpu65_debug.opcode] + cpu65_debug.opcycles;
    if (!opcycles)
    {
        opcycles = 2; // assume 2 cycles for UNK opcodes
    }
    cycle_count += opcycles;

    int8_t c = instruction_interval_counter%spin_adjust_interval ? spin_adjust_count : 0;
    _spin_loop(opcycles * (spinloop_count + c) );
    drift_interval_counter += c*cycle_nanoseconds;

    if (drift_interval_counter < calibrate_interval)
    {
        return;
    }

    // -------------------------------------------------------------------------
    // calibrate emulator clock to real clock ...

    clock_gettime(CLOCK_MONOTONIC, &tj);
    deltat = timespec_diff(ti, tj);
    ti=tj;

    // NOTE: these calculations could overflow if emulator speed is severely dampened back...
    unsigned long real_counter = NANOSECONDS * deltat.tv_sec;
    real_counter += deltat.tv_nsec;
    long diff_nsecs = real_counter - drift_interval_counter;    // whole +/- nsec diff

    float nsecs_per_oneloop = cycle_nanoseconds/(float)spinloop_count;
    unsigned int instruction_interval_nsecs = instruction_interval_counter * nsecs_per_oneloop;

    // reset
    drift_interval_counter=0;
    instruction_interval_counter=0;

    // calculate spin adjustment
    if (diff_nsecs == 0)
    {
        // nothing to do
    }
    else if (abs(diff_nsecs) > instruction_interval_nsecs)
    {
        // spin for additional +/- X each instruction
        spinloop_count += diff_nsecs / instruction_interval_nsecs;
        spin_adjust_interval=INT_MAX;
    }
    else
    {
        // sub adjustment : spin for additional +/- 1 every interval
        spin_adjust_count = diff_nsecs < 0 ? -1 : 1;
        spin_adjust_interval = instruction_interval_nsecs / abs(diff_nsecs);
    }
}

