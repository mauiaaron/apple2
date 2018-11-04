/*
 * Apple // emulator for *ix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 2018 Aaron Culliney
 *
 */

#ifndef A2_NTSC_H
#define A2_NTSC_H

#include "common.h"

void ntsc_plotBits(color_mode_t mode, uint16_t bits, PIXEL_TYPE *fb_ptr);

void ntsc_flushScanline(void);

#endif /* A2_NTSC_H */

