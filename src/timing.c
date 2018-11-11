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

#include "common.h"

#define DEBUG_TIMING 0 // enable to print timing stats
#if DEBUG_TIMING
#   define TIMING_LOG(...) LOG(__VA_ARGS__)
#else
#   define TIMING_LOG(...)
#endif

#define DISK_MOTOR_QUIET_NSECS 2000000

// cycle counting
double cycles_persec_target = CLK_6502;
unsigned long cycles_count_total = 0;           // Running at spec ~1MHz, this will approach overflow in ~4000secs (for 32bit architectures)
unsigned int cycles_video_frame = 0;
int cycles_speaker_feedback = 0;
static int32_t cycles_checkpoint_count = 0;

// scaling and speed adjustments
static bool auto_adjust_speed = true;
static bool is_paused = false;
static unsigned long _pause_spinLock = 0;

double cpu_scale_factor = 1.0;
double cpu_altscale_factor = 1.0;
bool is_fullspeed = false;
bool alt_speed_enabled = false;

// misc
static bool emul_reinitialize_audio = false;
static bool emul_pause_audio = false;
static bool emul_resume_audio = false;
static bool emul_video_dirty = false;
static bool cpu_shutting_down = false;
pthread_t cpu_thread_id = 0;
pthread_mutex_t interface_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t dbg_thread_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t cpu_thread_cond = PTHREAD_COND_INITIALIZER;

// -----------------------------------------------------------------------------

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
    is_fullspeed = (scale > CPU_SCALE_FASTEST_PIVOT);
    if (!is_fullspeed) {
        cycles_persec_target = CLK_6502 * scale;
    }
    speaker_reset();
    //TIMING_LOG("ClockRate:%0.2lf  ClockCyclesPerSpeakerSample:%0.2lf", cycles_persec_target, speaker_cyclesPerSample());
}

#if !TESTING
static
#endif
void reinitialize(void) {
#if !TESTING
    ASSERT_ON_CPU_THREAD();
#endif

    cycles_count_total = 0;
    cycles_video_frame = 0;
#if !TEST_CPU
    video_scannerReset();
#endif

#if TESTING
    extern unsigned long (*testing_getCyclesCount)(void);
    if (testing_getCyclesCount) {
        cycles_count_total = testing_getCyclesCount();
    }
#endif

    vm_initialize();

    cpu65_init();

    timing_initialize();

    MB_Reset();
}

void timing_initialize(void) {
#if !TESTING
    assert(cpu_isPaused() || (pthread_self() == cpu_thread_id));
#endif
    _timing_initialize(alt_speed_enabled ? cpu_altscale_factor : cpu_scale_factor);
}

void timing_toggleCPUSpeed(void) {
    assert(cpu_isPaused() || (pthread_self() == cpu_thread_id));
    alt_speed_enabled = !alt_speed_enabled;
    timing_initialize();
}

static void timing_reinitializeAudio(void) {
    SPINLOCK_ACQUIRE(&_pause_spinLock);
    ASSERT_NOT_ON_CPU_THREAD();
#if !TESTING
    assert(cpu_isPaused());
#endif
    emul_reinitialize_audio = true;
    emul_pause_audio = false;
    emul_resume_audio = false;
    emul_video_dirty = false;
    SPINLOCK_RELINQUISH(&_pause_spinLock);
}

void cpu_pause(void) {
    ASSERT_NOT_ON_CPU_THREAD();

    SPINLOCK_ACQUIRE(&_pause_spinLock);
    do {
        if (is_paused) {
            break;
        }

        // CPU thread will be paused when it next tries to acquire interface_mutex
        LOG("PAUSING CPU...");
        if (!emul_reinitialize_audio) {
            emul_pause_audio = true;
        }
        pthread_mutex_lock(&interface_mutex);
        is_paused = true;
    } while (0);
    SPINLOCK_RELINQUISH(&_pause_spinLock);
}

