/*
 * Apple // emulator for *ix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 1994 Alexander Jean-Claude Bottema
 * Copyright 1995 Stephen Lee
 * Copyright 1997, 1998 Aaron Culliney
 * Copyright 1998, 1999, 2000 Michael Deutschmann
 * Copyright 2013-2015 Aaron Culliney
 *
 */

#include "common.h"
#include "video/video.h"

#if __clang__
#   pragma clang diagnostic push
#   pragma clang diagnostic ignored "-Wunused-variable"
#elif __GNUC__
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wunused-variable"
#   pragma GCC diagnostic ignored "-Wunused-const-variable"
#endif

#define SCANSTEP (SCANWIDTH-12)
#define SCANDSTEP (SCANWIDTH-6)

#define DYNAMIC_SZ 11 // 7 pixels (as bytes) + 2pre + 2post

A2Color_s colormap[256] = { { 0 } };
static pthread_mutex_t display_scan_mutex = PTHREAD_MUTEX_INITIALIZER;

static uint8_t video__wider_font[0x8000] = { 0 };
static uint8_t video__font[0x4000] = { 0 };
static uint8_t video__int_font[5][0x4000] = { { 0 } }; // interface font

static color_mode_t color_mode = COLOR_NONE;

// Precalculated framebuffer offsets given VM addr
static unsigned int video__screen_addresses[8192] = { INT_MIN };
static uint8_t video__columns[8192] = { 0 };

static uint8_t video__hires_even[0x800] = { 0 };
static uint8_t video__hires_odd[0x800] = { 0 };

static volatile unsigned long _vid_dirty = 0;

// Video constants -- sourced from AppleWin
static const bool bVideoScannerNTSC = true;
static const int kHBurstClock   =    53; // clock when Color Burst starts
static const int kHBurstClocks  =     4; // clocks per Color Burst duration
static const int kHClock0State  =  0x18; // H[543210] = 011000
static const int kHClocks       =    65; // clocks per horizontal scan (including HBL)
static const int kHPEClock      =    40; // clock when HPE (horizontal preset enable) goes low
static const int kHPresetClock  =    41; // clock when H state presets
static const int kHSyncClock    =    49; // clock when HSync starts
static const int kHSyncClocks   =     4; // clocks per HSync duration
static const int kNTSCScanLines =   262; // total scan lines including VBL (NTSC)
static const int kNTSCVSyncLine =   224; // line when VSync starts (NTSC)
static const int kPALScanLines  =   312; // total scan lines including VBL (PAL)
static const int kPALVSyncLine  =   264; // line when VSync starts (PAL)
static const int kVLine0State   = 0x100; // V[543210CBA] = 100000000
static const int kVPresetLine   =   256; // line when V state presets
static const int kVSyncLines    =     4; // lines per VSync duration

static uint8_t video__odd_colors[2] = { COLOR_LIGHT_PURPLE, COLOR_LIGHT_BLUE };
static uint8_t video__even_colors[2] = { COLOR_LIGHT_GREEN, COLOR_LIGHT_RED };

// TODO FIXME : support linked list of callbacks here ...
static display_update_fn textCallbackFn = NULL;
static display_update_fn hiresCallbackFn = NULL;
static display_update_fn modeCallbackFn = NULL;

// 40col/80col/lores/hires/dhires line offsets
static uint16_t video__line_offset[TEXT_ROWS] = {
  0x000, 0x080, 0x100, 0x180, 0x200, 0x280, 0x300, 0x380,
  0x028, 0x0A8, 0x128, 0x1A8, 0x228, 0x2A8, 0x328, 0x3A8,
  0x050, 0x0D0, 0x150, 0x1D0, 0x250, 0x2D0, 0x350, 0x3D0
};

static uint8_t video__dhires1[256] = {
    0x0,0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,0x9,0xa,0xb,0xc,0xd,0xe,0xf,
    0x0,0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,0x9,0xa,0xb,0xc,0xd,0xe,0xf,
    0x0,0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,0x9,0xa,0xb,0xc,0xd,0xe,0xf,
    0x0,0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,0x9,0xa,0xb,0xc,0xd,0xe,0xf,
    0x0,0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,0x9,0xa,0xb,0xc,0xd,0xe,0xf,
    0x0,0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,0x9,0xa,0xb,0xc,0xd,0xe,0xf,
    0x0,0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,0x9,0xa,0xb,0xc,0xd,0xe,0xf,
    0x0,0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,0x9,0xa,0xb,0xc,0xd,0xe,0xf,
};

static uint8_t video__dhires2[256] = {
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,
    0x1,0x1,0x1,0x1,0x1,0x1,0x1,0x1,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,
    0x2,0x2,0x2,0x2,0x2,0x2,0x2,0x2,0xa,0xa,0xa,0xa,0xa,0xa,0xa,0xa,
    0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0xb,0xb,0xb,0xb,0xb,0xb,0xb,0xb,
    0x4,0x4,0x4,0x4,0x4,0x4,0x4,0x4,0xc,0xc,0xc,0xc,0xc,0xc,0xc,0xc,
    0x5,0x5,0x5,0x5,0x5,0x5,0x5,0x5,0xd,0xd,0xd,0xd,0xd,0xd,0xd,0xd,
    0x6,0x6,0x6,0x6,0x6,0x6,0x6,0x6,0xe,0xe,0xe,0xe,0xe,0xe,0xe,0xe,
    0x7,0x7,0x7,0x7,0x7,0x7,0x7,0x7,0xf,0xf,0xf,0xf,0xf,0xf,0xf,0xf,
};


// forward decls of VM entry points

void video__write_2e_text0(uint16_t, uint8_t);
void video__write_2e_text0_mixed(uint16_t, uint8_t);
void video__write_2e_text1(uint16_t, uint8_t);
void video__write_2e_text1_mixed(uint16_t, uint8_t);
void video__write_2e_hgr0(uint16_t, uint8_t);
void video__write_2e_hgr0_mixed(uint16_t, uint8_t);
void video__write_2e_hgr1(uint16_t, uint8_t);
void video__write_2e_hgr1_mixed(uint16_t, uint8_t);

// ----------------------------------------------------------------------------
// Initialization routines

static void _initialize_dhires_values(void) {
    for (unsigned int i = 0; i < 0x80; i++) {
        video__dhires1[i+0x80] = video__dhires1[i];
        video__dhires2[i+0x80] = video__dhires2[i];
    }
}

