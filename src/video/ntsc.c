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

// NOTE: routines here are copied from William (Sheldon) Simms's NTSC implementation in AppleWin
// TODO: implement these in a shader?

/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 2010-2011, William S Simms
Copyright (C) 2014-2016, Michael Pohoreski, Tom Charlesworth

AppleWin is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

AppleWin is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AppleWin; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "common.h"
#include "video/ntsc.h"

#define NTSC_NUM_PHASES     4
#define NTSC_NUM_SEQUENCES  4096

typedef void (*fb_plotter_fn)(color_mode_t, uint16_t, PIXEL_TYPE *);
typedef PIXEL_TYPE (*fb_scanline_fn)(PIXEL_TYPE color0, PIXEL_TYPE color2);

static uint8_t half_scanlines = 1;
static fb_plotter_fn pixelPlotter[NUM_COLOROPTS] = { NULL };
static fb_scanline_fn getHalfColor[NUM_COLOROPTS][2] = { { NULL } };

static unsigned int ntsc_color_phase = 0;
unsigned int ntsc_signal_bits = 0;

static PIXEL_TYPE monoPixelsMonitor [NTSC_NUM_SEQUENCES];
static PIXEL_TYPE monoPixelsTV      [NTSC_NUM_SEQUENCES];

static PIXEL_TYPE colorPixelsMonitor[NTSC_NUM_PHASES][NTSC_NUM_SEQUENCES];
static PIXEL_TYPE colorPixelsTV     [NTSC_NUM_PHASES][NTSC_NUM_SEQUENCES];

// ----------------------------------------------------------------------------
// TV pixel updates

static PIXEL_TYPE halfScanlineTV(PIXEL_TYPE color0, PIXEL_TYPE color2) {
#if APPLE2IX
    (void)color2;
    PIXEL_TYPE color1 = ((color0 & 0xFEFEFEFE) >> 1) | (0xFF << SHIFT_A);
    return color1;
#else
    // HACK NOTE : original code for sampling color2 from 2 scanlines below requires at least 2.X full CYCLES_FRAME iterations
    //      - first pass does not pick up potentially changed color2
    //      - second pass will pick up correct color2
    //      - this leads to "interesting" artifacts for moving sprites
#error FIXME TODO ...
    PIXEL_TYPE color1;

    int r = (color0 >> SHIFT_R) & 0xFF;
    int g = (color0 >> SHIFT_G) & 0xFF;
    int b = (color0 >> SHIFT_B) & 0xFF;
    uint32_t color2_prime = (color2 & 0xFCFCFCFC) >> 2;
    r -= (color2_prime >> SHIFT_R) & 0xFF; if (r<0) { r=0; }    // clamp to 0 on underflow
    g -= (color2_prime >> SHIFT_G) & 0xFF; if (g<0) { g=0; }    // clamp to 0 on underflow
    b -= (color2_prime >> SHIFT_B) & 0xFF; if (b<0) { b=0; }    // clamp to 0 on underflow
    color1 = (r<<SHIFT_R) | (g<<SHIFT_G) | (b<<SHIFT_B) | (0xFF<<SHIFT_A);

    return color1;
#endif
}

static PIXEL_TYPE doubleScanlineTV(PIXEL_TYPE color0, PIXEL_TYPE color2) {
#if APPLE2IX
    (void)color2;
    return color0;
#else
#error FIXME TODO ...
    PIXEL_TYPE color1 = ((color0 & 0xFEFEFEFE) >> 1) + ((color2 & 0xFEFEFEFE) >> 1); // 50% Blend
    return color1 | (0xFF<<SHIFT_A);
#endif
}

static void updateFBCommon(color_mode_t mode, uint16_t signal, PIXEL_TYPE *fb_ptr, PIXEL_TYPE *color_table) {
    PIXEL_TYPE *fb_ptr0 = fb_ptr;
    PIXEL_TYPE *fb_ptr1 = fb_ptr + (SCANWIDTH<<0);
    PIXEL_TYPE *fb_ptr2 = fb_ptr + (SCANWIDTH<<1);

    PIXEL_TYPE color0;
    {
        ntsc_signal_bits = ((ntsc_signal_bits << 1) | signal) & 0xFFF; // 12-bit ?
        color0 = color_table[ntsc_signal_bits];
    }
    PIXEL_TYPE color2 = *fb_ptr2;
    PIXEL_TYPE color1 = getHalfColor[mode][half_scanlines](color0, color2);

    *fb_ptr0 = color0;
    *fb_ptr1 = color1;

    ntsc_color_phase = (ntsc_color_phase + 1) & 0x03;
}

