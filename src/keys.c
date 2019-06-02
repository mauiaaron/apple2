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

#include "common.h"

static int next_key = -1;
static int last_scancode = -1;

bool caps_lock = true; // default enabled because so much breaks otherwise
bool use_system_caps_lock = false;

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
    convert ascii character to scancode
   ------------------------------------------------------------------------- */

static int scode_map[MAP_SIZE] =
{ -1, -1, -1, -1, -1, -1, -1, -1,                                               /* 00-07   */
  SCODE_BS, SCODE_TAB, SCODE_RET, -1, -1, SCODE_RET, -1, -1,                    /* 08-15   */
  -1, -1, -1, -1, -1, -1, -1, -1,                                               /* 16-23   */
  -1, -1, -1, SCODE_ESC, -1, -1, -1, -1,                                        /* 24-31   */
  /* */57, /*!*/2 , /*"*/40, /*#*/4 , /*$*/5 , /*%*/6 , /*&*/8 , /*'*/40,       /* 32-39   */
  /*(*/10, /*)*/11, /***/9 , /*+*/13, /*,*/51, /*-*/12, /*.*/52, /*/*/53,       /* 40-47   */
  /*0*/11, /*1*/2 , /*2*/3 , /*3*/4 , /*4*/5 , /*5*/6 , /*6*/7 , /*7*/8 ,       /* 48-55   */
  /*8*/9 , /*9*/10, /*:*/39, /*;*/39, /*<*/51, /*=*/13, /*>*/52, /*?*/53,       /* 56-63   */
  /*@*/3 , /*A*/30, /*B*/48, /*C*/46, /*D*/32, /*E*/18, /*F*/33, /*G*/34,       /* 64-71   */
  /*H*/35, /*I*/23, /*J*/36, /*K*/37, /*L*/38, /*M*/50, /*N*/49, /*O*/24,       /* 72-79   */
  /*P*/25, /*Q*/16, /*R*/19, /*S*/31, /*T*/20, /*U*/22, /*V*/47, /*W*/17,       /* 80-87   */
  /*X*/45, /*Y*/21, /*Z*/44, /*[*/26, /*\*/43, /*]*/27, /*^*/7 , /*_*/12,       /* 88-95   */
  /*`*/41, /*a*/30, /*b*/48, /*c*/46, /*d*/32, /*e*/18, /*f*/33, /*g*/34,       /* 96-103  */
  /*h*/35, /*i*/23, /*j*/36, /*k*/37, /*l*/38, /*m*/50, /*n*/49, /*o*/24,       /* 104-111 */
  /*p*/25, /*q*/16, /*r*/19, /*s*/31, /*t*/20, /*u*/22, /*v*/47, /*w*/17,       /* 112-119 */
  /*x*/45, /*y*/21, /*z*/44, /*{*/26, /*|*/43, /*}*/27, /*~*/41, SCODE_DEL };   /* 120-127 */

