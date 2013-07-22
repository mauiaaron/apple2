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

#ifndef _TIMING_H_
#define _TIMING_H_

#define APPLE2_HZ      1020000
#define NANOSECONDS 1000000000

// 0 = run as fast as possible, 1 = approximate apple, X = 1/X rate
void timing_set_cpu_scale(unsigned int scale);

void timing_initialize();

void timing_throttle();

#endif // whole file
