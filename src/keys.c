/*
 * Apple // emulator for Linux: Keyboard handler
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

#ifdef __linux__
#include <linux/keyboard.h>
#endif

#include "common.h"
#include "keys.h"
#include "misc.h"
#include "video.h"
#include "interface.h"
#include "cpu.h"
#include "prefs.h"
#include "timing.h"
#include "soundcore.h"

/* from misc.c */
extern uid_t user, privileged;

/* from debugger.c */
extern void c_do_debugging();

/* parameters for generic and keyboard-simulated joysticks */
short joy_x = 127;
short joy_y = 127;
unsigned char joy_button0 = 0;
unsigned char joy_button1 = 0;
unsigned char joy_button2 = 0;

/* mutex used to synchronize between cpu and main threads */
pthread_mutex_t interface_mutex = PTHREAD_MUTEX_INITIALIZER;

#ifdef PC_JOYSTICK
#include <linux/joystick.h>
int x_val, y_val;
#endif

#define SCODE_BS 14

static int next_key = -1;
static int last_scancode = -1;
static char caps_lock = 1;              /* is enabled */
static int in_mygetch = 0;

/* ----------------------------------------------------
    Keymap. Mapping scancodes to Apple II+ US Keyboard
   ---------------------------------------------------- */
static int apple_ii_keymap_plain[128] =
{ -1, 27, '1', '2', '3', '4', '5', '6',         /* 00-07   */
  '7', '8', '9', '0', ':', '-',  8, 27,         /* 08-15   */
  'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',       /* 16-23   */
  'O', 'P', -1, 8, 13, -1, 'A', 'S',            /* 24-31   */
  'D', 'F', 'G', 'H', 'J', 'K', 'L', ';',       /* 32-39   */
  8, -1, -1, 21, 'Z', 'X', 'C', 'V',            /* 40-47   */
  'B', 'N', 'M', ',', '.', '/', -1, -1,         /* 48-55   */
  JB0, ' ', -1, kF1, kF2, kF3, kF4, kF5,        /* 56-63   */
  kF6, kF7, kF8, kF9, kF10, -1, -1, JUL,        /* 64-71   */
  J_U, JUR, S_D, J_L, J_C, J_R, S_I, JDL,       /* 72-79   */
  J_D, JDR, -1, -1, -1, kF11, kF12, -1,         /* 80-87   */
  -1, -1, -1, -1, -1, -1, -1, -1,               /* 88-95   */
  -1, -1, -1, -1, JB1, -1, kHOME, -1,           /* 96-103  */
  kPGUP, 8, 21, kEND, -1, kPGDN, JB2, -1,       /* 104-111 */
  -1, -1, -1, -1, -1, -1, -1, kPAUSE,           /* 112-119 */
  -1, -1, -1, -1, -1, -1, -1, -1 };             /* 120-127 */

static int apple_ii_keymap_ctrl[128] =
{ -1, 027, '1', '2', '3', '4', '5', '6',        /* 00-07   */
  '7', '8', '9', '0', ':', '-',  8, 27,         /* 08-15   */
  17, 23, 5, 18, 20, 25, 21, 9,                 /* 16-23   */
  15, 16, -1, 8, 13, -1, 1, 19,                 /* 24-31   */
  4, 6, 7, 8, 10, 11, 12, ';',                  /* 32-39   */
  8, -1, -1, 21, 26, 24, 3, 22,                 /* 40-47   */
  2, 14, 13, ',', '.', '/', -1, -1,             /* 48-55   */
  JB0, ' ', -1, kF1, kF2, kF3, kF4, kF5,        /* 56-63   */
  kF6, kF7, kF8, kF9, kF10, -1, -1, JUL,        /* 64-71   */
  J_U, JUR, S_D, J_L, J_C, J_R, S_I, JDL,       /* 72-79   */
  J_D, JDR, -1, -1, -1, kF11, kF12, -1,         /* 80-87   */
  -1, -1, -1, -1, -1, -1, -1, -1,               /* 88-95   */
  -1, -1, -1, -1, JB1, -1, kHOME, -1,           /* 96-103  */
  kPGUP, 8, 21, kEND, -1, kPGDN, JB2, -1,       /* 104-111 */
  -1, -1, -1, -1, -1, -1, -1, kPAUSE,           /* 112-119 */
  -1, -1, -1, -1, -1, -1, -1, -1 };             /* 120-127 */