static void _initialize_hires_values(void) {
    // precalculate colors for all the 256*8 bit combinations
    for (unsigned int value = 0x00; value <= 0xFF; value++) {
        for (unsigned int e = value<<3, last_not_black=0, v=value, b=0; b < 7; b++, v >>= 1, e++) {
            if (v & 1) {
                if (last_not_black) {
                    video__hires_even[e] = COLOR_LIGHT_WHITE;
                    video__hires_odd[e] = COLOR_LIGHT_WHITE;
                    if (b > 0)
                    {
                        video__hires_even[e-1] = COLOR_LIGHT_WHITE;
                        video__hires_odd [e-1] = COLOR_LIGHT_WHITE;
                    }
                } else {
                    if (b & 1) {
                        if (value & 0x80) {
                            video__hires_even[e] = COLOR_LIGHT_RED;
                            video__hires_odd [e] = COLOR_LIGHT_BLUE;
                        } else {
                            video__hires_even[e] = COLOR_LIGHT_GREEN;
                            video__hires_odd [e] = COLOR_LIGHT_PURPLE;
                        }
                    } else {
                        if (value & 0x80) {
                            video__hires_even[e] = COLOR_LIGHT_BLUE;
                            video__hires_odd [e] = COLOR_LIGHT_RED;
                        } else {
                            video__hires_even[e] = COLOR_LIGHT_PURPLE;
                            video__hires_odd [e] = COLOR_LIGHT_GREEN;
                        }
                    }
                }
                last_not_black = 1;
            } else {
                video__hires_even[e] = COLOR_BLACK;
                video__hires_odd [e] = COLOR_BLACK;
                last_not_black = 0;
            }
        }
    }

    if (color_mode == COLOR_NONE) {
        for (unsigned int value = 0x00; value <= 0xFF; value++) {
            for (unsigned int b = 0, e = value * 8; b < 7; b++, e++) {
                if (video__hires_even[e] != COLOR_BLACK) {
                    video__hires_even[e] = COLOR_LIGHT_WHITE;
                }
                if (video__hires_odd[e] != COLOR_BLACK) {
                    video__hires_odd[e] = COLOR_LIGHT_WHITE;
                }
            }
        }
    } else if (color_mode == COLOR_INTERP) {
        for (unsigned int value = 0x00; value <= 0xFF; value++) {
            for (unsigned int b=1, e=value*8 + 1; b <= 5; b += 2, e += 2) {
                if (video__hires_even[e] == COLOR_BLACK) {
                    if (video__hires_even[e-1] != COLOR_BLACK &&
                        video__hires_even[e+1] != COLOR_BLACK &&
                        video__hires_even[e-1] != COLOR_LIGHT_WHITE &&
                        video__hires_even[e+1] != COLOR_LIGHT_WHITE)
                    {
                        video__hires_even[e] = video__hires_even[e-1];
                    }
                    else if (
                        video__hires_even[e-1] != COLOR_BLACK &&
                        video__hires_even[e+1] != COLOR_BLACK &&
                        video__hires_even[e-1] != COLOR_LIGHT_WHITE &&
                        video__hires_even[e+1] == COLOR_LIGHT_WHITE)
                    {
                        video__hires_even[e] = video__hires_even[e-1];
                    }
                    else if (
                        video__hires_even[e-1] != COLOR_BLACK &&
                        video__hires_even[e+1] != COLOR_BLACK &&
                        video__hires_even[e-1] == COLOR_LIGHT_WHITE &&
                        video__hires_even[e+1] != COLOR_LIGHT_WHITE)
                    {
                        video__hires_even[e] = video__hires_even[e+1];
                    }
                    else if (
                        video__hires_even[e-1] == COLOR_LIGHT_WHITE &&
                        video__hires_even[e+1] == COLOR_LIGHT_WHITE)
                    {
                        video__hires_even[e] = (value & 0x80) ? COLOR_LIGHT_BLUE : COLOR_LIGHT_PURPLE;
                    }
                }

                if (video__hires_odd[e] == COLOR_BLACK) {
                    if (video__hires_odd[e-1] != COLOR_BLACK &&
                        video__hires_odd[e+1] != COLOR_BLACK &&
                        video__hires_odd[e-1] != COLOR_LIGHT_WHITE &&
                        video__hires_odd[e+1] != COLOR_LIGHT_WHITE)
                    {
                        video__hires_odd[e] = video__hires_odd[e-1];
                    }
                    else if (
                        video__hires_odd[e-1] != COLOR_BLACK &&
                        video__hires_odd[e+1] != COLOR_BLACK &&
                        video__hires_odd[e-1] != COLOR_LIGHT_WHITE &&
                        video__hires_odd[e+1] == COLOR_LIGHT_WHITE)
                    {
                        video__hires_odd[e] = video__hires_odd[e-1];
                    }
                    else if (
                        video__hires_odd[e-1] != COLOR_BLACK &&
                        video__hires_odd[e+1] != COLOR_BLACK &&
                        video__hires_odd[e-1] == COLOR_LIGHT_WHITE &&
                        video__hires_odd[e+1] != COLOR_LIGHT_WHITE)
                    {
                        video__hires_odd[e] = video__hires_odd[e+1];
                    }
                    else if (
                        video__hires_odd[e-1] == COLOR_LIGHT_WHITE &&
                        video__hires_odd[e+1] == COLOR_LIGHT_WHITE)
                    {
                        video__hires_odd[e] = (value & 0x80) ? COLOR_LIGHT_RED : COLOR_LIGHT_GREEN;
                    }
                }
            }

            for (unsigned int b = 0, e = value * 8; b <= 6; b += 2, e += 2) {
                if (video__hires_even[ e ] == COLOR_BLACK) {
                    if (b > 0 && b < 6) {
                        if (video__hires_even[e-1] != COLOR_BLACK &&
                            video__hires_even[e+1] != COLOR_BLACK &&
                            video__hires_even[e-1] != COLOR_LIGHT_WHITE &&
                            video__hires_even[e+1] != COLOR_LIGHT_WHITE)
                        {
                            video__hires_even[e] = video__hires_even[e-1];
                        }
                        else if (
                            video__hires_even[e-1] != COLOR_BLACK &&
                            video__hires_even[e+1] != COLOR_BLACK &&
                            video__hires_even[e-1] != COLOR_LIGHT_WHITE &&
                            video__hires_even[e+1] == COLOR_LIGHT_WHITE)
                        {
                            video__hires_even[e] = video__hires_even[e-1];
                        }
                        else if (
                            video__hires_even[e-1] != COLOR_BLACK &&
                            video__hires_even[e+1] != COLOR_BLACK &&
                            video__hires_even[e-1] == COLOR_LIGHT_WHITE &&
                            video__hires_even[e+1] != COLOR_LIGHT_WHITE)
                        {
                            video__hires_even[e] = video__hires_even[e+1];
                        }
                        else if (
                            video__hires_even[e-1] == COLOR_LIGHT_WHITE &&
                            video__hires_even[e+1] == COLOR_LIGHT_WHITE)
                        {
                            video__hires_even[e] = (value & 0x80) ? COLOR_LIGHT_RED : COLOR_LIGHT_GREEN;
                        }
                    }
                }

                if (video__hires_odd[e] == COLOR_BLACK) {
                    if (b > 0 && b < 6) {
                        if (video__hires_odd[e-1] != COLOR_BLACK &&
                            video__hires_odd[e+1] != COLOR_BLACK &&
                            video__hires_odd[e-1] != COLOR_LIGHT_WHITE &&
                            video__hires_odd[e+1] != COLOR_LIGHT_WHITE)
                        {
                            video__hires_odd[e] = video__hires_odd[e-1];
                        }
                        else if (
                            video__hires_odd[e-1] != COLOR_BLACK &&
                            video__hires_odd[e+1] != COLOR_BLACK &&
                            video__hires_odd[e-1] != COLOR_LIGHT_WHITE &&
                            video__hires_odd[e+1] == COLOR_LIGHT_WHITE)
                        {
                            video__hires_odd[e] = video__hires_odd[e-1];
                        }
                        else if (
                            video__hires_odd[e-1] != COLOR_BLACK &&
                            video__hires_odd[e+1] != COLOR_BLACK &&
                            video__hires_odd[e-1] == COLOR_LIGHT_WHITE &&
                            video__hires_odd[e+1] != COLOR_LIGHT_WHITE)
                        {
                            video__hires_odd[e] = video__hires_odd[e+1];
                        }
                        else if (
                            video__hires_odd[e-1] == COLOR_LIGHT_WHITE &&
                            video__hires_odd[e+1] == COLOR_LIGHT_WHITE)
                        {
                            video__hires_odd[e] = (value & 0x80) ? COLOR_LIGHT_BLUE : COLOR_LIGHT_PURPLE;
                        }
                    }
                }
            }
        }
    }
}

static void _initialize_row_col_tables(void) {
    for (unsigned int y = 0; y < TEXT_ROWS; y++) {
        for (unsigned int y2 = 0; y2 < FONT_GLYPH_Y; y2++) {
            for (unsigned int x = 0; x < 40; x++) {
                video__screen_addresses[video__line_offset[y] + (0x400*y2) + x] = ((y*FONT_HEIGHT_PIXELS + 2*y2) * SCANWIDTH) + (x*FONT_WIDTH_PIXELS) + _INTERPOLATED_PIXEL_ADJUSTMENT_PRE;
                video__columns         [video__line_offset[y] + (0x400*y2) + x] = (uint8_t)x;
            }
        }
    }
    for (unsigned int i = 0; i < 8192; i++) {
        assert(video__screen_addresses[i] != INT_MIN);
    }
}

static void _initialize_tables_video(void) {
    // initialize text/lores & hires graphics routines
    for (unsigned int y = 0; y < TEXT_ROWS; y++) {
        for (unsigned int x = 0; x < TEXT_COLS; x++) {
            unsigned int idx = video__line_offset[y] + x;
            // text/lores pages
            if (y < 20) {
                cpu65_vmem_w[idx+0x400] = video__write_2e_text0;
                cpu65_vmem_w[idx+0x800] = video__write_2e_text1;
            } else {
                cpu65_vmem_w[idx+0x400] = video__write_2e_text0_mixed;
                cpu65_vmem_w[idx+0x800] = video__write_2e_text1_mixed;
            }

            // hires/dhires pages
            for (unsigned int i = 0; i < 8; i++) {
                idx = video__line_offset[ y ] + (0x400*i) + x;
                if (y < 20) {
                    cpu65_vmem_w[idx+0x2000] = video__write_2e_hgr0;
                    cpu65_vmem_w[idx+0x4000] = video__write_2e_hgr1;
                } else {
                    cpu65_vmem_w[idx+0x2000] = video__write_2e_hgr0_mixed;
                    cpu65_vmem_w[idx+0x4000] = video__write_2e_hgr1_mixed;
                }
            }
        }
    }
}

