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

/* Apple //e speaker support. Source inspired/derived from AppleWin.
 *
 *  - ~46 //e cycles per PC sample (played back at 22.050kHz) -- (CLK_6502/SPKR_SAMPLE_RATE)
 *
 * The soundcard output drives how much 6502 emulation is done in real-time.  If the soundcard buffer is running out of
 * sample-data, then more 6502 cycles need to be executed to top-up the buffer, and vice-versa.
 *
 * This is in contrast to the AY8910 voices (mockingboard/phasor), which can simply generate more data if their buffers
 * are running low.
 */

#include "common.h"

#define DEBUG_SPEAKER (!defined(NDEBUG) && 0) // enable to print timing stats
#if DEBUG_SPEAKER
#   define SPEAKER_LOG(...) LOG(__VA_ARGS__)
#else
#   define SPEAKER_LOG(...)
#endif

#define MAX_REMAINDER_BUFFER (((CLK_6502_INT*(unsigned int)CPU_SCALE_FASTEST)/SPKR_SAMPLE_RATE)+1)

#define SOUNDCORE_BUFFER_SIZE (MAX_SAMPLES*sizeof(int16_t)*1/*mono*/)
#define QUARTER_SIZE (SOUNDCORE_BUFFER_SIZE/4)
#define IDEAL_MIN (SOUNDCORE_BUFFER_SIZE/4) // minimum goldilocks zone for samples-in-play
#define IDEAL_MAX (SOUNDCORE_BUFFER_SIZE/2) // maximum goldilocks-zone for samples-in-play

static int16_t samples_buffer[SPKR_SAMPLE_RATE * sizeof(int16_t)] = { 0 }; // holds max 1 second of samples
static int16_t remainder_buffer[MAX_REMAINDER_BUFFER * sizeof(int16_t)] = { 0 }; // holds enough to create one sample (averaged)
static unsigned int samples_buffer_idx = 0;
static unsigned int remainder_buffer_size = 0;
static unsigned int remainder_buffer_idx = 0;

static int16_t speaker_amplitude = SPKR_DATA_INIT;
static int16_t speaker_data = 0;

static double cycles_per_sample = 0.0;
static unsigned long long cycles_last_update = 0;
static unsigned long long cycles_quiet_time = 0;

static bool speaker_accessed_since_last_flush = false;
static bool speaker_recently_active = false;

static bool speaker_going_silent = false;
static unsigned int speaker_silent_step = 0;

static int samples_adjustment_counter = 0;

static AudioBuffer_s *speakerBuffer = NULL;

// --------------------------------------------------------------------------------------------------------------------

/*
 * Because disk image loading is slow (AKA close-to-original-//e-speed), we may auto-switch to "fullspeed" for faster
 * loading when all the following heuristics hold true:
 *      - Disk motor is on
 *      - Speaker has not been toggled in some time (is not "active")
 *      - The graphics state is not "dirty"
 *
 * In fullspeed mode we output only quiet samples (zero-amplitude) at such a rate as to prevent the streaming audio from
 * either underflowing or overflowing.
 *
 * We will also auto-switch back to the last configured "scaled" speed when the speaker is toggled.
 */
static void _speaker_init_timing(void) {
    // 46.28 //e cycles for 22.05kHz sample rate

    // AppleWin NOTE : use integer value: Better for MJ Mahon's RT.SYNTH.DSK (integer multiples of 1.023MHz Clk)
    cycles_per_sample = (unsigned int)(cycles_persec_target / (double)SPKR_SAMPLE_RATE);

    unsigned int last_remainder_buffer_size = remainder_buffer_size;
    remainder_buffer_size = (unsigned int)cycles_per_sample;
    if ((double)remainder_buffer_size != cycles_per_sample) {
        ++remainder_buffer_size;
    }
    assert(remainder_buffer_size <= MAX_REMAINDER_BUFFER);

    if (last_remainder_buffer_size == remainder_buffer_size) {
        // no change ... insure seamless remainder_buffer
    } else {
        SPEAKER_LOG("changing remainder buffer size");
        remainder_buffer_idx = 0;
    }
    if (cycles_last_update > cycles_count_total) {
        SPEAKER_LOG("resetting speaker cycles counter");
        cycles_last_update = 0;
    }

    if (is_fullspeed) {
        remainder_buffer_idx = 0;
        samples_buffer_idx = 0;
    }
}

