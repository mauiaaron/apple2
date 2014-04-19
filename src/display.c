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

#define TEXT_ROWS 24
#define BEGIN_MIX 20
#define TEXT_COLS 40

static uint8_t vga_mem_page_0[SCANWIDTH*SCANHEIGHT];              /* page0 framebuffer */
static uint8_t vga_mem_page_1[SCANWIDTH*SCANHEIGHT];              /* page1 framebuffer */

uint8_t video__wider_font[0x8000];
uint8_t video__font[0x4000];

/* --- Precalculated hi-res page offsets given addr --- */
unsigned int video__screen_addresses[8192];
uint8_t video__columns[8192];

uint8_t *video__fb1,*video__fb2;

uint8_t video__wider_hires_even[0x1000];
uint8_t video__wider_hires_odd[0x1000];
uint8_t video__hires_even[0x800];
uint8_t video__hires_odd[0x800];

uint8_t video__dhires1[256];
uint8_t video__dhires2[256];

// Interface font
static uint8_t video__int_font[3][0x4000];

int video__current_page;        /* Current visual page */

int video__strictcolors = 1;// 0 is deprecated

void video_loadfont(int first,
                    int quantity,
                    const unsigned char *data,
                    int mode)
{
    int i,j;
    unsigned char x,y,fg,bg;

    switch (mode)
    {
    case 2:
        fg = COLOR_BLACK; bg = COLOR_LIGHT_WHITE; break;
    case 3:
        fg = COLOR_FLASHING_WHITE; bg = COLOR_FLASHING_BLACK; break;
    default:
        fg = COLOR_LIGHT_WHITE; bg = COLOR_BLACK; break;
    }

    i = quantity * 8;

    while (i--)
    {
        j = 8;
        x = data[i];
        while (j--)
        {
            y = (x & 128) ? fg : bg;

            video__wider_font[(first << 7) + (i << 4) + (j << 1)] =
                video__wider_font[(first << 7) + (i << 4) + (j << 1) + 1] =
            video__font[(first << 6) + (i << 3) + j] = y;
            x <<= 1;
        }
    }
}

uint8_t video__odd_colors[2] = { COLOR_LIGHT_PURPLE, COLOR_LIGHT_BLUE };
uint8_t video__even_colors[2] = { COLOR_LIGHT_GREEN, COLOR_LIGHT_RED };

/* 40col/80col/lores/hires/dhires line offsets */
unsigned short video__line_offset[TEXT_ROWS] =
{ 0x000, 0x080, 0x100, 0x180, 0x200, 0x280, 0x300, 0x380,
  0x028, 0x0A8, 0x128, 0x1A8, 0x228, 0x2A8, 0x328, 0x3A8,
  0x050, 0x0D0, 0x150, 0x1D0, 0x250, 0x2D0, 0x350, 0x3D0 };

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


/* -------------------------------------------------------------------------
    c_initialize_dhires_values()
   ------------------------------------------------------------------------- */
static void c_initialize_dhires_values(void) {
    int i;
/*     int value, v; */
/*     unsigned char locolor, hicolor; */

    /* precalculate the colors for all the 256*8 bit combinations. */
/*     for (value = 0x00, v = 0; value <= 0xFF; value++) { */
/*      locolor = (value & 0x0F) | 0x10; */
/*      hicolor = (value << 4) | 0x10; */

/*      dhires_colors[v++] = locolor; */
/*      dhires_colors[v++] = locolor; */
/*      dhires_colors[v++] = locolor; */
/*      dhires_colors[v++] = locolor; */

/*      dhires_colors[v++] = hicolor; */
/*      dhires_colors[v++] = hicolor; */
/*      dhires_colors[v++] = hicolor; */
/*      dhires_colors[v++] = hicolor; */
/*     } */

    for (i = 0; i < 0x80; i++)
    {
        video__dhires1[i+0x80] = video__dhires1[i];
        video__dhires2[i+0x80] = video__dhires2[i];
    }
}

/* -------------------------------------------------------------------------
    c_initialize_hires_values()
   ------------------------------------------------------------------------- */

