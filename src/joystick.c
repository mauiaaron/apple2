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

#ifdef INTERFACE_CLASSIC
#define TEMPSIZE 256
extern void copy_and_pad_string(char *dest, const char* src, const char c, const int len, const char cap);
#endif

joystick_mode_t joy_mode = JOY_PCJOY;

/* parameters for generic and keyboard-simulated joysticks */
uint16_t joy_x = HALF_JOY_RANGE;
uint16_t joy_y = HALF_JOY_RANGE;
uint8_t joy_button0 = 0;
uint8_t joy_button1 = 0;
bool joy_clip_to_radius = false;

#ifdef KEYPAD_JOYSTICK
short joy_step = 1;
bool joy_auto_recenter = false;
#endif

void (*joydriver_resetJoystick)(void) = NULL;

static void joystick_prefsChanged(const char *domain) {
    assert(strcmp(domain, PREF_DOMAIN_JOYSTICK) == 0);

#ifdef KEYPAD_JOYSTICK
    long lVal = 0;
    prefs_parseLongValue(domain, PREF_JOYSTICK_KPAD_STEP, &lVal, /*base:*/10);
    joy_step = (short)lVal;
    if (joy_step < 1) {
        joy_step = 1;
    }
    if (joy_step > 255) {
        joy_step = 255;
    }

    prefs_parseBoolValue(domain, PREF_JOYSTICK_KPAD_AUTO_RECENTER, &joy_auto_recenter);
#endif
}

static __attribute__((constructor)) void _init_joystick(void) {
    prefs_registerListener(PREF_DOMAIN_JOYSTICK, &joystick_prefsChanged);
}

#ifdef INTERFACE_CLASSIC

/* -------------------------------------------------------------------------
    c_calibrate_pc_joystick() - calibrates joystick.  determines extreme
    and center coordinates.  assumes that it can write to the interface
    screen.
   ------------------------------------------------------------------------- */
static void c_calibrate_pc_joystick()
{
    char temp[TEMPSIZE];

#define CALIBRATE_JOYMENU_H 20
#define CALIBRATE_JOYMENU_W 40
#define CALIBRATE_TURTLE_X0 4
#define CALIBRATE_TURTLE_Y0 3
#define CALIBRATE_TURTLE_STEP_X (30.f / 255.f)
#define CALIBRATE_TURTLE_STEP_Y (10.f / 255.f)
    char joymenu[CALIBRATE_JOYMENU_H][CALIBRATE_JOYMENU_W+1] =
    //1.  5.  10.  15.  20.  25.  30.  35.  40.
    { "||||||||||||||||||||||||||||||||||||||||",
      "|                                      |",
      "|  |||||||||||||||||||||||||||||||||   |",
      "|  |                               |   |",
      "|  |                               |   |",
      "|  |                               |   |",
      "|  |                               |   |",
      "|  |                               |   |",
      "|  |                               |   |",
      "|  |                               |   |",
      "|  |                               |   |",
      "|  |                               |   |",
      "|  |                               |   |",
      "|  |                               |   |",
      "|  |||||||||||||||||||||||||||||||||   |",
      "|                                      |",
      "|  btn1:@ btn2:@      x:@@@@  y:@@@@   |",
      "|                                      |",
      "| ESC quits calibration                |",
      "||||||||||||||||||||||||||||||||||||||||" };

    uint8_t x_last=CALIBRATE_JOYMENU_W>>1, y_last=CALIBRATE_JOYMENU_H>>1;
    const char* const spinney = "|/-\\";
    uint8_t spinney_idx=0;
    for (;;)
    {
        int ch = c_mygetch(0);

        int x_plot = CALIBRATE_TURTLE_X0 + (int)(joy_x * CALIBRATE_TURTLE_STEP_X);
        int y_plot = CALIBRATE_TURTLE_Y0 + (int)(joy_y * CALIBRATE_TURTLE_STEP_Y);

        joymenu[y_last][x_last] = ' ';
        joymenu[y_plot][x_plot] = spinney[spinney_idx];

        x_last = x_plot;
        y_last = y_plot;

        joymenu[CALIBRATE_JOYMENU_H-4][8]  = joy_button0 ? 'X' : ' ';
        joymenu[CALIBRATE_JOYMENU_H-4][15] = joy_button1 ? 'X' : ' ';

        snprintf(temp, TEMPSIZE, "%04x", (short)(joy_x));
        copy_and_pad_string(&joymenu[CALIBRATE_JOYMENU_H-4][24], temp, ' ', 5, ' ');
        snprintf(temp, TEMPSIZE, "%04x", (short)(joy_y));
        copy_and_pad_string(&joymenu[CALIBRATE_JOYMENU_H-4][32], temp, ' ', 5, ' ');
        c_interface_print_submenu_centered(joymenu[0], CALIBRATE_JOYMENU_W, CALIBRATE_JOYMENU_H);

        spinney_idx = (spinney_idx+1) % 4;

        if (ch == kESC)
        {
            break;
        }

        static struct timespec ts = { .tv_sec=0, .tv_nsec=33333333 };
        nanosleep(&ts, NULL);
    }
}

