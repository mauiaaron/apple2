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

#include "common.h"

/* from misc.c */
extern uid_t user, privileged;

/* parameters for generic and keyboard-simulated joysticks */
extern short joy_x;
extern short joy_y;
extern unsigned char joy_button0;
extern unsigned char joy_button1;

/* mutex used to synchronize between cpu and main threads */
pthread_mutex_t interface_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t ui_thread_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t cpu_thread_cond = PTHREAD_COND_INITIALIZER;

#ifdef LINUX_JOYSTICK
#include <linux/joystick.h>
extern int raw_js_x;
extern int raw_js_y;
#endif

static int next_key = -1;
static int last_scancode = -1;
bool caps_lock = false;              /* is enabled */
static bool in_interface = false;

/* ----------------------------------------------------
    //e Keymap. Mapping scancodes to Apple //e US Keyboard
   ---------------------------------------------------- */
#define MAP_SIZE 128
static int apple_iie_keymap_plain[MAP_SIZE] =
{ -1, kESC, '1', '2', '3', '4', '5', '6',       /* 00-07   */
  '7', '8', '9', '0', '-', '=',  kLT,  kTAB,    /* 08-15   */
  'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',       /* 16-23   */
  'o', 'p', '[', ']', kRET, -1, 'a', 's',       /* 24-31   */
  'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',       /* 32-39   */
  '\'', '`', -1,'\\', 'z', 'x', 'c', 'v',       /* 40-47   */
  'b', 'n', 'm', ',', '.', '/', -1, -1,         /* 48-55   */
  JB0, ' ', -1, kF1, kF2, kF3, kF4, kF5,        /* 56-63   */
  kF6, kF7, kF8, kF9, kF10, kF11, kF12, JUL,    /* 64-71   */
  J_U, JUR, -1, J_L, J_C, J_R, -1, JDL,         /* 72-79   */
  J_D, JDR, -1, -1, -1, kF11, kF12, -1,         /* 80-87   */
  -1, -1, -1, -1, -1, -1, -1, -1,               /* 88-95   */
  -1, -1, -1, -1, JB1, -1, kHOME, kUP,          /* 96-103  */
  kPGUP, kLT, kRT, kEND, kDN, kPGDN, JB2, kDEL, /* 104-111 */
  -1, -1, -1, -1, -1, -1, -1, kPAUSE,           /* 112-119 */
  -1, -1, -1, -1, -1, -1, -1, -1 };             /* 120-127 */

// TODO FIXME : remove magic constants and verify ctrl-keys
static int apple_iie_keymap_ctrl[MAP_SIZE] =
{ -1, kESC, '1', '2', '3', '4', '5', '6',       /* 00-07   */
  '7', '8', '9', '0', '-', '=',  kLT,  kTAB,    /* 08-15   */
  17, 23, 5, 18, 20, 25, kRT,  kTAB,            /* 16-23   */
  15, 16, kESC, 29, kRET, -1, 1, 19,            /* 24-31   */
  4, 6, 7, kLT, kDN, kUP, 12, ';',              /* 32-39   */
  '\'', '`', -1,'\\', 26, 24, 3, 22,            /* 40-47   */
  2, 14, kRET, ',', '.', '/', -1, -1,           /* 48-55   */
  JB0, ' ', -1, kF1, kF2, kF3, kF4, kF5,        /* 56-63   */
  kF6, kF7, kF8, kF9, kF10, kF11, kF12, JUL,    /* 64-71   */
  J_U, JUR, -1, J_L, J_C, J_R, -1, JDL,         /* 72-79   */
  J_D, JDR, -1, -1, -1, kF11, kF12, -1,         /* 80-87   */
  -1, -1, -1, -1, -1, -1, -1, -1,               /* 88-95   */
  -1, -1, -1, -1, JB1, -1, kHOME, kUP,          /* 96-103  */
  kPGUP, kLT, kRT, kEND, kDN, kPGDN, JB2, kDEL, /* 104-111 */
  -1, -1, -1, -1, -1, -1, -1, kPAUSE,           /* 112-119 */
  -1, -1, -1, -1, -1, -1, -1, -1 };             /* 120-127 */

