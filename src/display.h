/*
 * Apple // emulator for *ix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 2016 Aaron Culliney
 *
 */

#ifndef _DISPLAY_H_
#define _DISPLAY_H_

#if USE_RGBA4444
#error HACK FIXME TODO : 2018/09/23 currently untested
#   define PIXEL_TYPE uint16_t
#   define MAX_SATURATION 0xf
#   define SHIFT_R 12
#   define SHIFT_G 8
#   define SHIFT_B 4
#   define SHIFT_A 0
#   define RGB_MASK 0xFFF0
#else
// assuming RGBA8888 ...
#   define PIXEL_TYPE uint32_t
#   define MAX_SATURATION 0xff
#   define SHIFT_R 0
#   define SHIFT_G 8
#   define SHIFT_B 16
#   define SHIFT_A 24
#   define RGB_MASK 0xFFFFFF
#endif

#define PIXEL_STRIDE (sizeof(PIXEL_TYPE))

/*
 * Scanline data
 */
typedef struct scan_data_t {
    uint8_t *scanline;      // CYCLES_VIS<<1 == 80
    uint32_t softswitches;
    unsigned int scanrow;   // 0 >= x < 192
    unsigned int scancol;   // 0 >  x <= 80
    unsigned int scanend;   // 0 >= x < 80
} scan_data_t;

#define IDX_BLACK     0x00 // 0000
#define IDX_MAGENTA   0x01 // 0001
#define IDX_DARKBLUE  0x02 // 0010
#define IDX_PURPLE    0x03 // 0011 - HIRES40
#define IDX_DARKGREEN 0x04 // 0100
#define IDX_DARKGREY  0x05 // 0101
#define IDX_MEDBLUE   0x06 // 0110 - HIRES40
#define IDX_LIGHTBLUE 0x07 // 0111
#define IDX_BROWN     0x08 // 1000
#define IDX_ORANGE    0x09 // 1001 - HIRES40
#define IDX_LIGHTGREY 0x0a // 1010
#define IDX_PINK      0x0b // 1011
#define IDX_GREEN     0x0c // 1100 - HIRES40
#define IDX_YELLOW    0x0d // 1101
#define IDX_AQUA      0x0e // 1110
#define IDX_WHITE     0x0f // 1111

// Colors duplicated and muted at this offset
#define IDX_LUMINANCE_HALF 0x40

// Interface colors
#define IDX_ICOLOR_R  0x21
#define IDX_ICOLOR_G  0x22
#define IDX_ICOLOR_B  0x23

typedef enum interface_colorscheme_t {
    GREEN_ON_BLACK = 0,
    GREEN_ON_BLUE,
    RED_ON_BLACK,
    BLUE_ON_BLACK,
    WHITE_ON_BLACK,

    // WARNING : changing here requires updating display.c ncvideo.c

    BLACK_ON_RED,

    // 16 COLORS -- ncvideo.c
    BLACK_ON_BLACK,
    BLACK_ON_MAGENTA,
    BLACK_ON_DARKBLUE,
    BLACK_ON_PURPLE,
    BLACK_ON_DARKGREEN,
    BLACK_ON_DARKGREY,
    BLACK_ON_MEDBLUE,
    BLACK_ON_LIGHTBLUE,
    BLACK_ON_BROWN,
    BLACK_ON_ORANGE,
    BLACK_ON_LIGHTGREY,
    BLACK_ON_PINK,
    BLACK_ON_GREEN,
    BLACK_ON_YELLOW,
    BLACK_ON_AQUA,
    BLACK_ON_WHITE,

    NUM_INTERFACE_COLORSCHEMES,
    COLOR16 = 0x80,
    INVALID_COLORSCHEME = 0xFF,
} interface_colorscheme_t;

/*
 * Color options
 */
typedef enum color_mode_t {
    // original modes ...
    COLOR_MODE_MONO = 0,
    COLOR_MODE_COLOR,
    COLOR_MODE_INTERP,
    // NTSC color modes ...
    COLOR_MODE_COLOR_MONITOR,
    COLOR_MODE_MONO_TV,
    COLOR_MODE_COLOR_TV,
    NUM_COLOROPTS,
} color_mode_t;

#define COLOR_MODE_DEFAULT COLOR_MODE_COLOR_TV
static inline color_mode_t getColorMode(long lVal) {
    if (lVal < 0 || lVal >= NUM_COLOROPTS) {
        lVal = COLOR_MODE_DEFAULT;
    }
    return (color_mode_t)lVal;
}

typedef enum mono_mode_t {
    MONO_MODE_BW = 0,
    MONO_MODE_GREEN,
    NUM_MONOOPTS,
} mono_mode_t;