static void c_initialize_hires_values(void)
{
    int value, b, v, e, /*color_toggle,*/ last_not_black;

    /* precalculate the colors for all the 256*8 bit combinations. */
    for (value = 0x00; value <= 0xFF; value++)
    {
        for (e = value * 8, last_not_black = 0, v = value, b = 0;
             b < 7; b++, v >>= 1, e++)
        {
            if (v & 1)
            {
                video__hires_even[ e ] = last_not_black ?
                                         COLOR_LIGHT_WHITE :
                                         ((b & 1) ?
                                          ((value & 0x80) ?
                                           COLOR_LIGHT_RED :
                                           COLOR_LIGHT_GREEN) :
                                          ((value & 0x80) ?
                                           COLOR_LIGHT_BLUE :
                                           COLOR_LIGHT_PURPLE));

                video__hires_odd[ e ] = last_not_black ?
                                        COLOR_LIGHT_WHITE :
                                        ((b & 1) ?
                                         ((value & 0x80) ?
                                          COLOR_LIGHT_BLUE :
                                          COLOR_LIGHT_PURPLE) :
                                         ((value & 0x80) ?
                                          COLOR_LIGHT_RED :
                                          COLOR_LIGHT_GREEN));

                if (last_not_black && b > 0)
                {
                    video__hires_even[ e - 1 ] = COLOR_LIGHT_WHITE,
                    video__hires_odd[ e - 1 ] = COLOR_LIGHT_WHITE;
                }

                last_not_black = 1;
            }
            else
            {
                video__hires_even[ e ] = COLOR_BLACK,
                video__hires_odd[ e ] = COLOR_BLACK,
                last_not_black = 0;
            }
        }
    }

    if (color_mode == COLOR_NONE)        /* Black and White */
    {
        for (value = 0x00; value <= 0xFF; value++)
        {
            for (b = 0, e = value * 8; b < 7; b++, e++)
            {
                if (video__hires_even[ e ] != COLOR_BLACK)
                {
                    video__hires_even[ e ] = COLOR_LIGHT_WHITE;
                }

                if (video__hires_odd[ e ] != COLOR_BLACK)
                {
                    video__hires_odd[ e ] = COLOR_LIGHT_WHITE;
                }
            }
        }
    }
    else if (color_mode == COLOR_INTERP)             /* Color and strict interpolation */
    {
        for (value = 0x00; value <= 0xFF; value++)
        {
            for (b = 1, e = value * 8 + 1; b <= 5; b += 2, e += 2)
            {
                if (video__hires_even[e] == COLOR_BLACK)
                {
                    if (video__hires_even[e-1] != COLOR_BLACK &&
                        video__hires_even[e+1] != COLOR_BLACK &&
                        video__hires_even[e-1] != COLOR_LIGHT_WHITE &&
                        video__hires_even[e+1] != COLOR_LIGHT_WHITE)
                    {
                        video__hires_even[e] =
                            video__hires_even[e-1];
                    }
                    else if (
                        video__hires_even[e-1] != COLOR_BLACK &&
                        video__hires_even[e+1] != COLOR_BLACK &&
                        video__hires_even[e-1] != COLOR_LIGHT_WHITE &&
                        video__hires_even[e+1] == COLOR_LIGHT_WHITE)
                    {
                        video__hires_even[e] =
                            video__hires_even[e-1];
                    }
                    else if (
                        video__hires_even[e-1] != COLOR_BLACK &&
                        video__hires_even[e+1] != COLOR_BLACK &&
                        video__hires_even[e-1] == COLOR_LIGHT_WHITE &&
                        video__hires_even[e+1] != COLOR_LIGHT_WHITE)
                    {
                        video__hires_even[e] =
                            video__hires_even[e+1];
                    }
                    else if (
                        video__hires_even[e-1] == COLOR_LIGHT_WHITE &&
                        video__hires_even[e+1] == COLOR_LIGHT_WHITE)
                    {
                        video__hires_even[e] = (value & 0x80)
                                               ? COLOR_LIGHT_BLUE : COLOR_LIGHT_PURPLE;
                    }

                }

                if (video__hires_odd[e] == COLOR_BLACK)
                {
                    if (video__hires_odd[e-1] != COLOR_BLACK &&
                        video__hires_odd[e+1] != COLOR_BLACK &&
                        video__hires_odd[e-1] != COLOR_LIGHT_WHITE &&
                        video__hires_odd[e+1] != COLOR_LIGHT_WHITE)
                    {
                        video__hires_odd[e] =
                            video__hires_odd[e-1];
                    }
                    else if (
                        video__hires_odd[e-1] != COLOR_BLACK &&
                        video__hires_odd[e+1] != COLOR_BLACK &&
                        video__hires_odd[e-1] != COLOR_LIGHT_WHITE &&
                        video__hires_odd[e+1] == COLOR_LIGHT_WHITE)
                    {
                        video__hires_odd[e] =
                            video__hires_odd[e-1];
                    }
                    else if (
                        video__hires_odd[e-1] != COLOR_BLACK &&
                        video__hires_odd[e+1] != COLOR_BLACK &&
                        video__hires_odd[e-1] == COLOR_LIGHT_WHITE &&
                        video__hires_odd[e+1] != COLOR_LIGHT_WHITE)
                    {
                        video__hires_odd[e] =
                            video__hires_odd[e+1];
                    }
                    else if (
                        video__hires_odd[e-1] == COLOR_LIGHT_WHITE &&
                        video__hires_odd[e+1] == COLOR_LIGHT_WHITE)
                    {
                        video__hires_odd[e] = (value & 0x80)
                                              ? COLOR_LIGHT_RED : COLOR_LIGHT_GREEN;
                    }
                }
            }

            for (b = 0, e = value * 8; b <= 6; b += 2, e += 2)
            {
                if (video__hires_even[ e ] == COLOR_BLACK)
                {
                    if (b > 0 && b < 6)
                    {
                        if (video__hires_even[e-1] != COLOR_BLACK &&
                            video__hires_even[e+1] != COLOR_BLACK &&
                            video__hires_even[e-1] != COLOR_LIGHT_WHITE &&
                            video__hires_even[e+1] != COLOR_LIGHT_WHITE)
                        {
                            video__hires_even[e] =
                                video__hires_even[e-1];
                        }
                        else if (
                            video__hires_even[e-1] != COLOR_BLACK &&
                            video__hires_even[e+1] != COLOR_BLACK &&
                            video__hires_even[e-1] != COLOR_LIGHT_WHITE &&
                            video__hires_even[e+1] == COLOR_LIGHT_WHITE)
                        {
                            video__hires_even[e] =
                                video__hires_even[e-1];
                        }
                        else if (
                            video__hires_even[e-1] != COLOR_BLACK &&
                            video__hires_even[e+1] != COLOR_BLACK &&
                            video__hires_even[e-1] == COLOR_LIGHT_WHITE &&
                            video__hires_even[e+1] != COLOR_LIGHT_WHITE)
                        {
                            video__hires_even[e] =
                                video__hires_even[e+1];
                        }
                        else if (
                            video__hires_even[e-1] == COLOR_LIGHT_WHITE &&
                            video__hires_even[e+1] == COLOR_LIGHT_WHITE)
                        {
                            video__hires_even[e] = (value & 0x80)
                                                   ? COLOR_LIGHT_RED : COLOR_LIGHT_GREEN;
                        }
                    }
                }

                if (video__hires_odd[e] == COLOR_BLACK)
                {
                    if (b > 0 && b < 6)
                    {
                        if (video__hires_odd[e-1] != COLOR_BLACK &&
                            video__hires_odd[e+1] != COLOR_BLACK &&
                            video__hires_odd[e-1] != COLOR_LIGHT_WHITE &&
                            video__hires_odd[e+1] != COLOR_LIGHT_WHITE)
                        {
                            video__hires_odd[e] =
                                video__hires_odd[e-1];
                        }
                        else if (
                            video__hires_odd[e-1] != COLOR_BLACK &&
                            video__hires_odd[e+1] != COLOR_BLACK &&
                            video__hires_odd[e-1] != COLOR_LIGHT_WHITE &&
                            video__hires_odd[e+1] == COLOR_LIGHT_WHITE)
                        {
                            video__hires_odd[e] =
                                video__hires_odd[e-1];
                        }
                        else if (
                            video__hires_odd[e-1] != COLOR_BLACK &&
                            video__hires_odd[e+1] != COLOR_BLACK &&
                            video__hires_odd[e-1] == COLOR_LIGHT_WHITE &&
                            video__hires_odd[e+1] != COLOR_LIGHT_WHITE)
                        {
                            video__hires_odd[e] =
                                video__hires_odd[e+1];
                        }
                        else if (
                            video__hires_odd[e-1] == COLOR_LIGHT_WHITE &&
                            video__hires_odd[e+1] == COLOR_LIGHT_WHITE)
                        {
                            video__hires_odd[e] = (value & 0x80)
                                                  ? COLOR_LIGHT_BLUE : COLOR_LIGHT_PURPLE;
                        }
                    }
                }
            }
        }
    }

    /* *2 for 640x400 */
    for (b=0, e=0; b<4096; b++, e++)
    {
        video__wider_hires_even[b] = video__hires_even[e];
        video__wider_hires_odd[b] = video__hires_odd[e];
        b++;
        video__wider_hires_even[b] = video__hires_even[e];
        video__wider_hires_odd[b] = video__hires_odd[e];
    }
}