static int apple_iie_keymap_shifted[MAP_SIZE] =
{ -1, kESC, '!', '@', '#', '$', '%', '^',       /* 00-07   */
  '&', '*', '(', ')', '_', '+',  kLT,  kTAB,    /* 08-15   */
  'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',       /* 16-23   */
  'O', 'P', '{', '}', kRET, -1, 'A', 'S',       /* 24-31   */
  'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',       /* 32-39   */
  '"', '~', -1, '|', 'Z', 'X', 'C', 'V',        /* 40-47   */
  'B', 'N', 'M', '<', '>', '?', -1, -1,         /* 48-55   */
  JB0, ' ', -1, kF1, kF2, kF3, kF4, kF5,        /* 56-63   */
  kF6, kF7, kF8, kF9, kF10, kF11, kF12, JUL,    /* 64-71   */
  J_U, JUR, -1, J_L, J_C, J_R, -1, JDL,         /* 72-79   */
  J_D, JDR, -1, -1, -1, kF11, kF12, -1,         /* 80-87   */
  -1, -1, -1, -1, -1, -1, -1, -1,               /* 88-95   */
  -1, -1, -1, -1, JB1, -1, kHOME, kUP,          /* 96-103  */
  kPGUP, kLT, kRT, kEND, kDN, kPGDN, JB2, kDEL, /* 104-111 */
  -1, -1, -1, -1, -1, -1, -1, kPAUSE,           /* 112-119 */
  -1, -1, -1, -1, -1, -1, -1, -1 };             /* 120-127 */

static int apple_iie_keymap_caps[MAP_SIZE] =
{ -1, kESC, '1', '2', '3', '4', '5', '6',       /* 00-07   */
  '7', '8', '9', '0', '-', '=',  kLT,  kTAB,    /* 08-15   */
  'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',       /* 16-23   */
  'O', 'P', '[', ']', kRET, -1, 'A', 'S',       /* 24-31   */
  'D', 'F', 'G', 'H', 'J', 'K', 'L', ';',       /* 32-39   */
  '\'', '`', -1,'\\', 'Z', 'X', 'C', 'V',       /* 40-47   */
  'B', 'N', 'M', ',', '.', '/', -1, -1,         /* 48-55   */
  JB0, ' ', -1, kF1, kF2, kF3, kF4, kF5,        /* 56-63   */
  kF6, kF7, kF8, kF9, kF10, kF11, kF12, JUL,    /* 64-71   */
  J_U, JUR, -1, J_L, J_C, J_R, -1, JDL,         /* 72-79   */
  J_D, JDR, -1, -1, -1, kF11, kF12, -1,         /* 80-87   */
  -1, -1, -1, -1, -1, -1, -1, -1,               /* 88-95   */
  -1, -1, -1, -1, JB1, -1, kHOME, kUP,          /* 96-103  */
  kPGUP, kLT, kRT, kEND, kDN, kPGDN, JB2, kDEL, /* 104-111 */
  -1, -1, -1, -1, -1, -1, -1, kPAUSE,           /* 112-119 */
  -1, -1, -1, -1, -1, -1, -1, -1 };             /* 120-127 */