void cpu_resume(void) {
    ASSERT_NOT_ON_CPU_THREAD();

    SPINLOCK_ACQUIRE(&_pause_spinLock);
    do {
        if (!is_paused) {
            break;
        }

        // CPU thread will be unblocked to acquire interface_mutex
        if (!emul_reinitialize_audio) {
            emul_resume_audio = true;
            emul_video_dirty = true;
        }
        LOG("RESUMING CPU...");
        is_paused = false;
        pthread_mutex_unlock(&interface_mutex);
    } while (0);
    SPINLOCK_RELINQUISH(&_pause_spinLock);
}

bool cpu_isPaused(void) {
    return is_paused;
}

bool timing_shouldAutoAdjustSpeed(void) {
    double speed = alt_speed_enabled ? cpu_altscale_factor : cpu_scale_factor;
    return auto_adjust_speed && (speed <= CPU_SCALE_FASTEST_PIVOT);
}

static void *cpu_thread(void *dummyptr) {

#ifndef NDEBUG // Spamsung Galaxy Y running Gingerbread triggers this, wTf?!
    ASSERT_ON_CPU_THREAD();
#endif

    LOG("cpu_thread : initialized...");

    struct timespec deltat = { 0 };
    struct timespec disk_motor_time = { 0 };
    struct timespec t0 = { 0 }; // the target timer
    struct timespec ti = { 0 }; // actual before time sample
    struct timespec tj = { 0 }; // actual after time sample
    bool negative = false;
    long drift_adj_nsecs = 0;   // generic drift adjustment between target and actual

    int debugging_cycles = 0;

    unsigned long dbg_ticks = 0;
#if DEBUG_TIMING
    int speaker_neg_feedback = 0;
    int speaker_pos_feedback = 0;
    unsigned long dbg_cycles_executed = 0;
#endif

    audio_init();
    speaker_init();
    MB_Initialize();

    run_args.emul_reinitialize = 1;

cpu_runloop:
    do {
        LOG("CPUTHREAD %lu LOCKING FOR MAYBE INITIALIZING AUDIO ...", (unsigned long)cpu_thread_id);
        pthread_mutex_lock(&interface_mutex);
        if (emul_reinitialize_audio) {
            emul_reinitialize_audio = false;

            speaker_destroy();
            extern void MB_SoftDestroy(void);
            MB_SoftDestroy();
            audio_shutdown();

            audio_init();
            speaker_init();
            extern void MB_SoftInitialize(void);
            MB_SoftInitialize();
        }
        pthread_mutex_unlock(&interface_mutex);
        LOG("UNLOCKING FOR MAYBE INITIALIZING AUDIO ...");

        if (run_args.emul_reinitialize) {
            reinitialize();
        }

        LOG("cpu_thread : begin main loop ...");

        clock_gettime(CLOCK_MONOTONIC, &t0);

        do {
            SCOPE_TRACE_CPU("CPU mainloop");
            // -LOCK----------------------------------------------------------------------------------------- SAMPLE ti
            if (UNLIKELY(emul_pause_audio)) {
                emul_pause_audio = false;
                audio_pause();
            }
            pthread_mutex_lock(&interface_mutex);
            if (UNLIKELY(emul_resume_audio)) {
                emul_resume_audio = false;
                audio_resume();
            }
            if (UNLIKELY(emul_video_dirty)) {
                emul_video_dirty = false;
                video_setDirty(A2_DIRTY_FLAG);
            }
            clock_gettime(CLOCK_MONOTONIC, &ti);

            deltat = timespec_diff(t0, ti, &negative);
            if (deltat.tv_sec) {
                if (!is_fullspeed) {
                    TIMING_LOG("NOTE : serious divergence from target time ...");
                }
                t0 = ti;
                deltat = timespec_diff(t0, ti, &negative);
            }
            t0 = timespec_add(t0, EXECUTION_PERIOD_NSECS); // expected interval
            drift_adj_nsecs = negative ? ~deltat.tv_nsec : deltat.tv_nsec;

            // set up increment & decrement counters
            run_args.cpu65_cycles_to_execute = (cycles_persec_target / 1000); // cycles_persec_target * EXECUTION_PERIOD_NSECS / NANOSECONDS_PER_SECOND
            if (!is_fullspeed) {
                run_args.cpu65_cycles_to_execute += cycles_speaker_feedback;
            }
            if (run_args.cpu65_cycles_to_execute < 0) {
                run_args.cpu65_cycles_to_execute = 0;
            }

            MB_StartOfCpuExecute();
            if (is_debugging) {
                debugging_cycles = run_args.cpu65_cycles_to_execute;
            }

            do {
                if (is_debugging) {
                    run_args.cpu65_cycles_to_execute = 1;
                }

                run_args.cpu65_cycle_count = 0;
                cycles_checkpoint_count = 0;

                cpu65_run(&run_args); // run emulation for cpu65_cycles_to_execute cycles ...

#if DEBUG_TIMING
                dbg_cycles_executed += run_args.cpu65_cycle_count;
#endif

                if (is_debugging) {
                    debugging_cycles -= run_args.cpu65_cycle_count;
                    timing_checkpointCycles();

                    if (c_debugger_should_break() || (debugging_cycles <= 0)) {
                        int err = 0;
                        if ((err = pthread_cond_signal(&dbg_thread_cond))) {
                            LOG("pthread_cond_signal : %d", err);
                        }
                        if ((err = pthread_cond_wait(&cpu_thread_cond, &interface_mutex))) {
                            LOG("pthread_cond_wait : %d", err);
                        }
                        if (debugging_cycles <= 0) {
                            break;
                        }
                    }

                    if (run_args.emul_reinitialize) {
                        pthread_mutex_unlock(&interface_mutex);
                        goto cpu_runloop;
                    }
                }
            } while (is_debugging);

            MB_UpdateCycles();
            // TODO : modularize MB and other peripheral card cycles/interrupts ...

            speaker_flush(); // play audio

            video_scannerUpdate();

            clock_gettime(CLOCK_MONOTONIC, &tj);
            pthread_mutex_unlock(&interface_mutex);
            // -UNLOCK--------------------------------------------------------------------------------------- SAMPLE tj

            if (timing_shouldAutoAdjustSpeed()) {
                disk_motor_time = timespec_diff(disk6.motor_time, tj, &negative);
                if (UNLIKELY(negative)) {
                    // 2016/05/05 : crash report from the wild on Android if we assert(!negative)
                    LOG("WHOA... time went backwards #1! Did you just cross a timezone?");
                    disk_motor_time.tv_sec = 1;
                }
                if (!is_fullspeed &&
                        !speaker_isActive() &&
                        !video_isDirty(A2_DIRTY_FLAG) && (!disk6.motor_off && (disk_motor_time.tv_sec || disk_motor_time.tv_nsec > DISK_MOTOR_QUIET_NSECS)) )
                {
                    TIMING_LOG("auto switching to full speed");
                    _timing_initialize(CPU_SCALE_FASTEST);
                }
            }

            if (!is_fullspeed) {
                deltat = timespec_diff(ti, tj, &negative);
                if (UNLIKELY(negative)) {
                    // 2016/05/05 : crash report from the wild on Android if we assert(!negative)
                    LOG("WHOA... time went backwards #2! Did you just cross a timezone?");
                    deltat.tv_sec = 1;
                }
                long sleepfor = 0;
                if (LIKELY(!deltat.tv_sec))
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

                if ((dbg_ticks % NANOSECONDS_PER_SECOND) == 0)
                {
                    TIMING_LOG("tick:(%ld.%ld) real:(%ld.%ld) cycles exe: %d ... speaker feedback: %d/%d", t0.tv_sec, t0.tv_nsec, ti.tv_sec, ti.tv_nsec, dbg_cycles_executed, speaker_neg_feedback, speaker_pos_feedback);
                    dbg_cycles_executed = 0;
                    speaker_neg_feedback = 0;
                    speaker_pos_feedback = 0;
                }
#endif
                if ((dbg_ticks % NANOSECONDS_PER_SECOND) == 0) {
                    dbg_ticks = 0;
                }
            }

            if (timing_shouldAutoAdjustSpeed()) {
                if (is_fullspeed && (
                            speaker_isActive() ||
                            video_isDirty(A2_DIRTY_FLAG) || (disk6.motor_off && (disk_motor_time.tv_sec || disk_motor_time.tv_nsec > DISK_MOTOR_QUIET_NSECS))) )
                {
                    double speed = alt_speed_enabled ? cpu_altscale_factor : cpu_scale_factor;
                    if (speed <= CPU_SCALE_FASTEST_PIVOT) {
                        TIMING_LOG("auto switching to configured speed");
                        _timing_initialize(speed);
                    }
                }
            }

            if (UNLIKELY(run_args.emul_reinitialize)) {
                break;
            }

            if (UNLIKELY(emul_reinitialize_audio)) {
                break;
            }

            if (UNLIKELY(cpu_shutting_down)) {
                break;
            }
        } while (1);

        if (UNLIKELY(cpu_shutting_down)) {
            break;
        }
    } while (1);

    speaker_destroy();
    MB_Destroy();
    audio_shutdown();

    cpu_thread_id = 0;
    cpu_pause();

    disk6_eject(0);
    disk6_eject(1);

    return NULL;
}

