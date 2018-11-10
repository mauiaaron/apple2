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
 * Copyright 2013-2018 Aaron Culliney
 *
 */

#include "common.h"
#include "video/video.h"
#include "video/ntsc.h"

/*
 * Color structure
 */
typedef struct A2Color_s {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} A2Color_s;

typedef uint8_t (*glyph_getter_fn)(uint8_t idx, unsigned int row_off);
typedef void (*plot_fn)(color_mode_t mode, uint16_t bits14, unsigned int col, uint32_t *colors16, unsigned int fb_off);
typedef void (*flush_fn)(void);
typedef PIXEL_TYPE (*scanline_color_fn)(PIXEL_TYPE color);

static A2Color_s colormap[256] = { { 0 } };

static uint8_t scan_last_bit = 0x0;

static glyph_getter_fn glyph_getter[256>>5] = { NULL }; // /32 == 8 sections
static glyph_getter_fn flash_getter = NULL;

static plot_fn plot[NUM_COLOROPTS] = { NULL };
static flush_fn flush[6][NUM_COLOROPTS] = { { NULL } }; // 0-7, 8-15, 16-23, 24-31, 32-39, 40

static uint8_t glyph_map[256<<3] = { 0 }; // *8 rows
static uint8_t interface_font[5][0x4000] = { { 0 } }; // interface font

static color_mode_t color_mode = COLOR_MODE_DEFAULT;
static mono_mode_t mono_mode = MONO_MODE_DEFAULT;
static scanline_color_fn scanline_color[2] = { NULL };
static uint8_t half_scanlines = false;

// video line offsets
uint16_t video_line_offset[TEXT_ROWS + /*VBL:*/ 8 + /*extra:*/1] = {
    0x000, 0x080, 0x100, 0x180, 0x200, 0x280, 0x300, 0x380,
    0x028, 0x0A8, 0x128, 0x1A8, 0x228, 0x2A8, 0x328, 0x3A8,
    0x050, 0x0D0, 0x150, 0x1D0, 0x250, 0x2D0, 0x350, 0x3D0,
    0x078, 0x0F8, 0x178, 0x1F8, 0x278, 0x2F8, 0x378, 0x3F8,
    0x3F8
};

// Precalculated framebuffer offsets given VM addr
static unsigned int screen_addresses[8192] = { INT_MIN };

// Precalculated COLOR_MODE_COLOR and COLOR_MODE_INTERP colors
static PIXEL_TYPE general_interp[4096 << 2] = { 0 };
static PIXEL_TYPE general_color [4096 << 2] = { 0 };
static PIXEL_TYPE hires40_interp[4096 << 2] = { 0 };
static PIXEL_TYPE hires40_color [4096 << 2] = { 0 };

static PIXEL_TYPE *hires40_colors[NUM_COLOROPTS] = { 0 };
static PIXEL_TYPE *general_colors[NUM_COLOROPTS] = { 0 };

#define FB_SIZ (SCANWIDTH*SCANHEIGHT)

static PIXEL_TYPE fbFull[FB_SIZ + (SCANWIDTH<<1)] = { 0 }; // HACK NOTE: extra scanlines used for sampling

// ----------------------------------------------------------------------------
// Initialization routines

static void _load_interface_font(int first, int quantity, const uint8_t *data) {
    unsigned int i = quantity * 8;
    while (i--) {
        unsigned int j = 8;
        uint8_t x = data[i];
        while (j--) {
            unsigned int y = (first << 6) + (i << 3) + j;
            assert(y < 0x4000);
            if (x & 128) {
                interface_font[GREEN_ON_BLACK][y] = IDX_ICOLOR_G;
                interface_font[GREEN_ON_BLUE][y] = IDX_ICOLOR_G;
                interface_font[RED_ON_BLACK][y] = IDX_ICOLOR_R;
                interface_font[BLUE_ON_BLACK][y] = IDX_ICOLOR_B;
                interface_font[WHITE_ON_BLACK][y] = IDX_WHITE;
            } else {
                interface_font[GREEN_ON_BLACK][y] = IDX_BLACK;
                interface_font[GREEN_ON_BLUE][y] = IDX_ICOLOR_B;
                interface_font[RED_ON_BLACK][y] = IDX_BLACK;
                interface_font[BLUE_ON_BLACK][y] = IDX_BLACK;
                interface_font[WHITE_ON_BLACK][y] = IDX_BLACK;
            }
            x <<= 1;
        }
    }
}

