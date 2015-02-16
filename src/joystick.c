/*
 * Apple // emulator for Linux: Joystick calibration routines
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

#ifdef INTERFACE_CLASSIC
#define TEMPSIZE 256
extern void copy_and_pad_string(char *dest, const char* src, const char c, const int len, const char cap);
#endif

/* parameters for generic and keyboard-simulated joysticks */
uint16_t joy_x = HALF_JOY_RANGE;
uint16_t joy_y = HALF_JOY_RANGE;
uint8_t joy_button0 = 0;
uint8_t joy_button1 = 0;
uint8_t joy_button2 = 0; // unused?
bool joy_clip_to_radius = false;

#ifdef KEYPAD_JOYSTICK
short joy_step = 1;
uint8_t joy_auto_recenter = 0;
#endif

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

#if defined(KEYPAD_JOYSTICK)
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
}
#endif // KEYPAD_JOYSTICK
#endif // INTERFACE_CLASSIC

#ifdef TOUCH_JOYSTICK
// TBD ...
#endif

/* ---------------------------------------------------------------------- */

#ifdef INTERFACE_CLASSIC
void c_calibrate_joystick()
{
    if (joy_mode == JOY_PCJOY)
    {
        c_calibrate_pc_joystick();
    }

#ifdef KEYPAD_JOYSTICK
    if (joy_mode == JOY_KPAD)
    {
        c_calibrate_keypad_joystick();
    }
#endif
}
#endif // INTERFACE_CLASSIC

extern void gldriver_joystick_reset(void);
void c_joystick_reset()
{
#if VIDEO_OPENGL && !TESTING
    if (!is_headless) {
        gldriver_joystick_reset();
    }
#endif
    joy_button0 = 0x0;
    joy_button1 = 0x0;
    joy_button2 = 0x0;
#ifdef KEYPAD_JOYSTICK
    if (joy_mode == JOY_KPAD)
    {
        joy_x = HALF_JOY_RANGE;
        joy_y = HALF_JOY_RANGE;
    }
#endif
}

