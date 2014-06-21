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

#include "common.h"
#include "darwin-shim.h"
#include <mach/mach_time.h>

// Derived from http://stackoverflow.com/questions/5167269/clock-gettime-alternative-in-mac-os-x
// 2014/06/21

#define ORWL_NANO (+1.0E-9)
#define ORWL_GIGA UINT64_C(1000000000)

static double orwl_timebase = 0.0;
static uint64_t orwl_timestart = 0;

__attribute__((constructor))
static void __init_darwin_shim() {
    mach_timebase_info_data_t tb = { 0 };
    mach_timebase_info(&tb);
    orwl_timebase = tb.numer;
    orwl_timebase /= tb.denom;
    orwl_timestart = mach_absolute_time();
}

int clock_gettime(int clk_id, struct timespec *tp) {
    double diff = (mach_absolute_time() - orwl_timestart) * orwl_timebase;
    tp->tv_sec = diff * ORWL_NANO;
    tp->tv_nsec = diff - (tp->tv_sec * ORWL_GIGA);
    return 0;
}