static void _initialize_colormap(void) {

#if 0 // TRADITIONAL COLORS
        colormap[IDX_BLACK    ] = (A2Color_s) { .r = 0,   .g = 0,   .b = 0   };
        colormap[IDX_MAGENTA  ] = (A2Color_s) { .r = 195, .g = 0,   .b = 48  };
        colormap[IDX_DARKBLUE ] = (A2Color_s) { .r = 0,   .g = 0,   .b = 130 };
        colormap[IDX_PURPLE   ] = (A2Color_s) { .r = 166, .g = 52,  .b = 170 };
        colormap[IDX_DARKGREEN] = (A2Color_s) { .r = 0,   .g = 146, .b = 0   };
        colormap[IDX_DARKGREY ] = (A2Color_s) { .r = 105, .g = 105, .b = 105 };
        colormap[IDX_MEDBLUE  ] = (A2Color_s) { .r = 24,  .g = 113, .b = 255 };
        colormap[IDX_LIGHTBLUE] = (A2Color_s) { .r = 12,  .g = 190, .b = 235 };
        colormap[IDX_BROWN    ] = (A2Color_s) { .r = 150, .g = 85,  .b = 40  };
        colormap[IDX_ORANGE   ] = (A2Color_s) { .r = 255, .g = 24,  .b = 44  };
        colormap[IDX_LIGHTGREY] = (A2Color_s) { .r = 150, .g = 170, .b = 170 };
        colormap[IDX_PINK     ] = (A2Color_s) { .r = 255, .g = 158, .b = 150 };
        colormap[IDX_GREEN    ] = (A2Color_s) { .r = 0,   .g = 255, .b = 0   };
        colormap[IDX_YELLOW   ] = (A2Color_s) { .r = 255, .g = 255, .b = 0   };
        colormap[IDX_AQUA     ] = (A2Color_s) { .r = 130, .g = 255, .b = 130 };
        colormap[IDX_WHITE    ] = (A2Color_s) { .r = 255, .g = 255, .b = 255 };
#else
        colormap[IDX_BLACK    ] = (A2Color_s) { .r = 0,   .g = 0,   .b = 0   };
        colormap[IDX_MAGENTA  ] = (A2Color_s) { .r = 227, .g = 30,  .b = 96  };
        colormap[IDX_DARKBLUE ] = (A2Color_s) { .r = 96,  .g = 78,  .b = 189 };
        colormap[IDX_PURPLE   ] = (A2Color_s) { .r = 255, .g = 68,  .b = 253 };
        colormap[IDX_DARKGREEN] = (A2Color_s) { .r = 0,   .g = 163, .b = 96  };
        colormap[IDX_DARKGREY ] = (A2Color_s) { .r = 156, .g = 156, .b = 156 };
        colormap[IDX_MEDBLUE  ] = (A2Color_s) { .r = 20,  .g = 207, .b = 253 };
        colormap[IDX_LIGHTBLUE] = (A2Color_s) { .r = 208, .g = 195, .b = 255 };
        colormap[IDX_BROWN    ] = (A2Color_s) { .r = 96,  .g = 114, .b = 3   };
        colormap[IDX_ORANGE   ] = (A2Color_s) { .r = 255, .g = 106, .b = 60  };
        colormap[IDX_LIGHTGREY] = (A2Color_s) { .r = 156, .g = 156, .b = 156 };
        colormap[IDX_PINK     ] = (A2Color_s) { .r = 255, .g = 160, .b = 208 };
        colormap[IDX_GREEN    ] = (A2Color_s) { .r = 20,  .g = 245, .b = 60  };
        colormap[IDX_YELLOW   ] = (A2Color_s) { .r = 208, .g = 221, .b = 141 };
        colormap[IDX_AQUA     ] = (A2Color_s) { .r = 114, .g = 255, .b = 208 };
        colormap[IDX_WHITE    ] = (A2Color_s) { .r = 255, .g = 255, .b = 255 };
#endif

    for (unsigned int nyb=0x0; nyb<0x10; nyb++) {
        unsigned int idx = nyb + IDX_LUMINANCE_HALF;
        colormap[idx] = colormap[nyb];

        colormap[idx].r >>= 1;
        colormap[idx].g >>= 1;
        colormap[idx].b >>= 1;
    }

    colormap[IDX_ICOLOR_R ] = (A2Color_s) { .r = 255, .g = 0,   .b = 0   };
    colormap[IDX_ICOLOR_G ] = (A2Color_s) { .r = 0,   .g = 255, .b = 0   };
    colormap[IDX_ICOLOR_B ] = (A2Color_s) { .r = 0,   .g = 0,   .b = 255 };

#if USE_RGBA4444
    for (unsigned int i=0; i<256; i++) {
        colormap[i].r = (colormap[i].r >>4);
        colormap[i].g = (colormap[i].g >>4);
        colormap[i].b = (colormap[i].b >>4);
    }
#endif
}