static void _initialize_color() {
    unsigned char col2[ 3 ] = { 255,255,255 };

    /* align the palette for hires graphics */
    for (unsigned int i = 0; i < 8; i++) {
        for (unsigned int j = 0; j < 3; j++) {
            unsigned int c = 0;
            c = (i & 1) ? col2[ j ] : 0;
            colormap[ j+i*3+32].red = c;
            c = (i & 2) ? col2[ j ] : 0;
            colormap[ j+i*3+32].green = c;
            c = (i & 4) ? col2[ j ] : 0;
            colormap[ j+i*3+32].blue = c;
        }
    }

    colormap[ COLOR_FLASHING_BLACK].red = 0;
    colormap[ COLOR_FLASHING_BLACK].green = 0;
    colormap[ COLOR_FLASHING_BLACK].blue = 0;

    colormap[ COLOR_LIGHT_WHITE].red   = 255;
    colormap[ COLOR_LIGHT_WHITE].green = 255;
    colormap[ COLOR_LIGHT_WHITE].blue  = 255;

    colormap[ COLOR_FLASHING_WHITE].red   = 255;
    colormap[ COLOR_FLASHING_WHITE].green = 255;
    colormap[ COLOR_FLASHING_WHITE].blue  = 255;

    colormap[IDX_BLACK    ] = (A2Color_s) { .red = 0,   .green = 0,   .blue = 0   };
    colormap[IDX_MAGENTA  ] = (A2Color_s) { .red = 195, .green = 0,   .blue = 48  };
    colormap[IDX_DARKBLUE ] = (A2Color_s) { .red = 0,   .green = 0,   .blue = 130 };
    colormap[IDX_PURPLE   ] = (A2Color_s) { .red = 166, .green = 52,  .blue = 170 };
    colormap[IDX_DARKGREEN] = (A2Color_s) { .red = 0,   .green = 146, .blue = 0   };
    colormap[IDX_DARKGREY ] = (A2Color_s) { .red = 105, .green = 105, .blue = 105 };
    colormap[IDX_MEDBLUE  ] = (A2Color_s) { .red = 24,  .green = 113, .blue = 255 };
    colormap[IDX_LIGHTBLUE] = (A2Color_s) { .red = 12,  .green = 190, .blue = 235 };
    colormap[IDX_BROWN    ] = (A2Color_s) { .red = 150, .green = 85,  .blue = 40  };
    colormap[IDX_ORANGE   ] = (A2Color_s) { .red = 255, .green = 24,  .blue = 44  };
    colormap[IDX_LIGHTGREY] = (A2Color_s) { .red = 150, .green = 170, .blue = 170 };
    colormap[IDX_PINK     ] = (A2Color_s) { .red = 255, .green = 158, .blue = 150 };
    colormap[IDX_GREEN    ] = (A2Color_s) { .red = 0,   .green = 255, .blue = 0   };
    colormap[IDX_YELLOW   ] = (A2Color_s) { .red = 255, .green = 255, .blue = 0   };
    colormap[IDX_AQUA     ] = (A2Color_s) { .red = 130, .green = 255, .blue = 130 };
    colormap[IDX_WHITE    ] = (A2Color_s) { .red = 255, .green = 255, .blue = 255 };

    /* mirror of lores colormap optimized for dhires code */
    colormap[0x00].red = 0; colormap[0x00].green = 0;
    colormap[0x00].blue = 0;   /* Black */
    colormap[0x08].red = 195; colormap[0x08].green = 0;
    colormap[0x08].blue = 48;  /* Magenta */
    colormap[0x01].red = 0; colormap[0x01].green = 0;
    colormap[0x01].blue = 130; /* Dark Blue */
    colormap[0x09].red = 166; colormap[0x09].green = 52;
    colormap[0x09].blue = 170; /* Purple */
    colormap[0x02].red = 0; colormap[0x02].green = 146;
    colormap[0x02].blue = 0;   /* Dark Green */
    colormap[0x0a].red = 105; colormap[0x0a].green = 105;
    colormap[0x0a].blue = 105; /* Dark Grey*/
    colormap[0x03].red = 24; colormap[0x03].green = 113;
    colormap[0x03].blue = 255; /* Medium Blue */
    colormap[0x0b].red = 12; colormap[0x0b].green = 190;
    colormap[0x0b].blue = 235; /* Light Blue */
    colormap[0x04].red = 150; colormap[0x04].green = 85;
    colormap[0x04].blue = 40; /* Brown */
    colormap[0x0c].red = 255; colormap[0x0c].green = 24;
    colormap[0x0c].blue = 44; /* Orange */
    colormap[0x05].red = 150; colormap[0x05].green = 170;
    colormap[0x05].blue = 170; /* Light Gray */
    colormap[0x0d].red = 255; colormap[0x0d].green = 158;
    colormap[0x0d].blue = 150; /* Pink */
    colormap[0x06].red = 0; colormap[0x06].green = 255;
    colormap[0x06].blue = 0; /* Green */
    colormap[0x0e].red = 255; colormap[0x0e].green = 255;
    colormap[0x0e].blue = 0; /* Yellow */
    colormap[0x07].red = 130; colormap[0x07].green = 255;
    colormap[0x07].blue = 130; /* Aqua */
    colormap[0x0f].red = 255; colormap[0x0f].green = 255;
    colormap[0x0f].blue = 255; /* White */

#if USE_RGBA4444
    for (unsigned int i=0; i<256; i++) {
        colormap[i].red   = (colormap[i].red   >>4);
        colormap[i].green = (colormap[i].green >>4);
        colormap[i].blue  = (colormap[i].blue  >>4);
    }
#endif
}

static void video_prefsChanged(const char *domain) {
    long val = COLOR_INTERP;
    prefs_parseLongValue(domain, PREF_COLOR_MODE, &val, /*base:*/10);
    if (val < 0) {
        val = COLOR_INTERP;
    }
    if (val >= NUM_COLOROPTS) {
        val = COLOR_INTERP;
    }
    color_mode = (color_mode_t)val;
    display_reset();
}

void display_reset(void) {
    _initialize_hires_values();
    _initialize_tables_video();
    video_setDirty(A2_DIRTY_FLAG);
}

void display_loadFont(const uint8_t first, const uint8_t quantity, const uint8_t *data, font_mode_t mode) {
    uint8_t fg = 0;
    uint8_t bg = 0;
    switch (mode) {
        case FONT_MODE_INVERSE:
            fg = COLOR_BLACK;
            bg = COLOR_LIGHT_WHITE;
            break;
        case FONT_MODE_FLASH:
            fg = COLOR_FLASHING_WHITE;
            bg = COLOR_FLASHING_BLACK;
            break;
        default:
            fg = COLOR_LIGHT_WHITE;
            bg = COLOR_BLACK;
            break;
    }

    unsigned int i = quantity * 8;
    while (i--) {
        unsigned int j = 8;
        uint8_t x = data[i];
        while (j--) {
            uint8_t y = (x & 128) ? fg : bg;
            video__wider_font[(first << 7) + (i << 4) + (j << 1)] = video__wider_font[(first << 7) + (i << 4) + (j << 1) + 1] =
            video__font[(first << 6) + (i << 3) + j] = y;
            x <<= 1;
        }
    }
}

static void _loadfont_int(int first, int quantity, const uint8_t *data) {
    unsigned int i = quantity * 8;
    while (i--) {
        unsigned int j = 8;
        uint8_t x = data[i];
        while (j--) {
            unsigned int y = (first << 6) + (i << 3) + j;
            if (x & 128) {
                video__int_font[GREEN_ON_BLACK][y] = COLOR_LIGHT_GREEN;
                video__int_font[GREEN_ON_BLUE][y] = COLOR_LIGHT_GREEN;
                video__int_font[RED_ON_BLACK][y] = COLOR_LIGHT_RED;
                video__int_font[BLUE_ON_BLACK][y] = COLOR_LIGHT_BLUE;
                video__int_font[WHITE_ON_BLACK][y] = COLOR_LIGHT_WHITE;
            } else {
                video__int_font[GREEN_ON_BLACK][y] = COLOR_BLACK;
                video__int_font[GREEN_ON_BLUE][y] = COLOR_MEDIUM_BLUE;
                video__int_font[RED_ON_BLACK][y] = COLOR_BLACK;
                video__int_font[BLUE_ON_BLACK][y] = COLOR_BLACK;
                video__int_font[WHITE_ON_BLACK][y] = COLOR_BLACK;
            }
            x <<= 1;
        }
    }
}