bool timing_isCPUThread(void) {
    return pthread_self() == cpu_thread_id;
}

void timing_startCPU(void) {
    cpu_shutting_down = false;
    int err = TEMP_FAILURE_RETRY(pthread_create(&cpu_thread_id, NULL, (void *)&cpu_thread, (void *)NULL));
    if (err) {
        LOG("pthread_create failed!");
        assert(false);
    }
}

void timing_stopCPU(void) {
    cpu_shutting_down = true;

    LOG("Emulator waiting for CPU thread clean up...");
    if (pthread_join(cpu_thread_id, NULL)) {
        LOG("OOPS: pthread_join of CPU thread ...");
    }
}

// Called when accurate global cycle count info is needed
void timing_checkpointCycles(void) {
    ASSERT_ON_CPU_THREAD();

    const int32_t d = run_args.cpu65_cycle_count - cycles_checkpoint_count;
    assert(d >= 0);
    cycles_video_frame += d;
#if !TESTING
    cycles_count_total += d;
#else
    unsigned long previous_cycles_count_total = cycles_count_total;
    cycles_count_total += d;
    if (UNLIKELY(cycles_count_total < previous_cycles_count_total)) {
        extern void (*testing_cyclesOverflow)(void);
        if (testing_cyclesOverflow) {
            testing_cyclesOverflow();
        }
    }
#endif
    cycles_checkpoint_count = run_args.cpu65_cycle_count;
}

