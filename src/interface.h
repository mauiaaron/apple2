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

#ifndef A2_INTERFACE_H
#define A2_INTERFACE_H

#define INTERFACE_SCREEN_X 80

typedef enum interface_colorscheme_t {
    GREEN_ON_BLACK = 0,
    GREEN_ON_BLUE,
    RED_ON_BLACK,
} interface_colorscheme_t;

#if INTERFACE_CLASSIC
void c_interface_begin(int current_key);
void c_interface_print(int x, int y, const interface_colorscheme_t cs, const char *s);
void c_interface_print_submenu_centered(char *submenu, int xlen, int ylen);
void c_interface_keyboard_layout();
void c_interface_parameters();
void c_interface_credits();
void c_interface_exit(int ch);
void c_interface_translate_screen(char screen[24][INTERFACE_SCREEN_X+1]);
void c_interface_select_diskette(int);
bool interface_isShowing(void);
void interface_setStagingFramebuffer(uint8_t *stagingFB);
#endif

// Plot message into pixel buffer
void interface_plotMessage(uint8_t *fb, const interface_colorscheme_t cs, char *message, const uint8_t message_cols, const uint8_t message_rows);

#if INTERFACE_TOUCH
typedef enum interface_device_t {
    TOUCH_DEVICE_NONE = 0,
    TOUCH_DEVICE_FRAMEBUFFER = 0,
    TOUCH_DEVICE_JOYSTICK,
    TOUCH_DEVICE_JOYSTICK_KEYPAD,
    TOUCH_DEVICE_KEYBOARD,
    TOUCH_DEVICE_TOPMENU,
    TOUCH_DEVICE_ALERT,
    // ...
    TOUCH_DEVICE_DEVICE_MAX,
} interface_device_t;

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
    TOUCH_FLAGS_JOY_KPAD            = (1<<8),
    TOUCH_FLAGS_INPUT_DEVICE_CHANGE = (1<<16),
    TOUCH_FLAGS_CPU_SPEED_DEC       = (1<<17),
    TOUCH_FLAGS_CPU_SPEED_INC       = (1<<18),
#define TOUCH_FLAGS_ASCII_AND_SCANCODE_SHIFT 32
#define TOUCH_FLAGS_ASCII_AND_SCANCODE_MASK  (0xFFFFLL << TOUCH_FLAGS_ASCII_AND_SCANCODE_SHIFT)
} interface_touch_event_flags;

// handle touch event
extern int64_t (*interface_onTouchEvent)(interface_touch_event_t action, int pointer_count, int pointer_idx, float *x_coords, float *y_coords);
#endif // INTERFACE_TOUCH

#define EXT_GZ  ".gz"
#define _GZLEN (sizeof(EXT_GZ)-1)

// ----------------------------------------------------------------------------
// file extension handling

static inline bool is_gz(const char * const name) {
    size_t len = strlen(name);
    if (len <= _GZLEN) {
        return false;
    }
    return strncmp(name+len-_GZLEN, EXT_GZ, _GZLEN) == 0;
}

static inline void cut_gz(char *name) {
    size_t len = strlen(name);
    if (len <= _GZLEN) {
        return;
    }
    *(name+len-_GZLEN) = '\0';
}

#endif