static void updateFBMonoTV(color_mode_t mode, uint16_t signal, PIXEL_TYPE *fb_ptr) {
    updateFBCommon(mode, signal, fb_ptr, monoPixelsTV);
}

static void updateFBColorTV(color_mode_t mode, uint16_t signal, PIXEL_TYPE *fb_ptr) {
    updateFBCommon(mode, signal, fb_ptr, colorPixelsTV[ntsc_color_phase]);
}

// ----------------------------------------------------------------------------
// Monitor pixel updates

static PIXEL_TYPE halfScanlineMonitor(PIXEL_TYPE color0, PIXEL_TYPE color2) {
    (void)color2;
    PIXEL_TYPE color1 = ((color0 & 0xFEFEFEFE) >> 1) | (0xFF << SHIFT_A);
    return color1;
}

static PIXEL_TYPE doubleScanlineMonitor(PIXEL_TYPE color0, PIXEL_TYPE color2) {
    (void)color2;
    return color0;
}

static void updateFBMonoMonitor(color_mode_t mode, uint16_t signal, PIXEL_TYPE *fb_ptr) {
    updateFBCommon(mode, signal, fb_ptr, monoPixelsMonitor);
}

static void updateFBColorMonitor(color_mode_t mode, uint16_t signal, PIXEL_TYPE *fb_ptr) {
    updateFBCommon(mode, signal, fb_ptr, colorPixelsMonitor[ntsc_color_phase]);
}

// ----------------------------------------------------------------------------

void ntsc_plotBits(color_mode_t mode, uint16_t bits14, PIXEL_TYPE *fb_ptr) {
    assert(!(mode == COLOR_MODE_COLOR || mode == COLOR_MODE_INTERP));

    pixelPlotter[mode](mode, bits14 & 1, fb_ptr); bits14 >>= 1; ++fb_ptr;
    pixelPlotter[mode](mode, bits14 & 1, fb_ptr); bits14 >>= 1; ++fb_ptr;

    pixelPlotter[mode](mode, bits14 & 1, fb_ptr); bits14 >>= 1; ++fb_ptr;
    pixelPlotter[mode](mode, bits14 & 1, fb_ptr); bits14 >>= 1; ++fb_ptr;

    pixelPlotter[mode](mode, bits14 & 1, fb_ptr); bits14 >>= 1; ++fb_ptr;
    pixelPlotter[mode](mode, bits14 & 1, fb_ptr); bits14 >>= 1; ++fb_ptr;

    pixelPlotter[mode](mode, bits14 & 1, fb_ptr); bits14 >>= 1; ++fb_ptr;
    pixelPlotter[mode](mode, bits14 & 1, fb_ptr); bits14 >>= 1; ++fb_ptr;

    pixelPlotter[mode](mode, bits14 & 1, fb_ptr); bits14 >>= 1; ++fb_ptr;
    pixelPlotter[mode](mode, bits14 & 1, fb_ptr); bits14 >>= 1; ++fb_ptr;

    pixelPlotter[mode](mode, bits14 & 1, fb_ptr); bits14 >>= 1; ++fb_ptr;
    pixelPlotter[mode](mode, bits14 & 1, fb_ptr); bits14 >>= 1; ++fb_ptr;

    pixelPlotter[mode](mode, bits14 & 1, fb_ptr); bits14 >>= 1; ++fb_ptr;
    pixelPlotter[mode](mode, bits14 & 1, fb_ptr);               ++fb_ptr;
}

void ntsc_flushScanline(color_mode_t mode, PIXEL_TYPE *fb_ptr) {

    pixelPlotter[mode](mode, 0, fb_ptr); ++fb_ptr;
    pixelPlotter[mode](mode, 0, fb_ptr); ++fb_ptr;
    pixelPlotter[mode](mode, 0, fb_ptr); ++fb_ptr;
    pixelPlotter[mode](mode, 0, fb_ptr); ++fb_ptr;

    ntsc_color_phase = 0;
    ntsc_signal_bits = 0;
}