// ----------------------------------------------------------------------------

bool timing_saveState(StateHelper_s *helper) {
    bool saved = false;
    int fd = helper->fd;

    do {
        uint32_t lVal = 0;
        uint8_t serialized[4] = { 0 };

        assert(cpu_scale_factor >= 0);
        assert(cpu_altscale_factor >= 0);

        lVal = (cpu_scale_factor * 100.);
        serialized[0] = (uint8_t)((lVal & 0xFF000000) >> 24);
        serialized[1] = (uint8_t)((lVal & 0xFF0000  ) >> 16);
        serialized[2] = (uint8_t)((lVal & 0xFF00    ) >>  8);
        serialized[3] = (uint8_t)((lVal & 0xFF      ) >>  0);
        if (!helper->save(fd, serialized, sizeof(serialized))) {
            break;
        }

        lVal = (cpu_altscale_factor * 100.);
        serialized[0] = (uint8_t)((lVal & 0xFF000000) >> 24);
        serialized[1] = (uint8_t)((lVal & 0xFF0000  ) >> 16);
        serialized[2] = (uint8_t)((lVal & 0xFF00    ) >>  8);
        serialized[3] = (uint8_t)((lVal & 0xFF      ) >>  0);
        if (!helper->save(fd, serialized, sizeof(serialized))) {
            break;
        }

        uint8_t bVal = alt_speed_enabled ? 1 : 0;
        if (!helper->save(fd, &bVal, sizeof(bVal))) {
            break;
        }

        saved = true;
    } while (0);

    return saved;
}

