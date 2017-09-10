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

/*
 * Color structure
 */
typedef struct A2Color_s {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} A2Color_s;

#define IDX_BLACK     0x00
#define IDX_MAGENTA   0x10
#define IDX_DARKBLUE  0x20
#define IDX_PURPLE    0x30
#define IDX_DARKGREEN 0x40
#define IDX_DARKGREY  0x50
#define IDX_MEDBLUE   0x60
#define IDX_LIGHTBLUE 0x70
#define IDX_BROWN     0x80
#define IDX_ORANGE    0x90
#define IDX_LIGHTGREY 0xa0
#define IDX_PINK      0xb0
#define IDX_GREEN     0xc0
#define IDX_YELLOW    0xd0
#define IDX_AQUA      0xe0
#define IDX_WHITE     0xf0

/*
 * Reference to the internal 8bit-indexed color format
 */
extern A2Color_s colormap[];

/*
 * Color options
 */
typedef enum color_mode_t {
    COLOR_NONE = 0,
    COLOR,
    COLOR_INTERP,
    NUM_COLOROPTS
} color_mode_t;

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
    DRAWPAGE_TEXT = 1,
    DRAWPAGE_HIRES,
    NUM_DRAWPAGE_MODES,
} drawpage_mode_t;

/*
 * Pixel deltas
 */
typedef struct pixel_delta_t {
    uint8_t row; // row
    uint8_t col; // col
    uint8_t b;   // byte
    uint8_t cs;  // colorscheme (interface updates only)
} pixel_delta_t;

/*
 * Text/hires callback
 */
typedef void (*display_update_fn)(pixel_delta_t);

/*
 * Setup the display. This may be called multiple times in a run, and is
 * used when graphics parameters may have changed.
 *
 * This function is responsible for inserting any needed video-specific hooks
 * into the 6502 memory indirection table.  It should *not* hook the
 * soft-switches.
 *
 */
void display_reset(void);

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

/*
 * Plot character into staging framebuffer.
 *
 *  - Framebuffer may be NULL or should be exactly SCANWIDTH*SCANHEIGHT*sizeof(uint8_t) size
 *  - Pixels in the framebuffer are 8bit indexed color into the colormap array
 *  - Triggers text update callback
 *  - This should only be called from video backends/interface
 */
void display_plotChar(uint8_t *fb, const uint8_t col, const uint8_t row, const interface_colorscheme_t cs, const uint8_t c);

/*
 * Plot NULL_terminated string into staging framebuffer.
 *  - See display_plotChar() ...
 *
 */
void display_plotLine(uint8_t *fb, const uint8_t col, const uint8_t row, const interface_colorscheme_t cs, const char *message);

/*
 * Plot multi-line message into staging framebuffer.
 *  - See display_plotChar() ...
 */
void display_plotMessage(uint8_t *fb, const interface_colorscheme_t cs, const char *message, const uint8_t message_cols, const uint8_t message_rows);

// ----------------------------------------------------------------------------

/*
 * Current dominant screen mode, TEXT or MIXED/HIRES
 */
drawpage_mode_t display_currentDrawpageMode(uint32_t currswitches);

/*
 * Set TEXT/HIRES update callback(s).
 */
void display_setUpdateCallback(drawpage_mode_t mode, display_update_fn updateFn);

/*
 * Flushes currently set Apple //e video memory into staging framebuffer.
 *
 *  - Framebuffer should be exactly SCANWIDTH*SCANHEIGHT*sizeof(uint8_t) size
 *  - Pixels in the framebuffer are 8bit indexed color into the colormap array
 *  - This should only be called from video backends or testsuite
 */
void display_renderStagingFramebuffer(uint8_t *stagingFB);

// ----------------------------------------------------------------------------

/*
 * VBL routines
 */
uint16_t video_scanner_get_address(bool *vblBarOut);
uint8_t floating_bus(void);
uint8_t floating_bus_hibit(const bool hibit);

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
#define _INTERPOLATED_PIXEL_ADJUSTMENT_PRE 4
#define _INTERPOLATED_PIXEL_ADJUSTMENT_POST 4
#define INTERPOLATED_PIXEL_ADJUSTMENT (_INTERPOLATED_PIXEL_ADJUSTMENT_PRE+_INTERPOLATED_PIXEL_ADJUSTMENT_POST)
#define SCANWIDTH (_SCANWIDTH+INTERPOLATED_PIXEL_ADJUSTMENT)

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

#define COLOR_BLACK             0

#define COLOR_DARK_RED          35
#define COLOR_MEDIUM_RED        36
#define COLOR_LIGHT_RED         37      /* hgr used */

#define COLOR_DARK_GREEN        38
#define COLOR_MEDIUM_GREEN      39
#define COLOR_LIGHT_GREEN       40      /* hgr used */

#define COLOR_DARK_YELLOW       41
#define COLOR_MEDIUM_YELLOW     42
#define COLOR_LIGHT_YELLOW      43

#define COLOR_DARK_BLUE         44
#define COLOR_MEDIUM_BLUE       45
#define COLOR_LIGHT_BLUE        46      /* hgr used */

#define COLOR_DARK_PURPLE       47
#define COLOR_MEDIUM_PURPLE     48
#define COLOR_LIGHT_PURPLE      49      /* hgr used */

#define COLOR_DARK_CYAN         50
#define COLOR_MEDIUM_CYAN       51
#define COLOR_LIGHT_CYAN        52

#define COLOR_DARK_WHITE        53
#define COLOR_MEDIUM_WHITE      54
#define COLOR_LIGHT_WHITE       55

#define COLOR_FLASHING_BLACK    56
#define COLOR_FLASHING_WHITE    57
#define COLOR_FLASHING_UNGREEN  58
#define COLOR_FLASHING_GREEN    59


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