/* -------------------------------------------------------------------------
    c_initialize_row_col_tables()
   ------------------------------------------------------------------------- */
static void c_initialize_row_col_tables(void)
{
    int x, y, off, i;

    /* hires page offsets. initialize to invalid values. */
    for (i = 0; i < 8192; i++)
    {
        video__screen_addresses[i] = -1;
    }

    for (y = 0; y < TEXT_ROWS; y++)
    {
        for (off = 0; off < 8; off++)
        {
            for (x = 0; x < 40; x++)
            {
                video__screen_addresses[video__line_offset[y] + 0x400*off + x ] =
                    (y*16 + 2*off /* + 8*/) * SCANWIDTH + x*14 + 4;
                video__columns[video__line_offset[y] + 0x400*off + x] =
                    (uint8_t)x;
            }
        }
    }
}

static void c_initialize_tables_video(void) {
    int x, y, i;

    /* initialize text/lores & hires graphics */
    for (y = 0; y < TEXT_ROWS; y++)
    {
        for (x = 0; x < TEXT_COLS; x++)
        {
            /* //e mode: text/lores page 0 */
            cpu65_vmem[ video__line_offset[ y ] + x + 0x400].w =
                (y < 20) ? video__write_2e_text0 :
                video__write_2e_text0_mixed;

            cpu65_vmem[ video__line_offset[ y ] + x + 0x800].w =
                (y < 20) ? video__write_2e_text1 :
                video__write_2e_text1_mixed;

            for (i = 0; i < 8; i++)
            {
                /* //e mode: hires/double hires page 0 */
                cpu65_vmem[ 0x2000 + video__line_offset[ y ]
                            + 0x400 * i + x ].w =
                    (y < 20) ? ((x & 1) ? video__write_2e_odd0 :
                                video__write_2e_even0)
                    : ((x & 1) ? video__write_2e_odd0_mixed :
                       video__write_2e_even0_mixed);

                cpu65_vmem[ 0x4000 + video__line_offset[ y ]
                            + 0x400 * i + x ].w =
                    (y < 20) ? ((x & 1) ? video__write_2e_odd1 :
                                video__write_2e_even1)
                    : ((x & 1) ? video__write_2e_odd1_mixed :
                       video__write_2e_even1_mixed);
            }
        }
    }
}

