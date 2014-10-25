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

#ifdef LINUX_JOYSTICK
#include <linux/joystick.h>
#endif

/* parameters for generic and keyboard-simulated joysticks */
uint16_t joy_x = HALF_JOY_RANGE;
uint16_t joy_y = HALF_JOY_RANGE;
uint8_t joy_button0 = 0;
uint8_t joy_button1 = 0;
uint8_t joy_button2 = 0; // unused?

#ifdef KEYPAD_JOYSTICK
short joy_step = 1;
uint8_t joy_auto_recenter = 0;
#endif

#ifdef LINUX_JOYSTICK
int js_center_x;
int js_center_y;
int js_max_x;
int js_max_y;
int js_min_x;
int js_min_y;

int js_fd = -1;                 /* joystick file descriptor */
struct JS_DATA_TYPE js;         /* joystick data struct */

int raw_js_x;
int raw_js_y;

int js_lowerrange_x,
    js_upperrange_x,
    js_lowerrange_y,
    js_upperrange_y,
    js_offset_x,
    js_offset_y;

float
    js_adjustlow_x,
    js_adjustlow_y,
    js_adjusthigh_x,
    js_adjusthigh_y;

/* -------------------------------------------------------------------------
    c_open_pc_joystick() - opens joystick device
   ------------------------------------------------------------------------- */
static void c_calculate_pc_joystick_parms();
int c_open_pc_joystick()
{
    if (js_fd < 0)
    {
        if ((js_fd = open("/dev/js0", O_RDONLY)) < 0)
        {

            /* try again with another name */
            if ((js_fd = open("/dev/joystick", O_RDONLY)) < 0)
            {
                if ((js_fd = open("/dev/input/js0", O_RDONLY)) < 0)
                {
                    return 1; /* problem */
                }
            }
        }
    }

    c_calculate_pc_joystick_parms();

    return 0; /* no problem */
}

/* -------------------------------------------------------------------------
    c_close_pc_joystick() - closes joystick device
   ------------------------------------------------------------------------- */
void c_close_pc_joystick()
{
    if (js_fd < 0)
    {
        return;
    }

    close(js_fd);
    js_fd = -1;
}


/* -------------------------------------------------------------------------
 * c_calculate_pc_joystick_parms() - calculates parameters for joystick
 * device.  assumes that device extremes have already been determined.
 * ------------------------------------------------------------------------- */
static void c_calculate_pc_joystick_parms()
{
    js_lowerrange_x = js_center_x - js_min_x;
    js_upperrange_x = js_max_x - js_center_x;
    js_lowerrange_y = js_center_y - js_min_y;
    js_upperrange_y = js_max_y - js_center_y;

    js_offset_x = js_min_x;
    js_offset_y = js_min_y;

    js_adjustlow_x  = (float)HALF_JOY_RANGE / (float)js_lowerrange_x;
    js_adjustlow_y  = (float)HALF_JOY_RANGE / (float)js_lowerrange_y;
    js_adjusthigh_x = (float)HALF_JOY_RANGE / (float)js_upperrange_x;
    js_adjusthigh_y = (float)HALF_JOY_RANGE / (float)js_upperrange_y;
}

#ifdef INTERFACE_CLASSIC
/* -------------------------------------------------------------------------
    c_calibrate_pc_joystick() - calibrates joystick.  determines extreme
    and center coordinates.  assumes that it can write to the interface
    screen.
   ------------------------------------------------------------------------- */