#define MONO_MODE_DEFAULT MONO_MODE_BW
static inline mono_mode_t getMonoMode(long lVal) {
    if (lVal < 0 || lVal >= NUM_MONOOPTS) {
        lVal = MONO_MODE_DEFAULT;
    }
    return (mono_mode_t)lVal;
}

/*
 * Font mode
 */
typedef enum font_mode_t {
    FONT_MODE_NORMAL=0,
    FONT_MODE_MOUSETEXT,
    FONT_MODE_INVERSE,
    FONT_MODE_FLASH,
    NUM_FONT_MODES,
} font_mode_t;

/*
 * Graphics mode
 */
typedef enum drawpage_mode_t {
    DRAWPAGE_TEXT = 0,
    DRAWPAGE_HIRES,
    NUM_DRAWPAGE_MODES,
} drawpage_mode_t;

/*
 * Set the font used by the display.  QTY characters are loaded starting
 * with FIRST, from DATA.  DATA contains 8 bytes for each character, each
 * byte representing a row (top-to-bottom).  The row byte contains 7
 * pixels in little-endian format.
 *
 * MODE selects the colors to use
 *
 *   0 - Normal text
 *   1 - MouseText (usually treat as Normal)
 *   2 - Inverse
 *   3 - Flash
 *
 * The extra MouseText mode is in case we want to emulate certain RGB
 * adaptors which color normal text and MouseText differently.  I had one
 * once for a //c.
 */
void display_loadFont(const uint8_t start, const uint8_t qty, const uint8_t *data, font_mode_t mode);

/*
 * Toggles FLASHing text between NORMAL and INVERSE character sets.
 */
void display_flashText(void);

// ----------------------------------------------------------------------------

#if INTERFACE_CLASSIC
/*
 * Plot character into interface framebuffer.
 *  - This should only be called from video backends/interface
 */
void display_plotChar(const uint8_t col, const uint8_t row, const interface_colorscheme_t cs, const uint8_t c);

/*
 * Plot NULL_terminated string into interface framebuffer.
 *  - See display_plotChar() ...
 */
void display_plotLine(const uint8_t col, const uint8_t row, const interface_colorscheme_t cs, const char *message);
#endif

/*
 * Plot multi-line message into given framebuffer (not the interface framebuffer).
 *  - See display_plotChar() ...
 */
void display_plotMessage(PIXEL_TYPE *fb, const interface_colorscheme_t cs, const char *message, const uint8_t message_cols, const uint8_t message_rows);

/*
 * Get video line offset for TEXT mode row
 */
uint16_t display_getVideoLineOffset(uint8_t txtRow);

// ----------------------------------------------------------------------------
// video generation

/*
 * Called by video backend to get the current complete staging framebuffer.
 *  - Framebuffer is exactly SCANWIDTH*SCANHEIGHT*sizeof(PIXEL_TYPE)
 */
PIXEL_TYPE *display_getCurrentFramebuffer(void) CALL_ON_UI_THREAD;

/*
 * Handler for toggling text flashing
 */
void display_flashText(void) CALL_ON_CPU_THREAD;

/*
 * Handler for video scanner scanline completion
 */
void display_flushScanline(scan_data_t *scandata) CALL_ON_CPU_THREAD;

/*
 * Handler called when entire frame is complete
 */
void display_frameComplete(void) CALL_ON_CPU_THREAD;

#if TESTING
/*
 * Wait for frame complete and return staging framebuffer.
 */
uint8_t *display_waitForNextCompleteFramebuffer(void);
#endif

// ----------------------------------------------------------------------------


#define TEXT_ROWS 24
#define BEGIN_MIX 20
#define TEXT_COLS 40
#define TEXT80_COLS (TEXT_COLS*2)

#define FONT_HEIGHT_PIXELS 16
#define FONT_WIDTH_PIXELS  14
#define FONT80_WIDTH_PIXELS (FONT_WIDTH_PIXELS>>1)

#define _SCANWIDTH (TEXT_COLS * FONT_WIDTH_PIXELS)  // 560
#define SCANHEIGHT (TEXT_ROWS * FONT_HEIGHT_PIXELS) // 384

// Extra bytes on each side of internal framebuffers for color interpolation hack
#define _FB_OFF 4
#define _INTERPOLATED_PIXEL_ADJUSTMENT_PRE _FB_OFF
#define _INTERPOLATED_PIXEL_ADJUSTMENT_POST _FB_OFF
#define INTERPOLATED_PIXEL_ADJUSTMENT (_INTERPOLATED_PIXEL_ADJUSTMENT_PRE+_INTERPOLATED_PIXEL_ADJUSTMENT_POST)
#define SCANWIDTH (_SCANWIDTH+INTERPOLATED_PIXEL_ADJUSTMENT) // 568