void video_set(int flags)
{
    video__strictcolors = (color_mode == COLOR_INTERP) ? 2 : 1;

    c_initialize_hires_values();        /* precalculate hires values */
    c_initialize_row_col_tables();      /* precalculate hires offsets */
    c_initialize_tables_video();        /* memory jump tables for video */

    c_initialize_dhires_values();       /* set up dhires colors */
}

void video_loadfont_int(int first, int quantity, const unsigned char *data)
{
    int i,j;
    unsigned char x;
    int y;

    i = quantity * 8;

    while (i--)
    {
        j = 8;
        x = data[i];
        while (j--)
        {
            y = (first << 6) + (i << 3) + j;
            if (x & 128)
            {
                video__int_font[0][y] =
                    video__int_font[1][y] = COLOR_LIGHT_GREEN;
                video__int_font[2][y] = COLOR_LIGHT_RED;
            }
            else
            {
                video__int_font[0][y] =
                    video__int_font[2][y] = COLOR_BLACK;
                video__int_font[1][y] = COLOR_MEDIUM_BLUE;
            }
            x <<= 1;
        }
    }
}

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

void video_plotchar( int x, int y, int scheme, unsigned char c )
{
    int off;
    unsigned char *d;
    unsigned char *s;

    off = y * SCANWIDTH * 16 + x * 7 + 4;
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

#ifdef VIDEO_X11
extern void X11_video_init();
extern void X11_video_shutdown();
#endif
void video_init() {

    video__fb1 = vga_mem_page_0;
    video__fb2 = vga_mem_page_1;

    // reset Apple2 softframebuffers
    memset(video__fb1,0,SCANWIDTH*SCANHEIGHT);
    memset(video__fb2,0,SCANWIDTH*SCANHEIGHT);

#ifdef VIDEO_X11
    X11_video_init();
#endif
}

void video_shutdown(void)
{
#ifdef VIDEO_X11
    X11_video_shutdown();
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

static inline void _plot_character(const unsigned int font_off, const unsigned int fb_off, uint8_t *fb_base) {
    uint8_t *fb_ptr = fb_base+fb_off;
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

GLUE_C_WRITE(plot_character0)
{
    _plot_character(b<<7/* *128 */, video__screen_addresses[ea-0x0400], video__fb1);
}

GLUE_C_WRITE(plot_character1)
{
    _plot_character(b<<7/* *128 */, video__screen_addresses[ea-0x0800], video__fb2);
}

static inline void _plot_80character(const unsigned int font_off, const unsigned int fb_off, uint8_t *fb_base) {
    uint8_t *fb_ptr = fb_base+fb_off;
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
GLUE_C_WRITE(plot_80character0)
//static inline void _plot_80character0(uint16_t ea, uint8_t b)
{
    b = apple_ii_64k[1][ea];
    _plot_80character(b<<6/* *64 */, video__screen_addresses[ea-0x0400], video__fb1);
    b = apple_ii_64k[0][ea];
    _plot_80character(b<<6/* *64 */, video__screen_addresses[ea-0x0400]+7, video__fb1);
}

GLUE_C_WRITE(plot_80character1)
//static inline void _plot_80character1(uint16_t ea, uint8_t b)
{
    b = apple_ii_64k[1][ea];
    _plot_80character(b<<6/* *64 */, video__screen_addresses[ea-0x0800], video__fb2);
    b = apple_ii_64k[0][ea];
    _plot_80character(b<<6/* *64 */, video__screen_addresses[ea-0x0800]+7, video__fb2);
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
GLUE_C_WRITE(plot_block0)
//static inline void _plot_block0(uint16_t ea, uint8_t b)
{
    _plot_block(b, video__fb1+video__screen_addresses[ea-0x0400]);
}

GLUE_C_WRITE(plot_block1)
//static inline void _plot_block1(uint16_t ea, uint8_t b)
{
    _plot_block(b, video__fb2+video__screen_addresses[ea-0x0800]);
}

#define DRAW_TEXT(PAGE, SW) \
    do { \
        if (softswitches & SS_TEXT) { \
            if (softswitches & SS_80COL) { \
                c_plot_80character##PAGE(ea, b); \
            } else if (softswitches & SW) { \
                /* ??? */ \
            } else { \
                c_plot_character##PAGE(ea, b); \
            } \
        } else { \
            if (softswitches & (SS_HIRES|SW)) { \
                /* ??? */ \
            } else { \
                c_plot_block##PAGE(ea, b); \
            } \
        } \
    } while(0)


#define DRAW_MIXED(PAGE, SW) \
    do { \
        if (softswitches & (SS_TEXT|SS_MIXED)) { \
            if (softswitches & SS_80COL) { \
                c_plot_80character##PAGE(ea, b); \
            } else if (softswitches & SW) { \
                /* ??? */ \
            } else { \
                c_plot_character##PAGE(ea, b); \
            } \
        } else { \
            if (softswitches & (SS_HIRES|SW)) { \
                /* ??? */ \
            } else { \
                c_plot_block##PAGE(ea, b); \
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