static void _initialize_interface_fonts(void) {
    _loadfont_int(0x00,0x40,ucase_glyphs);
    _loadfont_int(0x40,0x20,ucase_glyphs);
    _loadfont_int(0x60,0x20,lcase_glyphs);
    _loadfont_int(0x80,0x40,ucase_glyphs);
    _loadfont_int(0xC0,0x20,ucase_glyphs);
    _loadfont_int(0xE0,0x20,lcase_glyphs);
    _loadfont_int(MOUSETEXT_BEGIN,0x20,mousetext_glyphs);
    _loadfont_int(ICONTEXT_BEGIN,0x20,interface_glyphs);
}

// ----------------------------------------------------------------------------
// lores/char plotting routines

static inline void _plot_char40(uint8_t **d, uint8_t **s) {
    *((uint32_t *)(*d)) = *((uint32_t *)(*s));
    *d += 4; *s += 4;
    *((uint32_t *)(*d)) = *((uint32_t *)(*s));
    *d += 4; *s += 4;
    *((uint32_t *)(*d)) = *((uint32_t *)(*s));
    *d += 4; *s += 4;
    *((uint16_t *)(*d)) = *((uint16_t *)(*s));
    *d += SCANSTEP; *s -= 12;
    *((uint32_t *)(*d)) = *((uint32_t *)(*s));
    *d += 4; *s += 4;
    *((uint32_t *)(*d)) = *((uint32_t *)(*s));
    *d += 4; *s += 4;
    *((uint32_t *)(*d)) = *((uint32_t *)(*s));
    *d += 4; *s += 4;
    *((uint16_t *)(*d)) = *((uint16_t *)(*s));
    *d += SCANSTEP; *s += 4;
}

static inline void _plot_char80(uint8_t **d, uint8_t **s, const unsigned int fb_width) {
    // FIXME : this is implicitly scaling at FONT_GLYPH_SCALE_Y ... make it explicit
    *((uint32_t *)(*d)) = *((uint32_t *)(*s));
    *d += 4; *s += 4;
    *((uint16_t *)(*d)) = *((uint16_t *)(*s));
    *d += 2; *s += 2;
    *((uint8_t *)(*d)) = *((uint8_t *)(*s));
    *d += fb_width-6; *s -= 6;
    *((uint32_t *)(*d)) = *((uint32_t *)(*s));
    *d += 4; *s += 4;
    *((uint16_t *)(*d)) = *((uint16_t *)(*s));
    *d += 2; *s += 2;
    *((uint8_t *)(*d)) = *((uint8_t *)(*s));
    *d += fb_width-6; *s += 2;
}

static inline void _plot_lores40(uint8_t **d, const uint32_t val) {
    *((uint32_t *)(*d)) = val;
    *d += 4;
    *((uint32_t *)(*d)) = val;
    *d += 4;
    *((uint32_t *)(*d)) = val;
    *d += 4;
    *((uint16_t *)(*d)) = (uint16_t)(val & 0xffff);
    *d += SCANSTEP;
    *((uint32_t *)(*d)) = val;
    *d += 4;
    *((uint32_t *)(*d)) = val;
    *d += 4;
    *((uint32_t *)(*d)) = val;
    *d += 4;
    *((uint16_t *)(*d)) = (uint16_t)(val & 0xffff);
}

static inline void _plot_lores80(uint8_t **d, const uint32_t val) {
    *((uint32_t *)(*d)) = val;
    *d += 4;
    *((uint16_t *)(*d)) = (uint16_t)val;
    *d += 2;
    *((uint8_t *)(*d))  = (uint8_t)val;
    *d += SCANDSTEP;
    *((uint32_t *)(*d)) = val;
    *d += 4;
    *((uint16_t *)(*d)) = (uint16_t)val;
    *d += 2;
    *((uint8_t *)(*d))  = (uint8_t)val;
}

static inline void __plot_character40(const unsigned int font_off, uint8_t *fb_ptr) {
    uint8_t *font_ptr = video__wider_font+font_off;
    _plot_char40(/*dst*/&fb_ptr, /*src*/&font_ptr);
    _plot_char40(/*dst*/&fb_ptr, /*src*/&font_ptr);
    _plot_char40(/*dst*/&fb_ptr, /*src*/&font_ptr);
    _plot_char40(/*dst*/&fb_ptr, /*src*/&font_ptr);
    _plot_char40(/*dst*/&fb_ptr, /*src*/&font_ptr);
    _plot_char40(/*dst*/&fb_ptr, /*src*/&font_ptr);
    _plot_char40(/*dst*/&fb_ptr, /*src*/&font_ptr);
    _plot_char40(/*dst*/&fb_ptr, /*src*/&font_ptr);
}

static void _plot_character40(uint8_t col, uint8_t row, int page, int bank, uint8_t *fb_ptr) {
    assert(bank == 0); // FIXME TODO ...
    uint16_t base = page ? 0x0800 : 0x0400;
    uint16_t off = video__line_offset[row] + col;
    uint16_t ea = base+off;
    uint8_t b = apple_ii_64k[bank][ea];
    __plot_character40(b<<7/* *128 */, fb_ptr+video__screen_addresses[off]);
    if (textCallbackFn) {
        textCallbackFn((pixel_delta_t){ .row = row, .col = col, .b = b, .cs = INVALID_COLORSCHEME });
    }
}

static inline void __plot_character80(const unsigned int font_off, uint8_t *fb_ptr) {
    uint8_t *font_ptr = video__font+font_off;
    _plot_char80(/*dst*/&fb_ptr, /*src*/&font_ptr, SCANWIDTH);
    _plot_char80(/*dst*/&fb_ptr, /*src*/&font_ptr, SCANWIDTH);
    _plot_char80(/*dst*/&fb_ptr, /*src*/&font_ptr, SCANWIDTH);
    _plot_char80(/*dst*/&fb_ptr, /*src*/&font_ptr, SCANWIDTH);
    _plot_char80(/*dst*/&fb_ptr, /*src*/&font_ptr, SCANWIDTH);
    _plot_char80(/*dst*/&fb_ptr, /*src*/&font_ptr, SCANWIDTH);
    _plot_char80(/*dst*/&fb_ptr, /*src*/&font_ptr, SCANWIDTH);
    _plot_char80(/*dst*/&fb_ptr, /*src*/&font_ptr, SCANWIDTH);
}

static void _plot_character80(uint8_t col, uint8_t row, int page, int bank, uint8_t *fb_ptr) {
    assert(bank == 0); // FIXME TODO ...
    uint16_t base = page ? 0x0800 : 0x0400;
    uint16_t off = video__line_offset[row] + col;
    uint16_t ea = base+off;
    {
        uint8_t b = apple_ii_64k[1][ea];
        __plot_character80(b<<6/* *64 */, fb_ptr+video__screen_addresses[off]);
        if (textCallbackFn) {
            textCallbackFn((pixel_delta_t){ .row = row, .col = (col<<1), .b = b, .cs = INVALID_COLORSCHEME });
        }
    }
    {
        uint8_t b = apple_ii_64k[0][ea];
        __plot_character80(b<<6/* *64 */, fb_ptr+video__screen_addresses[off]+7);
        if (textCallbackFn) {
            textCallbackFn((pixel_delta_t){ .row = row, .col = (col<<1)+1, .b = b, .cs = INVALID_COLORSCHEME });
        }
    }
}

static inline void __plot_block40(const uint8_t val, uint8_t col, uint8_t *fb_ptr) {

    uint8_t color = (val & 0x0F) << 4;

    uint32_t val32;
    if (color_mode == COLOR_NONE) {
        if (color != 0x0 && color != 0xf) {
            uint8_t rot2 = ((col % 2) << 1); // 2 phases at double rotation
            color = (color << rot2) | ((color & 0xC0) >> rot2);
        }
        val32 =  ((color & 0x10) ? COLOR_LIGHT_WHITE : COLOR_BLACK) << 0;
        val32 |= ((color & 0x20) ? COLOR_LIGHT_WHITE : COLOR_BLACK) << 8;
        val32 |= ((color & 0x40) ? COLOR_LIGHT_WHITE : COLOR_BLACK) << 16;
        val32 |= ((color & 0x80) ? COLOR_LIGHT_WHITE : COLOR_BLACK) << 24;
    } else {
        val32 = (color << 24) | (color << 16) | (color << 8) | color;
    }

    _plot_lores40(/*dst*/&fb_ptr, val32);
    fb_ptr += SCANSTEP;
    _plot_lores40(/*dst*/&fb_ptr, val32);
    fb_ptr += SCANSTEP;
    _plot_lores40(/*dst*/&fb_ptr, val32);
    fb_ptr += SCANSTEP;
    _plot_lores40(/*dst*/&fb_ptr, val32);

    fb_ptr += SCANSTEP;
    color = val & 0xF0;

    if (color_mode == COLOR_NONE) {
        if (color != 0x0 && color != 0xf) {
            uint8_t rot2 = ((col % 2) << 1); // 2 phases at double rotation
            color = (color << rot2) | ((color & 0xC0) >> rot2);
        }
        val32 =  ((color & 0x10) ? COLOR_LIGHT_WHITE : COLOR_BLACK) << 0;
        val32 |= ((color & 0x20) ? COLOR_LIGHT_WHITE : COLOR_BLACK) << 8;
        val32 |= ((color & 0x40) ? COLOR_LIGHT_WHITE : COLOR_BLACK) << 16;
        val32 |= ((color & 0x80) ? COLOR_LIGHT_WHITE : COLOR_BLACK) << 24;
    } else {
        val32 = (color << 24) | (color << 16) | (color << 8) | color;
    }

    _plot_lores40(/*dst*/&fb_ptr, val32);
    fb_ptr += SCANSTEP;
    _plot_lores40(/*dst*/&fb_ptr, val32);
    fb_ptr += SCANSTEP;
    _plot_lores40(/*dst*/&fb_ptr, val32);
    fb_ptr += SCANSTEP;
    _plot_lores40(/*dst*/&fb_ptr, val32);
}

