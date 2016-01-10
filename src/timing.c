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
 * 65c02 CPU timing support. Source inspired/derived from AppleWin.
 *
 * Simplified timing loop for each execution period:
 *
 * ..{...+....[....|..................|.........]....^....|....^....^....}......
 *  ti  MBB       CHK                CHK            MBE  CHX  SPK  MBX  tj   ZZZ
 *
 *      - ti  : timing sample begin (lock out interface thread)
 *      - tj  : timing sample end   (unlock interface thread)
 *      -  [  : cpu65_run()
 *      -  ]  : cpu65_run() finished
 *      - CHK : incoming timing_checkpoint_cycles() call from IO (bumps cycles_count_total)
 *      - CHX : update remainder of timing_checkpoint_cycles() for execution period
 *      - MBB : Mockingboard begin
 *      - MBE : Mockingboard end/flush (output)
 *      - MBX : Mockingboard end video frame (output)
 *      - SPK : Speaker output
 *      - ZZZ : housekeeping+sleep (or not)
 *
 */

#include "common.h"

#define DEBUG_TIMING (!defined(NDEBUG) && 0) // enable to print timing stats
#if DEBUG_TIMING
#   define TIMING_LOG(...) LOG(__VA_ARGS__)
#else
#   define TIMING_LOG(...)
#endif

#define DISK_MOTOR_QUIET_NSECS 2000000

// VBL constants?
#define uCyclesPerLine 65 // 25 cycles of HBL & 40 cycles of HBL'
#define uVisibleLinesPerFrame (64*3) // 192
#define uLinesPerFrame (262) // 64 in each third of the screen & 70 in VBL
#define dwClksPerFrame (uCyclesPerLine * uLinesPerFrame) // 17030

// cycle counting
double cycles_persec_target = CLK_6502;
unsigned long long cycles_count_total = 0;
int cycles_speaker_feedback = 0;
int32_t cpu65_cycles_to_execute = 0;            // cycles-to-execute by cpu65_run()
int32_t cpu65_cycle_count = 0;                  // cycles currently excuted by cpu65_run()
static int32_t cycles_checkpoint_count = 0;
static unsigned int g_dwCyclesThisFrame = 0;

// scaling and speed adjustments
#if !MOBILE_DEVICE
static bool auto_adjust_speed = true;
#endif
static bool is_paused = false;
static unsigned long _pause_spinLock = 0;

double cpu_scale_factor = 1.0;
double cpu_altscale_factor = 1.0;
bool is_fullspeed = false;
bool alt_speed_enabled = false;

// misc
volatile uint8_t emul_reinitialize = 1;
#ifdef AUDIO_ENABLED
static bool emul_reinitialize_audio = true;
static bool emul_pause_audio = false;
static bool emul_resume_audio = false;
#endif
static bool cpu_shutting_down = false;
pthread_t cpu_thread_id = 0;
pthread_mutex_t interface_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t dbg_thread_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t cpu_thread_cond = PTHREAD_COND_INITIALIZER;

// -----------------------------------------------------------------------------

__attribute__((constructor(CTOR_PRIORITY_LATE)))
static void _init_timing(void) {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutex_init(&interface_mutex, &attr);
}

struct timespec timespec_diff(struct timespec start, struct timespec end, bool *negative) {
    struct timespec t;

    if (negative)
    {
        *negative = false;
    }

    // if start > end, swizzle...
    if ( (start.tv_sec > end.tv_sec) || ((start.tv_sec == end.tv_sec) && (start.tv_nsec > end.tv_nsec)) )
    {
        t=start;
        start=end;
        end=t;
        if (negative)
        {
            *negative = true;
        }
    }

    // assuming time_t is signed ...
    if (end.tv_nsec < start.tv_nsec)
    {
        t.tv_sec  = end.tv_sec - start.tv_sec - 1;
        t.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
    }
    else
    {
        t.tv_sec  = end.tv_sec  - start.tv_sec;
        t.tv_nsec = end.tv_nsec - start.tv_nsec;
    }

    return t;
}

struct timespec timespec_add(struct timespec start, unsigned long nsecs) {

    start.tv_nsec += nsecs;
    if (start.tv_nsec > NANOSECONDS_PER_SECOND)
    {
        start.tv_sec += (start.tv_nsec / NANOSECONDS_PER_SECOND);
        start.tv_nsec %= NANOSECONDS_PER_SECOND;
    }

    return start;
}

static void _timing_initialize(double scale) {
    is_fullspeed = (scale >= CPU_SCALE_FASTEST);
    if (!is_fullspeed) {
        cycles_persec_target = CLK_6502 * scale;
    }
#ifdef AUDIO_ENABLED
    speaker_reset();
    //TIMING_LOG("ClockRate:%0.2lf  ClockCyclesPerSpeakerSample:%0.2lf", cycles_persec_target, speaker_cyclesPerSample());
#endif
}