//----------------------------------------------------------------------------
// pixel color creation

#define CHROMA_ZEROS 2
#define CHROMA_POLES 2
#define CHROMA_GAIN  7.438011255f // Should this be 7.15909 MHz ?
#define CHROMA_0    -0.7318893645f
#define CHROMA_1     1.2336442711f

#define LUMA_ZEROS  2
#define LUMA_POLES  2
#define LUMA_GAIN  13.71331570f   // Should this be 14.318180 MHz ?
#define LUMA_0     -0.3961075449f
#define LUMA_1      1.1044202472f

#define SIGNAL_ZEROS 2
#define SIGNAL_POLES 2
#define SIGNAL_GAIN  7.614490548f  // Should this be 7.15909 MHz ?
#define SIGNAL_0    -0.2718798058f
#define SIGNAL_1     0.7465656072f

#if !defined(M_PI)
#   error M_PI not defined on this platform?
#   define M_PI 3.1415926535898f
#endif

#define DEG_TO_RAD(x) (M_PI*(x)/180.f)
#define RAD_45  (M_PI*0.25f)
#define RAD_90  (M_PI*0.5f)
#define RAD_360 (M_PI*2.f)

#define CYCLESTART (DEG_TO_RAD(45))

#define clampZeroOne(x) ({ \
    typeof(x) _rc = x; \
    if (_rc < 0.f) { _rc = 0.f; } \
    if (_rc > 1.f) { _rc = 1.f; } \
    _rc; \
})

// What filter is this ??
// Filter Order: 2 -> poles for low pass
static double initFilterChroma(double z) {
    static double x[CHROMA_ZEROS + 1] = {0,0,0};
    static double y[CHROMA_POLES + 1] = {0,0,0};

    x[0] = x[1];   x[1] = x[2];   x[2] = z / CHROMA_GAIN;
    y[0] = y[1];   y[1] = y[2];   y[2] = -x[0] + x[2] + (CHROMA_0*y[0]) + (CHROMA_1*y[1]); // inverted x[0]

    return y[2];
}

// Butterworth Lowpass digital filter
// Filter Order: 2 -> poles for low pass
static double initFilterLuma0(double z) {
    static double x[LUMA_ZEROS + 1] = { 0,0,0 };
    static double y[LUMA_POLES + 1] = { 0,0,0 };

    x[0] = x[1];   x[1] = x[2];   x[2] = z / LUMA_GAIN;
    y[0] = y[1];   y[1] = y[2];   y[2] = x[0] + x[2] + (2.f*x[1]) + (LUMA_0*y[0]) + (LUMA_1*y[1]);

    return y[2];
}

// Butterworth Lowpass digital filter
// Filter Order: 2 -> poles for low pass
static double initFilterLuma1(double z) {
    static double x[LUMA_ZEROS + 1] = { 0,0,0};
    static double y[LUMA_POLES + 1] = { 0,0,0};

    x[0] = x[1];   x[1] = x[2];   x[2] = z / LUMA_GAIN;
    y[0] = y[1];   y[1] = y[2];   y[2] = x[0] + x[2] + (2.f*x[1]) + (LUMA_0*y[0]) + (LUMA_1*y[1]);

    return y[2];
}

// Butterworth Lowpass digital filter
// Filter Order: 2 -> poles for low pass
static double initFilterSignal(double z) {
    static double x[SIGNAL_ZEROS + 1] = { 0,0,0 };
    static double y[SIGNAL_POLES + 1] = { 0,0,0 };

    x[0] = x[1];   x[1] = x[2];   x[2] = z / SIGNAL_GAIN;
    y[0] = y[1];   y[1] = y[2];   y[2] = x[0] + x[2] + (2.f*x[1]) + (SIGNAL_0*y[0]) + (SIGNAL_1*y[1]);

    return y[2];
}