// TODO FIXME : remove magic constants and verify ctrl-keys
static int apple_iie_keymap_shift_ctrl[MAP_SIZE] =
{ -1, kESC, '1',  0, '3', '4', '5', 30,         /* 00-07   */
  '7', '8', '9', '0', 31, '=',  kLT,  kTAB,     /* 08-15   */
  17, 23, 5, 18, 20, 25, kRT,  kTAB,            /* 16-23   */
  15, 16, kESC, 29, kRET, -1, 1, 19,            /* 24-31   */
  4, 6, 7, kLT, kDN, kUP, 12, ';',              /* 32-39   */
  '\'', '`', 28, -1, 26, 24, 3, 22,             /* 40-47   */
  2, 14, kRET, ',', '.', '/', -1, -1,           /* 48-55   */
  JB0, ' ', -1, kF1, kF2, kF3, kF4, kF5,        /* 56-63   */
  kF6, kF7, kF8, kF9, kF10, kF11, kF12, JUL,    /* 64-71   */
  J_U, JUR, -1, J_L, J_C, J_R, -1, JDL,         /* 72-79   */
  J_D, JDR, -1, -1, -1, kF11, kF12, -1,         /* 80-87   */
  -1, -1, -1, -1, -1, -1, -1, -1,               /* 88-95   */
  -1, -1, -1, -1, JB1, -1, kHOME, kUP,          /* 96-103  */
  kPGUP, kLT, kRT, kEND, kDN, kPGDN, JB2, kDEL, /* 104-111 */
  -1, -1, -1, -1, -1, -1, -1, kPAUSE,           /* 112-119 */
  -1, -1, -1, -1, -1, -1, -1, -1 };             /* 120-127 */

static char key_pressed[ 256 ] = { 0 };

/* -------------------------------------------------------------------------
    void c_handle_input() : Handle input : keys and joystick.
   ------------------------------------------------------------------------- */
void c_handle_input(int scancode, int pressed)
{
    int *keymap = NULL;

    assert(scancode < 0x80);
    if (scancode >= 0)
    {
        last_scancode = scancode;

        if ((key_pressed[ SCODE_L_SHIFT ] || key_pressed[ SCODE_R_SHIFT ]) &&
            (key_pressed[ SCODE_L_CTRL ] || key_pressed[ SCODE_R_CTRL ]))
        {
            keymap = apple_iie_keymap_shift_ctrl;
        }
        else if (key_pressed[ SCODE_L_CTRL ] || key_pressed[ SCODE_R_CTRL ])
        {
            keymap = apple_iie_keymap_ctrl;
        }
        else if (key_pressed[ SCODE_L_SHIFT ] || key_pressed[ SCODE_R_SHIFT ])
        {
            keymap = apple_iie_keymap_shifted;
        }
        else if (caps_lock)
        {
            keymap = apple_iie_keymap_caps;
        }
        else
        {
            keymap = apple_iie_keymap_plain;
        }

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
            default:
                next_key = keymap[scancode];
                break;
            }
        }
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
            default:
                break;
            }
        }
    }

    // key input consumption

    if ((next_key >= 0) && !in_interface)
    {
        do {
            int current_key = next_key;
            next_key = -1;

            if (current_key < 128)
            {
                apple_ii_64k[0][0xC000] = current_key | 0x80;
                apple_ii_64k[1][0xC000] = current_key | 0x80;
                break;
            }

            if (current_key == kF9)
            {
                timing_toggle_cpu_speed();
                break;
            }

            if (current_key == kEND)
            {
                if (key_pressed[ SCODE_L_CTRL ] || key_pressed[ SCODE_R_CTRL ])
                {
                    cpu65_interrupt(ResetSig);
                }
                break;
            }

            if ( !(c_keys_is_interface_key(current_key) || (current_key == kPAUSE)) )
            {
                break;
            }

            pthread_mutex_lock(&interface_mutex);
#ifdef AUDIO_ENABLED
            SoundSystemPause();
#endif
            in_interface = true;

            switch (current_key)
            {
#ifdef INTERFACE_CLASSIC
            case kF1:
                c_interface_select_diskette( 0 );
                break;

            case kF2:
                c_interface_select_diskette( 1 );
                break;
#endif

            case kPAUSE:
                while (c_mygetch(1) == -1)
                {
                    struct timespec ts = { .tv_sec=0, .tv_nsec=1 };
                    nanosleep(&ts, NULL);
                }
                break;

#ifdef INTERFACE_CLASSIC
            case kF5:
                c_interface_keyboard_layout();
                break;

#ifdef DEBUGGER
            case kF7:
                c_interface_debugging();
                break;
#endif

            case kF8:
                c_interface_credits();
                break;

            case kF10:
                c_interface_parameters();
                break;
#endif

            default:
                break;
            }

#ifdef AUDIO_ENABLED
            SoundSystemUnpause();
#endif
            c_joystick_reset();
            pthread_mutex_unlock(&interface_mutex);
            in_interface = false;

        } while(0);
    }

    // joystick processing

    if (joy_mode == JOY_OFF)
    {
        joy_x = joy_y = 0xFF;
    }