#if !TESTING
static
#endif
void reinitialize(void) {
#if !TESTING
    assert(pthread_self() == cpu_thread_id);
#endif

    cycles_count_total = 0;

    vm_initialize();

    softswitches = SS_TEXT | SS_IOUDIS | SS_C3ROM | SS_LCWRT | SS_LCSEC;

    video_setpage( 0 );

    video_redraw();

    cpu65_init();

    timing_initialize();

#ifdef AUDIO_ENABLED
    MB_Reset();
#endif
}

void timing_initialize(void) {
#if !TESTING
#   ifdef __APPLE__
#       warning FIXME TODO : this assert is firing on iOS port ... but the assert is valid ... fix soon 
#   else
    assert(cpu_isPaused() || (pthread_self() == cpu_thread_id));
#   endif
#endif
    _timing_initialize(alt_speed_enabled ? cpu_altscale_factor : cpu_scale_factor);
}

void timing_toggleCPUSpeed(void) {
    assert(cpu_isPaused() || (pthread_self() == cpu_thread_id));
    alt_speed_enabled = !alt_speed_enabled;
    timing_initialize();
}

#ifdef AUDIO_ENABLED
void timing_reinitializeAudio(void) {
    SPINLOCK_ACQUIRE(&_pause_spinLock);
    assert(pthread_self() != cpu_thread_id);
    assert(cpu_isPaused());
    emul_reinitialize_audio = true;
    emul_pause_audio = false;
    emul_resume_audio = false;
    SPINLOCK_RELINQUISH(&_pause_spinLock);
}
#endif

void cpu_pause(void) {
    assert(pthread_self() != cpu_thread_id);

    SPINLOCK_ACQUIRE(&_pause_spinLock);
    do {
        if (is_paused) {
            break;
        }

        // CPU thread will be paused when it next tries to acquire interface_mutex
        LOG("PAUSING CPU...");
#ifdef AUDIO_ENABLED
        if (!emul_reinitialize_audio) {
            emul_pause_audio = true;
        }
#endif
        pthread_mutex_lock(&interface_mutex);
        is_paused = true;
    } while (0);
    SPINLOCK_RELINQUISH(&_pause_spinLock);
}

void cpu_resume(void) {
    assert(pthread_self() != cpu_thread_id);

    SPINLOCK_ACQUIRE(&_pause_spinLock);
    do {
        if (!is_paused) {
            break;
        }

        // CPU thread will be unblocked to acquire interface_mutex
#ifdef AUDIO_ENABLED
        if (!emul_reinitialize_audio) {
            emul_resume_audio = true;
        }
#endif
        LOG("RESUMING CPU...");
        pthread_mutex_unlock(&interface_mutex);
        is_paused = false;
    } while (0);
    SPINLOCK_RELINQUISH(&_pause_spinLock);
}

bool cpu_isPaused(void) {
    return is_paused;
}

#if !MOBILE_DEVICE
bool timing_shouldAutoAdjustSpeed(void) {
    double speed = alt_speed_enabled ? cpu_altscale_factor : cpu_scale_factor;
    return auto_adjust_speed && (speed < CPU_SCALE_FASTEST);
}
#endif

