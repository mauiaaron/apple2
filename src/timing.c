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

static unsigned long CPU_TARGET_HZ = APPLE2_HZ;                             // target clock speed
static unsigned long CALIBRATE_INTERVAL_NSECS = NANOSECONDS / CALIBRATE_HZ; // calibration interval for drifting
static float CYCLE_NSECS = NANOSECONDS / (float)APPLE2_HZ;                  // nanosecs per cycle

static struct timespec ti;
static float spin_ratio=0.0;

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
    unsigned int spinloop_count = 250000000;
    do
    {
        clock_gettime(CLOCK_MONOTONIC, &s0);
        _spin_loop(spinloop_count);
        clock_gettime(CLOCK_MONOTONIC, &s1);
        deltat = timespec_diff(s0, s1);

        if (deltat.tv_sec > 0)
        {
            LOG("oops long wait (>= %lu sec) adjusting loop count (%d -> %d)", deltat.tv_sec, spinloop_count, spinloop_count>>1);
            spinloop_count >>= 1;
            i = 0;
            avg_spin_nsecs = 0;
            continue;
        }

        LOG("spinloop = %lu nsec", deltat.tv_nsec);
        avg_spin_nsecs += deltat.tv_nsec;
        ++i;
    } while (i<samples);

    avg_spin_nsecs = (avg_spin_nsecs / samples);
    LOG("average  = %lu nsec , spinloop_count = %u , samples = %u", avg_spin_nsecs, spinloop_count, samples);

    // counter for 1 nsec
    spin_ratio = avg_spin_nsecs / ((float)spinloop_count);

    LOG("%fns cycle spins for average %f", CYCLE_NSECS, spin_ratio);
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
 * Throttles 6502 CPU down to the target CPU frequency (default is speed of original Apple //e).
 *
 * This uses an adaptive spin loop to stay closer to the target CPU frequency.
 */
void timing_throttle()
{
    struct timespec tj, deltat;
    clock_gettime(CLOCK_MONOTONIC, &tj);
    deltat = timespec_diff(ti, tj);
    ti=tj;

    static time_t severe_lag=0;
    if (deltat.tv_sec != 0)
    {
        // severely lagging...
        if (severe_lag < time(NULL))
        {
            severe_lag = time(NULL)+2;
            LOG("Severe lag detected...");
        }
        return;
    }

    uint8_t opcycles = cpu65__opcycles[cpu65_debug.opcode] + cpu65_debug.opcycles;
    unsigned long opcycles_nsecs = opcycles * (CYCLE_NSECS/2);

    if (deltat.tv_nsec >= opcycles_nsecs)
    {
        // lagging
        return;
    }

    unsigned long diff_nsec = opcycles_nsecs - deltat.tv_nsec;

    static time_t sample_time=0;
    if (sample_time < time(NULL))
    {
        sample_time = time(NULL)+1;
        //LOG("sample diff_nsec : %lu", diff_nsec);
    }

    unsigned long spin_count = spin_ratio * diff_nsec;

    // spin for the rest of the interval ...
    _spin_loop(spin_count);
}

