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

#ifndef _CPU_REGS_H_
#define _CPU_REGS_H_

#include "common.h"

#ifdef __APPLE__

#define CLOCK_MONOTONIC 1

int clock_gettime(int, struct timespec *);

#endif // __APPLE__

#endif