static void *cpu_thread(void *dummyptr) {

    assert(pthread_self() == cpu_thread_id);

    LOG("cpu_thread : initialized...");

    struct timespec deltat = { 0 };
#if !MOBILE_DEVICE
    struct timespec disk_motor_time = { 0 };
#endif
    struct timespec t0 = { 0 }; // the target timer
    struct timespec ti = { 0 }; // actual before time sample
    struct timespec tj = { 0 }; // actual after time sample
    bool negative = false;
    long drift_adj_nsecs = 0;   // generic drift adjustment between target and actual

    int debugging_cycles0 = 0;
    int debugging_cycles = 0;

#if DEBUG_TIMING
    unsigned long dbg_ticks = 0;
    int speaker_neg_feedback = 0;
    int speaker_pos_feedback = 0;
    unsigned int dbg_cycles_executed = 0;
#endif

    do
    {
#ifdef AUDIO_ENABLED
        LOG("CPUTHREAD %lu LOCKING FOR MAYBE INITIALIZING AUDIO ...", cpu_thread_id);
        pthread_mutex_lock(&interface_mutex);
        if (emul_reinitialize_audio) {
            emul_reinitialize_audio = false;

            speaker_destroy();
            MB_Destroy();
            audio_shutdown();

            audio_init();
            speaker_init();
            MB_Initialize();
        }
        pthread_mutex_unlock(&interface_mutex);
        LOG("UNLOCKING FOR MAYBE INITIALIZING AUDIO ...");
#endif

        if (emul_reinitialize) {
            reinitialize();
        }

        LOG("cpu_thread : begin main loop ...");

        clock_gettime(CLOCK_MONOTONIC, &t0);

        do {
            SCOPE_TRACE_CPU("CPU mainloop");
            // -LOCK----------------------------------------------------------------------------------------- SAMPLE ti
#ifdef AUDIO_ENABLED
            if (UNLIKELY(emul_pause_audio)) {
                emul_pause_audio = false;
                audio_pause();
            }
#endif
            pthread_mutex_lock(&interface_mutex);
#ifdef AUDIO_ENABLED
            if (UNLIKELY(emul_resume_audio)) {
                emul_resume_audio = false;
                audio_resume();
            }
#endif
            clock_gettime(CLOCK_MONOTONIC, &ti);

            deltat = timespec_diff(t0, ti, &negative);
            if (deltat.tv_sec)
            {
                if (!is_fullspeed) {
                    TIMING_LOG("NOTE : serious divergence from target time ...");
                }
                t0 = ti;
                deltat = timespec_diff(t0, ti, &negative);
            }
            t0 = timespec_add(t0, EXECUTION_PERIOD_NSECS); // expected interval
            drift_adj_nsecs = negative ? ~deltat.tv_nsec : deltat.tv_nsec;

            // set up increment & decrement counters
            cpu65_cycles_to_execute = (cycles_persec_target / 1000); // cycles_persec_target * EXECUTION_PERIOD_NSECS / NANOSECONDS_PER_SECOND
            if (!is_fullspeed) {
                cpu65_cycles_to_execute += cycles_speaker_feedback;
            }
            if (cpu65_cycles_to_execute < 0)
            {
                cpu65_cycles_to_execute = 0;
            }

#ifdef AUDIO_ENABLED
            MB_StartOfCpuExecute();
#endif
            if (is_debugging) {
                debugging_cycles0 = cpu65_cycles_to_execute;
                debugging_cycles  = cpu65_cycles_to_execute;
            }

            do {
                if (is_debugging) {
                    cpu65_cycles_to_execute = 1;
                }

                cpu65_cycle_count = 0;
                cycles_checkpoint_count = 0;
                cpu65_run(); // run emulation for cpu65_cycles_to_execute cycles ...

                if (is_debugging) {
                    debugging_cycles -= cpu65_cycle_count;
                    if (c_debugger_should_break() || (debugging_cycles <= 0)) {
                        int err = 0;
                        if ((err = pthread_cond_signal(&dbg_thread_cond))) {
                            ERRLOG("pthread_cond_signal : %d", err);
                        }
                        if ((err = pthread_cond_wait(&cpu_thread_cond, &interface_mutex))) {
                            ERRLOG("pthread_cond_wait : %d", err);
                        }
                        if (debugging_cycles <= 0) {
                            cpu65_cycle_count = debugging_cycles0 - debugging_cycles/*<=0*/;
                            break;
                        }
                    }
                }
                if (emul_reinitialize) {
                    reinitialize();
                }
            } while (is_debugging);
#if DEBUG_TIMING
            dbg_cycles_executed += cpu65_cycle_count;
#endif
            g_dwCyclesThisFrame += cpu65_cycle_count;

#ifdef AUDIO_ENABLED
            MB_UpdateCycles(); // update 6522s (NOTE: do this before updating cycles_count_total)
#endif

            timing_checkpoint_cycles();
#if CPU_TRACING
            cpu65_trace_checkpoint();
#endif

#ifdef AUDIO_ENABLED
            speaker_flush(); // play audio
#endif

            if (g_dwCyclesThisFrame >= dwClksPerFrame) {
                g_dwCyclesThisFrame -= dwClksPerFrame;
#ifdef AUDIO_ENABLED
                MB_EndOfVideoFrame();
#endif
            }

            clock_gettime(CLOCK_MONOTONIC, &tj);
            pthread_mutex_unlock(&interface_mutex);
            // -UNLOCK--------------------------------------------------------------------------------------- SAMPLE tj

#if !MOBILE_DEVICE
            if (timing_shouldAutoAdjustSpeed()) {
                disk_motor_time = timespec_diff(disk6.motor_time, tj, &negative);
                assert(!negative);
                if (!is_fullspeed &&
#ifdef AUDIO_ENABLED
                        !speaker_isActive() &&
#endif
                        !video_isDirty() && (!disk6.motor_off && (disk_motor_time.tv_sec || disk_motor_time.tv_nsec > DISK_MOTOR_QUIET_NSECS)) )
                {
                    TIMING_LOG("auto switching to full speed");
                    _timing_initialize(CPU_SCALE_FASTEST);
                }
            }
#endif

            if (!is_fullspeed) {
                deltat = timespec_diff(ti, tj, &negative);
                assert(!negative);
                long sleepfor = 0;
                if (!deltat.tv_sec)
                {
                    sleepfor = EXECUTION_PERIOD_NSECS - drift_adj_nsecs - deltat.tv_nsec;
                }

                if (sleepfor <= 0)
                {
                    // lagging ...
                    static time_t throttle_warning = 0;
                    if (t0.tv_sec - throttle_warning > 0)
                    {
                        TIMING_LOG("not sleeping to catch up ... %ld . %ld", deltat.tv_sec, deltat.tv_nsec);
                        throttle_warning = t0.tv_sec;
                    }
                }
                else
                {
                    deltat.tv_sec = 0;
                    deltat.tv_nsec = sleepfor;
                    TRACE_CPU_BEGIN("sleep");
                    nanosleep(&deltat, NULL);
                    TRACE_CPU_END();
                }

#if DEBUG_TIMING
                // collect timing statistics
                if (speaker_neg_feedback > cycles_speaker_feedback)
                {
                    speaker_neg_feedback = cycles_speaker_feedback;
                }
                if (speaker_pos_feedback < cycles_speaker_feedback)
                {
                    speaker_pos_feedback = cycles_speaker_feedback;
                }

                dbg_ticks += EXECUTION_PERIOD_NSECS;
                if ((dbg_ticks % NANOSECONDS_PER_SECOND) == 0)
                {
                    TIMING_LOG("tick:(%ld.%ld) real:(%ld.%ld) cycles exe: %d ... speaker feedback: %d/%d", t0.tv_sec, t0.tv_nsec, ti.tv_sec, ti.tv_nsec, dbg_cycles_executed, speaker_neg_feedback, speaker_pos_feedback);
                    dbg_cycles_executed = 0;
                    dbg_ticks = 0;
                    speaker_neg_feedback = 0;
                    speaker_pos_feedback = 0;
                }
#endif
            }

#if !MOBILE_DEVICE
            if (timing_shouldAutoAdjustSpeed()) {
                if (is_fullspeed && (
#ifdef AUDIO_ENABLED
                            speaker_isActive() ||
#endif
                            video_isDirty() || (disk6.motor_off && (disk_motor_time.tv_sec || disk_motor_time.tv_nsec > DISK_MOTOR_QUIET_NSECS))) )
                {
                    double speed = alt_speed_enabled ? cpu_altscale_factor : cpu_scale_factor;
                    if (speed < CPU_SCALE_FASTEST) {
                        TIMING_LOG("auto switching to configured speed");
                        _timing_initialize(speed);
                    }
                }
            }
#endif

            if (UNLIKELY(emul_reinitialize)) {
                break;
            }

#ifdef AUDIO_ENABLED
            if (UNLIKELY(emul_reinitialize_audio)) {
                break;
            }
#endif

            if (UNLIKELY(cpu_shutting_down)) {
                break;
            }
        } while (1);

        if (UNLIKELY(cpu_shutting_down)) {
            break;
        }
    } while (1);

#ifdef AUDIO_ENABLED
    speaker_destroy();
    MB_Destroy();
    audio_shutdown();
#endif

    return NULL;
}

void timing_startCPU(void) {
    cpu_shutting_down = false;
    int err = TEMP_FAILURE_RETRY(pthread_create(&cpu_thread_id, NULL, (void *)&cpu_thread, (void *)NULL));
    if (err) {
        RELEASE_ERRLOG("pthread_create failed!");
        RELEASE_BREAK();
    }
}

void timing_stopCPU(void) {
    cpu_shutting_down = true;

    LOG("Emulator waiting for CPU thread clean up...");
    if (pthread_join(cpu_thread_id, NULL)) {
        ERRLOG("OOPS: pthread_join of CPU thread ...");
    }
}

unsigned int CpuGetCyclesThisVideoFrame(void) {
    timing_checkpoint_cycles();
    return g_dwCyclesThisFrame + cycles_checkpoint_count;
}

// Called when an IO-reg is accessed & accurate global cycle count info is needed
void timing_checkpoint_cycles(void) {
    assert(pthread_self() == cpu_thread_id);

    const int32_t d = cpu65_cycle_count - cycles_checkpoint_count;
    assert(d >= 0);
    cycles_count_total += d;
    cycles_checkpoint_count = cpu65_cycle_count;
}