extern void copy_and_pad_string(char *dest, const char* src, const char c, const int len, const char cap);
static void c_calibrate_pc_joystick()
{
#define JOYERR_PAD 35
#define JOYERR_SUBMENU_H 9
#define JOYERR_SUBMENU_W 40
    char errmenu[JOYERR_SUBMENU_H][JOYERR_SUBMENU_W+1] =
    //1.  5.  10.  15.  20.  25.  30.  35.  40.
    { "||||||||||||||||||||||||||||||||||||||||",
      "|                                      |",
      "| An error occurred:                   |",
      "|                                      |",
      "|                                      |",
      "| Is a joystick device connected?      |",
      "| Is the proper kernel module loaded?  |",
      "|                                      |",
      "||||||||||||||||||||||||||||||||||||||||" };

    /* reset all the extremes */
    js_max_x = INT_MIN;
    js_max_y = INT_MIN;
    js_min_x = INT_MAX;
    js_min_y = INT_MAX;

    /* open joystick device if not open */
    if (js_fd < 0)
    {
        if (c_open_pc_joystick())
        {
            const char *err = strerror(errno);
            ERRLOG("OOPS, cannot open pc joystick : %s", err);
#ifdef INTERFACE_CLASSIC
            copy_and_pad_string(&errmenu[3][2], err, ' ', JOYERR_PAD, ' ');
            c_interface_print_submenu_centered(errmenu[0], JOYERR_SUBMENU_W, JOYERR_SUBMENU_H);
            while (c_mygetch(1) == -1) {
                // ...
            }
#endif
            return;
        }
    }

#define TEMPSIZE 256
    char temp[TEMPSIZE];

#define CALIBRATE_SUBMENU_H 9
#define CALIBRATE_SUBMENU_W 40
    char submenu[CALIBRATE_SUBMENU_H][CALIBRATE_SUBMENU_W+1] =
    //1.  5.  10.  15.  20.  25.  30.  35.  40.
    { "||||||||||||||||||||||||||||||||||||||||",
      "|                                      |",
      "| Move joystick to extremes of x and y |",
      "| axes and then center it.             |",
      "|                                      |",
      "|  btn1:@ btn2:@      x:@@@@  y:@@@@   |",
      "|                                      |",
      "| ESC to continue...                   |",
      "||||||||||||||||||||||||||||||||||||||||" };

#define SHOW_JOYSTICK_AXES(MENU, WIDTH, HEIGHT, X, Y) \
    snprintf(temp, TEMPSIZE, "%04x", (short)(X)); \
    copy_and_pad_string(&MENU[HEIGHT-4][24], temp, ' ', 5, ' '); \
    snprintf(temp, TEMPSIZE, "%04x", (short)(Y)); \
    copy_and_pad_string(&MENU[HEIGHT-4][32], temp, ' ', 5, ' '); \
    c_interface_print_submenu_centered(MENU[0], WIDTH, HEIGHT);

#define SHOW_BUTTONS(MENU, HEIGHT) \
    MENU[HEIGHT-4][8]  = joy_button0 ? 'X' : ' '; \
    MENU[HEIGHT-4][15] = joy_button1 ? 'X' : ' ';

    for (;;)
    {
        int ch = c_mygetch(0);

        SHOW_BUTTONS(submenu, CALIBRATE_SUBMENU_H);
        SHOW_JOYSTICK_AXES(submenu, CALIBRATE_SUBMENU_W, CALIBRATE_SUBMENU_H, raw_js_x, raw_js_y);

        if (js_max_x < raw_js_x)
        {
            js_max_x = raw_js_x;
        }

        if (js_max_y < raw_js_y)
        {
            js_max_y = raw_js_y;
        }

        if (js_min_x > raw_js_x)
        {
            js_min_x = raw_js_x;
        }

        if (js_min_y > raw_js_y)
        {
            js_min_y = raw_js_y;
        }

        if (ch == kESC)
        {
            break;
        }

        static struct timespec ts = { .tv_sec=0, .tv_nsec=33333333 };
        nanosleep(&ts, NULL);
    }

    js_center_x = raw_js_x;
    js_center_y = raw_js_y;

#ifndef NDEBUG
    LOG("js_min_x = %d", js_min_x);
    LOG("js_min_y = %d", js_min_y);
    LOG("js_max_x = %d", js_max_x);
    LOG("js_max_y = %d", js_max_y);
    LOG("js_center_x = %d", js_center_x);
    LOG("js_center_y = %d", js_center_y);
    LOG(" ");
#endif

    c_calculate_pc_joystick_parms();

#ifndef NDEBUG
    LOG("js_lowerrange_x = %d", js_lowerrange_x);
    LOG("js_lowerrange_y = %d", js_lowerrange_y);
    LOG("js_upperrange_x = %d", js_upperrange_x);
    LOG("js_upperrange_y = %d", js_upperrange_y);
    LOG(" ");
    LOG("js_offset_x = %d", js_offset_x);
    LOG("js_offset_y = %d", js_offset_y);
    LOG(" ");
    LOG("js_adjustlow_x = %f", js_adjustlow_x);
    LOG("js_adjustlow_y = %f", js_adjustlow_y);
    LOG("js_adjusthigh_x = %f", js_adjusthigh_x);
    LOG("js_adjusthigh_y = %f", js_adjusthigh_y);
    LOG(" ");
#endif

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

        SHOW_BUTTONS(joymenu, CALIBRATE_JOYMENU_H);
        SHOW_JOYSTICK_AXES(joymenu, CALIBRATE_JOYMENU_W, CALIBRATE_JOYMENU_H, joy_x, joy_y);

        spinney_idx = (spinney_idx+1) % 4;

        if (ch == kESC)
        {
            break;
        }

        static struct timespec ts = { .tv_sec=0, .tv_nsec=33333333 };
        nanosleep(&ts, NULL);
    }
}
#endif // INTERFACE_CLASSIC
#endif // LINUX_JOYSTICK

void c_initialize_joystick(void) {
    joy_button0 = 0x0;
    joy_button1 = 0x0;
    joy_button2 = 0x0;
}

#if defined(KEYPAD_JOYSTICK) && defined(INTERFACE_CLASSIC)
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
#endif // KEYPAD_JOYSTICK && INTERFACE_CLASSIC

#ifdef TOUCH_JOYSTICK
// TBD ...
#endif

/* ---------------------------------------------------------------------- */

void c_open_joystick()
{
#ifdef LINUX_JOYSTICK
    if (joy_mode == JOY_PCJOY)
    {
        c_open_pc_joystick();
    }
#endif

#ifdef KEYPAD_JOYSTICK
    if (joy_mode == JOY_KPAD)
    {
        // NOP
    }
#endif
}

void c_close_joystick()
{
#ifdef LINUX_JOYSTICK
    c_close_pc_joystick();
#endif

#ifdef KEYPAD_JOYSTICK
    if (joy_mode == JOY_KPAD)
    {
        // NOP
    }
#endif
}

#ifdef INTERFACE_CLASSIC
void c_calibrate_joystick()
{
#ifdef LINUX_JOYSTICK
    if (joy_mode == JOY_PCJOY)
    {
        c_calibrate_pc_joystick();
    }
#endif

#ifdef KEYPAD_JOYSTICK
    if (joy_mode == JOY_KPAD)
    {
        c_calibrate_keypad_joystick();
    }
#endif
}
#endif // INTERFACE_CLASSIC

void c_joystick_reset()
{
#ifdef KEYPAD_JOYSTICK
    if (joy_mode == JOY_KPAD)
    {
        joy_x = HALF_JOY_RANGE;
        joy_y = HALF_JOY_RANGE;
    }
#endif
}