static void _initialize_color_values(PIXEL_TYPE *color_pixels, PIXEL_TYPE *interp_pixels, bool adjustHIRES40) {

    // NEXT xxxx PREV -- 12 bits
    for (unsigned int nybPrev=0x00; nybPrev<0x10; nybPrev++) {

        uint8_t prevLumCount = 0;
        {
            for (int i=3; i>=0; i--) {
                if (nybPrev & (1 << i)) {
                    ++prevLumCount;
                } else {
                    break;
                }
            }
        }

        for (unsigned int nybNext=0x00; nybNext<0x10; nybNext++) {

            uint8_t nextLumCount = 0;
            for (unsigned int i=0; i<4; i++) {
                if (nybNext & (1 << i)) {
                    ++nextLumCount;
                } else {
                    break;
                }
            }

            for (unsigned int nyb=0x00; nyb<0x10; nyb++) {

                uint8_t color = nyb;

                if (adjustHIRES40) {
                    // HACK NOTE: mimics old style COLOR_MODE_COLOR setting ...
                    if (color == IDX_MAGENTA || color == IDX_BROWN) {
                        color = IDX_ORANGE;
                    } else if (color == IDX_DARKBLUE) {
                        color = IDX_MEDBLUE;
                    } else if (color == 0x0e) {
                        color = IDX_WHITE;
                    } else if (color == 0x07) {
                        color = IDX_WHITE;
                    }
                }

                uint8_t lum1 = (nyb & 0x1);
                uint8_t lum2 = (nyb & 0x2);
                uint8_t lum4 = (nyb & 0x4);
                uint8_t lum8 = (nyb & 0x8);

                uint8_t color1 = lum1 ? color : IDX_BLACK;
                uint8_t color2 = lum2 ? color : IDX_BLACK;
                uint8_t color4 = lum4 ? color : IDX_BLACK;
                uint8_t color8 = lum8 ? color : IDX_BLACK;

                if (adjustHIRES40) {
                    if (lum1) {
                        if (prevLumCount > 1) {
                            color1 = IDX_WHITE;
                        } else if (lum2 && lum4) {
                            color1 = color2 = color4 = IDX_WHITE;
                        }
                    }

                    if (lum2) {
                        if (lum1 && prevLumCount > 0) {
                            color1 = color2 = IDX_WHITE;
                        } else if (lum4 && lum8) {
                            color2 = color4 = color8 = IDX_WHITE;
                        }
                    }

                    if (lum8) {
                        if (nextLumCount > 1) {
                            color8 = IDX_WHITE;
                        } else if (lum4 && lum2) {
                            color2 = color4 = color8 = IDX_WHITE;
                        }
                    }

                    if (lum4) {
                        if (lum8 && nextLumCount > 0) {
                            color4 = color8 = IDX_WHITE;
                        } else if (lum1 && lum2) {
                            color1 = color2 = color4 = IDX_WHITE;
                        }
                    }
                }

                unsigned int idx = (nybNext << 8) | (nyb << 4) | nybPrev;
                idx <<= (PIXEL_STRIDE>>1);

                PIXEL_TYPE *pixelsColor  = &color_pixels[idx];

                pixelsColor[0] = (colormap[color1].r << SHIFT_R) | (colormap[color1].g << SHIFT_G) | (colormap[color1].b << SHIFT_B) | (MAX_SATURATION << SHIFT_A);
                pixelsColor[1] = (colormap[color2].r << SHIFT_R) | (colormap[color2].g << SHIFT_G) | (colormap[color2].b << SHIFT_B) | (MAX_SATURATION << SHIFT_A);
                pixelsColor[2] = (colormap[color4].r << SHIFT_R) | (colormap[color4].g << SHIFT_G) | (colormap[color4].b << SHIFT_B) | (MAX_SATURATION << SHIFT_A);
                pixelsColor[3] = (colormap[color8].r << SHIFT_R) | (colormap[color8].g << SHIFT_G) | (colormap[color8].b << SHIFT_B) | (MAX_SATURATION << SHIFT_A);

                // handle interpolated ...

                if (nyb && nyb != IDX_WHITE) {
                    // fill in inner black values
                    lum2 = (lum2 || (lum1 && (lum4 || lum8))) ? 0x2 : 0;
                    lum4 = (lum4 || (lum8 && (lum1 || lum2))) ? 0x4 : 0;

                    if (nyb == nybPrev && nyb == nybNext) {
                        lum1 = 0x1;
                        lum2 = 0x2;
                        lum4 = 0x4;
                        lum8 = 0x8;
                    } else {
                        if (nyb == nybPrev || (adjustHIRES40 && nybPrev == IDX_WHITE)) {
                            if (lum2 || lum4 || lum8) {
                                lum1 = 0x1;
                            }
                            if (lum4 || lum8) {
                                lum2 = 0x2;
                            }
                            if (lum8) {
                                lum4 = 0x4;
                            }
                        }
                        if (nyb == nybNext || (adjustHIRES40 && nybNext == IDX_WHITE)) {
                            if (lum1 || lum2 || lum4) {
                                lum8 = 0x8;
                            }
                            if (lum1 || lum2) {
                                lum4 = 0x4;
                            }
                            if (lum1) {
                                lum2 = 0x2;
                            }
                        }
                    }
                }
                color1 = lum1 ? (color1 ? color1 : color) : IDX_BLACK;
                color2 = lum2 ? (color2 ? color2 : color) : IDX_BLACK;
                color4 = lum4 ? (color4 ? color4 : color) : IDX_BLACK;
                color8 = lum8 ? (color8 ? color8 : color) : IDX_BLACK;

                PIXEL_TYPE *interpColor = &interp_pixels[idx];
                interpColor[0] = (colormap[color1].r << SHIFT_R) | (colormap[color1].g << SHIFT_G) | (colormap[color1].b << SHIFT_B) | (MAX_SATURATION << SHIFT_A);
                interpColor[1] = (colormap[color2].r << SHIFT_R) | (colormap[color2].g << SHIFT_G) | (colormap[color2].b << SHIFT_B) | (MAX_SATURATION << SHIFT_A);
                interpColor[2] = (colormap[color4].r << SHIFT_R) | (colormap[color4].g << SHIFT_G) | (colormap[color4].b << SHIFT_B) | (MAX_SATURATION << SHIFT_A);
                interpColor[3] = (colormap[color8].r << SHIFT_R) | (colormap[color8].g << SHIFT_G) | (colormap[color8].b << SHIFT_B) | (MAX_SATURATION << SHIFT_A);
            }
        }
    }
}

