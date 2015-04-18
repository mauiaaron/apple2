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

#ifndef _CPU_REGS_H_
#define _CPU_REGS_H_

#include "common.h"

#ifdef __APPLE__

#define CLOCK_MONOTONIC 1

int clock_gettime(int, struct timespec *);

#endif // __APPLE__

#endif