/*
 * Adds to the samples_buffer the number of samples since the last invocation of this function.
 *
 * Speaker output square wave example:
 *        _______             ____      _____________________!        . +speaker_amplitude
 *                                                    silence _       .
 *                                                  threshold  _      .
 *               _ remainder                                    _     .
 *                 average                                       _    .
 * _______        ____________    ______                          ___ . 0
 *
 *      - When the speaker is accessed by the emulated program, the output (speaker_data) is toggled between the
 *        positive amplitude or zero
 *      - Evenly-divisible samples since last function call (cycles_diff/cycles_per_sample) are put directly into the
 *        samples_buffer for output to audio system backend
 *      - (+) Fractional samples are put into a remainder_buffer to be averaged and then transfered to the sample_buffer
 *        when there is enough data for 1 whole sample (possibly on a subsequent invocation of this function)
 *      - (+) If the speaker has not been toggled with output at +amplitude for a certain number of machine cycles, we
 *        gradually step the samples down to the zero bound of true quiet.  (This is done to avoid glitching when
 *        pausing/unpausing emulation for GUI/menus and auto-switching between full and configured speeds)
 */
static void _speaker_update(/*bool toggled*/) {

    if (!is_fullspeed) {

        unsigned long long cycles_diff = cycles_count_total - cycles_last_update;

        if (remainder_buffer_idx) {
            // top-off previous previous fractional remainder cycles
            while (cycles_diff && (remainder_buffer_idx < remainder_buffer_size)) {
                remainder_buffer[remainder_buffer_idx] = speaker_data;
                ++remainder_buffer_idx;
                --cycles_diff;
            }

            if (remainder_buffer_idx == remainder_buffer_size) {
                // now have a complete extra sample from (previous + now) ... calculate mean value
                remainder_buffer_idx = 0;
                int sample_mean = 0;
                for (unsigned int i=0; i<remainder_buffer_size; i++) {
                    sample_mean += (int)remainder_buffer[i];
                }

                sample_mean /= (int)remainder_buffer_size;

                if (samples_buffer_idx < SPKR_SAMPLE_RATE) {
                    samples_buffer[samples_buffer_idx++] = (int16_t)sample_mean;
                }
            }
        }

        const unsigned long long samples_count = (unsigned long long)((double)cycles_diff / cycles_per_sample);
        unsigned long long num_samples = samples_count;
        const unsigned long long cycles_remainder = (unsigned long long)((double)cycles_diff - (double)num_samples * cycles_per_sample);

        // populate samples_buffer with whole samples
        while (num_samples && (samples_buffer_idx < SPKR_SAMPLE_RATE)) {
            samples_buffer[samples_buffer_idx++] = speaker_data;
            if (speaker_going_silent && speaker_data) {
                speaker_data -= speaker_silent_step;
            }
            --num_samples;
        }

        if (cycles_remainder > 0) {
            // populate remainder_buffer with fractional samples
            assert(remainder_buffer_idx == 0 && "should have already dealt with remainder buffer");
            assert(cycles_remainder < remainder_buffer_size && "otherwise there should have been another whole sample");

            while (remainder_buffer_idx<cycles_remainder) {
                remainder_buffer[remainder_buffer_idx] = speaker_data;
                ++remainder_buffer_idx;
            }
#if 0
        } else if (toggled && samples_count) {
            samples_buffer[samples_buffer_idx-1] = 0;
#endif
        }

        if (UNLIKELY(samples_buffer_idx >= SPKR_SAMPLE_RATE)) {
            assert(samples_buffer_idx == SPKR_SAMPLE_RATE && "should be at exactly the end, no further");
            ERRLOG("OOPS, overflow in speaker samples buffer");
        }
    }

    cycles_last_update = cycles_count_total;
}

