/*
 * Apple // emulator for Linux: Video support
 *
 * Copyright 1994 Alexander Jean-Claude Bottema
 * Copyright 1995 Stephen Lee
 * Copyright 1997, 1998 Aaron Culliney
 * Copyright 1998, 1999, 2000 Michael Deutschmann
 *
 * This software package is subject to the GNU General Public License
 * version 2 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * THERE ARE NO WARRANTIES WHATSOEVER.
 *
 */

#include "common.h"

#define DYNAMIC_SZ 11 // 7 pixels (as bytes) + 2pre + 2post

#define TEXT_ROWS 24
#define BEGIN_MIX 20
#define TEXT_COLS 40

// framebuffers
static uint8_t vga_mem_page_0[SCANWIDTH*SCANHEIGHT];
static uint8_t vga_mem_page_1[SCANWIDTH*SCANHEIGHT];

uint8_t video__wider_font[0x8000];
uint8_t video__font[0x4000];

// Precalculated framebuffer offsets given VM addr
unsigned int video__screen_addresses[8192];
uint8_t video__columns[8192];

uint8_t *video__fb1 = NULL;
uint8_t *video__fb2 = NULL;

uint8_t video__hires_even[0x800];
uint8_t video__hires_odd[0x800];

uint8_t video__dhires1[256];
uint8_t video__dhires2[256];

// Interface font
static uint8_t video__int_font[3][0x4000];

int video__current_page; // current visual page
int video__strictcolors = 1;// refactor : should be static