static int apple_ii_keymap_shifted[128] =
{ -1, 27, '!', '"', '#', '$', '%', '&',         /* 00-07   */
  39, '(', ')', '0', '*', '=',  8, 27,          /* 08-15   */
  'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',       /* 16-23   */
  'O', '@', -1, 8, 13, -1, 'A', 'S',            /* 24-31   */
  'D', 'F', 'G', 'H', 'J', 'K', 'L', '+',       /* 32-39   */
  8, -1, -1, 21, 'Z', 'X', 'C', 'V',            /* 40-47   */
  'B', '^', 'M', '<', '>', '?', -1, -1,         /* 48-55   */
  JB0, ' ', -1, kF1, kF2, kF3, kF4, kF5,        /* 56-63   */
  kF6, kF7, kF8, kF9, kF10, -1, -1, JUL,        /* 64-71   */
  J_U, JUR, S_D, J_L, J_C, J_R, S_I, JDL,       /* 72-79   */
  J_D, JDR, -1, -1, -1, kF11, kF12, -1,         /* 80-87   */
  -1, -1, -1, -1, -1, -1, -1, -1,               /* 88-95   */
  -1, -1, -1, -1, JB1, -1, kHOME, -1,           /* 96-103  */
  kPGUP, 8, 21, kEND, -1, kPGDN, JB2, -1,       /* 104-111 */
  -1, -1, -1, -1, -1, -1, -1, kPAUSE,           /* 112-119 */
  -1, -1, -1, -1, -1, -1, -1, -1 };             /* 120-127 */

/* ----------------------------------------------------
    //e Keymap. Mapping scancodes to Apple //e US Keyboard
   ---------------------------------------------------- */
static int apple_iie_keymap_plain[128] =
{ -1, 27, '1', '2', '3', '4', '5', '6',         /* 00-07   */
  '7', '8', '9', '0', '-', '=',  8,  9,         /* 08-15   */
  'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',       /* 16-23   */
  'o', 'p', '[', ']', 13, -1, 'a', 's',         /* 24-31   */
  'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',       /* 32-39   */
  '\'', '`', -1,'\\', 'z', 'x', 'c', 'v',       /* 40-47   */
  'b', 'n', 'm', ',', '.', '/', -1, -1,         /* 48-55   */
  JB0, ' ', -1, kF1, kF2, kF3, kF4, kF5,        /* 56-63   */
  kF6, kF7, kF8, kF9, kF10, -1, -1, JUL,        /* 64-71   */
  J_U, JUR, S_D, J_L, J_C, J_R, S_I, JDL,       /* 72-79   */
  J_D, JDR, -1, -1, -1, kF11, kF12, -1,         /* 80-87   */
  -1, -1, -1, -1, -1, -1, -1, -1,               /* 88-95   */
  -1, -1, -1, -1, JB1, -1, kHOME, 11,           /* 96-103  */
  kPGUP, 8, 21, kEND, 10, kPGDN, JB2, 127,      /* 104-111 */
  -1, -1, -1, -1, -1, -1, -1, kPAUSE,           /* 112-119 */
  -1, -1, -1, -1, -1, -1, -1, -1 };             /* 120-127 */

static int apple_iie_keymap_ctrl[128] =
{ -1, 27, '1', '2', '3', '4', '5', '6',         /* 00-07   */
  '7', '8', '9', '0', '-', '=',  8,  9,         /* 08-15   */
  17, 23, 5, 18, 20, 25, 21,  9,                /* 16-23   */
  15, 16, 27, 29, 13, -1, 1, 19,                /* 24-31   */
  4, 6, 7, 8, 10, 11, 12, ';',                  /* 32-39   */
  '\'', '`', -1,'\\', 26, 24, 3, 22,            /* 40-47   */
  2, 14, 13, ',', '.', '/', -1, -1,             /* 48-55   */
  JB0, ' ', -1, kF1, kF2, kF3, kF4, kF5,        /* 56-63   */
  kF6, kF7, kF8, kF9, kF10, -1, -1, JUL,        /* 64-71   */
  J_U, JUR, S_D, J_L, J_C, J_R, S_I, JDL,       /* 72-79   */
  J_D, JDR, -1, -1, -1, kF11, kF12, -1,         /* 80-87   */
  -1, -1, -1, -1, -1, -1, -1, -1,               /* 88-95   */
  -1, -1, -1, -1, JB1, -1, kHOME, 11,           /* 96-103  */
  kPGUP, 8, 21, kEND, 10, kPGDN, JB2, 127,      /* 104-111 */
  -1, -1, -1, -1, -1, -1, -1, kPAUSE,           /* 112-119 */
  -1, -1, -1, -1, -1, -1, -1, -1 };             /* 120-127 */