#ifdef KEYPAD_JOYSTICK
static void c_calibrate_keypad_joystick()
{

#define KEYPAD_SUBMENU_H 20
#define KEYPAD_SUBMENU_W 40
#define CALIBRATE_TURTLE_KP_X0 4
#define CALIBRATE_TURTLE_KP_Y0 5
#define CALIBRATE_TURTLE_KP_STEP_X (14.f / 255.f)
    char submenu[KEYPAD_SUBMENU_H][KEYPAD_SUBMENU_W+1] =
    //1.  5.  10.  15.  20.  25.  30.  35.  40.
    { "||||||||||||||||||||||||||||||||||||||||",
      "|                                      |",
      "| Use keypad to test & tune joystick   |",
      "|                                      |",
      "|  ||||||||||||||||| [You may need to  |",
      "|  |               |  enable NumLock]  |",
      "|  |               |                   |",
      "|  |               |                   |",
      "|  |               |       7 @ 9       |",
      "|  |               |       @ 5 @       |",
      "|  |       .       |       1 @ 3       |",
      "|  |               | Alt-l       Alt-r |",
      "|  |               |                   |",
      "|  |               | + toggles auto-   |",
      "|  |               |  recentering: @@@ |",
      "|  |               | < or > to change  |",
      "|  |||||||||||||||||   sensitivity: @@ |",
      "|                                      |",
      "|  Alt btn1:@ Alt btn2:@     x:@@ y:@@ |",
      "||||||||||||||||||||||||||||||||||||||||" };

    submenu[8][29]  = MOUSETEXT_BEGIN + 0x0b;
    submenu[9][27]  = MOUSETEXT_BEGIN + 0x08;
    submenu[9][31]  = MOUSETEXT_BEGIN + 0x15;
    submenu[10][29] = MOUSETEXT_BEGIN + 0x0a;

    joy_x = HALF_JOY_RANGE;
    joy_y = HALF_JOY_RANGE;

    int ch = -1;
    uint8_t x_last=CALIBRATE_JOYMENU_W>>1, y_last=CALIBRATE_JOYMENU_H>>1;
    const char* const spinney = "|/-\\";
    uint8_t spinney_idx=0;
    char temp[TEMPSIZE];
    for (;;)
    {
        submenu[KEYPAD_SUBMENU_H-2][12] = joy_button0 ? 'X' : ' ';
        submenu[KEYPAD_SUBMENU_H-2][23] = joy_button1 ? 'X' : ' ';

        snprintf(temp, TEMPSIZE, "%02x", (uint8_t)joy_x);
        copy_and_pad_string(&submenu[KEYPAD_SUBMENU_H-2][31], temp, ' ', 3, ' ');
        snprintf(temp, TEMPSIZE, "%02x", (uint8_t)joy_y);
        copy_and_pad_string(&submenu[KEYPAD_SUBMENU_H-2][36], temp, ' ', 3, ' ');

        snprintf(temp, TEMPSIZE, "%02x", (uint8_t)joy_step);
        copy_and_pad_string(&submenu[KEYPAD_SUBMENU_H-4][36], temp, ' ', 3, ' ');

        snprintf(temp, TEMPSIZE, "%s", joy_auto_recenter ? " on" : "off" );
        copy_and_pad_string(&submenu[KEYPAD_SUBMENU_H-6][35], temp, ' ', 4, ' ');

        int x_plot = CALIBRATE_TURTLE_KP_X0 + (int)(joy_x * CALIBRATE_TURTLE_KP_STEP_X);
        int y_plot = CALIBRATE_TURTLE_KP_Y0 + (int)(joy_y * CALIBRATE_TURTLE_STEP_Y);

        submenu[y_last][x_last] = ' ';
        submenu[y_plot][x_plot] = spinney[spinney_idx];

        x_last = x_plot;
        y_last = y_plot;

        spinney_idx = (spinney_idx+1) % 4;

        c_interface_print_submenu_centered(submenu[0], KEYPAD_SUBMENU_W, KEYPAD_SUBMENU_H);

        ch = c_mygetch(0);

        if (ch == kESC)
        {
            break;
        }
        else if (ch == '<')
        {
            if (joy_step > 1)
            {
                --joy_step;
            }
        }
        else if (ch == '>')
        {
            if (joy_step < 0xFF)
            {
                ++joy_step;
            }
        }
        else if (ch == '+')
        {
            joy_auto_recenter = (joy_auto_recenter+1) % 2;
            if (joy_auto_recenter)
            {
                joy_x = HALF_JOY_RANGE;
                joy_y = HALF_JOY_RANGE;
            }
        }

        static struct timespec ts = { .tv_sec=0, .tv_nsec=33333333 };
        nanosleep(&ts, NULL);
    }

    prefs_setLongValue(PREF_DOMAIN_JOYSTICK, PREF_JOYSTICK_KPAD_STEP, joy_step);
    prefs_setBoolValue(PREF_DOMAIN_JOYSTICK, PREF_JOYSTICK_KPAD_AUTO_RECENTER, joy_auto_recenter);
}
#endif // KEYPAD_JOYSTICK