uint8_t keys_apple2ASCII(uint8_t c, OUTPARM font_mode_t *mode) {
    font_mode_t ignored_mode;
    if (mode == NULL) {
        mode = &ignored_mode;
    }

    if (c < 0x20) {
        *mode = FONT_MODE_INVERSE;
        return c + 0x40;                // Apple //e (0x00-0x1F) -> ASCII (0x40-0x5F)
    } else if (c < 0x40) {
        *mode = FONT_MODE_INVERSE;
        return c;                       // Apple //e (0x20-0x3F) -> ASCII (0x20-0x3F)
    } else if (c < 0x60) {
        if (/*altchar on:*/(run_args.softswitches & SS_ALTCHAR)) {
            *mode = FONT_MODE_MOUSETEXT;
            return c+0x40;
        } else {
            *mode = FONT_MODE_FLASH;
            return c;                   // Apple //e (0x40-0x5F) -> ASCII (0x40-0x5F)
        }
    } else if (c < 0x80) {
        if (/*altchar on:*/(run_args.softswitches & SS_ALTCHAR)) {
            *mode = FONT_MODE_INVERSE;
            return c;                   // Apple //e (0x60-0x7F) -> ASCII (0x60-0x7F)
        } else {
            *mode = FONT_MODE_FLASH;
            return c - 0x40;            // Apple //e (0x60-0x7F) -> ASCII (0x20-0x3F)
        }
    } else if (c < 0xA0) {
        *mode = FONT_MODE_NORMAL;
        return c - 0x40;                // Apple //e (0x80-0x9F) -> ASCII (0x40-0x5F)
    } else if (c < 0xC0) {
        *mode = FONT_MODE_NORMAL;
        return c - 0x80;                // Apple //e (0xA0-0xBF) -> ASCII (0x20-0x3F)
    } else if (c < 0xE0) {
        *mode = FONT_MODE_NORMAL;
        return c - 0x80;                // Apple //e (0xC0-0xDF) -> ASCII (0x40-0x5F)
    } else {
        *mode = FONT_MODE_NORMAL;
        return c - 0x80;                // Apple //e (0xE0-0xFF) -> ASCII (0x60-0x7F)
    }
}

int keys_ascii2Scancode(uint8_t c) {
    return scode_map[c&0x7f];
}

int keys_scancode2ASCII(int scancode, bool is_shifted, bool is_ctrl) {
    if (scancode < 0) {
        return -1;
    }
    int *keymap = NULL;
    if (is_shifted && is_ctrl) {
        keymap = apple_iie_keymap_shift_ctrl;
    } else if (is_ctrl) {
        keymap = apple_iie_keymap_ctrl;
    } else if (is_shifted) {
        keymap = apple_iie_keymap_shifted;
    } else {
        keymap = apple_iie_keymap_plain;
    }
    return keymap[scancode & 0x7f];
}

bool keys_isShifted(void) {
    return key_pressed[SCODE_L_SHIFT] || key_pressed[SCODE_R_SHIFT];
}