/*
 * Submits "quiet" samples to the audio system backend when CPU thread is running fullspeed, to keep the audio streaming
 * topped up.
 *
 * 20150131 NOTE : it seems that there are still cases where we glitch OpenAL (seemingly on transitioning back to
 * regular speed sample submissions).  I have not been able to fully isolate the cause, but this has been minimized by
 * always trending the samples to the zero value when there has been a sufficient amount of speaker silence.
 */
static void _submit_samples_buffer_fullspeed(void) {
    samples_adjustment_counter = 0;

    unsigned long bytes_queued = 0;
    long err = speakerBuffer->GetCurrentPosition(speakerBuffer->_this, &bytes_queued, NULL);
    if (err) {
        return;
    }
    assert(bytes_queued <= SOUNDCORE_BUFFER_SIZE);

    if (bytes_queued >= IDEAL_MAX) {
        return;
    }

    unsigned int num_samples_pad = (IDEAL_MAX - bytes_queued) / sizeof(int16_t);
    if (num_samples_pad == 0) {
        return;
    }

    unsigned long system_buffer_size = 0;
    int16_t *system_samples_buffer = NULL;
    if (speakerBuffer->Lock(speakerBuffer->_this, /*unused*/ 0, num_samples_pad*sizeof(int16_t), &system_samples_buffer, &system_buffer_size, NULL, NULL, 0)) {
        return;
    }
    assert(num_samples_pad*sizeof(int16_t) <= system_buffer_size);

    //SPEAKER_LOG("bytes_queued:%d enqueueing %d quiet samples", bytes_queued, num_samples_pad);
    for (unsigned int i=0; i<num_samples_pad; i++) {
        system_samples_buffer[i] = speaker_data;
    }

    speakerBuffer->Unlock(speakerBuffer->_this, system_samples_buffer, system_buffer_size, NULL, 0);
}

// Submits samples from the samples_buffer to the audio system backend when running at a normal scaled-speed.  This also
// generates cycles feedback to the main CPU timing routine depending on the needs of the streaming audio (more or less
// data).
static unsigned int _submit_samples_buffer(const unsigned int num_samples) {

    assert(num_samples);

    unsigned long bytes_queued = 0;
    long err = speakerBuffer->GetCurrentPosition(speakerBuffer->_this, &bytes_queued, NULL);
    if (err) {
        return num_samples;
    }
    assert(bytes_queued <= SOUNDCORE_BUFFER_SIZE);

    //
    // calculate CPU cycles feedback adjustment to prevent system audio buffer under/overflow
    //

    if (bytes_queued < IDEAL_MIN) {
        samples_adjustment_counter += SOUNDCORE_ERROR_INC; // need moar data
    } else if (bytes_queued > IDEAL_MAX) {
        samples_adjustment_counter -= SOUNDCORE_ERROR_INC; // need less data
    } else {
        samples_adjustment_counter = 0; // Acceptable amount of data in buffer
    }

    if (samples_adjustment_counter < -SOUNDCORE_ERROR_MAX) {
        samples_adjustment_counter = -SOUNDCORE_ERROR_MAX;
    } else if (samples_adjustment_counter > SOUNDCORE_ERROR_MAX) {
        samples_adjustment_counter =  SOUNDCORE_ERROR_MAX;
    }

    cycles_speaker_feedback = (int)(samples_adjustment_counter * cycles_per_sample);

    //SPEAKER_LOG("feedback:%d samples_adjustment_counter:%d", cycles_speaker_feedback, samples_adjustment_counter);

    //
    // copy samples to audio system backend
    //

    const unsigned int bytes_free = SOUNDCORE_BUFFER_SIZE - bytes_queued;
    unsigned int num_samples_to_use = num_samples;

    if (num_samples_to_use * sizeof(int16_t) > bytes_free) {
        num_samples_to_use = bytes_free / sizeof(int16_t);
    }

    if (num_samples_to_use) {
        unsigned long system_buffer_size = 0;
        int16_t *system_samples_buffer = NULL;

        if (speakerBuffer->Lock(speakerBuffer->_this, /*unused*/0, (unsigned long)num_samples_to_use*sizeof(int16_t), &system_samples_buffer, &system_buffer_size, NULL, NULL, 0)) {
            return num_samples;
        }

        memcpy(system_samples_buffer, &samples_buffer[0], system_buffer_size);

        err = speakerBuffer->Unlock(speakerBuffer->_this, (void*)system_samples_buffer, system_buffer_size, NULL, 0);
        if (err) {
            return num_samples;
        }
    }

    return num_samples_to_use;
}