static void _initialize_display(void) {

    // screen addresses ...
    for (unsigned int y = 0; y < TEXT_ROWS; y++) {
        for (unsigned int y2 = 0; y2 < FONT_GLYPH_Y; y2++) {
            for (unsigned int x = 0; x < 40; x++) {
                unsigned int idx = video_line_offset[y] + (0x400*y2) + x;
                assert(idx < 8192);
                screen_addresses[idx] = ((y*FONT_HEIGHT_PIXELS + 2*y2) * SCANWIDTH) + (x*FONT_WIDTH_PIXELS) + _FB_OFF;
            }
        }
    }
    for (unsigned int i = 0; i < 8192; i++) {
        assert(screen_addresses[i] != INT_MIN);
    }

    // interface/display fonts ...

    _load_interface_font(0x00,0x40,ucase_glyphs);
    _load_interface_font(0x40,0x20,ucase_glyphs);
    _load_interface_font(0x60,0x20,lcase_glyphs);
    _load_interface_font(0x80,0x40,ucase_glyphs);
    _load_interface_font(0xC0,0x20,ucase_glyphs);
    _load_interface_font(0xE0,0x20,lcase_glyphs);
    _load_interface_font(MOUSETEXT_BEGIN,0x20,mousetext_glyphs);
    _load_interface_font(ICONTEXT_BEGIN,0x20,interface_glyphs);

    _initialize_colormap();
    _initialize_color_values(general_color, general_interp, /*adjustHIRES40:*/false);
    _initialize_color_values(hires40_color, hires40_interp, /*adjustHIRES40:*/true);
}

static uint8_t _glyph_normal(uint8_t idx, unsigned int row_off) {
    unsigned int glyph_base = idx<<3;
    uint8_t glyph_bits7 = glyph_map[glyph_base + row_off] /* & 0x7F */;
    return glyph_bits7;
}

static uint8_t _glyph_inverse(uint8_t idx, unsigned int row_off) {
    return ~_glyph_normal(idx, row_off);
}

static uint8_t _glyph_flash(uint8_t idx, unsigned int row_off) {
    return flash_getter(idx, row_off);
}

void display_loadFont(const uint8_t first, const uint8_t quantity, const uint8_t *data, font_mode_t mode) {
    uint8_t idx = (first >> 5);
    uint8_t len = idx + (quantity >> 5);
    for (unsigned int i=idx; i<len; i++) {
        switch (mode) {
            case FONT_MODE_NORMAL:
            case FONT_MODE_MOUSETEXT:
                glyph_getter[i] = _glyph_normal;
                break;
            case FONT_MODE_INVERSE:
                glyph_getter[i] = _glyph_inverse;
                break;
            case FONT_MODE_FLASH:
                glyph_getter[i] = _glyph_flash;
                break;
            default:
                assert(false);
                break;
        }
    }
    memcpy(&glyph_map[first<<3], data, quantity<<3);
}

// ----------------------------------------------------------------------------
// common plotting and filtering routines

static PIXEL_TYPE _color_half_scanline(PIXEL_TYPE color0) {
    PIXEL_TYPE color1 = ((color0 & 0xFEFEFEFE) >> 1) | (0xFF << SHIFT_A);
    return color1;
}

static PIXEL_TYPE _color_full_scanline(PIXEL_TYPE color0) {
    return color0;
}