#if defined(KEYPAD_JOYSTICK)
    // Keypad emulated joystick relies on "raw" keyboard input
    else if (joy_mode == JOY_KPAD)
    {
        bool joy_axis_unpressed = !( key_pressed[SCODE_KPAD_U]  || key_pressed[SCODE_KPAD_D]  || key_pressed[SCODE_KPAD_L]  || key_pressed[SCODE_KPAD_R] ||
                                     key_pressed[SCODE_KPAD_UL] || key_pressed[SCODE_KPAD_DL] || key_pressed[SCODE_KPAD_UR] || key_pressed[SCODE_KPAD_DR] ||
                                     // and allow regular PC arrow keys to manipulate joystick...
                                     key_pressed[SCODE_U]       || key_pressed[SCODE_D]       || key_pressed[SCODE_L]       || key_pressed[SCODE_R]);

        if (key_pressed[ SCODE_KPAD_C ] || (auto_recenter && joy_axis_unpressed))
        {
            joy_x = HALF_JOY_RANGE;
            joy_y = HALF_JOY_RANGE;
        }

        if (key_pressed[ SCODE_KPAD_UL ] || key_pressed[ SCODE_KPAD_U ] || key_pressed[ SCODE_KPAD_UR ] ||/* regular arrow up */key_pressed[ SCODE_U ])
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

        if (key_pressed[ SCODE_KPAD_DL ] || key_pressed[ SCODE_KPAD_D ] || key_pressed[ SCODE_KPAD_DR ] ||/* regular arrow dn */key_pressed[ SCODE_D ])
        {
            if (joy_y < JOY_RANGE - joy_step)
            {
                joy_y += joy_step;
            }
            else
            {
                joy_y = JOY_RANGE-1;
            }
        }

        if (key_pressed[ SCODE_KPAD_UL ] || key_pressed[ SCODE_KPAD_L ] || key_pressed[ SCODE_KPAD_DL ] ||/* regular arrow l */key_pressed[ SCODE_L ])
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

        if (key_pressed[ SCODE_KPAD_UR ] || key_pressed[ SCODE_KPAD_R ] || key_pressed[ SCODE_KPAD_DR ] ||/* regular arrow r */key_pressed[ SCODE_R ])
        {
            if (joy_x < JOY_RANGE - joy_step)
            {
                joy_x += joy_step;
            }
            else
            {
                joy_x = JOY_RANGE-1;
            }
        }
    }
#endif

#if defined(LINUX_JOYSTICK)
    else if ((joy_mode == JOY_PCJOY) && !(js_fd < 0))
    {
        if (read(js_fd, &js, JS_RETURN) == -1)
        {
            // error
        }

        raw_js_x = js.x;
        raw_js_y = js.y;

        int x_val = (js.x < js_center_x)
                ? (js.x - js_offset_x) * js_adjustlow_x
                : (js.x - js_center_x) * js_adjusthigh_x + HALF_JOY_RANGE;

        int y_val = (js.y < js_center_y)
                ? (js.y - js_offset_y) * js_adjustlow_y
                : (js.y - js_center_y) * js_adjusthigh_y + HALF_JOY_RANGE;

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
    }
#endif
}

int c_rawkey()
{
    return last_scancode;
}

int c_mygetch(int block)
{
    int retval;

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

    retval = next_key;
    next_key = -1;

    return retval;
}

void c_keys_set_key(int key)
{
    next_key = key;
}

bool c_keys_is_interface_key(int key)
{
    switch (key)
    {
        case kF1:
        case kF2:
        case kF5:
#ifdef DEBUGGER
        case kF7:
#endif
        case kF8:
        case kF10:
            return true;
        default:
            break;
    }
    return false;
}

