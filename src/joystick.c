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

    half_joy_range = joy_range/2;
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

    js_adjustlow_x = (float)half_joy_range / (float)js_lowerrange_x;
    js_adjustlow_y = (float)half_joy_range / (float)js_lowerrange_y;
    js_adjusthigh_x = (float)half_joy_range / (float)js_upperrange_x;
    js_adjusthigh_y = (float)half_joy_range / (float)js_upperrange_y;
}

/* -------------------------------------------------------------------------
    c_calibrate_pc_joystick() - calibrates joystick.  determines extreme
    and center coordinates.  assumes that it can write to the interface
    screen.
   ------------------------------------------------------------------------- */
static void c_calibrate_pc_joystick()
{
    int almost_done, done;
    unsigned char x_val, y_val;

    /* reset all the extremes */
    js_max_x = -1;
    js_max_y = -1;
    js_min_x = MAXINT;
    js_min_y = MAXINT;

    /* open joystick device if not open */
    if (js_fd < 0)
    {
        if (c_open_pc_joystick())          /* problem opening device */
        {
            c_interface_print(
                1, 21, 0, "                                      " );
            c_interface_print(
                1, 22, 0, "     cannot open joystick device.     " );
            video_sync(0);
            usleep(1500000);
            c_interface_redo_bottom();
            return; /* problem */
        }
    }

    c_interface_print(
        1, 21, 0, "  Move joystick to all extremes then  " );
    c_interface_print(
        1, 22, 0, "    center it and press a button.     " );
    video_sync(0);
    usleep(1500000);
    c_interface_print(
        1, 21, 0, "                                      " );
    c_interface_print(
        1, 22, 0, "                                      " );

    almost_done = done = 0;             /* not done calibrating */
    while ((read(js_fd, &js, JS_RETURN) > 0) && (!done))
    {
        sprintf(temp, "  x = %04x, y = %04x", js.x, js.y);
        c_interface_print(1, 22, 0, temp);
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

        if (js.buttons != 0x00)                         /* press */
        {
            almost_done = 1;
        }

        if (almost_done && (js.buttons == 0x00))        /* release */
        {
            done = 1;
        }
    }

    js_center_x = js.x;
    js_center_y = js.y;

    printf("js_min_x = %d\n", js_min_x);
    printf("js_min_y = %d\n", js_min_y);
    printf("js_max_x = %d\n", js_max_x);
    printf("js_max_y = %d\n", js_max_y);
    printf("js_center_x = %d\n", js_center_x);
    printf("js_center_y = %d\n", js_center_y);
    printf("\n");

    c_calculate_pc_joystick_parms();       /* determine the parms */

    printf("js_lowerrange_x = %d\n", js_lowerrange_x);
    printf("js_lowerrange_y = %d\n", js_lowerrange_y);
    printf("js_upperrange_x = %d\n", js_upperrange_x);
    printf("js_upperrange_y = %d\n", js_upperrange_y);
    printf("\n");
    printf("js_offset_x = %d\n", js_offset_x);
    printf("js_offset_y = %d\n", js_offset_y);
    printf("\n");
    printf("js_adjustlow_x = %f\n", js_adjustlow_x);
    printf("js_adjustlow_y = %f\n", js_adjustlow_y);
    printf("js_adjusthigh_x = %f\n", js_adjusthigh_x);
    printf("js_adjusthigh_y = %f\n", js_adjusthigh_y);
    printf("\n");

    c_interface_print(
        1, 21, 0, "     Press a button to continue.      " );
    video_sync(0);

    /* show the normalized values until user presses button */
    while ((read(js_fd, &js, JS_RETURN) > 0) && js.buttons == 0x00)
    {
        x_val = (js.x < js_center_x)
                ? (js.x - js_offset_x) * js_adjustlow_x
                : (js.x - (js_center_x /*+js_offset_x*/)) * js_adjusthigh_x +
                half_joy_range;

        y_val = (js.y < js_center_y)
                ? (js.y - js_offset_y) * js_adjustlow_y
                : (js.y - (js_center_y /*+js_offset_y*/)) * js_adjusthigh_y +
                half_joy_range;
        sprintf(temp, "    x = %02x,   y = %02x", x_val, y_val);
        c_interface_print(1, 22, 0, temp);
        video_sync(0);
    }

    c_interface_redo_bottom();
    video_sync(0);
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