static int apple_iie_keymap_shifted[128] =
{ -1, 27, '!', '@', '#', '$', '%', '^',         /* 00-07   */
  '&', '*', '(', ')', '_', '+',  8,  9,         /* 08-15   */
  'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',       /* 16-23   */
  'O', 'P', '{', '}', 13, -1, 'A', 'S',         /* 24-31   */
  'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',       /* 32-39   */
  '"', '~', -1, '|', 'Z', 'X', 'C', 'V',        /* 40-47   */
  'B', 'N', 'M', '<', '>', '?', -1, -1,         /* 48-55   */
  JB0, ' ', -1, kF1, kF2, kF3, kF4, kF5,        /* 56-63   */
  kF6, kF7, kF8, kF9, kF10, -1, -1, JUL,        /* 64-71   */
  J_U, JUR, S_D, J_L, J_C, J_R, S_I, JDL,       /* 72-79   */
  J_D, JDR, -1, -1, -1, kF11, kF12, -1,         /* 80-87   */
  -1, -1, -1, -1, -1, -1, -1, -1,               /* 88-95   */
  -1, -1, -1, -1, JB1, -1, kHOME, 11,           /* 96-103  */
  kPGUP, 8, 21, kEND, 10, kPGDN, JB2, 127,      /* 104-111 */
  -1, -1, -1, -1, -1, -1, -1, kPAUSE,           /* 112-119 */
  -1, -1, -1, -1, -1, -1, -1, -1 };             /* 120-127 */

static int apple_iie_keymap_caps[128] =
{ -1, 27, '1', '2', '3', '4', '5', '6',         /* 00-07   */
  '7', '8', '9', '0', '-', '=',  8,  9,         /* 08-15   */
  'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',       /* 16-23   */
  'O', 'P', '[', ']', 13, -1, 'A', 'S',         /* 24-31   */
  'D', 'F', 'G', 'H', 'J', 'K', 'L', ';',       /* 32-39   */
  '\'', '`', -1,'\\', 'Z', 'X', 'C', 'V',       /* 40-47   */
  'B', 'N', 'M', ',', '.', '/', -1, -1,         /* 48-55   */
  JB0, ' ', -1, kF1, kF2, kF3, kF4, kF5,        /* 56-63   */
  kF6, kF7, kF8, kF9, kF10, -1, -1, JUL,        /* 64-71   */
  J_U, JUR, S_D, J_L, J_C, J_R, S_I, JDL,       /* 72-79   */
  J_D, JDR, -1, -1, -1, kF11, kF12, -1,         /* 80-87   */
  -1, -1, -1, -1, -1, -1, -1, -1,               /* 88-95   */
  -1, -1, -1, -1, JB1, -1, kHOME, 11,           /* 96-103  */
  kPGUP, 8, 21, kEND, 10, kPGDN, JB2, 127,      /* 104-111 */
  -1, -1, -1, -1, -1, -1, -1, kPAUSE,           /* 112-119 */
  -1, -1, -1, -1, -1, -1, -1, -1 };             /* 120-127 */

static int apple_iie_keymap_shift_ctrl[128] =
{ -1, 27, '1',  0, '3', '4', '5', 30,           /* 00-07   */
  '7', '8', '9', '0', 31, '=',  8,  9,          /* 08-15   */
  17, 23, 5, 18, 20, 25, 21,  9,                /* 16-23   */
  15, 16, 27, 29, 13, -1, 1, 19,                /* 24-31   */
  4, 6, 7, 8, 10, 11, 12, ';',                  /* 32-39   */
  '\'', '`', 28, -1, 26, 24, 3, 22,             /* 40-47   */
  2, 14, 13, ',', '.', '/', -1, -1,             /* 48-55   */
  JB0, ' ', -1, kF1, kF2, kF3, kF4, kF5,        /* 56-63   */
  kF6, kF7, kF8, kF9, kF10, -1, -1, JUL,        /* 64-71   */
  J_U, JUR, S_D, J_L, J_C, J_R, S_I, JDL,       /* 72-79   */
  J_D, JDR, -1, -1, -1, kF11, kF12, -1,         /* 80-87   */
  -1, -1, -1, -1, -1, -1, -1, -1,               /* 88-95   */
  -1, -1, -1, -1, JB1, -1, kHOME, 11,           /* 96-103  */
  kPGUP, 8, 21, kEND, 10, kPGDN, JB2, 127,      /* 104-111 */
  -1, -1, -1, -1, -1, -1, -1, kPAUSE,           /* 112-119 */
  -1, -1, -1, -1, -1, -1, -1, -1 };             /* 120-127 */