// Build the 4 phase chroma lookup table
// The YI'Q' colors are hard-coded
static void initChromaPhaseTables(color_mode_t color_mode, mono_mode_t mono_mode) {
    int phase,s,t,n;
    double z = 0;
    double y0,y1,c,i,q = 0;
    double phi,zz;
    float brightness;
    double r64,g64,b64;
    float  r32,g32,b32;

    for (phase = 0; phase < 4; phase++) {
        phi = (phase * RAD_90) + CYCLESTART;
        for (s = 0; s < NTSC_NUM_SEQUENCES; s++) {
            t = s;
            y0 = y1 = c = i = q = 0.0;

            for (n = 0; n < 12; n++) {
                z = (double)(0 != (t & 0x800));
                t = t << 1;

                for (unsigned int k = 0; k < 2; k++) {
                    //z = z * 1.25;
                    zz = initFilterSignal(z);
                    c  = initFilterChroma(zz); // "Mostly" correct _if_ CYCLESTART = PI/4 = 45 degrees
                    y0 = initFilterLuma0 (zz);
                    y1 = initFilterLuma1 (zz - c);

                    c = c * 2.f;
                    i = i + (c * cos(phi) - i) / 8.f;
                    q = q + (c * sin(phi) - q) / 8.f;

                    phi += RAD_45;
                } // k
            } // samples

            brightness = clampZeroOne((float)z);
            if (color_mode == COLOR_MODE_MONO && mono_mode == MONO_MODE_GREEN) {
                monoPixelsMonitor[s] = (((uint8_t)(brightness * 0xFF)) << SHIFT_G) | (0x00 << SHIFT_R) | (0x00 << SHIFT_B) | (0xFF << SHIFT_A);
                brightness = clampZeroOne((float)y1);
                monoPixelsTV[s] =      (((uint8_t)(brightness * 0xFF)) << SHIFT_G) | (0x00 << SHIFT_R) | (0x00 << SHIFT_B) | (0xFF << SHIFT_A);
            } else {
                monoPixelsMonitor[s] = (((uint8_t)(brightness * 0xFF)) << SHIFT_R) |
                                       (((uint8_t)(brightness * 0xFF)) << SHIFT_G) |
                                       (((uint8_t)(brightness * 0xFF)) << SHIFT_B) |
                                                               (0xFF   << SHIFT_A);

                brightness = clampZeroOne((float)y1);
                monoPixelsTV[s] =      (((uint8_t)(brightness * 0xFF)) << SHIFT_R) |
                                       (((uint8_t)(brightness * 0xFF)) << SHIFT_G) |
                                       (((uint8_t)(brightness * 0xFF)) << SHIFT_B) |
                                                               (0xFF   << SHIFT_A);
            }

            /*
                YI'V' to RGB
                [r g b] = [y i v][ 1      1      1    ]
                                 [0.956  -0.272 -1.105]
                                 [0.621  -0.647  1.702]
                [r]   [1   0.956  0.621][y]
                [g] = [1  -0.272 -0.647][i]
                [b]   [1  -1.105  1.702][v]
            */
            #define I_TO_R  0.956f
            #define I_TO_G -0.272f
            #define I_TO_B -1.105f

            #define Q_TO_R  0.621f
            #define Q_TO_G -0.647f
            #define Q_TO_B  1.702f

            r64 = y0 + (I_TO_R * i) + (Q_TO_R * q);
            g64 = y0 + (I_TO_G * i) + (Q_TO_G * q);
            b64 = y0 + (I_TO_B * i) + (Q_TO_B * q);

            r32 = clampZeroOne((float)r64);
            g32 = clampZeroOne((float)g64);
            b32 = clampZeroOne((float)b64);

            int color = s & 15;

            // NTSC_REMOVE_WHITE_RINGING
            if (color == IDX_WHITE) {
                r32 = 1;
                g32 = 1;
                b32 = 1;
            }

            // NTSC_REMOVE_BLACK_GHOSTING
            if (color == IDX_BLACK) {
                r32 = 0;
                g32 = 0;
                b32 = 0;
            }

            // NTSC_REMOVE_GRAY_CHROMA
            if (color == IDX_DARKGREY) {
                const float g = (float) 0x83 / (float) 0xFF;
                r32 = g;
                g32 = g;
                b32 = g;
            }

            if (color == IDX_LIGHTGREY) { // Gray2 & Gray1
                const float g = (float) 0x78 / (float) 0xFF;
                r32 = g;
                g32 = g;
                b32 = g;
            }

            colorPixelsMonitor[phase][s] = (((uint8_t)(r32 * 255)) << SHIFT_R) |
                                           (((uint8_t)(g32 * 255)) << SHIFT_G) |
                                           (((uint8_t)(b32 * 255)) << SHIFT_B) |
                                                             (0xFF << SHIFT_A);

            r64 = y1 + (I_TO_R * i) + (Q_TO_R * q);
            g64 = y1 + (I_TO_G * i) + (Q_TO_G * q);
            b64 = y1 + (I_TO_B * i) + (Q_TO_B * q);

            r32 = clampZeroOne((float)r64);
            g32 = clampZeroOne((float)g64);
            b32 = clampZeroOne((float)b64);

            // NTSC_REMOVE_WHITE_RINGING
            if (color == IDX_WHITE) {
                r32 = 1;
                g32 = 1;
                b32 = 1;
            }

            // NTSC_REMOVE_BLACK_GHOSTING
            if (color == IDX_BLACK) {
                r32 = 0;
                g32 = 0;
                b32 = 0;
            }

            colorPixelsTV[phase][s] = (((uint8_t)(r32 * 255)) << SHIFT_R) |
                                      (((uint8_t)(g32 * 255)) << SHIFT_G) |
                                      (((uint8_t)(b32 * 255)) << SHIFT_B) |
                                                        (0xFF << SHIFT_A);
        }
    }
}

