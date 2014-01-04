/*
 * Apple // emulator for Linux: Video definitions
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

#ifndef A2_VIDEO_H
#define A2_VIDEO_H

#ifndef __ASSEMBLER__

#include "prefs.h"

/* Prepare the video system, converting console to graphics mode, or
 * opening X window, or whatever.  This is called only once when the
 * emulator is run
 */
void video_init(void);

/* Undo anything done by video_init. This is called before exiting the
 * emulator.
 */
void video_shutdown(void);

/* Setup the display. This may be called multiple times in a run, and is
 * used when graphics parameters (II+ vs //e, hires color representation) may
 * have changed.
 *
 * In future, all relevant information will be communicated through
 * FLAGS. For now, however, it is ignored and global variables are used
 * instead.
 *
 * This function is responsible for inserting any needed video-specific hooks
 * into the 6502 memory indirection table.  It should *not* hook the
 * soft-switches.
 *
 */
void video_set(int flags);

/* Set the font used by the display.  QTY characters are loaded starting
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
void video_loadfont(int first,
                    int qty,
                    const unsigned char *data,
                    int mode);

/* Redraw the display. This is called after exiting an interface display,
 * when changes have been made to the Apple's emulated framebuffer that
 * bypass the driver's hooks, or when the video mode has changed.
 */
void video_redraw(void);

/* Change the displayed video page to PAGE
 *   0 - Page 1: $400-$7ff/$2000-$3fff
 *   1 - Page 2: $800-$bff/$4000-$5fff
 */
void video_setpage(int page);

/* Like loadfont, but writes to a seperate 256 character table used by
 * video_plotchar and not the apple text-mode display.
 */
void video_loadfont_int(int first, int qty, const unsigned char *data);

/* Plot a character to the text mode screen, *not* writing to apple
 * memory. This is used by the interface screens.
 *
 * ROW, COL, and CODE are self-expanatory. COLOR gives the color scheme
 * to use:
 *
 *  0 - Green text on Black background
 *  1 - Green text on Blue background
 *  2 - Red text on Black background
 */
void video_plotchar(int row, int col, int color, unsigned char code);

/* Called at about 30Hz (this may change in future), and when waiting in
 * the interface screens.
 *
 * Should flush any video data to the real screen (if any kind of caching
 * is in use), check for keyboard input (presently reported via
 * c_read_raw_key), and handle flashing text characters.
 */
void video_sync(int block);

void video_set_mode(a2_video_mode_t mode);

#endif /* !__ASSEMBLER__ */

/**** Private stuff follows *****/

#ifdef _640x400
/* 640x400 mode really isn't what it advertises.  It's really 560x384 with 4
 * extra bytes on each side for color interpolation hack.  This is yet another
 * area where I've traded the optimization gain (especially on older slower
 * machines) for a standard resolution.
 */
#define SCANWIDTH 568
#define SCANHEIGHT 384
#define SCANSTEP SCANWIDTH-12
#else /* !_640x400 */
#define SCANWIDTH 320
#define SCANHEIGHT 200
#define SCANSTEP SCANWIDTH-6
#endif /* !_640x400 */

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


#ifndef __ASSEMBLER__
/* ----------------------------------
    generic graphics globals
   ---------------------------------- */

/* Pointers to framebuffer (can be VGA memory or host buffer)
 */

extern unsigned char    *video__fb1,*video__fb2;

#ifdef _640x400
extern unsigned char video__wider_hires_even[0x1000];
extern unsigned char video__wider_hires_odd[0x1000];
#endif /* _640x400 */

extern unsigned char video__hires_even[0x800];
extern unsigned char video__hires_odd[0x800];

extern unsigned char video__dhires1[256];
extern unsigned char video__dhires2[256];

extern int video__current_page; /* Current visual page */

extern int video__strictcolors;

/* --- Precalculated hi-res page offsets given addr --- */
extern unsigned int video__screen_addresses[8192];
extern unsigned char video__columns[8192];

extern unsigned char video__odd_colors[2];
extern unsigned char video__even_colors[2];

/* Hooks */

void            video__write_text0(),
video__write_text0_mixed(),
video__write_text1(),
video__write_text1_mixed(),
video__write_even0(),
video__write_odd0(),
video__write_even0_mixed(),
video__write_odd0_mixed(),
video__write_even1(),
video__write_odd1(),
video__write_even1_mixed(),
video__write_odd1_mixed();

void            video__write_2e_text0(),
video__write_2e_text0_mixed(),
video__write_2e_text1(),
video__write_2e_text1_mixed(),
video__write_2e_odd0(),
video__write_2e_even0(),
video__write_2e_odd0_mixed(),
video__write_2e_even0_mixed(),
video__write_2e_odd1(),
video__write_2e_even1(),
video__write_2e_odd1_mixed(),
video__write_2e_even1_mixed();

#endif /* !__ASSEMBLER__ */

#endif /* !A2_VIDEO_H */