bool timing_loadState(StateHelper_s *helper) {
    bool loaded = false;
    int fd = helper->fd;

    do {
        uint32_t lVal = 0;
        uint8_t serialized[4] = { 0 };

        if (!helper->load(fd, serialized, sizeof(uint32_t))) {
            break;
        }
        lVal  = (uint32_t)(serialized[0] << 24);
        lVal |= (uint32_t)(serialized[1] << 16);
        lVal |= (uint32_t)(serialized[2] <<  8);
        lVal |= (uint32_t)(serialized[3] <<  0);
        cpu_scale_factor = lVal / 100.;

        if (!helper->load(fd, serialized, sizeof(uint32_t))) {
            break;
        }
        lVal  = (uint32_t)(serialized[0] << 24);
        lVal |= (uint32_t)(serialized[1] << 16);
        lVal |= (uint32_t)(serialized[2] <<  8);
        lVal |= (uint32_t)(serialized[3] <<  0);
        cpu_altscale_factor = lVal / 100.;

        uint8_t bVal = 0;
        if (!helper->load(fd, &bVal, sizeof(bVal))) {
            break;
        }
        alt_speed_enabled = !!bVal;

        timing_initialize();

        loaded = true;
    } while (0);

    return loaded;
}

// ----------------------------------------------------------------------------

static void timing_prefsChanged(const char *domain) {
    (void)domain;

    float fVal = 1.0;

    cpu_scale_factor = prefs_parseFloatValue(PREF_DOMAIN_VM, PREF_CPU_SCALE, &fVal) ?  fVal / 100.f : 1.f;
    if (cpu_scale_factor < CPU_SCALE_SLOWEST) {
        cpu_scale_factor = CPU_SCALE_SLOWEST;
    }
    if (cpu_scale_factor > CPU_SCALE_FASTEST_PIVOT) {
        cpu_scale_factor = CPU_SCALE_FASTEST;
    }
    cpu_altscale_factor = prefs_parseFloatValue(PREF_DOMAIN_VM, PREF_CPU_SCALE_ALT, &fVal) ?  fVal / 100.f : 1.f;
    if (cpu_altscale_factor < CPU_SCALE_SLOWEST) {
        cpu_altscale_factor = CPU_SCALE_SLOWEST;
    }
    if (cpu_altscale_factor > CPU_SCALE_FASTEST_PIVOT) {
        cpu_altscale_factor = CPU_SCALE_FASTEST;
    }

    static float audioLatency = 0.f;
    float latency = prefs_parseFloatValue(PREF_DOMAIN_AUDIO, PREF_AUDIO_LATENCY, &fVal) ? fVal : 0.25f;
#define SMALL_EPSILON (1.f/1024.f)
    if (fabsf(audioLatency - latency) > SMALL_EPSILON) {
        audioLatency = latency;
        audio_setLatency(latency);
        timing_reinitializeAudio();
    }

    static bool mbEnabled = false;
    bool bVal = false;
    bool enabled = prefs_parseBoolValue(PREF_DOMAIN_AUDIO, PREF_MOCKINGBOARD_ENABLED, &bVal) ? bVal : true;
    if (enabled != mbEnabled) {
        mbEnabled = enabled;
        MB_SetEnabled(enabled);
        timing_reinitializeAudio();
    }

    auto_adjust_speed = prefs_parseBoolValue(PREF_DOMAIN_INTERFACE, PREF_DISK_FAST_LOADING, &bVal) ? bVal : true;
}

static __attribute__((constructor)) void _init_timing(void) {
    prefs_registerListener(PREF_DOMAIN_VM, &timing_prefsChanged);
    prefs_registerListener(PREF_DOMAIN_AUDIO, &timing_prefsChanged);
    prefs_registerListener(PREF_DOMAIN_INTERFACE, &timing_prefsChanged);
}

