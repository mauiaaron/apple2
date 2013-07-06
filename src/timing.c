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

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>

#define DEFAULT_SLEEP 120

static unsigned int sleep_hz             = DEFAULT_SLEEP;               // sleep intervals per sec
static unsigned long cpu_target_hz       = APPLE2_HZ;                   // target clock speed
static unsigned long cycles_interval     = APPLE2_HZ / DEFAULT_SLEEP;   // Number of 65c02 instructions to be executed at sleep_hz
static unsigned long processing_interval = NANOSECONDS / DEFAULT_SLEEP; // Number of nanoseconds in sleep_hz intervals

static struct timespec deltat, t0, ti, tj;
static unsigned long cycle=0;
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

void timing_initialize() {
    clock_gettime(CLOCK_MONOTONIC, &t0);
    ti=t0;
}

void timing_set_cpu_target_hz(unsigned long hz) {
    cpu_target_hz = hz;
}

void timing_set_sleep_hz(unsigned int hz) {
    sleep_hz = hz;
}

/*
 * Throttles the 65c02 CPU down to a target frequency of X.
 * Currently set to target the Apple //e @ 1.02MHz
 *
 * This is called from cpu65_run() on the cpu-thread
 */
void timing_throttle() {
    ++cycle;

    static time_t severe_lag=0;

    if ((cycle%cycles_interval) == 0)
    {

        // wake render thread as we go to sleep
        pthread_mutex_lock(&mutex);
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mutex);

        clock_gettime(CLOCK_MONOTONIC, &tj);
        deltat = timespec_diff(ti, tj);
        ti=tj;
        if (deltat.tv_sec != 0)
        {
            // severely lagging, don't bother sleeping ...
            if (severe_lag < time(NULL))
            {
                severe_lag = time(NULL)+2;
                fprintf(stderr, "Severe lag detected...\n");
            }
        }
        else
        {
            deltat.tv_nsec = processing_interval - deltat.tv_nsec + sleep_adjust_inc;
            nanosleep(&deltat, NULL); // NOTE: spec says will return right away if deltat.tv_nsec value < 0 ...
            ti.tv_nsec += deltat.tv_nsec;
        }

        if ((cycle%cpu_target_hz) == 0)
        {
            clock_gettime(CLOCK_MONOTONIC, &tj);

            deltat = timespec_diff(t0, tj);
            struct timespec t = (struct timespec) {.tv_sec=1, .tv_nsec=0 };

            long adj = (deltat.tv_sec == 0)
                       ?      timespec_nsecs(timespec_diff(deltat, t))
                       : -1 * timespec_nsecs(timespec_diff(t, deltat));

            sleep_adjust += adj;
            sleep_adjust_inc = sleep_adjust/sleep_hz;

            t0=tj;
            ti=t0;
        }
    }
}