static void _plot_block40(uint8_t col, uint8_t row, int page, int bank, uint8_t *fb_ptr) {
    assert(bank == 0); // FIXME TODO ...
    uint16_t base = page ? 0x0800 : 0x0400;
    uint16_t off = video__line_offset[row] + col;
    uint16_t ea = base+off;
    uint8_t b = apple_ii_64k[bank][ea];
    __plot_block40(b, col, fb_ptr+video__screen_addresses[off]);
    if (textCallbackFn) {
        textCallbackFn((pixel_delta_t){ .row = row, .col = col, .b = b, .cs = COLOR16 });
    }
}

static inline void __plot_block80(const uint8_t val, uint8_t *fb_ptr) {
    uint8_t color = (val & 0x0F) << 4;
    uint32_t val32 = (color << 24) | (color << 16) | (color << 8) | color;

    _plot_lores80(/*dst*/&fb_ptr, val32);
    fb_ptr += SCANDSTEP;
    _plot_lores80(/*dst*/&fb_ptr, val32);
    fb_ptr += SCANDSTEP;
    _plot_lores80(/*dst*/&fb_ptr, val32);
    fb_ptr += SCANDSTEP;
    _plot_lores80(/*dst*/&fb_ptr, val32);

    fb_ptr += SCANDSTEP;
    color = val & 0xF0;
    val32 = (color << 24) | (color << 16) | (color << 8) | color;

    _plot_lores80(/*dst*/&fb_ptr, val32);
    fb_ptr += SCANDSTEP;
    _plot_lores80(/*dst*/&fb_ptr, val32);
    fb_ptr += SCANDSTEP;
    _plot_lores80(/*dst*/&fb_ptr, val32);
    fb_ptr += SCANDSTEP;
    _plot_lores80(/*dst*/&fb_ptr, val32);
}

static inline uint8_t __shift_block80(uint8_t b) {
    // plot even half-block from auxmem, rotate nybbles to match color (according to UTAIIe: 8-29)
    uint8_t b0 = (b & 0x0F);
    uint8_t b1 = (b & 0xF0) >> 4;
    uint8_t rot0 = ((b0 & 0x8) >> 3);
    uint8_t rot1 = ((b1 & 0x8) >> 3);
    b0 = (((b0<<5) | (rot0<<4)) >> 4);
    b1 =  ((b1<<5) | (rot1<<4));
    b = b0 | b1;
    return b;
}

static void _plot_block80(uint8_t col, uint8_t row, int page, int bank, uint8_t *fb_ptr) {
    assert(bank == 0); // FIXME TODO ...
    uint16_t base = page ? 0x0800 : 0x0400;
    uint16_t off = video__line_offset[row] + col;
    uint16_t ea = base+off;

#warning FIXME TODO INVESTIGATE : ... does RAMRD/80STORE/PAGE2 affect load order here?

    // plot even half-block from auxmem, rotate nybbles to match color (according to UTAIIe)
    {
        uint8_t b = apple_ii_64k[1][ea];
        b = __shift_block80(b);
        uint8_t *fb = fb_ptr+video__screen_addresses[off];
        __plot_block80(b, fb);
        if (textCallbackFn) {
            textCallbackFn((pixel_delta_t){ .row = row, .col = (col<<1), .b = b, .cs = COLOR16 });
        }
    }

    // plot odd half-block from main mem
    {
        uint8_t b = apple_ii_64k[0][ea];
        uint8_t *fb = fb_ptr+video__screen_addresses[off] + 7;
        __plot_block80(b, fb);
        if (textCallbackFn) {
            textCallbackFn((pixel_delta_t){ .row = row, .col = (col<<1)+1, .b = b, .cs = COLOR16 });
        }
    }
}

static void (*_textpage_plotter(uint32_t currswitches, uint32_t txtflags))(uint8_t, uint8_t, int, int, uint8_t*) {
    void (*plotFn)(uint8_t, uint8_t, int, int, uint8_t*) = NULL;

    if (currswitches & txtflags) {
        plotFn = (currswitches & SS_80COL) ? _plot_character80 : _plot_character40;
    } else {
        assert(!(currswitches & SS_HIRES) && "must be lores graphics or programmer error");
        if (!(currswitches & SS_80COL)) {
            plotFn = _plot_block40;
            if (!(currswitches & SS_DHIRES)) {
                // LORES40 ...
            } else {
                // TODO : abnormal LORES output.  See UTAIIe : 8-28
            }
        } else {
            if (currswitches & SS_DHIRES) {
                // LORES80 ...
                plotFn = _plot_block80;
            } else {
                /* ??? */
                LOG("!!!!!!!!!!!! what mode is this? !!!!!!!!!!!!");
                plotFn = _plot_block40;
#warning FIXME TODO ... verify this lores40/lores80 mode ...
            }
        }
    }

    return plotFn;
}

drawpage_mode_t display_currentDrawpageMode(uint32_t currswitches) {
    if (currswitches & SS_TEXT) {
        return DRAWPAGE_TEXT;
    } else  {
        if (currswitches & SS_HIRES) {
            return DRAWPAGE_HIRES;
        } else {
            return DRAWPAGE_TEXT; // (LORES)
        }
    }
}

static inline drawpage_mode_t _currentMainMode(uint32_t currswitches) {
    return display_currentDrawpageMode(currswitches);
}

static inline drawpage_mode_t _currentMixedMode(uint32_t currswitches) {
    if (currswitches & (SS_TEXT|SS_MIXED)) {
        return DRAWPAGE_TEXT;
    } else {
        if (currswitches & SS_HIRES) {
            return DRAWPAGE_HIRES;
        } else {
            return DRAWPAGE_TEXT; // (LORES)
        }
    }
}

GLUE_C_WRITE(video__write_2e_text0)
{
    run_args.base_textwrt[ea] = b;
    drawpage_mode_t mode = _currentMainMode(run_args.softswitches);
    if (mode == DRAWPAGE_HIRES) {
        return;
    }
    if (!(run_args.softswitches & SS_PAGE2)) {
        video_setDirty(A2_DIRTY_FLAG);
    }
}

GLUE_C_WRITE(video__write_2e_text0_mixed)
{
    run_args.base_textwrt[ea] = b;
    drawpage_mode_t mode = _currentMixedMode(run_args.softswitches);
    if (mode == DRAWPAGE_HIRES) {
        return;
    }
    if (!(run_args.softswitches & SS_PAGE2)) {
        video_setDirty(A2_DIRTY_FLAG);
    }
}

GLUE_C_WRITE(video__write_2e_text1)
{
    run_args.base_ramwrt[ea] = b;
    drawpage_mode_t mode = _currentMainMode(run_args.softswitches);
    if (mode == DRAWPAGE_HIRES) {
        return;
    }
    if (run_args.softswitches & SS_PAGE2) {
        video_setDirty(A2_DIRTY_FLAG);
    }
}

GLUE_C_WRITE(video__write_2e_text1_mixed)
{
    run_args.base_ramwrt[ea] = b;
    drawpage_mode_t mode = _currentMixedMode(run_args.softswitches);
    if (mode == DRAWPAGE_HIRES) {
        return;
    }
    if (run_args.softswitches & SS_PAGE2) {
        video_setDirty(A2_DIRTY_FLAG);
    }
}

// ----------------------------------------------------------------------------
// Classic interface and printing HUD messages