#define FONT_GLYPH_X (7+/*unused*/1)            // generated font.c uses a single byte (8bits) per font glyph line
#define FONT_GLYPH_Y (FONT_HEIGHT_PIXELS>>1)    // ... 8 bytes total for whole glyph
#define FONT_GLYPH_SCALE_Y (FONT_HEIGHT_PIXELS/FONT_GLYPH_Y) // FIXME NOTE : implicit 2x scaling in display.c ...

#define MOUSETEXT_BEGIN         0x80 // offset + 0x20 length
#define MOUSETEXT_RETURN        (MOUSETEXT_BEGIN+0x0d)
#define MOUSETEXT_UP            (MOUSETEXT_BEGIN+0x0b)
#define MOUSETEXT_LEFT          (MOUSETEXT_BEGIN+0x08)
#define MOUSETEXT_RIGHT         (MOUSETEXT_BEGIN+0x15)
#define MOUSETEXT_DOWN          (MOUSETEXT_BEGIN+0x0a)
#define MOUSETEXT_OPENAPPLE     (MOUSETEXT_BEGIN+0x01)
#define MOUSETEXT_CLOSEDAPPLE   (MOUSETEXT_BEGIN+0x00)
#define MOUSETEXT_HOURGLASS     (MOUSETEXT_BEGIN+0x03)
#define MOUSETEXT_CHECKMARK     (MOUSETEXT_BEGIN+0x04)
#define MOUSETEXT_CURSOR0       (MOUSETEXT_BEGIN+0x16)
#define MOUSETEXT_CURSOR1       (MOUSETEXT_BEGIN+0x17)

#define ICONTEXT_BEGIN          0xA0 // offset + 0x22 length
#define ICONTEXT_MENU_BEGIN     ICONTEXT_BEGIN
#define ICONTEXT_MENU_END       (ICONTEXT_MENU_BEGIN+0x0A)

#define ICONTEXT_DISK_UL        (ICONTEXT_BEGIN+0x0B)
#define ICONTEXT_DISK_UR        (ICONTEXT_BEGIN+0x0C)
#define ICONTEXT_DISK_LL        (ICONTEXT_BEGIN+0x0D)
#define ICONTEXT_DISK_LR        (ICONTEXT_BEGIN+0x0E)
#define ICONTEXT_UNLOCK         (ICONTEXT_BEGIN+0x0F)
#define ICONTEXT_GOTO           (ICONTEXT_BEGIN+0x10)
#define ICONTEXT_SPACE_VISUAL   (ICONTEXT_BEGIN+0x11)

#define ICONTEXT_MENU_SPROUT    (MOUSETEXT_BEGIN+0x1B)
#define ICONTEXT_MENU_TOUCHJOY  (ICONTEXT_BEGIN+0x12)
#define ICONTEXT_MENU_TOUCHJOY_KPAD (ICONTEXT_BEGIN+0x18)

#define ICONTEXT_KBD_BEGIN      (ICONTEXT_BEGIN+0x13)
#define ICONTEXT_CTRL           (ICONTEXT_KBD_BEGIN+0x00)
#define ICONTEXT_LOWERCASE      (ICONTEXT_KBD_BEGIN+0x01)
#define ICONTEXT_UPPERCASE      (ICONTEXT_KBD_BEGIN+0x02)
#define ICONTEXT_SHOWALT1       (ICONTEXT_KBD_BEGIN+0x03)
#define ICONTEXT_BACKSPACE      (ICONTEXT_KBD_BEGIN+0x04)

#define ICONTEXT_LEFTSPACE      (ICONTEXT_KBD_BEGIN+0x06)
#define ICONTEXT_MIDSPACE       (ICONTEXT_KBD_BEGIN+0x07)
#define ICONTEXT_RIGHTSPACE     (ICONTEXT_KBD_BEGIN+0x08)
#define ICONTEXT_ESC            (ICONTEXT_KBD_BEGIN+0x09)
#define ICONTEXT_RETURN_L       (ICONTEXT_KBD_BEGIN+0x0A)
#define ICONTEXT_RETURN_R       (ICONTEXT_KBD_BEGIN+0x0B)
#define ICONTEXT_NONACTIONABLE  (ICONTEXT_KBD_BEGIN+0x0C)

#define ICONTEXT_LEFT_TAB       (ICONTEXT_KBD_BEGIN+0x..)
#define ICONTEXT_RIGHT_TAB      (ICONTEXT_KBD_BEGIN+0x..)

/* ----------------------------------
    generic graphics globals
   ---------------------------------- */

/*
 * font glyph data
 */
extern const unsigned char ucase_glyphs[0x200];
extern const unsigned char lcase_glyphs[0x100];
extern const unsigned char mousetext_glyphs[0x100];
extern const unsigned char interface_glyphs[88];

#endif // whole file