static char key_pressed[ 256 ];


/* -------------------------------------------------------------------------
 * This routine is called periodically to update the state of the emulator.
 *      -handles switching to menus
 *      -polls PC Joystick
 *      -update palette for flashing text
 * ------------------------------------------------------------------------- */
void c_periodic_update(int dummysig) {
    int current_key;

    video_sync(0);

    if (next_key >= 0)
    {
        current_key = next_key;
        next_key = -1;

        if (current_key < 128)
        {
            apple_ii_64k[0][0xC000] = current_key | 0x80;
            apple_ii_64k[1][0xC000] = current_key | 0x80;
        }
        else
        {
            switch (current_key)
            {
            case kEND:
                if (key_pressed[ SCODE_L_CTRL ] || key_pressed[ SCODE_R_CTRL ])
                {
                    cpu65_interrupt(ResetSig);
                }
                break;

            case J_C:
                joy_x = joy_center_x;
                joy_y = joy_center_y;
                break;

            case kF1:
                pthread_mutex_lock(&interface_mutex);
                SoundSystemPause();
                c_interface_select_diskette( 0 );
                SoundSystemUnpause();
                pthread_mutex_unlock(&interface_mutex);
                break;

            case kF2:
                pthread_mutex_lock(&interface_mutex);
                SoundSystemPause();
                c_interface_select_diskette( 1 );
                SoundSystemUnpause();
                pthread_mutex_unlock(&interface_mutex);
                break;

            case kPAUSE:
                pthread_mutex_lock(&interface_mutex);
                SoundSystemPause();
                while (c_mygetch(1) == -1)
                {
                    struct timespec ts = { .tv_sec=0, .tv_nsec=1 };
                    nanosleep(&ts, NULL);
                }
                SoundSystemUnpause();
                pthread_mutex_unlock(&interface_mutex);
                break;

            case kF5:
                pthread_mutex_lock(&interface_mutex);
                SoundSystemPause();
                c_interface_keyboard_layout();
                SoundSystemUnpause();
                pthread_mutex_unlock(&interface_mutex);
                break;

            case kF7:
                pthread_mutex_lock(&interface_mutex);
                SoundSystemPause();
                c_do_debugging();
                SoundSystemUnpause();
                pthread_mutex_unlock(&interface_mutex);
                break;
#if 0
            case kF8:
                c_interface_words();
                break;
#endif
            case kF9:
                pthread_mutex_lock(&interface_mutex);
                timing_toggle_cpu_speed();
                pthread_mutex_unlock(&interface_mutex);
                break;

            case kF10:
                pthread_mutex_lock(&interface_mutex);
                SoundSystemPause();
                c_interface_parameters();
                SoundSystemUnpause();
                pthread_mutex_unlock(&interface_mutex);
                break;
            }
        }
    }

    /* simulated joystick */
    if (joy_mode == JOY_KYBD)
    {
        if (key_pressed[ SCODE_J_U ])
        {
            if (joy_y > joy_step)
            {
                joy_y -= joy_step;
            }
            else
            {
                joy_y = 0;
            }
        }

        if (key_pressed[ SCODE_J_D ])
        {
            if (joy_y < joy_range - joy_step)
            {
                joy_y += joy_step;
            }
            else
            {
                joy_y = joy_range-1;
            }
        }

        if (key_pressed[ SCODE_J_L ])
        {
            if (joy_x > joy_step)
            {
                joy_x -= joy_step;
            }
            else
            {
                joy_x = 0;
            }
        }

        if (key_pressed[ SCODE_J_R ])
        {
            if (joy_x < joy_range - joy_step)
            {
                joy_x += joy_step;
            }
            else
            {
                joy_x = joy_range-1;
            }
        }
    }

#ifdef PC_JOYSTICK
    else if ((joy_mode == JOY_PCJOY) && !(js_fd < 0))
    {
        if (read(js_fd, &js, JS_RETURN) == -1)
        {
            // error
        }

        x_val = (js.x < js_center_x)
                ? (js.x - js_offset_x) * js_adjustlow_x
                : (js.x - js_center_x) * js_adjusthigh_x + half_joy_range;

        y_val = (js.y < js_center_y)
                ? (js.y - js_offset_y) * js_adjustlow_y
                : (js.y - js_center_y) * js_adjusthigh_y + half_joy_range;

        joy_y = (y_val > 0xff) ? 0xff : (y_val < 0) ? 0 : y_val;
        joy_x = (x_val > 0xff) ? 0xff : (x_val < 0) ? 0 : x_val;

/*      almost_x = (x_val > 0xff) ? 0xff : (x_val < 0) ? 0 : x_val; */
/*      adj_x = (3-(joy_y/0x40)) + 10; */
/*      turnpt_x = joy_y + adj_x; */
/*      almost_x = (almost_x < turnpt_x) */
/*          ? almost_x */
/*          : (almost_x - turnpt_x) * adjusthigh_x; */

/*      joy_x = (almost_x > 0xff) ? 0xff : (almost_x < 0) ? 0 : almost_x; */

        /* sample buttons only if apple keys aren't pressed. keys get set to
         * 0xff, and js buttons are set to 0x80. */
        if (!(joy_button0 & 0x7f))
        {
            joy_button0 = (js.buttons & 0x01) ? 0x80 : 0x0;
        }

        if (!(joy_button1 & 0x7f))
        {
            joy_button1 = (js.buttons & 0x02) ? 0x80 : 0x0;
        }

        if (!(joy_button2 & 0x7f))
        {
            joy_button2 = (js.buttons & 0x03) ? 0x80 : 0x0;
        }
    }
#endif
    else if (joy_mode == JOY_OFF)
    {
        joy_x = joy_y = 256;
    }
}