static void _display_plotChar(uint8_t *fboff, const unsigned int fbPixWidth, const interface_colorscheme_t cs, const uint8_t c) {
    uint8_t *src = video__int_font[cs] + c * (FONT_GLYPH_X*FONT_GLYPH_Y);
    _plot_char80(&fboff, &src, fbPixWidth);
    _plot_char80(&fboff, &src, fbPixWidth);
    _plot_char80(&fboff, &src, fbPixWidth);
    _plot_char80(&fboff, &src, fbPixWidth);
    _plot_char80(&fboff, &src, fbPixWidth);
    _plot_char80(&fboff, &src, fbPixWidth);
    _plot_char80(&fboff, &src, fbPixWidth);
    _plot_char80(&fboff, &src, fbPixWidth);
}

void display_plotChar(uint8_t *fb, const uint8_t col, const uint8_t row, const interface_colorscheme_t cs, const uint8_t c) {
    assert(col < 80);
    assert(row < 24);

    if (textCallbackFn) {
        textCallbackFn((pixel_delta_t){ .row = row, .col = col, .b = c, .cs = cs });
    }

    if (fb) {
        unsigned int off = row * SCANWIDTH * FONT_HEIGHT_PIXELS + col * FONT80_WIDTH_PIXELS + _INTERPOLATED_PIXEL_ADJUSTMENT_PRE;
        _display_plotChar(fb+off, SCANWIDTH, cs, c);
        video_setDirty(FB_DIRTY_FLAG);
    }
}

static void _display_plotLine(uint8_t *fb, const unsigned int fbPixWidth, const unsigned int xAdjust, const uint8_t col, const uint8_t row, const interface_colorscheme_t cs, const char *line) {
    for (uint8_t x=col; *line; x++, line++) {
        char c = *line;

        if (textCallbackFn) {
            textCallbackFn((pixel_delta_t){ .row = row, .col = x, .b = c, .cs = cs });
        }

        if (fb) {
            unsigned int off = row * fbPixWidth * FONT_HEIGHT_PIXELS + x * FONT80_WIDTH_PIXELS + xAdjust;
            _display_plotChar(fb+off, fbPixWidth, cs, c);
        }
    }
}

void display_plotLine(uint8_t *fb, const uint8_t col, const uint8_t row, const interface_colorscheme_t cs, const char *message) {
    _display_plotLine(fb, /*fbPixWidth:*/SCANWIDTH, /*xAdjust:*/_INTERPOLATED_PIXEL_ADJUSTMENT_PRE, col, row, cs, message);
    video_setDirty(FB_DIRTY_FLAG);
}

void display_plotMessage(uint8_t *fb, const interface_colorscheme_t cs, const char *message, const uint8_t message_cols, const uint8_t message_rows) {
    assert(message_cols < 80);
    assert(message_rows < 24);

    int fbPixWidth = (message_cols*FONT80_WIDTH_PIXELS);
    for (int row=0, idx=0; row<message_rows; row++, idx+=message_cols+1) {
        _display_plotLine(fb, fbPixWidth, /*xAdjust:*/0, /*col:*/0, row, cs, &message[ idx ]);
    }
}

// ----------------------------------------------------------------------------
// Double-HIRES (HIRES80) graphics

static inline void __plot_hires80_pixels(uint8_t idx, uint8_t *fb_ptr) {
    if (color_mode == COLOR_NONE) {
        uint32_t b;
        b =   (idx & 0x1) ? COLOR_LIGHT_WHITE : COLOR_BLACK;
        b |= ((idx & 0x2) ? COLOR_LIGHT_WHITE : COLOR_BLACK) << 8;
        b |= ((idx & 0x4) ? COLOR_LIGHT_WHITE : COLOR_BLACK) << 16;
        b |= ((idx & 0x8) ? COLOR_LIGHT_WHITE : COLOR_BLACK) << 24;
        *((uint32_t *)fb_ptr) = b;
        *((uint32_t *)(fb_ptr+SCANWIDTH)) = b;
        return;
    }

    uint8_t b1 = video__dhires1[idx];
    uint8_t b2 = video__dhires2[idx];
    uint32_t b = (b2<<24) | (b1<<8);
    *((uint32_t *)fb_ptr) = b;
    *((uint32_t *)(fb_ptr+SCANWIDTH)) = b;
}

static inline void __plot_hires80(uint16_t base, uint16_t ea, uint8_t *fb_ptr) {
    ea &= ~0x1;

    uint16_t memoff = ea - base;
    fb_ptr = fb_ptr+video__screen_addresses[memoff];
    uint8_t col = video__columns[memoff];

    uint8_t b0 = 0x0;
    uint8_t b1 = 0x0;
    uint32_t b = 0x0;

    if (col) {
        b0 = apple_ii_64k[0][ea-1];
        b1 = apple_ii_64k[1][ea];

        b0 &= ~0x80;
        b0 = (b1<<4)|(b0>>3);

        __plot_hires80_pixels(b0, fb_ptr-4);
    }

    b1 = apple_ii_64k[1][ea+2];
    b = (b1<<28);

    b0 = apple_ii_64k[0][ea+1];
    b0 &= ~0x80;
    b |= (b0<<21);

    b1 = apple_ii_64k[1][ea+1];
    b1 &= ~0x80;
    b |= (b1<<14);

    b0 = apple_ii_64k[0][ea];
    b0 &= ~0x80;
    b |= (b0<<7);

    b1 = apple_ii_64k[1][ea];
    b1 &= ~0x80;
    b |= b1;

    // 00000001 11111122 22222333 3333xxxx

    __plot_hires80_pixels(b, fb_ptr);

    b >>= 4;
    fb_ptr += 4;
    __plot_hires80_pixels(b, fb_ptr);

    b >>= 4;
    fb_ptr += 4;
    __plot_hires80_pixels(b, fb_ptr);

    b >>= 4;
    fb_ptr += 4;
    __plot_hires80_pixels(b, fb_ptr);

    b >>= 4;
    fb_ptr += 4;
    __plot_hires80_pixels(b, fb_ptr);

    b >>= 4;
    fb_ptr += 4;
    __plot_hires80_pixels(b, fb_ptr);

    b >>= 4;
    fb_ptr += 4;
    __plot_hires80_pixels(b, fb_ptr);
}

static void _plot_hires80(uint16_t off, int page, int bank, bool is_even, uint8_t *fb_ptr) {
    assert(bank == 0); // FIXME TODO ...
    uint16_t base = page ? 0x4000 : 0x2000;
    uint16_t ea = base+off;
    __plot_hires80(base, ea, fb_ptr);
}

// ----------------------------------------------------------------------------
// Hires GRaphics

static inline void _calculate_interp_color(uint8_t *color_buf, const unsigned int idx, const uint8_t *interp_base, const uint16_t ea) {
    if (color_buf[idx] != 0x0) {
        return;
    }
    uint8_t pixR = color_buf[idx+1];
    if (pixR == 0x0) {
        return;
    }
    uint8_t pixL = color_buf[idx-1];
    if (pixL == 0x0) {
        return;
    }

    // Calculates the color at the edge of interpolated bytes: called 4 times in little endian order (...7 0...7 0...)
    if (pixL == COLOR_LIGHT_WHITE) {
        if (pixR == COLOR_LIGHT_WHITE) {
            pixL = apple_ii_64k[0][ea];
            color_buf[idx] = interp_base[pixL>>7];
        } else {
            color_buf[idx] = pixR;
        }
    } else {
        color_buf[idx] = pixL;
    }
}

static inline void _plot_hires_pixels(uint8_t *dst, const uint8_t *src) {
    for (unsigned int i=2; i; i--) {
        for (unsigned int j=0; j<DYNAMIC_SZ-1; j++) {
            uint16_t pix = *src;
            pix = ((pix<<8) | pix);
            *((uint16_t *)dst) = pix;
            ++src;
            dst+=2;
        }

        dst += (SCANWIDTH-18);
        src -= DYNAMIC_SZ-2;
    }
}