//----------------------------------------------------------------------------

static void ntsc_prefsChanged(const char *domain) {

    long lVal = 0;
    color_mode_t color_mode = prefs_parseLongValue(domain, PREF_COLOR_MODE, &lVal, /*base:*/10) ? getColorMode(lVal) : COLOR_MODE_DEFAULT;

    lVal = MONO_MODE_BW;
    mono_mode_t mono_mode = prefs_parseLongValue(domain, PREF_MONO_MODE, &lVal, /*base:*/10) ? getMonoMode(lVal) : MONO_MODE_DEFAULT;

    bool bVal = false;
    half_scanlines = prefs_parseBoolValue(domain, PREF_SHOW_HALF_SCANLINES, &bVal) ? (bVal ? 1 : 0) : 1;

    initChromaPhaseTables(color_mode, mono_mode);
}

static void _init_ntsc(void) {
    LOG("Initializing NTSC renderer");
    initChromaPhaseTables(COLOR_MODE_DEFAULT, MONO_MODE_DEFAULT);

    pixelPlotter[COLOR_MODE_MONO]             = updateFBMonoMonitor;
    pixelPlotter[COLOR_MODE_COLOR]            = updateFBMonoMonitor;
    pixelPlotter[COLOR_MODE_INTERP]           = updateFBMonoMonitor;
    pixelPlotter[COLOR_MODE_COLOR_MONITOR]    = updateFBColorMonitor;
    pixelPlotter[COLOR_MODE_MONO_TV]          = updateFBMonoTV;
    pixelPlotter[COLOR_MODE_COLOR_TV]         = updateFBColorTV;

    getHalfColor[COLOR_MODE_MONO][0]          = doubleScanlineMonitor;
    getHalfColor[COLOR_MODE_MONO][1]          = halfScanlineMonitor;
    getHalfColor[COLOR_MODE_COLOR][0]         = doubleScanlineMonitor;
    getHalfColor[COLOR_MODE_COLOR][1]         = halfScanlineMonitor;
    getHalfColor[COLOR_MODE_INTERP][0]        = doubleScanlineMonitor;
    getHalfColor[COLOR_MODE_INTERP][1]        = halfScanlineMonitor;
    getHalfColor[COLOR_MODE_COLOR_MONITOR][0] = doubleScanlineMonitor;
    getHalfColor[COLOR_MODE_COLOR_MONITOR][1] = halfScanlineMonitor;
    getHalfColor[COLOR_MODE_MONO_TV][0]       = doubleScanlineTV;
    getHalfColor[COLOR_MODE_MONO_TV][1]       = halfScanlineTV;
    getHalfColor[COLOR_MODE_COLOR_TV][0]      = doubleScanlineTV;
    getHalfColor[COLOR_MODE_COLOR_TV][1]      = halfScanlineTV;

    prefs_registerListener(PREF_DOMAIN_VIDEO, &ntsc_prefsChanged);
}

static __attribute__((constructor)) void __init_ntsc(void) {
    emulator_registerStartupCallback(CTOR_PRIORITY_LATE, &_init_ntsc);
}