static void _plot_oldschool(color_mode_t mode, uint16_t bits14, unsigned int col, PIXEL_TYPE *colors, unsigned int fb_off) {
    (void)mode;

    uint8_t shift = 0x8 | ((col & 0x1) << 1);
    uint16_t mask = ~(0xFF << shift); // 0xFF or 0x3FF
    unsigned int off = ((shift & 0x8) >> 1) + (shift & 0x2); // 4 or 6

    PIXEL_TYPE *fb_ptr = (&fbFull[0]) + _FB_OFF + fb_off - off;

    extern unsigned int ntsc_signal_bits; // HACK ...

    // 00BB,BBBB BAAA,AAAA dddd,dddc
    //                     -1 -> AAAA,dddd,dddc (redo 4 prior bits)
    //                      0 -> BAAA,AAAA,dddd
    //                      1 -> BBBB,BAAA,AAAA
    //                      2 -> ccBB,BBBB,BAAA xxx
    //                      3 -> cccc,ccBB,BBBB xxx

    // DDDD,DDDC CCCC,CCbb bbbb,baaa
    //                      2 -> CCbb,bbbb,baaa (redo 8 prior bits)
    //                      3 -> CCCC,CCbb,bbbb
    //                      4 -> DDDC,CCCC,CCbb
    //                      5 -> DDDD,DDDC,CCCC
    //                      6 -> aaaa,DDDD,DDDC xxx
    // -> 16bits rendered  (28bits total)
    uint32_t scanbits32 = (bits14 << shift) | (ntsc_signal_bits & mask);

    static unsigned int last_col_shift[6] = { 0, 0, 0, 0, 0, 1 };
    unsigned int count = 3 + (((shift >> 1) & 0x1) << last_col_shift[(col + 1) >> 3]);
    assert(count == 3 || count == 4 || count == 5);
    if (count == 5) {
        assert(true);
    }
    for (unsigned int i=0; i<count; i++) {
        uint16_t idx = (scanbits32 >> (4 * i)) & 0xFFF;

        idx <<= (PIXEL_STRIDE>>1);
        PIXEL_TYPE *pixels = &colors[idx];

        for (unsigned int j=0; j<4; j++) {
            fb_ptr[j] = pixels[j];
            fb_ptr[j + SCANWIDTH] = scanline_color[half_scanlines](pixels[j]);
        }

        fb_ptr += 4;
    }

    shift = ((shift & 0x8) >> 1) | (((shift >> 1) & 0x01) << 1);
    mask = ~(0xFFFF << (14-shift));
    ntsc_signal_bits = ((bits14 >> shift) & mask);
}

static void _plot_direct(color_mode_t mode, uint16_t bits14, unsigned int col, uint32_t *colors16, unsigned int fb_off) {
    (void)col;
    (void)colors16;
    PIXEL_TYPE *fb_ptr = (&fbFull[0]) + _FB_OFF + fb_off;
    ntsc_plotBits(mode, bits14, fb_ptr);
}

static void _flush_nop(void) {
    // no-op ...
}

static void _flush_scanline(void) {
    ntsc_flushScanline();
    scan_last_bit = 0x0;
}

// ----------------------------------------------------------------------------
// lores/char plotting routines
static inline void __plot_char80(PIXEL_TYPE **d, uint8_t **s, const unsigned int fb_width) {
    **d = (colormap[(**s)].r << SHIFT_R) | (colormap[(**s)].g << SHIFT_G) | (colormap[(**s)].b << SHIFT_B) | (MAX_SATURATION << SHIFT_A);
    *d += 1; *s += 1;
    **d = (colormap[(**s)].r << SHIFT_R) | (colormap[(**s)].g << SHIFT_G) | (colormap[(**s)].b << SHIFT_B) | (MAX_SATURATION << SHIFT_A);
    *d += 1; *s += 1;
    **d = (colormap[(**s)].r << SHIFT_R) | (colormap[(**s)].g << SHIFT_G) | (colormap[(**s)].b << SHIFT_B) | (MAX_SATURATION << SHIFT_A);
    *d += 1; *s += 1;
    **d = (colormap[(**s)].r << SHIFT_R) | (colormap[(**s)].g << SHIFT_G) | (colormap[(**s)].b << SHIFT_B) | (MAX_SATURATION << SHIFT_A);
    *d += 1; *s += 1;
    **d = (colormap[(**s)].r << SHIFT_R) | (colormap[(**s)].g << SHIFT_G) | (colormap[(**s)].b << SHIFT_B) | (MAX_SATURATION << SHIFT_A);
    *d += 1; *s += 1;
    **d = (colormap[(**s)].r << SHIFT_R) | (colormap[(**s)].g << SHIFT_G) | (colormap[(**s)].b << SHIFT_B) | (MAX_SATURATION << SHIFT_A);
    *d += 1; *s += 1;
    **d = (colormap[(**s)].r << SHIFT_R) | (colormap[(**s)].g << SHIFT_G) | (colormap[(**s)].b << SHIFT_B) | (MAX_SATURATION << SHIFT_A);
}

static inline void _plot_char80(PIXEL_TYPE **d, uint8_t **s, const unsigned int fb_width) {
    // FIXME : this is implicitly scaling at FONT_GLYPH_SCALE_Y ... make it explicit

    __plot_char80(d, s, fb_width);
    *d += fb_width-6; *s -= 6;

    __plot_char80(d, s, fb_width);
    *d += fb_width-6; *s += 2;
}

static void _plot_char40_scanline(scan_data_t *scandata) {
    uint8_t *scanline = scandata->scanline;
    unsigned int scanrow = scandata->scanrow;
    unsigned int scancol = scandata->scancol;
    unsigned int scanend = scandata->scanend;

    uint16_t row_off = (scanrow&0x07);

    uint16_t fb_base = video_line_offset[scanrow>>3];
    unsigned int fb_row = ((row_off<<1) * SCANWIDTH);

    for (unsigned int col = scancol; col < scanend; col++)
    {
        uint16_t glyph_bits14 = 0;
        {
            uint8_t mbd = scanline[(col<<1)+1]; // MBD data only
            uint8_t glyph_bits7 = glyph_getter[mbd>>5](mbd, row_off);
            for (unsigned int i=0; i<7; i++) {
                uint8_t b = glyph_bits7 & (1 << i);
                glyph_bits14 |= (b << (i+0));
                glyph_bits14 |= (b << (i+1));
            }
        }

        plot[COLOR_MODE_MONO](COLOR_MODE_MONO, glyph_bits14, col, NULL, screen_addresses[fb_base+col] + fb_row);
    }

    int filter_idx = (scanend >> 3);
    flush[filter_idx][COLOR_MODE_MONO](); // filter triggers on scanline completion
}