static void _plot_hires40(uint16_t off, int page, int bank, bool is_even, uint8_t *fb_ptr) {
    assert(bank == 0); // FIXME TODO ...
    uint16_t base = page ? 0x4000 : 0x2000;
    uint16_t ea = base+off;
    uint8_t b = apple_ii_64k[bank][ea];

    fb_ptr = fb_ptr+video__screen_addresses[off];

    uint8_t _buf[DYNAMIC_SZ] = { 0 };
    uint8_t *color_buf = (uint8_t *)_buf; // <--- work around for -Wstrict-aliasing
    uint8_t *apple2_vmem = (uint8_t *)apple_ii_64k[bank];

    uint8_t *hires_ptr = NULL;
    if (is_even) {
        hires_ptr = (uint8_t *)&video__hires_even[b<<3];
    } else {
        hires_ptr = (uint8_t *)&video__hires_odd[b<<3];
    }
    *((uint32_t *)&color_buf[2]) = *((uint32_t *)(hires_ptr+0));
    *((uint16_t *)&color_buf[6]) = *((uint16_t *)(hires_ptr+4));
    *((uint8_t  *)&color_buf[8]) = *((uint8_t  *)(hires_ptr+6));
    hires_ptr = NULL;

    // copy adjacent pixel bytes
    *((uint16_t *)&color_buf[0]) = *((uint16_t *)(fb_ptr-3));
    *((uint16_t *)&color_buf[DYNAMIC_SZ-2]) = *((uint16_t *)(fb_ptr+15));

    if (color_mode != COLOR_NONE) {
        uint8_t *hires_altbase = NULL;
        if (is_even) {
            hires_altbase = (uint8_t *)&video__hires_odd[0];
        } else {
            hires_altbase = (uint8_t *)&video__hires_even[0];
        }

        // if right-side color is not black, re-calculate edge values
        if (color_buf[DYNAMIC_SZ-2] & 0xff) {
            uint16_t pix16 = *((uint16_t *)(apple2_vmem+ea));
            if ((pix16 & 0x100) && (pix16 & 0x40)) {
                *((uint16_t *)&color_buf[DYNAMIC_SZ-3]) = (uint16_t)0x3737;// COLOR_LIGHT_WHITE
            }
        }

        // if left-side color is not black, re-calculate edge values
        if (color_buf[1] & 0xff) {
            uint16_t pix16 = *((uint16_t *)(apple2_vmem+ea-1));
            if ((pix16 & 0x100) && (pix16 & 0x40)) {
                *((uint16_t *)&color_buf[1]) = (uint16_t)0x3737;// COLOR_LIGHT_WHITE
            }
        }

        if (color_mode == COLOR_INTERP) {
            uint8_t *interp_base = NULL;
            uint8_t *interp_altbase = NULL;
            if (is_even) {
                interp_base = (uint8_t *)&video__even_colors[0];
                interp_altbase = (uint8_t *)&video__odd_colors[0];
            } else {
                interp_base = (uint8_t *)&video__odd_colors[0];
                interp_altbase = (uint8_t *)&video__even_colors[0];
            }

            // calculate interpolated/bleed colors
            // NOTE that this doesn't check under/overflow of ea (for example at 0x2000, 0x4000, 0x3FFF, 0x5FFF)
            // ... but don't think this really matters much here =P
            _calculate_interp_color(color_buf, 1, interp_altbase, ea-1);
            _calculate_interp_color(color_buf, 2, interp_base, ea);
            _calculate_interp_color(color_buf, 8, interp_base, ea);
            _calculate_interp_color(color_buf, 9, interp_altbase, ea+1);
        }
    }

    _plot_hires_pixels(/*dst:*/fb_ptr-4, /*src:*/color_buf);
}

static void (*_hirespage_plotter(uint32_t currswitches))(uint16_t, int, int, bool, uint8_t*) {
    return ((currswitches & SS_80COL) && (currswitches & SS_DHIRES)) ? _plot_hires80 : _plot_hires40;
}

GLUE_C_WRITE(video__write_2e_hgr0)
{
    run_args.base_hgrwrt[ea] = b;
    drawpage_mode_t mode = _currentMainMode(run_args.softswitches);
    if (mode == DRAWPAGE_TEXT) {
        return;
    }
    if (!(run_args.softswitches & SS_PAGE2)) {
        video_setDirty(A2_DIRTY_FLAG);
    }
}

GLUE_C_WRITE(video__write_2e_hgr0_mixed)
{
    run_args.base_hgrwrt[ea] = b;
    drawpage_mode_t mode = _currentMixedMode(run_args.softswitches);
    if (mode == DRAWPAGE_TEXT) {
        return;
    }
    if (!(run_args.softswitches & SS_PAGE2)) {
        video_setDirty(A2_DIRTY_FLAG);
    }
}

GLUE_C_WRITE(video__write_2e_hgr1)
{
    run_args.base_ramwrt[ea] = b;
    drawpage_mode_t mode = _currentMainMode(run_args.softswitches);
    if (mode == DRAWPAGE_TEXT) {
        return;
    }
    if (run_args.softswitches & SS_PAGE2) {
        video_setDirty(A2_DIRTY_FLAG);
    }
}

GLUE_C_WRITE(video__write_2e_hgr1_mixed)
{
    run_args.base_ramwrt[ea] = b;
    drawpage_mode_t mode = _currentMixedMode(run_args.softswitches);
    if (mode == DRAWPAGE_TEXT) {
        return;
    }
    if (run_args.softswitches & SS_PAGE2) {
        video_setDirty(A2_DIRTY_FLAG);
    }
}

// ----------------------------------------------------------------------------

static inline void _currentPageAndBank(uint32_t currswitches, drawpage_mode_t mode, OUTPARM int *page, OUTPARM int *bank) {
    // UTAIIe : 5-25
    if (currswitches & SS_80STORE) {
        *page = 0;
        //*bank = !!(currswitches & SS_PAGE2);
        *bank = 0;
        if (mode != DRAWPAGE_TEXT) {
            assert(currswitches & SS_HIRES);
        }
        return;
    }

    *page = !!(currswitches & SS_PAGE2);
    //*bank = !!(currswitches & SS_RAMRD);
    *bank = 0;
}

void display_setUpdateCallback(drawpage_mode_t mode, display_update_fn updateFn) {
    if (mode == DRAWPAGE_TEXT) {
        textCallbackFn = updateFn;
    } else if (mode == DRAWPAGE_HIRES) {
        hiresCallbackFn = updateFn;
    } else if (mode == DRAWPAGE_MODE_CHANGE) {
        modeCallbackFn = updateFn;
    } else {
        assert(false);
    }
}

void display_renderStagingFramebuffer(uint8_t *stagingFB) {

#warning FIXME TODO ... this needs to scan memory in the same way as the actually //e video scanner

    pthread_mutex_lock(&display_scan_mutex);

    int page = 0;
    int bank = 0;
    const uint32_t mainswitches = run_args.softswitches;

    // render main portion of screen ...

    drawpage_mode_t mainDrawPageMode = _currentMainMode(mainswitches);
    _currentPageAndBank(mainswitches, mainDrawPageMode, &page, &bank);

    if (mainDrawPageMode == DRAWPAGE_TEXT) {
        void (*textMainPlotFn)(uint8_t, uint8_t, int, int, uint8_t*) = _textpage_plotter(mainswitches, SS_TEXT);
        for (unsigned int y=0; y < TEXT_ROWS-4; y++) {
            for (unsigned int x=0; x < TEXT_COLS; x++) {
                textMainPlotFn(x, y, page, bank, stagingFB);
            }
        }
    } else {
        assert(!(mainswitches & SS_TEXT) && "TEXT should not be set");
        assert((mainswitches & SS_HIRES) && "HIRES should be set");
        void (*hiresMainPlotFn)(uint16_t, int, int, bool, uint8_t*) = _hirespage_plotter(mainswitches);
        for (unsigned int y=0; y < TEXT_ROWS-4; y++) {
            for (unsigned int x=0; x < TEXT_COLS; x++) {
                for (unsigned int i = 0; i < 8; i++) {
                    uint16_t off = video__line_offset[y] + (0x400*i) + x;
                    hiresMainPlotFn(off, page, bank, /*even*/!(x & 1), stagingFB);
                }
            }
        }
    }

    // resample current switches ... and render mixed portion of screen
    const uint32_t mixedswitches = run_args.softswitches;

    drawpage_mode_t mixedDrawPageMode = _currentMixedMode(mixedswitches);
    _currentPageAndBank(mixedswitches, mixedDrawPageMode, &page, &bank);

    if (mixedDrawPageMode == DRAWPAGE_TEXT) {
        void (*textMixedPlotFn)(uint8_t, uint8_t, int, int, uint8_t*) = _textpage_plotter(mixedswitches, (SS_TEXT|SS_MIXED));
        for (unsigned int y=TEXT_ROWS-4; y < TEXT_ROWS; y++) {
            for (unsigned int x=0; x < TEXT_COLS; x++) {
                textMixedPlotFn(x, y, page, bank, stagingFB);
            }
        }
    } else {
        //assert(!(mixedswitches & SS_TEXT) && "TEXT should not be set"); // TEXT may have been reset from last sample?
        assert(!(mixedswitches & SS_MIXED) && "MIXED should not be set");
        assert((mixedswitches & SS_HIRES) && "HIRES should be set");
        void (*hiresMixedPlotFn)(uint16_t, int, int, bool, uint8_t*) = _hirespage_plotter(mixedswitches);
        for (unsigned int y=TEXT_ROWS-4; y < TEXT_ROWS; y++) {
            for (unsigned int x=0; x < TEXT_COLS; x++) {
                for (unsigned int i = 0; i < 8; i++) {
                    uint16_t off = video__line_offset[y] + (0x400*i) + x;
                    hiresMixedPlotFn(off, page, bank, /*even*/!(x & 1), stagingFB);
                }
            }
        }
    }

    video_setDirty(FB_DIRTY_FLAG);

    pthread_mutex_unlock(&display_scan_mutex);
}