// ----------------------------------------------------------------------------
// Handle input, both for keys and joystick.
//
// NOTE: `scancode` can be either a scancode or an ASCII key.
void keys_handleInput(int scancode, bool is_pressed, bool is_ascii) {
    SCOPE_TRACE_INTERFACE("keys_handleInput: scancode:%d is_pressed:%d is_ascii:%d", scancode, is_pressed, is_ascii);

    if (is_ascii) {
        last_scancode = -1;
        if (!is_pressed) {
            return;
        }
        if (! (key_pressed[ SCODE_L_CTRL ] || key_pressed[ SCODE_R_CTRL ]) ) {
            if (caps_lock && scancode >= 'a' && scancode <= 'z') {
                scancode -= 32;
            }
        }
        next_key = scancode;
    } else if (scancode >= 0) {
        assert(scancode < 0x80);
        last_scancode = scancode;

        int *keymap = NULL;
        if ((key_pressed[ SCODE_L_SHIFT ] || key_pressed[ SCODE_R_SHIFT ]) &&
                (key_pressed[ SCODE_L_CTRL ] || key_pressed[ SCODE_R_CTRL ]))
        {
            keymap = apple_iie_keymap_shift_ctrl;
        } else if (key_pressed[ SCODE_L_CTRL ] || key_pressed[ SCODE_R_CTRL ]) {
            keymap = apple_iie_keymap_ctrl;
        } else if (key_pressed[ SCODE_L_SHIFT ] || key_pressed[ SCODE_R_SHIFT ]) {
            keymap = apple_iie_keymap_shifted;
        } else if (caps_lock) {
            keymap = apple_iie_keymap_caps;
        } else {
            keymap = apple_iie_keymap_plain;
        }

        if (is_pressed) {
            key_pressed[ scancode ] = 1;
            switch (keymap[ scancode ]) {
                case JB0:
                    run_args.joy_button0 = 0xff; /* open apple */
                    break;
                case JB1:
                    run_args.joy_button1 = 0xff; /* closed apple */
                    break;
                default:
                    next_key = keymap[ scancode ];
                    break;
            }
        } else {
            key_pressed[ scancode ] = 0;
            switch (keymap[ scancode ]) {
                case JB0:
                    run_args.joy_button0 = 0x00;
                    break;
                case JB1:
                    run_args.joy_button1 = 0x00;
                    break;
                default:
                    break;
            }
        }
    }

    // key input consumption

    if ((next_key >= 0)
#ifdef INTERFACE_CLASSIC
            && !cpu_isPaused()
#endif
       )
    {
        do {
            int current_key = next_key;
            next_key = -1;

            if (current_key < 0x80) {
                apple_ii_64k[0][0xC000] = current_key | 0x80;
                apple_ii_64k[1][0xC000] = current_key | 0x80;
                break;
            }

#ifdef INTERFACE_CLASSIC
            if (current_key == kF9) {
                cpu_pause();
                timing_toggleCPUSpeed();
                cpu_resume();
                if (video_getAnimationDriver()->animation_showCPUSpeed) {
                    video_getAnimationDriver()->animation_showCPUSpeed();
                }
                break;
            }
            if (current_key == kF3) {

                double scale = (alt_speed_enabled ? cpu_altscale_factor : cpu_scale_factor);
                int percent_scale = (int)round(scale * 100);
                if (scale > CPU_SCALE_FASTEST_PIVOT) {
                    scale = CPU_SCALE_FASTEST_PIVOT;
                    percent_scale = (int)round(scale * 100);
                } else {
                    if (percent_scale > 100) {
                        percent_scale -= 25;
                    } else {
                        percent_scale -= 5;
                    }
                }
                scale = percent_scale/100.0;

                if (scale < CPU_SCALE_SLOWEST) {
                    scale = CPU_SCALE_SLOWEST;
                }

                if (alt_speed_enabled) {
                    cpu_altscale_factor = scale;
                } else {
                    cpu_scale_factor = scale;
                }

                if (video_getAnimationDriver()->animation_showCPUSpeed) {
                    video_getAnimationDriver()->animation_showCPUSpeed();
                }

                cpu_pause();
                timing_initialize();
                cpu_resume();
                break;
            }
            if (current_key == kF4) {

                int percent_scale = (int)round((alt_speed_enabled ? cpu_altscale_factor : cpu_scale_factor) * 100);
                double scale = 0.0;
                if (percent_scale >= 100) {
                    percent_scale += 25;
                } else {
                    percent_scale += 5;
                }
                scale = percent_scale/100.0;

                if (scale > CPU_SCALE_FASTEST) {
                    scale = CPU_SCALE_FASTEST;
                }

                if (alt_speed_enabled) {
                    cpu_altscale_factor = scale;
                } else {
                    cpu_scale_factor = scale;
                }

                if (video_getAnimationDriver()->animation_showCPUSpeed) {
                    video_getAnimationDriver()->animation_showCPUSpeed();
                }

                cpu_pause();
                timing_initialize();
                cpu_resume();
                break;
            }
#endif

            if (current_key == kEND) {
                if (key_pressed[ SCODE_L_CTRL ] || key_pressed[ SCODE_R_CTRL ]) {
                    cpu65_interrupt(ResetSig);
                }
                break;
            }

#ifdef INTERFACE_CLASSIC
            if ( c_keys_is_interface_key(current_key) || (current_key == kPAUSE) ) {
                c_interface_begin(current_key);
            }
#endif

        } while(0);
    }

#ifdef KEYPAD_JOYSTICK
    // Keypad emulated joystick relies on "raw" keyboard input
    if (joy_mode == JOY_KPAD) {
        bool joy_x_axis_unpressed = !( key_pressed[SCODE_KPAD_L]  || key_pressed[SCODE_KPAD_R] ||
                                      key_pressed[SCODE_KPAD_UL] || key_pressed[SCODE_KPAD_DL] || key_pressed[SCODE_KPAD_UR] || key_pressed[SCODE_KPAD_DR] ||
                                      // and allow regular PC arrow keys to manipulate joystick...
                                      key_pressed[SCODE_L] || key_pressed[SCODE_R]);
        bool joy_y_axis_unpressed = !( key_pressed[SCODE_KPAD_U]  || key_pressed[SCODE_KPAD_D] ||
                                     key_pressed[SCODE_KPAD_UL] || key_pressed[SCODE_KPAD_DL] || key_pressed[SCODE_KPAD_UR] || key_pressed[SCODE_KPAD_DR] ||
                                     key_pressed[SCODE_U] || key_pressed[SCODE_D]);

        if (key_pressed[ SCODE_KPAD_C ]) {
            joy_x = HALF_JOY_RANGE;
            joy_y = HALF_JOY_RANGE;
        }

        if (joy_auto_recenter) {
            static int x_unpressed_count = 0;
            if (joy_x_axis_unpressed) {
                ++x_unpressed_count;
                if (x_unpressed_count > 2) {
                    x_unpressed_count = 0;
                    joy_x = HALF_JOY_RANGE;
                }
            } else {
                x_unpressed_count = 0;
            }
            static int y_unpressed_count = 0;
            if (joy_y_axis_unpressed) {
                ++y_unpressed_count;
                if (y_unpressed_count > 2) {
                    y_unpressed_count = 0;
                    joy_y = HALF_JOY_RANGE;
                }
            } else {
                y_unpressed_count = 0;
            }
        }

        if (key_pressed[ SCODE_KPAD_UL ] || key_pressed[ SCODE_KPAD_U ] || key_pressed[ SCODE_KPAD_UR ] ||/* regular arrow up */key_pressed[ SCODE_U ]) {
            if (joy_y > joy_step) {
                joy_y -= joy_step;
            } else {
                joy_y = 0;
            }
        }

        if (key_pressed[ SCODE_KPAD_DL ] || key_pressed[ SCODE_KPAD_D ] || key_pressed[ SCODE_KPAD_DR ] ||/* regular arrow dn */key_pressed[ SCODE_D ]) {
            if (joy_y < JOY_RANGE - joy_step) {
                joy_y += joy_step;
            } else {
                joy_y = JOY_RANGE-1;
            }
        }

        if (key_pressed[ SCODE_KPAD_UL ] || key_pressed[ SCODE_KPAD_L ] || key_pressed[ SCODE_KPAD_DL ] ||/* regular arrow l */key_pressed[ SCODE_L ]) {
            if (joy_x > joy_step) {
                joy_x -= joy_step;
            } else {
                joy_x = 0;
            }
        }

        if (key_pressed[ SCODE_KPAD_UR ] || key_pressed[ SCODE_KPAD_R ] || key_pressed[ SCODE_KPAD_DR ] ||/* regular arrow r */key_pressed[ SCODE_R ]) {
            if (joy_x < JOY_RANGE - joy_step) {
                joy_x += joy_step;
            } else {
                joy_x = JOY_RANGE-1;
            }
        }
    }
#endif
}

#ifdef INTERFACE_CLASSIC
int c_rawkey()
{
    return last_scancode;
}

int c_mygetch(int block)
{
    int retval;

    while (next_key == -1 && block)
    {
        static struct timespec ts = { .tv_sec=0, .tv_nsec=33333333 };
        nanosleep(&ts, NULL); // 30Hz framerate
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
        case kF7:
        case kF8:
        case kF10:
            return true;
        default:
            break;
    }
    return false;
}
#endif

static void keys_prefsChanged(const char *domain) {
    bool val = false;
    if (prefs_parseBoolValue(domain, PREF_KEYBOARD_CAPS, &val)) {
        caps_lock = val;
    }
}

static __attribute__((constructor)) void __init_keys(void) {
    prefs_registerListener(PREF_DOMAIN_KEYBOARD, &keys_prefsChanged);
}