static void _plot_char80_scanline(scan_data_t *scandata) {
    uint8_t *scanline = scandata->scanline;
    unsigned int scanrow = scandata->scanrow;
    unsigned int scancol = scandata->scancol;
    unsigned int scanend = scandata->scanend;

    uint16_t row_off = (scanrow&0x07);

    uint16_t fb_base = video_line_offset[scanrow>>3];
    unsigned int fb_row = ((row_off<<1) * SCANWIDTH);

    for (unsigned int col = scancol; col < scanend; col++)
    {
        uint16_t glyph_bits14 = 0;
        {
            uint8_t glyph_bits7;

            uint8_t aux = scanline[(col<<1)+0];
            glyph_bits7 = glyph_getter[aux>>5](aux, row_off);
            glyph_bits14 = glyph_bits7;

            uint8_t mbd = scanline[(col<<1)+1];
            glyph_bits7 = glyph_getter[mbd>>5](mbd, row_off);
            glyph_bits14 |= glyph_bits7 << 7;
        }

        plot[COLOR_MODE_MONO](COLOR_MODE_MONO, glyph_bits14, col, NULL, screen_addresses[fb_base+col] + fb_row);
    }

    int filter_idx = (scanend >> 3);
    flush[filter_idx][COLOR_MODE_MONO](); // filter triggers on scanline completion
}