void display_flashText(void) {
    static bool normal = false;

    normal = !normal;

    // flash only if it's text or mixed modes.
    if (run_args.softswitches & (SS_TEXT|SS_MIXED)) {
        if (normal) {
            colormap[ COLOR_FLASHING_BLACK].red   = 0;
            colormap[ COLOR_FLASHING_BLACK].green = 0;
            colormap[ COLOR_FLASHING_BLACK].blue  = 0;

            colormap[ COLOR_FLASHING_WHITE].red   = 0xff;
            colormap[ COLOR_FLASHING_WHITE].green = 0xff;
            colormap[ COLOR_FLASHING_WHITE].blue  = 0xff;
        } else {
            colormap[ COLOR_FLASHING_BLACK].red   = 0xff;
            colormap[ COLOR_FLASHING_BLACK].green = 0xff;
            colormap[ COLOR_FLASHING_BLACK].blue  = 0xff;

            colormap[ COLOR_FLASHING_WHITE].red   = 0;
            colormap[ COLOR_FLASHING_WHITE].green = 0;
            colormap[ COLOR_FLASHING_WHITE].blue  = 0;
        }

        video_setDirty(FB_DIRTY_FLAG);
    }
}

bool video_isDirty(unsigned long flags) {
    return (_vid_dirty & flags);
}

unsigned long video_setDirty(unsigned long flags) {
    if (modeCallbackFn) {
        modeCallbackFn((pixel_delta_t){ 0 });
    }
    return __sync_fetch_and_or(&_vid_dirty, flags);
}

unsigned long video_clearDirty(unsigned long flags) {
    return __sync_fetch_and_and(&_vid_dirty, ~flags);
}

// ----------------------------------------------------------------------------
// VBL/timing routines

// References to Jim Sather's books are given as eg:
// UTAIIe:5-7,P3 (Understanding the Apple IIe, chapter 5, page 7, Paragraph 3)

extern unsigned int CpuGetCyclesThisVideoFrame(void);
uint16_t video_scanner_get_address(bool *vblBarOut) {
    const bool SW_HIRES   = (run_args.softswitches & SS_HIRES);
    const bool SW_TEXT    = (run_args.softswitches & SS_TEXT);
    const bool SW_PAGE2   = (run_args.softswitches & SS_PAGE2);
    const bool SW_80STORE = (run_args.softswitches & SS_80STORE);
    const bool SW_MIXED   = (run_args.softswitches & SS_MIXED);

    // get video scanner position
    unsigned int nCycles = CpuGetCyclesThisVideoFrame();

    // machine state switches
    int nHires   = (SW_HIRES && !SW_TEXT) ? 1 : 0;
    int nPage2   = SW_PAGE2 ? 1 : 0;
    int n80Store = SW_80STORE ? 1 : 0;

    // calculate video parameters according to display standard
    int nScanLines  = bVideoScannerNTSC ? kNTSCScanLines : kPALScanLines;
    int nVSyncLine  = bVideoScannerNTSC ? kNTSCVSyncLine : kPALVSyncLine;
    int nScanCycles = nScanLines * kHClocks;

    // calculate horizontal scanning state
    int nHClock = (nCycles + kHPEClock) % kHClocks; // which horizontal scanning clock
    int nHState = kHClock0State + nHClock; // H state bits
    if (nHClock >= kHPresetClock) // check for horizontal preset
    {
        nHState -= 1; // correct for state preset (two 0 states)
    }
    int h_0 = (nHState >> 0) & 1; // get horizontal state bits
    int h_1 = (nHState >> 1) & 1;
    int h_2 = (nHState >> 2) & 1;
    int h_3 = (nHState >> 3) & 1;
    int h_4 = (nHState >> 4) & 1;
    int h_5 = (nHState >> 5) & 1;

    // calculate vertical scanning state
    int nVLine  = nCycles / kHClocks; // which vertical scanning line
    int nVState = kVLine0State + nVLine; // V state bits
    if ((nVLine >= kVPresetLine)) // check for previous vertical state preset
    {
        nVState -= nScanLines; // compensate for preset
    }
    int v_A = (nVState >> 0) & 1; // get vertical state bits
    int v_B = (nVState >> 1) & 1;
    int v_C = (nVState >> 2) & 1;
    int v_0 = (nVState >> 3) & 1;
    int v_1 = (nVState >> 4) & 1;
    int v_2 = (nVState >> 5) & 1;
    int v_3 = (nVState >> 6) & 1;
    int v_4 = (nVState >> 7) & 1;
    int v_5 = (nVState >> 8) & 1;

    // calculate scanning memory address
    if (nHires && SW_MIXED && v_4 && v_2) // HIRES TIME signal (UTAIIe:5-7,P3)
    {
        nHires = 0; // address is in text memory for mixed hires
    }

    int nAddend0 = 0x0D; // 1            1            0            1
    int nAddend1 =              (h_5 << 2) | (h_4 << 1) | (h_3 << 0);
    int nAddend2 = (v_4 << 3) | (v_3 << 2) | (v_4 << 1) | (v_3 << 0);
    int nSum     = (nAddend0 + nAddend1 + nAddend2) & 0x0F; // SUM (UTAIIe:5-9)

    unsigned int nAddress = 0; // build address from video scanner equations (UTAIIe:5-8,T5.1)
    nAddress |= h_0  << 0; // a0
    nAddress |= h_1  << 1; // a1
    nAddress |= h_2  << 2; // a2
    nAddress |= nSum << 3; // a3 - a6
    nAddress |= v_0  << 7; // a7
    nAddress |= v_1  << 8; // a8
    nAddress |= v_2  << 9; // a9

    int p2a = !(nPage2 && !n80Store);
    int p2b = nPage2 && !n80Store;

    if (nHires) // hires?
    {
        // Y: insert hires-only address bits
        nAddress |= v_A << 10; // a10
        nAddress |= v_B << 11; // a11
        nAddress |= v_C << 12; // a12
        nAddress |= p2a << 13; // a13
        nAddress |= p2b << 14; // a14
    }
    else
    {
        // N: insert text-only address bits
        nAddress |= p2a << 10; // a10
        nAddress |= p2b << 11; // a11

        // Apple ][ (not //e) and HBL?
        if (false/*IS_APPLE2*/ && // Apple II only (UTAIIe:I-4,#5)
            !h_5 && (!h_4 || !h_3)) // HBL (UTAIIe:8-10,F8.5)
        {
            nAddress |= 1 << 12; // Y: a12 (add $1000 to address!)
        }
    }

    // update VBL' state
    if (vblBarOut != NULL)
    {
        *vblBarOut = !v_4 || !v_3; // VBL' = (v_4 & v_3)' (UTAIIe:5-10,#3)
    }

    return (uint16_t)nAddress;
}

uint8_t floating_bus(void) {
#if TESTING
    // HACK FIXME TODO : where is the non-determinism sneaking in here?
    return 0;
#endif
    uint16_t scanner_addr = video_scanner_get_address(NULL);
    return apple_ii_64k[0][scanner_addr];
}

uint8_t floating_bus_hibit(const bool hibit) {
#if TESTING
    // HACK FIXME TODO : where is the non-determinism sneaking in here?
    return 0;
#endif
    uint16_t scanner_addr = video_scanner_get_address(NULL);
    uint8_t b = apple_ii_64k[0][scanner_addr];
    return (b & ~0x80) | (hibit ? 0x80 : 0);
}

static void _init_interface(void) {
    LOG("Initializing display subsystem");
    _initialize_interface_fonts();
    _initialize_hires_values();
    _initialize_row_col_tables();
    _initialize_dhires_values();
    _initialize_color();

    prefs_registerListener(PREF_DOMAIN_VIDEO, &video_prefsChanged);
}

static __attribute__((constructor)) void __init_interface(void) {
    emulator_registerStartupCallback(CTOR_PRIORITY_LATE, &_init_interface);
}

#if __clang__
#   pragma clang diagnostic pop
#elif __GNUC__
#   pragma GCC diagnostic pop
#endif