void c_calibrate_joystick()
{
    if (joy_mode == JOY_PCJOY)
    {
        c_calibrate_pc_joystick();
    }

#ifdef KEYPAD_JOYSTICK
    else if (joy_mode == JOY_KPAD)
    {
        c_calibrate_keypad_joystick();
    }
#endif
}
#endif // INTERFACE_CLASSIC

void c_joystick_reset(void)
{
    if (joydriver_resetJoystick) {
        joydriver_resetJoystick();
    }

    joy_button0 = 0x0;
    joy_button1 = 0x0;

    joy_x = HALF_JOY_RANGE;
    joy_y = HALF_JOY_RANGE;
}

// clamps modern gamepad controller axis values to the "corners" of a traditional joystick as used on the Apple //e
static inline void clampBeyondRadius(uint8_t *x, uint8_t *y) {
    float half_x = (*x) - HALF_JOY_RANGE;
    float half_y = (*y) - HALF_JOY_RANGE;
    float r = sqrtf(half_x*half_x + half_y*half_y);
    bool shouldClip = (r > HALF_JOY_RANGE);
    if (joy_clip_to_radius && shouldClip) {
        if ((*x) < HALF_JOY_RANGE) {
            (*x) = ((*x) < QUARTER_JOY_RANGE) ? 0.f : (*x);
        } else {
            (*x) = ((*x) < HALF_JOY_RANGE+QUARTER_JOY_RANGE) ? (*x) : 0xFF;
        }
        if ((*y) < HALF_JOY_RANGE) {
            (*y) = ((*y) < QUARTER_JOY_RANGE) ? 0.f : (*y);
        } else {
            (*y) = ((*y) < HALF_JOY_RANGE+QUARTER_JOY_RANGE) ? (*y) : JOY_RANGE-1;
        }
    }
}

void joydriver_setClampBeyondRadius(bool clamp) {
    joy_clip_to_radius = clamp;
}

void joydriver_setAxisValue(uint8_t x, uint8_t y) {
    clampBeyondRadius(&x, &y);
    joy_x = x;
    joy_y = y;
}

uint8_t joydriver_getAxisX(void) {
    return joy_x;
}

// return Y axis value
uint8_t joydriver_getAxisY(void) {
    return joy_y;
}

// set button 0 pressed
void joydriver_setButton0Pressed(bool pressed) {
    joy_button0 = (pressed) ? 0x80 : 0x0;
}

// set button 1 pressed
void joydriver_setButton1Pressed(bool pressed) {
    joy_button1 = (pressed) ? 0x80 : 0x0;
}