static void _plot_lores40_scanline(scan_data_t *scandata) {
    uint8_t *scanline = scandata->scanline;
    unsigned int scanrow = scandata->scanrow;
    unsigned int scancol = scandata->scancol;
    unsigned int scanend = scandata->scanend;

    uint16_t row_off = (scanrow&0x07);

    uint8_t hi_nyb = !!(row_off>>2); // 0,1,2,3 => 0  4,5,6,7 => 1
    uint8_t lores_shift = (hi_nyb<<2); // -> 0x0i
    uint8_t lores_mask = (0x0f << lores_shift ); // 0x0f --or-- 0xf0

    uint16_t fb_base = video_line_offset[scanrow>>3];
    unsigned int fb_row = ((row_off<<1) * SCANWIDTH);

    for (unsigned int col = scancol; col < scanend; col++)
    {
        uint8_t mbd = scanline[(col<<1)+1]; // MBD data only
        uint8_t val = (mbd & lores_mask) >> lores_shift;
        uint8_t rot2 = ((col & 0x1) << 1); // 2 phases at double rotation
        val = (val >> rot2) | ((val & 0x03) << rot2);

        uint16_t bits14 = val | (val << 4) | (val << 8) | (val << 12);
        bits14 &= 0x3FFF;

        plot[color_mode](color_mode, bits14, col, general_colors[color_mode], screen_addresses[fb_base+col] + fb_row);
    }

    int filter_idx = (scanend >> 3);
    flush[filter_idx][color_mode](); // filter triggers on scanline completion
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
    unsigned int scanrow = scandata->scanrow;
    unsigned int scancol = scandata->scancol;
    unsigned int scanend = scandata->scanend;

    uint16_t row_off = (scanrow&0x7);

    uint8_t hi_nyb = !!(row_off>>2); // 0,1,2,3 => 0  4,5,6,7 => 1
    uint8_t lores_shift = (hi_nyb<<2); // -> 0x0i
    uint8_t lores_mask = (0x0f << lores_shift ); // 0x0f --or-- 0xf0

    uint16_t fb_base = video_line_offset[scanrow>>3];
    unsigned int fb_row = ((row_off<<1) * SCANWIDTH);

    for (unsigned int col = scancol; col < scanend; col++)
    {
        uint16_t bits14;

        {
            unsigned int idx = (col<<1)+0; // AUX
            uint8_t b = scanline[idx];
            b = __shift_block80(b);
            uint8_t val = (b & lores_mask) >> lores_shift;
            val = (val << 4) | val;
            {
                uint16_t val16 = val | (val << 8);
                val16 = val16 >> (4 - (idx&0x3));
                val = (uint8_t)val16;
                val &= 0x7F;
            }
            bits14 = val;
        }

        {
            unsigned int idx = (col<<1)+1; // MBD
            uint8_t b = scanline[idx];
            uint8_t val = (b & lores_mask) >> lores_shift;
            val = (val << 4) | val;
            {
                uint16_t val16 = val | (val << 8);
                val16 = val16 >> (4 - (idx&0x3));
                val = (uint8_t)val16;
                val &= 0x7F;
            }
            bits14 |= (val<<7);
        }

        plot[color_mode](color_mode, bits14, col, general_colors[color_mode], screen_addresses[fb_base+col] + fb_row);
    }

    int filter_idx = (scanend >> 3);
    flush[filter_idx][color_mode](); // filter triggers on scanline completion
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

static void _display_plotChar(PIXEL_TYPE *fboff, const unsigned int fbPixWidth, const interface_colorscheme_t cs, const uint8_t c) {
    uint8_t *src = interface_font[cs] + c * (FONT_GLYPH_X*FONT_GLYPH_Y);
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
    unsigned int off = row * SCANWIDTH * FONT_HEIGHT_PIXELS + col * FONT80_WIDTH_PIXELS + _FB_OFF;
    _display_plotChar(fbFull+off, SCANWIDTH, cs, c);
    video_setDirty(FB_DIRTY_FLAG);
}
#endif

static void _display_plotLine(PIXEL_TYPE *fb, const unsigned int fbPixWidth, const unsigned int xAdjust, const uint8_t col, const uint8_t row, const interface_colorscheme_t cs, const char *line) {
    for (uint8_t x=col; *line; x++, line++) {
        char c = *line;
        unsigned int off = row * fbPixWidth * FONT_HEIGHT_PIXELS + x * FONT80_WIDTH_PIXELS + xAdjust;
        _display_plotChar(fb+off, fbPixWidth, cs, c);
    }
}

#if INTERFACE_CLASSIC
void display_plotLine(const uint8_t col, const uint8_t row, const interface_colorscheme_t cs, const char *message) {
    _display_plotLine(fbFull, /*fbPixWidth:*/SCANWIDTH, /*xAdjust:*/_FB_OFF, col, row, cs, message);
    video_setDirty(FB_DIRTY_FLAG);
}
#endif

void display_plotMessage(PIXEL_TYPE *fb, const interface_colorscheme_t cs, const char *message, const uint8_t message_cols, const uint8_t message_rows) {
    assert(message_cols < 80);
    assert(message_rows < 24);

    int fbPixWidth = (message_cols*FONT80_WIDTH_PIXELS);
    for (int row=0, idx=0; row<message_rows; row++, idx+=message_cols+1) {
        _display_plotLine(fb, fbPixWidth, /*xAdjust:*/0, /*col:*/0, row, cs, &message[ idx ]);
    }
}

// ----------------------------------------------------------------------------
// Double-HIRES (HIRES80) graphics

static void _plot_hires80_scanline(scan_data_t *scandata) {
    uint8_t *scanline = scandata->scanline;
    unsigned int scanrow = scandata->scanrow;
    unsigned int scancol = scandata->scancol;
    unsigned int scanend = scandata->scanend;

    uint16_t fb_base = video_line_offset[scanrow>>3] + (0x400 * (scanrow & 0x07));

    // 8 bytes (without hibit) smushed into 7 total bytes == 56 bits
    // |AUX 0a|MBD 0b |AUX 1c |MBD 1d |AUX 2e |MBD 2f |AUX 3g |MBD 3h ...
    // aaaaaaab bbbbbbcc cccccddd ddddeeee eeefffff ffgggggg ghhhhhhh ...
    // 01234560 12345601 23456012 34560123 45601234 56012345 60123456

    for (unsigned int col = scancol; col < scanend; col++)
    {
        uint8_t idx = (col<<1);

        uint8_t aux = scanline[idx+0] & 0x7f; // AUX
        uint8_t mbd = scanline[idx+1] & 0x7f; // MBD

        uint16_t bits14 = (((mbd << 7) | aux) << 1) | scan_last_bit;

        scan_last_bit = (bits14 >> 14) & 0x01;

        plot[color_mode](color_mode, bits14, col, general_colors[color_mode], screen_addresses[fb_base+col]);
    }

    int filter_idx = (scanend >> 3);
    flush[filter_idx][color_mode](); // filter triggers on scanline completion
}

// ----------------------------------------------------------------------------
// Hires GRaphics (HIRES40)

static void _plot_hires40_scanline(scan_data_t *scandata) {
    uint8_t *scanline = scandata->scanline;
    unsigned int scanrow = scandata->scanrow;
    unsigned int scancol = scandata->scancol;
    unsigned int scanend = scandata->scanend;

    uint16_t fb_base = video_line_offset[scanrow>>3] + (0x400 * (scanrow & 0x07));

    for (unsigned int col = scancol; col < scanend; col++)
    {
        uint16_t bits14 = 0;
        uint8_t shift;
        {
            uint8_t mbd = scanline[(col<<1)+1]; // MBD data only
            shift = (mbd & 0x80) >> 7;

            for (unsigned int i=0; i<7; i++) {
                uint8_t b = mbd & (1 << i);
                bits14 |= (b << (i+0));
                bits14 |= (b << (i+1));
            }

            bits14 = (bits14 << shift) | (scan_last_bit >> (1-shift));
            scan_last_bit = (mbd & 0x40) >> 6;
            bits14 &= 0x3FFF;
        }

        plot[color_mode](color_mode, bits14, col, hires40_colors[color_mode], screen_addresses[fb_base+col]);
    }

    int filter_idx = (scanend >> 3);
    flush[filter_idx][color_mode](); // filter triggers on scanline completion
}

static void (*_hirespage_plotter(uint32_t currswitches))(scan_data_t*) {
    return ((currswitches & SS_80COL) && (currswitches & SS_DHIRES)) ? _plot_hires80_scanline : _plot_hires40_scanline;
}

// ----------------------------------------------------------------------------

uint16_t display_getVideoLineOffset(uint8_t txtRow) {
    assert(txtRow <= TEXT_ROWS);
    return video_line_offset[txtRow];
}

PIXEL_TYPE *display_renderStagingFramebuffer(void) {

    const uint32_t mainswitches = run_args.softswitches;

    // render main portion of screen ...

    drawpage_mode_t mainDrawPageMode = video_currentMainMode(mainswitches);
    int page = video_currentPage(mainswitches);

    static uint8_t scanline[CYCLES_VIS<<1] = { 0 }; // 80 columns of data ...

    if (mainDrawPageMode == DRAWPAGE_TEXT) {
        uint16_t base = page ? 0x0800 : 0x0400;
        for (unsigned int row=0; row < TEXT_ROWS-4; row++) {
            for (unsigned int col=0; col < TEXT_COLS; col++) {
                uint16_t off = video_line_offset[row] + col;
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
            for (unsigned int i = 0; i < 8; i++) {
                for (unsigned int col=0; col < TEXT_COLS; col++) {
                    uint16_t off = video_line_offset[row] + (0x400*i) + col;
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
                uint16_t off = video_line_offset[row] + col;
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
            for (unsigned int i = 0; i < 8; i++) {
                for (unsigned int col=0; col < TEXT_COLS; col++) {
                    uint16_t off = video_line_offset[row] + (0x400*i) + col;
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
    return fbFull;
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
    return display_getCurrentFramebuffer();
}
#endif


void display_flashText(void) {
    static bool flash_normal = false;
    flash_normal = !flash_normal;

    if (flash_normal) {
        flash_getter = _glyph_normal;
    } else {
        flash_getter = _glyph_inverse;
    }

    video_setDirty(FB_DIRTY_FLAG);
}

PIXEL_TYPE *display_getCurrentFramebuffer(void) {
    return fbFull;
}

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

static void display_prefsChanged(const char *domain) {
    long lVal = 0;
    color_mode = prefs_parseLongValue(domain, PREF_COLOR_MODE, &lVal, /*base:*/10) ? getColorMode(lVal) : COLOR_MODE_DEFAULT;

    lVal = MONO_MODE_BW;
    mono_mode = prefs_parseLongValue(domain, PREF_MONO_MODE, &lVal, /*base:*/10) ? getMonoMode(lVal) : MONO_MODE_DEFAULT;

    bool bVal = false;
    half_scanlines = prefs_parseBoolValue(domain, PREF_SHOW_HALF_SCANLINES, &bVal) ? (bVal ? 1 : 0) : 0;

    _initialize_display();
}

static void _init_interface(void) {
    LOG("Initializing display subsystem");
    _initialize_display();

    plot[COLOR_MODE_MONO]          = _plot_direct;
    plot[COLOR_MODE_COLOR]         = _plot_oldschool;
    plot[COLOR_MODE_INTERP]        = _plot_oldschool;
    plot[COLOR_MODE_COLOR_MONITOR] = _plot_direct;
    plot[COLOR_MODE_MONO_TV]       = _plot_direct;
    plot[COLOR_MODE_COLOR_TV]      = _plot_direct;

    // scanline filtering
    for (unsigned int i=0; i<5; i++) {
        for (unsigned int j=0; j<NUM_COLOROPTS; j++) {
            flush[i][j] = _flush_nop;
        }
    }

    flush[5][COLOR_MODE_MONO]          = _flush_scanline;
    flush[5][COLOR_MODE_COLOR]         = _flush_scanline;
    flush[5][COLOR_MODE_INTERP]        = _flush_scanline;
    flush[5][COLOR_MODE_COLOR_MONITOR] = _flush_scanline;
    flush[5][COLOR_MODE_MONO_TV]       = _flush_scanline;
    flush[5][COLOR_MODE_COLOR_TV]      = _flush_scanline;

    hires40_colors[COLOR_MODE_COLOR]  = &hires40_color [0];
    hires40_colors[COLOR_MODE_INTERP] = &hires40_interp[0];
    general_colors[COLOR_MODE_COLOR]  = &general_color [0];
    general_colors[COLOR_MODE_INTERP] = &general_interp[0];

    scanline_color[0] = _color_full_scanline;
    scanline_color[1] = _color_half_scanline;

    flash_getter = _glyph_normal;

    prefs_registerListener(PREF_DOMAIN_VIDEO, &display_prefsChanged);
}

static __attribute__((constructor)) void __init_interface(void) {
    emulator_registerStartupCallback(CTOR_PRIORITY_LATE, &_init_interface);
}

