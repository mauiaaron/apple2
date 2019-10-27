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

#ifndef _SPEAKER_H_
#define _SPEAKER_H_

// leaky detail : max amplitude should be <= SHRT_MAX/2 to not overflow/clip 16bit samples when simple additive mixing
// between speaker and mockingboard
#define SPKR_DATA_INIT (SHRT_MAX>>3) // 0x0FFF

void speaker_init(void) CALL_ON_CPU_THREAD;
void speaker_destroy(void) CALL_ON_CPU_THREAD;
void speaker_reset(void);
void speaker_flush(void) CALL_ON_CPU_THREAD;
bool speaker_isActive(void);
uint8_t speaker_toggle(void) CALL_ON_CPU_THREAD;

/*
 * returns the machine cycles per sample
 *  - for example, emulator running at normal speed: CLK_6502 / 44.1kHz == ~23
 */
double speaker_cyclesPerSample(void);

#if SPEAKER_TRACING
void speaker_traceBegin(const char *trace_file);
void speaker_traceFlush(void);
void speaker_traceEnd(void);
#endif

#endif /* whole file */

