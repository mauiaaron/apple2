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

#define SCANSTEP (SCANWIDTH-12)
#define SCANDSTEP (SCANWIDTH-6)

#define DYNAMIC_SZ 11 // 7 pixels (as bytes) + 2pre + 2post

A2Color_s colormap[256] = { { 0 } };

static uint8_t video__wider_font[0x8000] = { 0 };
static uint8_t video__font[0x4000] = { 0 };
static uint8_t video__int_font[5][0x4000] = { { 0 } }; // interface font

static color_mode_t color_mode = COLOR_NONE;

// Precalculated framebuffer offsets given VM addr
static unsigned int video__screen_addresses[8192] = { INT_MIN };
static uint8_t video__columns[8192] = { 0 };

static uint8_t video__hires_even[0x800] = { 0 };
static uint8_t video__hires_odd[0x800] = { 0 };

#define FB_SIZ (SCANWIDTH*SCANHEIGHT*sizeof(uint8_t))

#if INTERFACE_CLASSIC
static uint8_t fbInterface[FB_SIZ] = { 0 };
#endif

static uint8_t fbStaging[FB_SIZ] = { 0 };
#define FB_BASE (&fbStaging[0])

static uint8_t video__odd_colors[2] = { COLOR_LIGHT_PURPLE, COLOR_LIGHT_BLUE };
static uint8_t video__even_colors[2] = { COLOR_LIGHT_GREEN, COLOR_LIGHT_RED };

