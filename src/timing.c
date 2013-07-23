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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include <limits.h>

#include "timing.h"
#include "misc.h"
#include "cpu.h"

#define CALIBRATE_HZ 100

static unsigned long CPU_TARGET_HZ = APPLE2_HZ;                         // target clock speed
static unsigned long CALIBRATE_INTERVAL = NANOSECONDS / CALIBRATE_HZ;   // calibration interval for drifting
static float CYCLE_NSECS = NANOSECONDS / (float)APPLE2_HZ;              // nanosecs per cycle

static struct timespec ti;

static int spinloop_count=0;                                            // spin loop counter

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
    struct timespec s0, s1, deltat;

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

    spinloop_count = CYCLE_NSECS * spinloop_count / avg_spin_nsecs;

    printf("counter for a single %fns cycle = %d\n", CYCLE_NSECS, spinloop_count);
}

void timing_initialize() {

    // should do this only on startup
    _determine_initial_spinloop_counter();

    clock_gettime(CLOCK_MONOTONIC, &ti);
}

void timing_set_cpu_scale(unsigned int scale)
{
    // ...
}

/*
 * Calibrate emulator clock to real clock ...
 *
 * NOTE: these calculations could overflow if emulator speed is severely dampened back...
 */
static int _calibrate_clock (long drift_interval_nsecs)
{
    return 0;
    // HACK FIXME : this is broken, plz debug, kthxbye!

    struct timespec tj, deltat;
    clock_gettime(CLOCK_MONOTONIC, &tj);
    deltat = timespec_diff(ti, tj);
    ti=tj;

    long real_nsecs = NANOSECONDS * deltat.tv_sec + deltat.tv_nsec;
    int drift_nsecs = (int)(drift_interval_nsecs - real_nsecs);             // +/- nsec drift
    int drift_count = (int)(drift_nsecs * (spinloop_count / CYCLE_NSECS) ); // +/- count drift

    // adjust spinloop_count ...
    spinloop_count += drift_count / CALIBRATE_INTERVAL;

    // ideally we should return a sub-interval here ...
    return 0;
}

/*
 * Throttles 6502 CPU down to the target CPU frequency (default is speed of original Apple //e).
 *
 * This uses an adaptive spin loop to stay closer to the target CPU frequency.
 */
void timing_throttle()
{
    static float drift_interval=0.0;
    static int spinloop_adjust=0;

    uint8_t opcycles = cpu65__opcycles[cpu65_debug.opcode] + cpu65_debug.opcycles;
    uint8_t c=0;
    if (spinloop_adjust < 0)
    {
        c=-1;
        ++spinloop_adjust;
    }
    else if (spinloop_adjust > 0)
    {
        c=1;
        --spinloop_adjust;
    }

    // spin for the desired/estimated number of nsecs
    _spin_loop(opcycles * (spinloop_count+c));

    drift_interval += opcycles*CYCLE_NSECS;

    if (drift_interval > CALIBRATE_INTERVAL)
    {
        // perform calibration
        spinloop_adjust = _calibrate_clock((long)drift_interval);
        drift_interval = 0.0;
    }
}