/* -------------------------------------------------------------------------
    void c_read_raw_key() : handle a scancode
   ------------------------------------------------------------------------- */
void c_read_raw_key(int scancode, int pressed) {
    int *keymap = NULL;

    last_scancode = scancode;

    /* determine which key mapping to use */
    if (apple_mode == IIE_MODE || in_mygetch)
    {
        /* set/reset caps lock */
        if (key_pressed[ SCODE_CAPS ])
        {
            caps_lock = !caps_lock;
        }

        if ((key_pressed[ SCODE_L_SHIFT ] || key_pressed[ SCODE_R_SHIFT ]) && /* shift-ctrl */
            (key_pressed[ SCODE_L_CTRL ] || key_pressed[ SCODE_R_CTRL ]))
        {
            keymap = apple_iie_keymap_shift_ctrl;
        }
        else if (key_pressed[ SCODE_L_CTRL ] || key_pressed[ SCODE_R_CTRL ]) /* ctrl */
        {
            keymap = apple_iie_keymap_ctrl;
        }
        else if (key_pressed[ SCODE_L_SHIFT ] || key_pressed[ SCODE_R_SHIFT ]) /* shift */
        {
            keymap = apple_iie_keymap_shifted;
        }
        else if (caps_lock)
        {
            keymap = apple_iie_keymap_caps;
        }
        else /* plain */
        {
            keymap = apple_iie_keymap_plain;
        }
    }
    else
    {
        if (key_pressed[ SCODE_L_CTRL ] || key_pressed[ SCODE_R_CTRL ])
        {
            keymap = apple_ii_keymap_ctrl;
        }
        else if (key_pressed[ SCODE_L_SHIFT ] || key_pressed[ SCODE_R_SHIFT ])
        {
            keymap = apple_ii_keymap_shifted;
        }
        else
        {
            keymap = apple_ii_keymap_plain;
        }
    }

    /* key is pressed */
    if (pressed)
    {
        key_pressed[ scancode ] = 1;

        switch (keymap[ scancode ])
        {
        case JB0:
            joy_button0 = 0xff; /* open apple */
            break;
        case JB1:
            joy_button1 = 0xff; /* closed apple */
            break;
        case JB2:
            joy_button2 = 0xff; /* unused? */
            break;
        default:
            next_key = keymap[scancode];
            break;
        }
    }
    /* key is released */
    else
    {
        key_pressed[ scancode ] = 0;
        switch (keymap[ scancode ])
        {
        case JB0:
            joy_button0 = 0x00;
            break;
        case JB1:
            joy_button1 = 0x00;
            break;
        case JB2:
            joy_button2 = 0x00;
            break;
        }
    }
}

bool is_backspace()
{
    return (last_scancode == SCODE_BS);
}

int c_mygetch(int block)
{
    int retval;

    in_mygetch = 1;

    if (block)
    {
        while (next_key == -1)
        {
            static struct timespec ts = { .tv_sec=0, .tv_nsec=33333333 };
            nanosleep(&ts, NULL); // 30Hz framerate
            video_sync(1);
        }
    }
    else
    {
        video_sync(0);
    }

    in_mygetch = 0;

    retval = next_key;
    next_key = -1;

    return retval;
}