// video line offsets
static uint16_t video__line_offset[TEXT_ROWS + /*VBL:*/ 8 + /*extra:*/1] = {
    0x000, 0x080, 0x100, 0x180, 0x200, 0x280, 0x300, 0x380,
    0x028, 0x0A8, 0x128, 0x1A8, 0x228, 0x2A8, 0x328, 0x3A8,
    0x050, 0x0D0, 0x150, 0x1D0, 0x250, 0x2D0, 0x350, 0x3D0,
    0x078, 0x0F8, 0x178, 0x1F8, 0x278, 0x2F8, 0x378, 0x3F8,
    0x3F8
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

#warning FIXME TODO : move _initialize_tables_video() to vm.c ...
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

static void display_prefsChanged(const char *domain) {
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

static inline void _plot_lores80(uint8_t *d, const uint32_t lo, const uint32_t hi) {
    *((uint32_t *)(d)) = lo;
    *((uint32_t *)(d + SCANWIDTH)) = lo;
    d += 4;
    *((uint32_t *)(d)) = hi;
    *((uint32_t *)(d + SCANWIDTH)) = hi;
}

static void _plot_char40_scanline(scan_data_t *scandata) {
    uint8_t *scanline = scandata->scanline;
    unsigned int scanrow = scandata->scanrow;
    unsigned int scancol = scandata->scancol;
    unsigned int scanend = scandata->scanend;

    uint16_t fb_base = video__line_offset[scanrow>>3];
    uint16_t glyph_off = (scanrow&0x7);

    for (unsigned int col = scancol; col < scanend; col++)
    {
        uint16_t fb_off = fb_base + col;
        uint8_t b = scanline[(col<<1)+1]; // MBD data only

        unsigned int glyph_base = (b<<7); // *128

        uint8_t *fb_ptr = FB_BASE + video__screen_addresses[fb_off] + ((glyph_off<<1) * SCANWIDTH);
        uint8_t *font_ptr = video__wider_font + glyph_base + (glyph_off<<4);

        _plot_char40(/*dst:*/&fb_ptr, /*src:*/&font_ptr);
    }
}

static void _plot_char80_scanline(scan_data_t *scandata) {
    uint8_t *scanline = scandata->scanline;
    unsigned int scanrow   = scandata->scanrow;
    unsigned int scancol   = scandata->scancol;
    unsigned int scanend = scandata->scanend;

    uint16_t fb_base = video__line_offset[scanrow>>3];
    uint16_t glyph_off = (scanrow&0x7);

    for (unsigned int col = scancol; col < scanend; col++)
    {
        uint16_t fb_off = fb_base + col;
        for (unsigned int x=0; x<2; x++) {
            uint8_t b = scanline[(col<<1)+x]; // AUX, MBD
            unsigned int glyph_base = (b<<6); // *64

            uint8_t *fb_ptr = FB_BASE + video__screen_addresses[fb_off] + (7*x) + ((glyph_off<<1) * SCANWIDTH);
            uint8_t *font_ptr = video__font + glyph_base + (glyph_off<<3);

            _plot_char80(/*dst:*/&fb_ptr, /*src:*/&font_ptr, SCANWIDTH);
        }
    }
}

static void _plot_lores40_scanline(scan_data_t *scandata) {
    uint8_t *scanline = scandata->scanline;
    unsigned int scanrow = scandata->scanrow;
    unsigned int scancol = scandata->scancol;
    unsigned int scanend = scandata->scanend;

    uint16_t fb_base = video__line_offset[scanrow>>3];
    uint16_t block_off = (scanrow&0x7);

    uint8_t hi_nyb = !!(block_off>>2); // 0,1,2,3 => 0  4,5,6,7 => 1
    uint8_t lores_mask = (0x0f << (hi_nyb<<2) ); // 0x0f --or-- 0xf0
    uint8_t lores_shift = ((1-hi_nyb)<<2); // -> 0xi0

    for (unsigned int col = scancol; col < scanend; col++)
    {
        uint16_t fb_off = fb_base + col;
        uint8_t *fb_ptr = FB_BASE + video__screen_addresses[fb_off] + ((block_off<<1) * SCANWIDTH);

        uint8_t b = scanline[(col<<1)+1]; // MBD data only
        uint8_t val = (b & lores_mask) << lores_shift;

        uint32_t val32;
        if (color_mode == COLOR_NONE) {
            uint8_t rot2 = ((col % 2) << 1); // 2 phases at double rotation
            val = (val << rot2) | ((val & 0xC0) >> rot2);
            val32 =  ((val & 0x10) ? COLOR_LIGHT_WHITE : COLOR_BLACK) << 0;
            val32 |= ((val & 0x20) ? COLOR_LIGHT_WHITE : COLOR_BLACK) << 8;
            val32 |= ((val & 0x40) ? COLOR_LIGHT_WHITE : COLOR_BLACK) << 16;
            val32 |= ((val & 0x80) ? COLOR_LIGHT_WHITE : COLOR_BLACK) << 24;
        } else {
            val32 = (val << 24) | (val << 16) | (val << 8) | val;
        }

        _plot_lores40(/*dst:*/&fb_ptr, val32);
    }
}

static inline uint8_t __shift_block80(uint8_t b) {
    // plot even half-block from auxmem, rotate nybbles to match color (according to UTAIIe: 8-29)
    uint8_t b0 = (b & 0x0F);
    uint8_t b1 = (b & 0xF0) >> 4;
    uint8_t rot0 = ((b0 & 0x8) >> 3);
    uint8_t rot1 = ((b1 & 0x8) >> 3);
    b0 = ((b0<<5) | (rot0<<4));
    b1 = ((b1<<5) | (rot1<<4));
    b = (b0>>4) | b1;
    return b;
}

static void _plot_lores80_scanline(scan_data_t *scandata) {
    uint8_t *scanline = scandata->scanline;
    unsigned int scanrow   = scandata->scanrow;
    unsigned int scancol   = scandata->scancol;
    unsigned int scanend = scandata->scanend;

    uint16_t fb_base = video__line_offset[scanrow>>3];
    uint16_t block_off = (scanrow&0x7);

    uint8_t hi_nyb = !!(block_off>>2); // 0,1,2,3 => 0  4,5,6,7 => 1
    uint8_t lores_mask = (0x0f << (hi_nyb<<2) ); // 0x0f --or-- 0xf0
    uint8_t lores_shift = ((1-hi_nyb)<<2); // -> 0xi0

    for (unsigned int col = scancol; col < scanend; col++)
    {
        uint16_t fb_off = fb_base + col;
        {
            uint8_t *fb_ptr = FB_BASE + video__screen_addresses[fb_off] + ((block_off<<1) * SCANWIDTH);

            unsigned int idx = (col<<1)+0; // AUX
            uint8_t b = scanline[idx];
            b = __shift_block80(b);
            uint8_t val = (b & lores_mask) << lores_shift;
            uint32_t val32_lo = 0x0;
            uint32_t val32_hi = 0x0;

            if (color_mode == COLOR_NONE && val != 0x0) {
                val = (val >> 4) | val;
                {
                    uint16_t val16 = val | (val << 8);
                    val16 = val16 >> (4 - (idx&0x3));
                    val = (uint8_t)val16;
                }

                val32_lo |= ((val & 0x01) ? COLOR_LIGHT_WHITE : COLOR_BLACK) << 0;
                val32_lo |= ((val & 0x02) ? COLOR_LIGHT_WHITE : COLOR_BLACK) << 8;
                val32_lo |= ((val & 0x04) ? COLOR_LIGHT_WHITE : COLOR_BLACK) << 16;
                val32_lo |= ((val & 0x08) ? COLOR_LIGHT_WHITE : COLOR_BLACK) << 24;
                val32_hi |= ((uint64_t)((val & 0x10) ? COLOR_LIGHT_WHITE : COLOR_BLACK)) << 0;
                val32_hi |= ((uint64_t)((val & 0x20) ? COLOR_LIGHT_WHITE : COLOR_BLACK)) << 8;
                val32_hi |= ((uint64_t)((val & 0x40) ? COLOR_LIGHT_WHITE : COLOR_BLACK)) << 16;
            } else {
                val32_hi = (val << 16) | (val << 8) | val;
                val32_lo = (val << 24) | val32_hi;
            }

            _plot_lores80(/*dst:*/fb_ptr, val32_lo, val32_hi);
        }

        {
            uint8_t *fb_ptr = FB_BASE + video__screen_addresses[fb_off] + 7 + ((block_off<<1) * SCANWIDTH);

            unsigned int idx = (col<<1)+1; // MBD
            uint8_t b = scanline[idx];
            uint8_t val = (b & lores_mask) << lores_shift;
            uint32_t val32_lo = 0x0;
            uint32_t val32_hi = 0x0;

            if (color_mode == COLOR_NONE && val != 0x0) {
                val = (val >> 4) | val;
                {
                    uint16_t val16 = val | (val << 8);
                    val16 = val16 >> (4 - (idx&0x3));
                    val = (uint8_t)val16;
                }

                val32_lo |= ((val & 0x01) ? COLOR_LIGHT_WHITE : COLOR_BLACK) << 0;
                val32_lo |= ((val & 0x02) ? COLOR_LIGHT_WHITE : COLOR_BLACK) << 8;
                val32_lo |= ((val & 0x04) ? COLOR_LIGHT_WHITE : COLOR_BLACK) << 16;
                val32_lo |= ((val & 0x08) ? COLOR_LIGHT_WHITE : COLOR_BLACK) << 24;
                val32_hi |= ((uint64_t)((val & 0x10) ? COLOR_LIGHT_WHITE : COLOR_BLACK)) << 0;
                val32_hi |= ((uint64_t)((val & 0x20) ? COLOR_LIGHT_WHITE : COLOR_BLACK)) << 8;
                val32_hi |= ((uint64_t)((val & 0x40) ? COLOR_LIGHT_WHITE : COLOR_BLACK)) << 16;
            } else {
                val32_hi = (val << 16) | (val << 8) | val;
                val32_lo = (val << 24) | val32_hi;
            }

            _plot_lores80(/*dst:*/fb_ptr, val32_lo, val32_hi);
        }
    }
}

static void (*_textpage_plotter(uint32_t currswitches, uint32_t txtflags))(scan_data_t*) {
    void (*plotFn)(scan_data_t*) = NULL;

    if (currswitches & txtflags) {
        plotFn = (currswitches & SS_80COL) ? _plot_char80_scanline : _plot_char40_scanline;
    } else {
        assert(!(currswitches & SS_HIRES) && "must be lores graphics or programmer error");
        if (!(currswitches & SS_80COL)) {
            plotFn = _plot_lores40_scanline;
            if (!(currswitches & SS_DHIRES)) {
                // LORES40 ...
            } else {
                // TODO : abnormal LORES output.  See UTAIIe : 8-28
            }
        } else {
            if (currswitches & SS_DHIRES) {
                // LORES80 ...
                plotFn = _plot_lores80_scanline;
            } else {
                /* ??? */
                //LOG("!!!!!!!!!!!! what mode is this? !!!!!!!!!!!!");
                plotFn = _plot_lores40_scanline;
#warning FIXME TODO ... verify this lores40/lores80 mode ...
            }
        }
    }

    return plotFn;
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

#if INTERFACE_CLASSIC
void display_plotChar(const uint8_t col, const uint8_t row, const interface_colorscheme_t cs, const uint8_t c) {
    assert(col < 80);
    assert(row < 24);
    unsigned int off = row * SCANWIDTH * FONT_HEIGHT_PIXELS + col * FONT80_WIDTH_PIXELS + _INTERPOLATED_PIXEL_ADJUSTMENT_PRE;
    _display_plotChar(fbInterface+off, SCANWIDTH, cs, c);
    video_setDirty(FB_DIRTY_FLAG);
}
#endif

static void _display_plotLine(uint8_t *fb, const unsigned int fbPixWidth, const unsigned int xAdjust, const uint8_t col, const uint8_t row, const interface_colorscheme_t cs, const char *line) {
    for (uint8_t x=col; *line; x++, line++) {
        char c = *line;
        unsigned int off = row * fbPixWidth * FONT_HEIGHT_PIXELS + x * FONT80_WIDTH_PIXELS + xAdjust;
        _display_plotChar(fb+off, fbPixWidth, cs, c);
    }
}

#if INTERFACE_CLASSIC
void display_plotLine(const uint8_t col, const uint8_t row, const interface_colorscheme_t cs, const char *message) {
    _display_plotLine(fbInterface, /*fbPixWidth:*/SCANWIDTH, /*xAdjust:*/_INTERPOLATED_PIXEL_ADJUSTMENT_PRE, col, row, cs, message);
    video_setDirty(FB_DIRTY_FLAG);
}
#endif

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
    uint8_t bCurr = idx;

    if (color_mode == COLOR_NONE) {
        uint32_t b32;
        b32 =   (bCurr & 0x1) ? COLOR_LIGHT_WHITE : COLOR_BLACK;
        b32 |= ((bCurr & 0x2) ? COLOR_LIGHT_WHITE : COLOR_BLACK) << 8;
        b32 |= ((bCurr & 0x4) ? COLOR_LIGHT_WHITE : COLOR_BLACK) << 16;
        b32 |= ((bCurr & 0x8) ? COLOR_LIGHT_WHITE : COLOR_BLACK) << 24;
        *((uint32_t *)fb_ptr) = b32;
        *((uint32_t *)(fb_ptr+SCANWIDTH)) = b32;
    } else {
        // TODO FIXME : handle interpolated here ...
        uint32_t b32;
        b32 =   (bCurr & 0x1) ? bCurr : COLOR_BLACK;
        b32 |= ((bCurr & 0x2) ? bCurr : COLOR_BLACK) << 8;
        b32 |= ((bCurr & 0x4) ? bCurr : COLOR_BLACK) << 16;
        b32 |= ((bCurr & 0x8) ? bCurr : COLOR_BLACK) << 24;
        *((uint32_t *)fb_ptr) = b32;
        *((uint32_t *)(fb_ptr+SCANWIDTH)) = b32;
    }
}

static void _plot_hires80_scanline(scan_data_t *scandata) {
    uint8_t *scanline = scandata->scanline;
    unsigned int scanrow = scandata->scanrow;
    unsigned int scancol = scandata->scancol;
    unsigned int scanend = scandata->scanend;

    uint16_t fb_base = video__line_offset[scanrow>>3] + (0x400 * (scanrow & 0x7));

    // |AUX 0a|MBD 0b |AUX 1c |MBD 1d |AUX 2e |MBD 2f |AUX 3g |MBD 3h ...
    // aaaaaaab bbbbbbcc cccccddd ddddeeee eeefffff ffgggggg ghhhhhhh ...
    // 01234560 12345601 23456012 34560123 45601234 56012345 60123456

    for (unsigned int col = scancol; col < scanend; col++)
    {
        uint16_t fb_off = fb_base + col;

        uint8_t idx = (col<<1);
        uint8_t is_odd = (col & 0x1);

        uint8_t *fb_ptr = FB_BASE + video__screen_addresses[fb_off];

        uint8_t aux = scanline[idx];   // AUX
        uint8_t mbd = scanline[idx+1]; // MBD

        if (!is_odd) {
            // idx = 0 -> aaaa aaab bbbb 0000
            // idx = 2 -> eeee eeef ffff 0000

            uint8_t auxLO = (aux & (0x0F<<0)) >> 0; // 0000aaaa -> 0000aaaa
            __plot_hires80_pixels(auxLO, fb_ptr);
            fb_ptr += 4;

            uint8_t auxHI = (aux & (0x07<<4)) >> 4; // 0aaa0000 -> 00000aaa
            uint8_t mbdLO = (mbd & (0x01<<0)) << 3; // 0000000b -> 0000b000
            __plot_hires80_pixels((mbdLO | auxHI), fb_ptr);
            fb_ptr += 4;

            uint8_t mbdXX = (mbd & (0x0F<<1)) >> 1; // 000bbbb0 -> 0000bbbb
            __plot_hires80_pixels(mbdXX, fb_ptr);
            fb_ptr += 4;

            /*
            // partial ... overwritten by next even scan ...
            uint8_t mbdHI = (mbd & (0x03<<5)) >> 5; // 0bb00000 -> 0000XXbb
            __plot_hires80_pixels(mbdHI, fb_ptr);
            */

        } else {
            // idx = 1 -> bbcc cccc cddd dddd
            // idx = 3 -> ffgg gggg ghhh hhhh

            fb_ptr -= 2;

            uint8_t mb0 = scanline[idx-1]; // MBD-1

            uint8_t mbdHI = (mb0 & (0x03<<5)) >> 5; // 0bb00000 -> 000000bb
            uint8_t auxLO = (aux & (0x03<<0)) << 2; // 000000cc -> 0000cc00
            __plot_hires80_pixels((auxLO | mbdHI), fb_ptr);
            fb_ptr += 4;

            uint8_t auxXX = (aux & (0x0F<<2)) >> 2; // 00cccc00 -> 0000cccc
            __plot_hires80_pixels(auxXX, fb_ptr);
            fb_ptr += 4;

            uint8_t auxHI = (aux & (0x01<<6)) >> 6; // 0c000000 -> 0000000c
            uint8_t mbdLO = (mbd & (0x07<<0)) << 1; // 00000ddd -> 0000ddd0
            __plot_hires80_pixels((mbdLO | auxHI), fb_ptr);
            fb_ptr += 4;

            uint8_t mbdXX = (mbd & (0x0F<<3)) >> 3; // 0dddd000 -> 0000dddd
            __plot_hires80_pixels(mbdXX, fb_ptr);
        }
    }
}

// ----------------------------------------------------------------------------
// Hires GRaphics (HIRES40)

static inline void _calculate_interp_color(uint8_t *color_buf, const unsigned int idx, const uint8_t *interp_base, uint8_t b) {
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
            pixL = b;
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

static void _plot_hires40_scanline(scan_data_t *scandata) {

    // FIXME TODO ... this can be further streamlined to keep track of previous data

    uint8_t *scanline = scandata->scanline;
    unsigned int scanrow   = scandata->scanrow;
    unsigned int scancol   = scandata->scancol;
    unsigned int scanend = scandata->scanend;
    assert(scancol < scanend);
    assert(scanend > 0);

    uint16_t fb_base = video__line_offset[scanrow>>3] + (0x400 * (scanrow & 0x7));
    for (unsigned int col = scancol; col < scanend; col++)
    {
        uint16_t fb_off = fb_base + col;
        uint8_t *fb_ptr = FB_BASE + video__screen_addresses[fb_off];

        bool is_even = !(col & 0x1);
        uint8_t idx = (col<<1)+1; // MBD data only
        uint8_t b = scanline[idx];

        uint8_t _buf[DYNAMIC_SZ] = { 0 };
        uint8_t *color_buf = (uint8_t *)_buf; // <--- work around for -Wstrict-aliasing

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
                if ((col < CYCLES_VIS-1) && (col < scanend - 1)) {
                    uint8_t bNext = scanline[idx+2];
                    if ((b & 0x40) && (bNext & 0x1)) {
                        *((uint16_t *)&color_buf[DYNAMIC_SZ-3]) = (uint16_t)0x3737;// COLOR_LIGHT_WHITE
                    }
                }
            }

            // if left-side color is not black, re-calculate edge values
            if (color_buf[1] & 0xff) {
                if (col > 0) {
                    uint8_t bPrev = scanline[idx-2];
                    if ((bPrev & 0x40) && (b & 0x1)) {
                        *((uint16_t *)&color_buf[1]) = (uint16_t)0x3737;// COLOR_LIGHT_WHITE
                    }
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
                uint8_t bl = 0x0;
                if (col > 0) {
                    bl = scanline[idx-2];
                }
                _calculate_interp_color(color_buf, 1, interp_altbase, bl);
                _calculate_interp_color(color_buf, 2, interp_base, b);
                _calculate_interp_color(color_buf, 8, interp_base, b);
                if (col < CYCLES_VIS-1) {
                    bl = scanline[idx+2];
                }
                _calculate_interp_color(color_buf, 9, interp_altbase, bl);
            }
        }

        _plot_hires_pixels(/*dst:*/fb_ptr-4, /*src:*/color_buf);
        ////fb_ptr += 7;
    }
}

static void (*_hirespage_plotter(uint32_t currswitches))(scan_data_t*) {
    return ((currswitches & SS_80COL) && (currswitches & SS_DHIRES)) ? _plot_hires80_scanline : _plot_hires40_scanline;
}

uint8_t *display_renderStagingFramebuffer(void) {

    const uint32_t mainswitches = run_args.softswitches;

    // render main portion of screen ...

    drawpage_mode_t mainDrawPageMode = video_currentMainMode(mainswitches);
    int page = video_currentPage(mainswitches);

    static uint8_t scanline[CYCLES_VIS<<1] = { 0 }; // 80 columns of data ...

    if (mainDrawPageMode == DRAWPAGE_TEXT) {
        uint16_t base = page ? 0x0800 : 0x0400;
        for (unsigned int row=0; row < TEXT_ROWS-4; row++) {
            for (unsigned int col=0; col < TEXT_COLS; col++) {
                uint16_t off = video__line_offset[row] + col;
                uint16_t ea = base+off;
                scanline[(col<<1)+0] = apple_ii_64k[1][ea]; // AUX
                scanline[(col<<1)+1] = apple_ii_64k[0][ea]; // MBD
            }
            for (unsigned int i = 0; i < 8; i++) {
                display_flushScanline(&((scan_data_t){
                    .scanline = &scanline[0],
                    .softswitches = mainswitches,
                    .scanrow = (row<<3) + i,
                    .scancol = 0,
                    .scanend = CYCLES_VIS,
                }));
            }
        }
    } else {
        assert(!(mainswitches & SS_TEXT) && "TEXT should not be set");
        assert((mainswitches & SS_HIRES) && "HIRES should be set");
        uint16_t base = page ? 0x4000 : 0x2000;
        for (unsigned int row=0; row < TEXT_ROWS-4; row++) {
            for (unsigned int col=0; col < TEXT_COLS; col++) {
                for (unsigned int i = 0; i < 8; i++) {
                    uint16_t off = video__line_offset[row] + (0x400*i) + col;
                    uint16_t ea = base+off;
                    scanline[(col<<1)+0] = apple_ii_64k[1][ea]; // AUX
                    scanline[(col<<1)+1] = apple_ii_64k[0][ea]; // MBD
                    display_flushScanline(&((scan_data_t){
                        .scanline = &scanline[0],
                        .softswitches = mainswitches,
                        .scanrow = (row<<3) + i,
                        .scancol = 0,
                        .scanend = CYCLES_VIS,
                    }));
                }
            }
        }
    }
    (void)mainswitches;

    // resample current switches ... and render mixed portion of screen
    const uint32_t mixedswitches = run_args.softswitches;

    drawpage_mode_t mixedDrawPageMode = video_currentMixedMode(mixedswitches);
    page = video_currentPage(mixedswitches);

    if (mixedDrawPageMode == DRAWPAGE_TEXT) {
        uint16_t base = page ? 0x0800 : 0x0400;
        for (unsigned int row=TEXT_ROWS-4; row < TEXT_ROWS; row++) {
            for (unsigned int col=0; col < TEXT_COLS; col++) {
                uint16_t off = video__line_offset[row] + col;
                uint16_t ea = base+off;
                scanline[(col<<1)+0] = apple_ii_64k[1][ea]; // AUX
                scanline[(col<<1)+1] = apple_ii_64k[0][ea]; // MBD
            }
            for (unsigned int i = 0; i < 8; i++) {
                display_flushScanline(&((scan_data_t){
                    .scanline = &scanline[0],
                    .softswitches = mixedswitches,
                    .scanrow = (row<<3) + i,
                    .scancol = 0,
                    .scanend = CYCLES_VIS,
                }));
            }
        }
    } else {
        //assert(!(mixedswitches & SS_TEXT) && "TEXT should not be set"); // TEXT may have been reset from last sample?
        assert(!(mixedswitches & SS_MIXED) && "MIXED should not be set");
        assert((mixedswitches & SS_HIRES) && "HIRES should be set");
        uint16_t base = page ? 0x4000 : 0x2000;
        for (unsigned int row=TEXT_ROWS-4; row < TEXT_ROWS; row++) {
            for (unsigned int col=0; col < TEXT_COLS; col++) {
                for (unsigned int i = 0; i < 8; i++) {
                    uint16_t off = video__line_offset[row] + (0x400*i) + col;
                    uint16_t ea = base+off;
                    scanline[(col<<1)+0] = apple_ii_64k[1][ea]; // AUX
                    scanline[(col<<1)+1] = apple_ii_64k[0][ea]; // MBD
                    display_flushScanline(&((scan_data_t){
                        .scanline = &scanline[0],
                        .softswitches = mixedswitches,
                        .scanrow = (row<<3) + i,
                        .scancol = 0,
                        .scanend = CYCLES_VIS,
                    }));
                }
            }
        }
    }

    video_setDirty(FB_DIRTY_FLAG);
    return display_getCurrentFramebuffer();
}

void display_flashText(void) {
    static bool normal = false;
    normal = !normal;

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

uint8_t *display_getCurrentFramebuffer(void) {
#if INTERFACE_CLASSIC
    if (interface_isShowing()) {
        return fbInterface;
    }
#endif
    return fbStaging;
}

#if TESTING
// HACK FIXME TODO ... should consolidate this into debugger ...
extern pthread_mutex_t interface_mutex;
extern pthread_cond_t cpu_thread_cond;
extern pthread_cond_t dbg_thread_cond;
uint8_t *display_waitForNextCompleteFramebuffer(void) {
    int err = 0;
    if ((err = pthread_cond_signal(&cpu_thread_cond))) {
        LOG("pthread_cond_signal : %d", err);
    }
    if ((err = pthread_cond_wait(&dbg_thread_cond, &interface_mutex))) {
        LOG("pthread_cond_wait : %d", err);
    }
    return display_getCurrentFramebuffer ();
}
#endif

void display_flushScanline(scan_data_t *scandata) {
#if TESTING
    // FIXME TODO ... remove this bracing when video refactoring is done
#else
    ASSERT_ON_CPU_THREAD();
#endif

    unsigned int scanrow = scandata->scanrow;
    unsigned int scancol = scandata->scancol;
    unsigned int scanend = scandata->scanend;

    assert((scanend > 0) && (scanend <= CYCLES_VIS));
    assert(scancol < CYCLES_VIS);
    assert(scanrow < SCANLINES_VBL_BEGIN);

    const uint32_t currswitches = scandata->softswitches;
    const drawpage_mode_t mode = (scanrow < SCANLINES_MIX) ? video_currentMainMode(currswitches) : video_currentMixedMode(currswitches);

    void (*plotFn)(scan_data_t *) = NULL;

    if (mode == DRAWPAGE_TEXT) {
        const uint32_t textmask = (scanrow < SCANLINES_MIX) ? SS_TEXT : (SS_TEXT|SS_MIXED);
        plotFn = _textpage_plotter(currswitches, textmask);
    } else {
        assert(!(currswitches & SS_TEXT) && "TEXT should not be set");
        assert((currswitches & SS_HIRES) && "HIRES should be set");
        plotFn = _hirespage_plotter(currswitches);
    }

    plotFn(scandata);
}

void display_frameComplete(void) {
    ASSERT_ON_CPU_THREAD();

    video_setDirty(FB_DIRTY_FLAG);

#if TESTING
    // HACK FIXME TODO ... should consolidate this into debugger ...
    int err = 0;
    if ((err = pthread_cond_signal(&dbg_thread_cond))) {
        LOG("pthread_cond_signal : %d", err);
    }
    if ((err = pthread_cond_wait(&cpu_thread_cond, &interface_mutex))) {
        LOG("pthread_cond_wait : %d", err);
    }
#endif
}

static void _init_interface(void) {
    LOG("Initializing display subsystem");
    _initialize_interface_fonts();
    _initialize_hires_values();
    _initialize_row_col_tables();
    _initialize_dhires_values();
    _initialize_color();

    prefs_registerListener(PREF_DOMAIN_VIDEO, &display_prefsChanged);
}

static __attribute__((constructor)) void __init_interface(void) {
    emulator_registerStartupCallback(CTOR_PRIORITY_LATE, &_init_interface);
}

