/*
 * Apple // emulator for Linux: Exported menu routines
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

#ifndef A2_INTERFACE_H
#define A2_INTERFACE_H

#define INTERFACE_SCREEN_X 80

typedef enum interface_colorscheme_t {
    GREEN_ON_BLACK = 0,
    GREEN_ON_BLUE,
    RED_ON_BLACK,
} interface_colorscheme_t;

#ifdef INTERFACE_CLASSIC
void video_plotchar(int col, int row, interface_colorscheme_t cs, uint8_t c);
void c_interface_begin(int current_key);
void c_interface_print(int x, int y, const interface_colorscheme_t cs, const char *s);
void c_interface_print_submenu_centered(char *submenu, int xlen, int ylen);
void c_interface_keyboard_layout();
void c_interface_parameters();
void c_interface_credits();
void c_interface_exit(int ch);
void c_interface_translate_screen(char screen[24][INTERFACE_SCREEN_X+1]);
void c_interface_select_diskette(int);
#endif

// Plot character at pixel buffer offset
void interface_plotChar(uint8_t *fboff, int fb_pix_width, interface_colorscheme_t cs, uint8_t c);

// Plot message into pixel buffer
void interface_plotMessage(uint8_t *fb, interface_colorscheme_t cs, char *message, int message_cols, int message_rows);

#if INTERFACE_TOUCH
typedef enum interface_touch_event_t {
    TOUCH_CANCEL = 0,
    TOUCH_DOWN,
    TOUCH_MOVE,
    TOUCH_UP,
    TOUCH_POINTER_DOWN,
    TOUCH_POINTER_UP,
} interface_touch_event_t;

typedef enum interface_touch_event_flags {
    TOUCH_FLAGS_HANDLED             = (1<<0),
    TOUCH_FLAGS_REQUEST_HOST_MENU   = (1<<1),
    TOUCH_FLAGS_KEY_TAP             = (1<<4),
    TOUCH_FLAGS_KBD                 = (1<<5),
    TOUCH_FLAGS_JOY                 = (1<<6),
    TOUCH_FLAGS_MENU                = (1<<7),
    TOUCH_FLAGS_INPUT_DEVICE_CHANGE = (1<<16),
    TOUCH_FLAGS_CPU_SPEED_DEC       = (1<<17),
    TOUCH_FLAGS_CPU_SPEED_INC       = (1<<18),
#define TOUCH_FLAGS_ASCII_AND_SCANCODE_SHIFT 32
#define TOUCH_FLAGS_ASCII_AND_SCANCODE_MASK  (0xFFFFLL << TOUCH_FLAGS_ASCII_AND_SCANCODE_SHIFT)
} interface_touch_event_flags;

// handle touch event
extern int64_t (*interface_onTouchEvent)(interface_touch_event_t action, int pointer_count, int pointer_idx, float *x_coords, float *y_coords);

// is the touch menu module itself available?
extern bool (*interface_isTouchMenuAvailable)(void);

// enable/disable touch menu HUD element
extern void (*interface_setTouchMenuEnabled)(bool enabled);

// set minimum alpha visibility of touch menu HUD element
extern void (*interface_setTouchMenuVisibility)(float alpha);
#endif


#endif