void video_loadfont(int first, int quantity, const uint8_t *data, int mode) {
    uint8_t fg = 0;
    uint8_t bg = 0;
    switch (mode) {
        case 2:
            fg = COLOR_BLACK;
            bg = COLOR_LIGHT_WHITE;
            break;
        case 3:
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

uint8_t video__odd_colors[2] = { COLOR_LIGHT_PURPLE, COLOR_LIGHT_BLUE };
uint8_t video__even_colors[2] = { COLOR_LIGHT_GREEN, COLOR_LIGHT_RED };

// 40col/80col/lores/hires/dhires line offsets
unsigned short video__line_offset[TEXT_ROWS] = {
  0x000, 0x080, 0x100, 0x180, 0x200, 0x280, 0x300, 0x380,
  0x028, 0x0A8, 0x128, 0x1A8, 0x228, 0x2A8, 0x328, 0x3A8,
  0x050, 0x0D0, 0x150, 0x1D0, 0x250, 0x2D0, 0x350, 0x3D0
};

uint8_t video__dhires1[256] = {
    0x0,0x1,0x2,0x3,0x4,0x5,0x6,0x7,0x8,0x9,0xa,0xb,0xc,0xd,0xe,0xf,
    0x0,0x1,0x3,0x3,0x5,0x5,0x7,0x7,0x9,0x9,0xb,0xb,0xd,0xd,0xf,0xf,
    0x0,0x1,0x2,0x3,0x6,0x5,0x6,0x7,0xa,0x9,0xa,0xb,0xe,0xd,0xe,0xf,
    0x0,0x1,0x3,0x3,0x7,0x5,0x7,0x7,0xb,0x9,0xb,0xb,0xf,0xd,0xf,0xf,
    0x0,0x1,0x2,0x3,0x4,0x4,0x6,0x7,0x8,0x9,0xa,0xb,0xc,0xd,0xe,0xf,
    0x0,0x1,0x3,0x3,0x5,0x5,0x7,0x7,0xd,0x9,0xb,0xb,0xd,0xd,0xf,0xf,
    0x0,0x1,0x2,0x3,0x4,0x5,0x6,0x7,0xe,0x9,0xa,0xb,0xe,0xd,0xe,0xf,
    0x0,0x1,0x7,0x3,0x7,0x5,0x7,0x7,0xf,0x9,0xb,0xb,0xf,0xd,0xf,0xf,
};

uint8_t video__dhires2[256] = {
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x8,0x8,0x0,0xb,0x8,0xd,0x0,0x0,
    0x1,0x1,0x1,0x1,0x0,0x5,0x1,0x1,0x0,0x9,0xb,0xb,0x0,0xd,0xf,0xf,
    0x0,0x1,0x2,0x2,0x2,0x5,0x2,0x2,0x0,0xa,0xa,0xa,0xe,0xd,0x2,0x2,
    0x3,0x3,0x3,0x3,0x7,0x5,0x7,0x7,0x0,0xb,0xb,0xb,0xf,0xd,0xf,0xf,
    0x0,0x0,0x4,0x0,0x4,0x4,0x4,0x4,0xc,0x8,0x4,0x8,0xc,0xd,0x4,0x4,
    0x5,0x5,0x5,0x5,0x5,0x5,0x5,0x5,0xd,0x4,0x4,0x4,0xd,0xd,0x4,0x4,
    0x6,0x6,0x6,0x2,0xe,0x6,0x6,0x6,0xe,0xe,0xa,0xa,0xe,0x6,0xe,0x6,
    0x7,0x7,0x7,0x7,0x7,0x7,0x7,0x7,0xf,0xf,0xb,0xb,0xf,0xf,0xf,0xf,
};


static void c_initialize_dhires_values(void) {
    for (unsigned int i = 0; i < 0x80; i++) {
        video__dhires1[i+0x80] = video__dhires1[i];
        video__dhires2[i+0x80] = video__dhires2[i];
    }
}

static void c_initialize_hires_values(void) {
    // precalculate colors for all the 256*8 bit combinations. */
    for (unsigned int value = 0x00; value <= 0xFF; value++) {
        for (unsigned int e = value*8, last_not_black=0, v=value, b=0; b < 7; b++, v >>= 1, e++) {
            if (v & 1) {
                if (last_not_black) {
                    video__hires_even[e] = COLOR_LIGHT_WHITE;
                    video__hires_odd[e] = COLOR_LIGHT_WHITE;
                    if (b > 0)
                    {
                        video__hires_even[e-1] = COLOR_LIGHT_WHITE,
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
                video__hires_even[e] = COLOR_BLACK,
                video__hires_odd [e] = COLOR_BLACK,
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

static void c_initialize_row_col_tables(void) {
    for (unsigned int i = 0; i < 8192; i++) {
        video__screen_addresses[i] = -1;
    }

    for (unsigned int y = 0; y < TEXT_ROWS; y++) {
        for (unsigned int off = 0; off < 8; off++) {
            for (unsigned int x = 0; x < 40; x++) {
                video__screen_addresses[video__line_offset[y] + (0x400*off) + x] = (y*16 + 2*off /* + 8*/) * SCANWIDTH + x*14 + 4;
                video__columns         [video__line_offset[y] + (0x400*off) + x] = (uint8_t)x;
            }
        }
    }
}

static void c_initialize_tables_video(void) {
    // initialize text/lores & hires graphics routines
    for (unsigned int y = 0; y < TEXT_ROWS; y++) {
        for (unsigned int x = 0; x < TEXT_COLS; x++) {
            unsigned int idx = video__line_offset[y] + x + 0x400;
            // text/lores pages
            if (y < 20) {
                cpu65_vmem[idx      ].w = video__write_2e_text0;
                cpu65_vmem[idx+0x400].w = video__write_2e_text1;
            } else {
                cpu65_vmem[idx      ].w = video__write_2e_text0_mixed;
                cpu65_vmem[idx+0x400].w = video__write_2e_text1_mixed;
            }

            // hires/dhires pages
            for (unsigned int i = 0; i < 8; i++) {
                idx = 0x2000 + video__line_offset[ y ] + (0x400*i) + x;
                if (y < 20) {
                    if (x & 1) {
                        cpu65_vmem[idx       ].w = video__write_2e_odd0;
                        cpu65_vmem[idx+0x2000].w = video__write_2e_odd1;
                    } else {
                        cpu65_vmem[idx       ].w = video__write_2e_even0;
                        cpu65_vmem[idx+0x2000].w = video__write_2e_even1;
                    }
                } else {
                    if (x & 1) {
                        cpu65_vmem[idx       ].w = video__write_2e_odd0_mixed;
                        cpu65_vmem[idx+0x2000].w = video__write_2e_odd1_mixed;
                    } else {
                        cpu65_vmem[idx       ].w = video__write_2e_even0_mixed;
                        cpu65_vmem[idx+0x2000].w = video__write_2e_even1_mixed;
                    }
                }
            }
        }
    }
}

void video_set(int flags) {
    video__strictcolors = (color_mode == COLOR_INTERP) ? 2 : 1;
    c_initialize_hires_values();
    c_initialize_row_col_tables();
    c_initialize_tables_video();
    c_initialize_dhires_values();
}

void video_loadfont_int(int first, int quantity, const uint8_t *data) {
    unsigned int i = quantity * 8;
    while (i--) {
        unsigned int j = 8;
        uint8_t x = data[i];
        while (j--) {
            unsigned int y = (first << 6) + (i << 3) + j;
            if (x & 128) {
                video__int_font[0][y] = video__int_font[1][y] = COLOR_LIGHT_GREEN;
                video__int_font[2][y] = COLOR_LIGHT_RED;
            } else {
                video__int_font[0][y] = video__int_font[2][y] = COLOR_BLACK;
                video__int_font[1][y] = COLOR_MEDIUM_BLUE;
            }
            x <<= 1;
        }
    }
}

// ----------------------------------------------------------------------------
// Plotting routines

static inline void _plot_char40(uint8_t **d, uint8_t **s) {
    *((uint32_t *)(*d)) = *((uint32_t *)(*s));
    *d += 4, *s += 4;
    *((uint32_t *)(*d)) = *((uint32_t *)(*s));
    *d += 4, *s += 4;
    *((uint32_t *)(*d)) = *((uint32_t *)(*s));
    *d += 4, *s += 4;
    *((uint16_t *)(*d)) = *((uint16_t *)(*s));
    *d += SCANSTEP, *s -= 12;
    *((uint32_t *)(*d)) = *((uint32_t *)(*s));
    *d += 4, *s += 4;
    *((uint32_t *)(*d)) = *((uint32_t *)(*s));
    *d += 4, *s += 4;
    *((uint32_t *)(*d)) = *((uint32_t *)(*s));
    *d += 4, *s += 4;
    *((uint16_t *)(*d)) = *((uint16_t *)(*s));
    *d += SCANSTEP, *s += 4;
}

static inline void _plot_char80(uint8_t **d, uint8_t **s) {
    *((uint32_t *)(*d)) = *((uint32_t *)(*s));
    *d += 4, *s += 4;
    *((uint16_t *)(*d)) = *((uint16_t *)(*s));
    *d += 2, *s += 2;
    *((uint8_t *)(*d)) = *((uint8_t *)(*s));
    *d += SCANWIDTH-6, *s -= 6;
    *((uint32_t *)(*d)) = *((uint32_t *)(*s));
    *d += 4, *s += 4;
    *((uint16_t *)(*d)) = *((uint16_t *)(*s));
    *d += 2, *s += 2;
    *((uint8_t *)(*d)) = *((uint8_t *)(*s));
    *d += SCANWIDTH-6, *s += 2;
}

static inline void _plot_lores(uint8_t **d, const uint32_t val) {
    *((uint32_t *)(*d)) = val;
    *d += 4;
    *((uint32_t *)(*d)) = val;
    *d += 4;
    *((uint32_t *)(*d)) = val;
    *d += 4;
    *((uint16_t *)(*d)) = (uint16_t)(val & 0xffff);
    *d += (SCANWIDTH-12);
    *((uint32_t *)(*d)) = val;
    *d += 4;
    *((uint32_t *)(*d)) = val;
    *d += 4;
    *((uint32_t *)(*d)) = val;
    *d += 4;
    *((uint16_t *)(*d)) = (uint16_t)(val & 0xffff);
}

void video_plotchar( int x, int y, int scheme, uint8_t c ) {
    uint8_t *d;
    uint8_t *s;

    unsigned int off = y * SCANWIDTH * 16 + x * 7 + 4;
    s = video__int_font[scheme] + c * 64;
    d = video__fb1 + off;

    _plot_char80(&d,&s);
    _plot_char80(&d,&s);
    _plot_char80(&d,&s);
    _plot_char80(&d,&s);
    _plot_char80(&d,&s);
    _plot_char80(&d,&s);
    _plot_char80(&d,&s);
    _plot_char80(&d,&s);
}

#if !HEADLESS
extern void video_driver_init();
extern void video_driver_shutdown();
#endif
void video_init() {

    video__fb1 = vga_mem_page_0;
    video__fb2 = vga_mem_page_1;

    // reset Apple2 softframebuffers
    memset(video__fb1,0,SCANWIDTH*SCANHEIGHT);
    memset(video__fb2,0,SCANWIDTH*SCANHEIGHT);

#if !HEADLESS
    if (!is_headless) {
        video_driver_init();
    }
#endif
}

void video_shutdown(void)
{
#if !HEADLESS
    if (!is_headless) {
        video_driver_shutdown();
    }
#endif
}

/* -------------------------------------------------------------------------
    video_setpage(p):    Switch to screen page p
   ------------------------------------------------------------------------- */
void video_setpage(int p)
{
    video__current_page = p;
}

const uint8_t * const video_current_framebuffer() {
    return !video__current_page ? video__fb1 : video__fb2;
}


// ----------------------------------------------------------------------------

static inline void _plot_character(const unsigned int font_off, uint8_t *fb_ptr) {
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

static inline void _plot_character0(uint16_t ea, uint8_t b)
{
    _plot_character(b<<7/* *128 */, video__fb1+video__screen_addresses[ea-0x0400]);
}

static inline void _plot_character1(uint16_t ea, uint8_t b)
{
    _plot_character(b<<7/* *128 */, video__fb2+video__screen_addresses[ea-0x0800]);
}

static inline void _plot_80character(const unsigned int font_off, uint8_t *fb_ptr) {
    uint8_t *font_ptr = video__font+font_off;
    _plot_char80(/*dst*/&fb_ptr, /*src*/&font_ptr);
    _plot_char80(/*dst*/&fb_ptr, /*src*/&font_ptr);
    _plot_char80(/*dst*/&fb_ptr, /*src*/&font_ptr);
    _plot_char80(/*dst*/&fb_ptr, /*src*/&font_ptr);
    _plot_char80(/*dst*/&fb_ptr, /*src*/&font_ptr);
    _plot_char80(/*dst*/&fb_ptr, /*src*/&font_ptr);
    _plot_char80(/*dst*/&fb_ptr, /*src*/&font_ptr);
    _plot_char80(/*dst*/&fb_ptr, /*src*/&font_ptr);
}

// FIXME TODO NOTE : dup'ing work here?
static inline void _plot_80character0(uint16_t ea, uint8_t b)
{
    b = apple_ii_64k[1][ea];
    _plot_80character(b<<6/* *64 */, video__fb1+video__screen_addresses[ea-0x0400]);
    b = apple_ii_64k[0][ea];
    _plot_80character(b<<6/* *64 */, video__fb1+video__screen_addresses[ea-0x0400]+7);
}

static inline void _plot_80character1(uint16_t ea, uint8_t b)
{
    b = apple_ii_64k[1][ea];
    _plot_80character(b<<6/* *64 */, video__fb2+video__screen_addresses[ea-0x0800]);
    b = apple_ii_64k[0][ea];
    _plot_80character(b<<6/* *64 */, video__fb2+video__screen_addresses[ea-0x0800]+7);
}

static inline void _plot_block(const uint32_t val, uint8_t *fb_ptr) {
    uint8_t color = (val & 0x0F) << 4;
    uint32_t val32 = (color << 24) | (color << 16) | (color << 8) | color;

    _plot_lores(/*dst*/&fb_ptr, val32);
    fb_ptr += SCANSTEP;
    _plot_lores(/*dst*/&fb_ptr, val32);
    fb_ptr += SCANSTEP;
    _plot_lores(/*dst*/&fb_ptr, val32);
    fb_ptr += SCANSTEP;
    _plot_lores(/*dst*/&fb_ptr, val32);

    fb_ptr += SCANSTEP;
    color = val & 0xF0;
    val32 = (color << 24) | (color << 16) | (color << 8) | color;

    _plot_lores(/*dst*/&fb_ptr, val32);
    fb_ptr += SCANSTEP;
    _plot_lores(/*dst*/&fb_ptr, val32);
    fb_ptr += SCANSTEP;
    _plot_lores(/*dst*/&fb_ptr, val32);
    fb_ptr += SCANSTEP;
    _plot_lores(/*dst*/&fb_ptr, val32);
}

/* plot lores block first page */
static inline void _plot_block0(uint16_t ea, uint8_t b)
{
    _plot_block(b, video__fb1+video__screen_addresses[ea-0x0400]);
}

static inline void _plot_block1(uint16_t ea, uint8_t b)
{
    _plot_block(b, video__fb2+video__screen_addresses[ea-0x0800]);
}

#define DRAW_TEXT(PAGE, SW) \
    do { \
        if (softswitches & SS_TEXT) { \
            if (softswitches & SS_80COL) { \
                _plot_80character##PAGE(ea, b); \
            } else if (softswitches & SW) { \
                /* ??? */ \
            } else { \
                _plot_character##PAGE(ea, b); \
            } \
        } else { \
            if (softswitches & (SS_HIRES|SW)) { \
                /* ??? */ \
            } else { \
                _plot_block##PAGE(ea, b); \
            } \
        } \
    } while(0)


#define DRAW_MIXED(PAGE, SW) \
    do { \
        if (softswitches & (SS_TEXT|SS_MIXED)) { \
            if (softswitches & SS_80COL) { \
                _plot_80character##PAGE(ea, b); \
            } else if (softswitches & SW) { \
                /* ??? */ \
            } else { \
                _plot_character##PAGE(ea, b); \
            } \
        } else { \
            if (softswitches & (SS_HIRES|SW)) { \
                /* ??? */ \
            } else { \
                _plot_block##PAGE(ea, b); \
            } \
        } \
    } while(0)

GLUE_C_WRITE(iie_soft_write_text0)
{
    DRAW_TEXT(0, SS_TEXTWRT);
}

GLUE_C_WRITE(video__write_2e_text0)
{
    base_textwrt[ea] = b;
    c_iie_soft_write_text0(ea, b);
}

GLUE_C_WRITE(iie_soft_write_text0_mixed)
{
    DRAW_MIXED(0, SS_TEXTWRT);
}

GLUE_C_WRITE(video__write_2e_text0_mixed)
{
    base_textwrt[ea] = b;
    c_iie_soft_write_text0_mixed(ea, b);
}

GLUE_C_WRITE(iie_soft_write_text1)
{
    DRAW_TEXT(1, SS_RAMWRT);
}

GLUE_C_WRITE(video__write_2e_text1)
{
    base_ramwrt[ea] = b;
    c_iie_soft_write_text1(ea, b);
}

GLUE_C_WRITE(iie_soft_write_text1_mixed)
{
    DRAW_MIXED(1, SS_RAMWRT);
}

GLUE_C_WRITE(video__write_2e_text1_mixed)
{
    base_ramwrt[ea] = b;
    c_iie_soft_write_text1_mixed(ea, b);
}

// ----------------------------------------------------------------------------
// Hires (and Double-Hires) GRaphics

// CalculateInterpColor
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

// PlotPixelsExtra
static inline void _plot_pixels(uint8_t *dst, const uint8_t *src) {
    uint8_t pix;

    for (unsigned int i=2; i; i--) {
        for (unsigned int j=DYNAMIC_SZ-1; j; j--) {
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

// PlotByte
static inline void _plot_hires(uint16_t ea, uint8_t b, bool is_even, uint8_t *fb_ptr) {
    uint8_t *hires_altbase = NULL;
    uint8_t *interp_base = NULL;
    uint8_t color_buf[DYNAMIC_SZ];

    uint8_t *hires_ptr = NULL;
    if (is_even) {
        hires_ptr = (uint8_t *)&video__hires_even[b<<3];
        hires_altbase = (uint8_t *)&video__hires_odd[0];
        interp_base = (uint8_t *)&video__even_colors[0];
    } else {
        hires_ptr = (uint8_t *)&video__hires_odd[b<<3];
        hires_altbase = (uint8_t *)&video__hires_even[0];
        interp_base = (uint8_t *)&video__odd_colors[0];
    }
    *((uint32_t *)&color_buf[2]) = *((uint32_t *)(hires_ptr+0));
    *((uint16_t *)&color_buf[6]) = *((uint16_t *)(hires_ptr+4));
    *((uint8_t  *)&color_buf[8]) = *((uint8_t  *)(hires_ptr+6));
    hires_ptr = NULL;

    // copy adjacent pixel bytes
    *((uint16_t *)&color_buf[0]) = *((uint16_t *)(fb_ptr-3));
    *((uint16_t *)&color_buf[DYNAMIC_SZ-2]) = *((uint16_t *)(fb_ptr+15));

    // %eax = +8
    if (color_mode != COLOR_NONE) {
        // if right-side color is not black, re-calculate edge values
        if (color_buf[DYNAMIC_SZ-2] & 0xff) {
            uint16_t pix16 = *((uint16_t *)&apple_ii_64k[0][ea]);
            if ((pix16 & 0x100) && (pix16 & 0x40)) {
                *((uint16_t *)&color_buf[DYNAMIC_SZ-3]) = (uint16_t)0x3737;// COLOR_LIGHT_WHITE
            } else {
                // PB_black0:
                pix16 >>= 8;
                color_buf[DYNAMIC_SZ-2] = hires_altbase[pix16<<3];
            }
        }

        // if left-side color is not black, re-calculate edge values
        if (color_buf[1] & 0xff) {
            uint16_t pix16 = *((uint16_t *)&apple_ii_64k[0][ea-1]);
            if ((pix16 & 0x100) && (pix16 & 0x40)) {
                *((uint16_t *)&color_buf[1]) = (uint16_t)0x3737;// COLOR_LIGHT_WHITE
            } else {
                // PB_black1:
                pix16 &= 0xFF;
                color_buf[1] = hires_altbase[(pix16<<3)+6];
            }
        }

        if (color_mode == COLOR_INTERP) {
            // calculate interpolated/bleed colors
            _calculate_interp_color(color_buf, 1, interp_base, ea-1);
            _calculate_interp_color(color_buf, 2, interp_base, ea);
            _calculate_interp_color(color_buf, 8, interp_base, ea);
            _calculate_interp_color(color_buf, 9, interp_base, ea+1);
        }
    }

    _plot_pixels(fb_ptr-4, color_buf);
}

// DRAW_GRAPHICS
static inline void _draw_hires_graphics(uint16_t ea, uint8_t b, bool is_even, uint8_t page, unsigned int ss_textflags) {
    if (softswitches & ss_textflags) {
        return;
    }
    if (!(softswitches & SS_HIRES)) {
        return;
    }
    if ((softswitches & SS_80COL) && (softswitches & SS_DHIRES)) {
        if (page) {
            extern void iie_plot_dhires1();
            iie_plot_dhires1();
        } else {
            extern void iie_plot_dhires0();
            iie_plot_dhires0();
        }
        return;
    }
    if (softswitches & SS_HGRWRT) {
        return;
    }

    uint16_t off = ea - 0x2000;
    uint8_t *fb_base = NULL;
    if (page) {
        off -= 0x2000;
        fb_base = video__fb2;
    } else {
        fb_base = video__fb1;
    }
    _plot_hires(ea, b, is_even, fb_base+video__screen_addresses[off]);
}

GLUE_C_WRITE(iie_soft_write_even0) // refactor : remove
{
    _draw_hires_graphics(ea, b, /*even*/true, 0, SS_TEXT);
}

GLUE_C_WRITE(video__write_2e_even0)
{
    base_hgrwrt[ea] = b;
    _draw_hires_graphics(ea, b, /*even*/true, 0, SS_TEXT);
}

GLUE_C_WRITE(iie_soft_write_even0_mixed) // refactor : remove
{
    _draw_hires_graphics(ea, b, /*even*/true, 0, (SS_TEXT|SS_MIXED));
}

GLUE_C_WRITE(video__write_2e_even0_mixed)
{
    base_hgrwrt[ea] = b;
    _draw_hires_graphics(ea, b, /*even*/true, 0, (SS_TEXT|SS_MIXED));
}

GLUE_C_WRITE(iie_soft_write_odd0) // refactor : remove
{
    _draw_hires_graphics(ea, b, /*even*/false, 0, SS_TEXT);
}

GLUE_C_WRITE(video__write_2e_odd0)
{
    base_hgrwrt[ea] = b;
    _draw_hires_graphics(ea, b, /*even*/false, 0, SS_TEXT);
}

GLUE_C_WRITE(iie_soft_write_odd0_mixed) // refactor : remove
{
    _draw_hires_graphics(ea, b, /*even*/false, 0, (SS_TEXT|SS_MIXED));
}

GLUE_C_WRITE(video__write_2e_odd0_mixed)
{
    base_hgrwrt[ea] = b;
    _draw_hires_graphics(ea, b, /*even*/false, 0, (SS_TEXT|SS_MIXED));
}

GLUE_C_WRITE(iie_soft_write_even1) // refactor : remove
{
    _draw_hires_graphics(ea, b, /*even*/true, 1, SS_TEXT);
}

GLUE_C_WRITE(video__write_2e_even1)
{
    base_ramwrt[ea] = b;
    _draw_hires_graphics(ea, b, /*even*/true, 1, SS_TEXT);
}

GLUE_C_WRITE(iie_soft_write_even1_mixed) // refactor : remove
{
    _draw_hires_graphics(ea, b, /*even*/true, 1, (SS_TEXT|SS_MIXED));
}

GLUE_C_WRITE(video__write_2e_even1_mixed)
{
    base_ramwrt[ea] = b;
    _draw_hires_graphics(ea, b, /*even*/true, 1, (SS_TEXT|SS_MIXED));
}

GLUE_C_WRITE(iie_soft_write_odd1) // refactor : remove
{
    _draw_hires_graphics(ea, b, /*even*/false, 1, SS_TEXT);
}

GLUE_C_WRITE(video__write_2e_odd1)
{
    base_ramwrt[ea] = b;
    _draw_hires_graphics(ea, b, /*even*/false, 1, SS_TEXT);
}

GLUE_C_WRITE(iie_soft_write_odd1_mixed) // refactor : remove
{
    _draw_hires_graphics(ea, b, /*even*/false, 1, (SS_TEXT|SS_MIXED));
}

GLUE_C_WRITE(video__write_2e_odd1_mixed)
{
    base_ramwrt[ea] = b;
    _draw_hires_graphics(ea, b, /*even*/false, 1, (SS_TEXT|SS_MIXED));
}

