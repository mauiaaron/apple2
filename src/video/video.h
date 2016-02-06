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

#ifndef A2_VIDEO_H
#define A2_VIDEO_H

typedef struct video_backend_s {

    // mandatory video backend functions
    void (*init)(void *context);
    void (*main_loop)(void);
    void (*reshape)(int width, int height);
    void (*render)(void);
    void (*shutdown)(void);

    // touch HUD functions
    void (*animation_showTouchKeyboard)(void);
    void (*animation_hideTouchKeyboard)(void);
    void (*animation_showTouchJoystick)(void);
    void (*animation_hideTouchJoystick)(void);
    void (*animation_showTouchMenu)(void);
    void (*animation_hideTouchMenu)(void);

    // misc animations
    void (*animation_showMessage)(char *message, unsigned int cols, unsigned int rows);
    void (*animation_showPaused)(void);
    void (*animation_showCPUSpeed)(void);
    void (*animation_showDiskChosen)(int drive);
    void (*animation_showTrackSector)(int drive, int track, int sect);
    void (*animation_setEnableShowTrackSector)(bool enabled);

} video_backend_s;

/*
 * The registered video backend (renderer).
 */
extern video_backend_s *video_backend;

/*
 * Color structure
 */
typedef struct A2Color_s {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} A2Color_s;

/*
 * Reference to the internal 8bit-indexed color format
 */
extern A2Color_s colormap[];

/*
 * Prepare the video system, converting console to graphics mode, or
 * opening X window, or whatever.  This is called only once when the
 * emulator is run
 */
void video_init(void);

/*
 * Enters emulator-managed main video loop--if backend rendering system requires it.  Currently only used by desktop X11
 * and desktop OpenGL/GLUT.
 */
void video_main_loop(void);

/*
 * Shutdown video system.  Should only be called on the render thread (unless render thread is in emulator-managed main
 * video loop).
 */
void video_shutdown(void);

/*
 * Begin a render pass (only for non-emulator-managed main video).  This should only be called on the render thread.
 */
void video_render(void);

/*
 * Set the render thread ID.  Use with caution.
 */
void _video_setRenderThread(pthread_t id);

/*
 * Reshape the display to particular dimensions.
 */
void video_reshape(int w, int h);

/*
 * Setup the display. This may be called multiple times in a run, and is
 * used when graphics parameters may have changed.
 *
 * This function is responsible for inserting any needed video-specific hooks
 * into the 6502 memory indirection table.  It should *not* hook the
 * soft-switches.
 *
 */
void video_reset(void);

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
void video_loadfont(int first, int qty, const uint8_t *data, int mode);

/*
 * Redraw the display. This is called after exiting an interface display,
 * when changes have been made to the Apple's emulated framebuffer that
 * bypass the driver's hooks, or when the video mode has changed.
 */
void video_redraw(void);

/*
 * Toggles FLASHing text between NORMAL and INVERSE character sets.
 */
void video_flashText(void);

/*
 * Clear the current display.
 */
void video_clear(void);

/*
 * Change the displayed video page to PAGE
 *   0 - Page 1: $400-$7ff/$2000-$3fff
 *   1 - Page 2: $800-$bff/$4000-$5fff
 */
void video_setpage(int page);

/*
 * Get a reference to current internal framebuffer
 */
const uint8_t * const video_current_framebuffer();

// do not access directly, but through inline accessor methods
extern volatile unsigned long _backend_vid_dirty;

/*
 * True if anything changed in framebuffer and not yet drawn
 */
static inline bool video_isDirty(void) {
    return _backend_vid_dirty;
}

/*
 * Atomically set dirty bit, return previous value
 */
static inline unsigned long video_setDirty(void) {
    return __sync_fetch_and_or(&_backend_vid_dirty, 1UL);
}

/*
 * Atomically clear dirty bit, return previous value
 */
static inline unsigned long video_clearDirty(void) {
    return __sync_fetch_and_and(&_backend_vid_dirty, 0UL);
}

extern bool video_saveState(StateHelper_s *helper);
extern bool video_loadState(StateHelper_s *helper);

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
 * Pointers to framebuffer (can be VGA memory or host buffer)
 */
extern uint8_t *video__fb1;
extern uint8_t *video__fb2;

/*
 * Current visual page
 */
extern READONLY int video__current_page;

/*
 * font glyph data
 */
extern const unsigned char ucase_glyphs[0x200];
extern const unsigned char lcase_glyphs[0x100];
extern const unsigned char mousetext_glyphs[0x100];
extern const unsigned char interface_glyphs[88];

#endif /* !A2_VIDEO_H */

