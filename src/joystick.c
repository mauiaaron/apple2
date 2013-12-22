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

#ifdef PC_JOYSTICK
#include <linux/joystick.h>
#endif

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <values.h>

#include "joystick.h"
#include "interface.h"
#include "video.h"
#include "keys.h"
#include "misc.h"
#include "prefs.h"

#ifdef PC_JOYSTICK
int js_fd = -1;                 /* joystick file descriptor */
struct JS_DATA_TYPE js;         /* joystick data struct */

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
    c_open_pc_joystick() - opens joystick device and sets timelimit value
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

        /* set timelimit value */
        if (ioctl(js_fd, JS_SET_TIMELIMIT, &js_timelimit) == -1)
        {
            return 1; /* problem */
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

/* -------------------------------------------------------------------------
    c_calibrate_pc_joystick() - calibrates joystick.  determines extreme
    and center coordinates.  assumes that it can write to the interface
    screen.
   ------------------------------------------------------------------------- */
extern void copy_and_pad_string(char *dest, const char* src, const char c, const int len, const char cap);
static void c_calibrate_pc_joystick()
{
#define JOYERR_PAD 35
#define JOYERR_SUBMENU_H 8
#define JOYERR_SUBMENU_W 40
    char errmenu[JOYERR_SUBMENU_H][JOYERR_SUBMENU_W+1] =
    //1.  5.  10.  15.  20.  25.  30.  35.  40.
    { "||||||||||||||||||||||||||||||||||||||||",
      "|                                      |",
      "| An error occurred:                   |",
      "|                                      |",
      "| Is a joystick device connected?      |",
      "| Is the proper kernel module loaded?  |",
      "|                                      |",
      "||||||||||||||||||||||||||||||||||||||||" };
#define JOYERR_SHOWERR(ERR) \
    copy_and_pad_string(&errmenu[3][2], ERR, ' ', JOYERR_PAD, ' '); \
    c_interface_print_submenu_centered(errmenu[0], JOYERR_SUBMENU_W, JOYERR_SUBMENU_H); \
    while (c_mygetch(1) == -1) { }

    /* reset all the extremes */
    js_max_x = -1;
    js_max_y = -1;
    js_min_x = MAXINT;
    js_min_y = MAXINT;

    /* open joystick device if not open */
    if (js_fd < 0)
    {
        if (c_open_pc_joystick())
        {
            JOYERR_SHOWERR(strerror(errno));
            return;
        }
    }

#define CALIBRATE_SUBMENU_H 7
#define CALIBRATE_SUBMENU_W 40
    char submenu[CALIBRATE_SUBMENU_H][CALIBRATE_SUBMENU_W+1] =
    //1.  5.  10.  15.  20.  25.  30.  35.  40.
    { "||||||||||||||||||||||||||||||||||||||||",
      "|                                      |",
      "| Move joystick to all extremes then   |",
      "| center it and long-press joy button  |",
      "|                                      |",
      "|  btn1:@ btn2:@      x:@@@@  y:@@@@   |",
      "||||||||||||||||||||||||||||||||||||||||" };

#define LONG_PRESS_THRESHOLD 120

#define SHOW_JOYSTICK_AXES(MENU, WIDTH, HEIGHT, X, Y) \
    sprintf(temp, "%04x", (short)(X)); \
    copy_and_pad_string(&MENU[HEIGHT-2][24], temp, ' ', 5, ' '); \
    sprintf(temp, "%04x", (short)(Y)); \
    copy_and_pad_string(&MENU[HEIGHT-2][32], temp, ' ', 5, ' '); \
    c_interface_print_submenu_centered(MENU[0], WIDTH, HEIGHT);

#define SHOW_BUTTONS(MENU, HEIGHT) \
    MENU[HEIGHT-2][8]  = (js.buttons & 0x01) ? 'X' : ' '; \
    MENU[HEIGHT-2][15] = (js.buttons & 0x02) ? 'X' : ' '; \
    if (js.buttons & 0x03) \
    { \
        ++long_press; \
    } \
    else \
    { \
        long_press = 0; \
    }

    unsigned int long_press = 0;
    while ((read(js_fd, &js, JS_RETURN) > 0))
    {
        SHOW_BUTTONS(submenu, CALIBRATE_SUBMENU_H);
        SHOW_JOYSTICK_AXES(submenu, CALIBRATE_SUBMENU_W, CALIBRATE_SUBMENU_H, js.x, js.y);
        video_sync(0);

        if (js_max_x < js.x)
        {
            js_max_x = js.x;
        }

        if (js_max_y < js.y)
        {
            js_max_y = js.y;
        }

        if (js_min_x > js.x)
        {
            js_min_x = js.x;
        }

        if (js_min_y > js.y)
        {
            js_min_y = js.y;
        }

        if (long_press > LONG_PRESS_THRESHOLD)
        {
            break;
        }
    }

    long_press = 0;
    js_center_x = js.x;
    js_center_y = js.y;

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
#define CALIBRATE_TURTLE_Y0 5
#define CALIBRATE_TURTLE_STEP_X (30.f / 255.f)
#define CALIBRATE_TURTLE_STEP_Y (10.f / 255.f)
    char joymenu[CALIBRATE_JOYMENU_H][CALIBRATE_JOYMENU_W+1] =
    //1.  5.  10.  15.  20.  25.  30.  35.  40.
    { "||||||||||||||||||||||||||||||||||||||||",
      "|                                      |",
      "| Long press joy button to quit        |",
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
      "||||||||||||||||||||||||||||||||||||||||" };

    uint8_t x_val=0, y_val=0;
    uint8_t x_last=CALIBRATE_JOYMENU_W>>1, y_last=CALIBRATE_JOYMENU_H>>1;
    const char* const spinney = "|/-\\";
    uint8_t spinney_idx=0;
    bool finished_press = false;
    while (read(js_fd, &js, JS_RETURN) > 0)
    {
        x_val = (js.x < js_center_x)
                ? (js.x - js_offset_x) * js_adjustlow_x
                : (js.x - (js_center_x /*+js_offset_x*/)) * js_adjusthigh_x +
                HALF_JOY_RANGE;

        y_val = (js.y < js_center_y)
                ? (js.y - js_offset_y) * js_adjustlow_y
                : (js.y - (js_center_y /*+js_offset_y*/)) * js_adjusthigh_y +
                HALF_JOY_RANGE;

        int x_plot = CALIBRATE_TURTLE_X0 + (int)(x_val * CALIBRATE_TURTLE_STEP_X);
        int y_plot = CALIBRATE_TURTLE_Y0 + (int)(y_val * CALIBRATE_TURTLE_STEP_Y);

        joymenu[y_last][x_last] = ' ';
        joymenu[y_plot][x_plot] = spinney[spinney_idx];

        x_last = x_plot;
        y_last = y_plot;

        SHOW_BUTTONS(joymenu, CALIBRATE_JOYMENU_H);
        SHOW_JOYSTICK_AXES(joymenu, CALIBRATE_JOYMENU_W, CALIBRATE_JOYMENU_H, x_val, y_val);
        video_sync(0);

        spinney_idx = (spinney_idx+1) % 4;

        if (!js.buttons)
        {
            finished_press = true;
        }
        if (finished_press && (long_press > LONG_PRESS_THRESHOLD))
        {
            break;
        }
    }
}
#endif // PC_JOYSTICK

#ifdef KEYPAD_JOYSTICK
static void c_calibrate_keypad_joystick()
{
    // TODO ....
}
#endif

#ifdef TOUCH_JOYSTICK
// TBD ...
#endif

/* ---------------------------------------------------------------------- */

void c_open_joystick()
{
#ifdef PC_JOYSTICK
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
#ifdef PC_JOYSTICK
    if (joy_mode == JOY_PCJOY)
    {
        c_close_pc_joystick();
    }
#endif

#ifdef KEYPAD_JOYSTICK
    if (joy_mode == JOY_KPAD)
    {
        // NOP
    }
#endif
}

void c_calibrate_joystick()
{
#ifdef PC_JOYSTICK
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