// --------------------------------------------------------------------------------------------------------------------
// speaker public API functions

void speaker_destroy(void) {
    if (speakerBuffer) {
        speakerBuffer->Stop(speakerBuffer->_this);
    }
    audio_destroySoundBuffer(&speakerBuffer);
}

void speaker_init(void) {
    long err = audio_createSoundBuffer(&speakerBuffer, SOUNDCORE_BUFFER_SIZE, SPKR_SAMPLE_RATE, 1);
    assert(!err);
    _speaker_init_timing();
}

void speaker_reset(void) {
    _speaker_init_timing();
}

void speaker_flush(void) {
    assert(pthread_self() == cpu_thread_id);

    if (is_fullspeed) {
        cycles_quiet_time = cycles_count_total;
        speaker_going_silent = false;
        speaker_accessed_since_last_flush = false;
    } else {
        if (speaker_accessed_since_last_flush) {
            cycles_quiet_time = 0;
            speaker_going_silent = false;
            speaker_accessed_since_last_flush = false;
        } else {
            if (!cycles_quiet_time) {
                cycles_quiet_time = cycles_count_total;
            }
            const unsigned int cycles_diff = (unsigned int)(cycles_persec_target/10); // 0.1sec of cycles
            if ((cycles_count_total - cycles_quiet_time) > cycles_diff*2) {
                // After 0.2sec of //e cycles time set inactive flag (allows auto-switch to full speed for fast disk access)
                speaker_recently_active = false;
                speaker_going_silent = false;
                speaker_data = 0;
            } else if ((speaker_data == speaker_amplitude) && (cycles_count_total - cycles_quiet_time > cycles_diff)) {
                // After 0.1sec of //e cycles time start reducing samples to zero (if they aren't there already).  This
                // process attempts to mask the extraneous clicks when freezing/restarting emulation (GUI access) and
                // glitching from the audio system backend
                speaker_going_silent = true;
                speaker_silent_step = 1;
                SPEAKER_LOG("speaker going silent");
            }
        }
    }
    _speaker_update(/*toggled:false*/);

    unsigned int samples_used = 0;
    if (is_fullspeed) {
        assert(!samples_buffer_idx && "should be all quiet samples");
        _submit_samples_buffer_fullspeed();
    } else if (samples_buffer_idx) {
        samples_used = _submit_samples_buffer(samples_buffer_idx);
    }
    assert(samples_used <= samples_buffer_idx);

    if (samples_used) {
        memmove(samples_buffer, &samples_buffer[samples_used], samples_buffer_idx-samples_used);
        samples_buffer_idx -= samples_used;
    }
}

bool speaker_is_active(void) {
    return speaker_recently_active;
}

void speaker_set_volume(int16_t amplitude) {
    speaker_amplitude = (amplitude/4);
}

double speaker_cycles_per_sample(void) {
    return cycles_per_sample;
}

// --------------------------------------------------------------------------------------------------------------------
// VM system entry point

GLUE_C_READ(speaker_toggle)
{
    assert(pthread_self() == cpu_thread_id);

    timing_checkpoint_cycles();

#if DIRECT_SPEAKER_ACCESS
#error this used to be implemented ...
    // AFAIK ... this requires an actual speaker device and ability to access it (usually requiring this program to be
    // running setuid-operator or (gasp) setuid-root ... maybe
#else
    speaker_accessed_since_last_flush = true;
    speaker_recently_active = true;

    if (timing_should_auto_adjust_speed()) {
        is_fullspeed = false;
    }

    _speaker_update(/*toggled:true*/);

    if (!is_fullspeed) {
        if (speaker_data) {
            speaker_data = 0;
        } else {
            speaker_data = speaker_amplitude;
        }
    }
#endif

    return floating_bus();
}

