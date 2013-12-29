/*
 * Apple // emulator for Linux: Preferences file maintenance
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

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "misc.h"
#include "prefs.h"
#include "keys.h"
#include "interface.h"
#include "timing.h"
#include "cpu.h"
#include "joystick.h"

#define         PRM_NONE                        0
#define         PRM_SPEED                       1
#define         PRM_ALTSPEED                    101
#define         PRM_MODE                        2
#define         PRM_DISK_PATH                   3
#define         PRM_HIRES_COLOR                 4
#define         PRM_VOLUME                      5
#define         PRM_JOY_INPUT                   6
#define         PRM_JOY_PC_CALIBRATE            10
#define         PRM_JOY_KPAD_CALIBRATE          11
#define         PRM_ROM_PATH                    12


char system_path[SYSSIZE];
char disk_path[DISKSIZE];

int apple_mode;
int sound_volume;
color_mode_t color_mode;
joystick_mode_t joy_mode;

static char *config_filename = NULL;

struct match_table
{
    const char *tag;
    int value;
};

static const struct match_table prefs_table[] =
{
    { "speed", PRM_SPEED },
    { "altspeed", PRM_ALTSPEED },
    { "mode", PRM_MODE },
    { "path", PRM_DISK_PATH },
    { "disk path", PRM_DISK_PATH },
    { "disk_path", PRM_DISK_PATH },
    { "path", PRM_DISK_PATH },
    { "color", PRM_HIRES_COLOR },
    { "volume", PRM_VOLUME },
    { "joystick", PRM_JOY_INPUT },
    { "pc joystick parms", PRM_JOY_PC_CALIBRATE },
    { "pc_joystick_parms", PRM_JOY_PC_CALIBRATE },
    { "keypad joystick parms", PRM_JOY_KPAD_CALIBRATE },
    { "keypad_joystick_parms", PRM_JOY_KPAD_CALIBRATE },
    { "system path", PRM_ROM_PATH },
    { "system_path", PRM_ROM_PATH },
    { 0, PRM_NONE }
};

static const struct match_table modes_table[] =
{
    { "][+", II_MODE },
    { "][+ undocumented", IIU_MODE },
    { "//e", IIE_MODE },
    { 0, IIE_MODE }
};

static const struct match_table color_table[] =
{
    { "black/white", COLOR_NONE },
    /*{ "lazy color", LAZY_COLOR }, deprecated*/
    { "color", COLOR },
    /*{ "lazy interpolated", LAZY_INTERP }, deprecated*/
    { "interpolated", COLOR_INTERP },
    { "off", 0 },
    { "on", COLOR },
    { 0, COLOR }
};

static const struct match_table volume_table[] =
{
    { "0", 0 },
    { "1", 1 },
    { "2", 2 },
    { "3", 3 },
    { "4", 4 },
    { "5", 5 },
    { "6", 6 },
    { "7", 7 },
    { "8", 8 },
    { "9", 9 },
    { "10", 10 },
    { 0, 10 },
};

static const struct match_table joy_input_table[] =
{
    { "off", JOY_OFF },
#ifdef KEYPAD_JOYSTICK
    { "joy keypad", JOY_KPAD },
    { "joy_keypad", JOY_KPAD },
#endif
#ifdef PC_JOYSTICK
    { "pc joystick", JOY_PCJOY },
    { "pc_joystick", JOY_PCJOY },
#endif                          /* PC_JOYSTICK */
    { 0, JOY_OFF }
};

/* Find the number assigned to KEYWORD in a match table PARADIGM. If no match,
 * then the value associated with the terminating entry is used as a
 * default. */
static int match(const struct match_table *paradigm, const char *keyword)
{
    while (paradigm->tag && strcasecmp(paradigm->tag, keyword))
    {
        paradigm++;
    }

    return paradigm->value;
}

/* Reverse match -- find a keyword associated with number KEY in
 * PARADIGM. The first match is used -- synonym keywords appearing later
 * in the table are not chosen.
 *
 * A null is returned for no match.
 */
static const char *reverse_match(const struct match_table *paradigm, int key)
{
    while (paradigm->tag && key != paradigm->value)
    {
        paradigm++;
    }

    return paradigm->tag;
}

/* Eat leading and trailing whitespace of string X.  The old string is
 * overwritten and a new pointer is returned.
 */
static char * clean_string(char *x)
{
    size_t y;

    /* Leading white space */
    while (isspace(*x))
    {
        x++;
    }

    /* Trailing white space */
    y = strlen(x);
    while (y && x[y--] == ' ')
    {
    }

    x[y] = 0;

    return x;
}

/* Load the configuration. Must be called *once* at start. */
void load_settings(void)
{
    /* set system defaults before user defaults. */
    strcpy(disk_path, "./disks");
    strcpy(system_path, "./rom");

    {
        const char *homedir;

        homedir = getenv("HOME");
        config_filename = malloc(strlen(homedir) + 9);
        strcpy(config_filename, homedir);
        strcat(config_filename, "/.apple2");

        /* config_filename is left allocated for convinence in
         * save_settings */
    }

    {
        FILE *config_file;
        char *buffer = 0;
        size_t size = 0;

        config_file = fopen(config_filename, "r");
        if (config_file == NULL)
        {
            printf(
                "Warning. Cannot open the .apple2 system defaults file.\n"
                "Make sure it's readable in your home directory.");
            printf("Press RETURN to continue...");
            getchar();
            return;
        }

        while (getline(&buffer, &size, config_file) != -1)
        {
            char *parameter;
            char *argument;

            /* Split line between parameter and argument */

            parameter = buffer;
            argument = strchr(buffer, '=');
            argument[0] = 0;
            argument++;

            parameter = clean_string(parameter);
            argument = clean_string(argument);

            int main_match = match(prefs_table, parameter);
            switch (main_match)
            {
            case PRM_NONE:
                fprintf(stderr, "Unrecognized config parameter `%s'", parameter);
                break;

            case PRM_SPEED:
            case PRM_ALTSPEED:
            {
                double x = strtod(argument, NULL);
                if (x > CPU_SCALE_FASTEST)
                {
                    x = CPU_SCALE_FASTEST;
                }
                else if (x < CPU_SCALE_SLOWEST)
                {
                    x = CPU_SCALE_SLOWEST;
                }
                if (main_match == PRM_SPEED)
                {
                    cpu_scale_factor = x;
                }
                else
                {
                    cpu_altscale_factor = x;
                }
                break;
            }

            case PRM_MODE:
                apple_mode = match(modes_table, argument);
                break;

            case PRM_DISK_PATH:
                strncpy(disk_path, argument, DISKSIZE);
                break;

            case PRM_HIRES_COLOR:
                color_mode = match(color_table, argument);
                break;

            case PRM_VOLUME:
                sound_volume = match(volume_table, argument);
                break;

            case PRM_JOY_INPUT:
                joy_mode = match(joy_input_table, argument);
                break;

            case PRM_JOY_PC_CALIBRATE:
#ifdef PC_JOYSTICK
                /* pc joystick parms generated by the calibration routine
                   (shouldn't need to hand tweak these) = origin_x origin_y max_x
                   min_x max_y min_y js_timelimit */
                js_center_x = strtol(argument, &argument, 10);
                js_center_y = strtol(argument, &argument, 10);
                js_max_x = strtol(argument, &argument, 10);
                if (js_max_x < 0)
                {
                    js_max_x = 0;
                }

                js_min_x = strtol(argument, &argument, 10);
                if (js_min_x < 0)
                {
                    js_min_x = 0;
                }

                js_max_y = strtol(argument, &argument, 10);
                if (js_max_y < 0)
                {
                    js_max_y = 0;
                }

                js_min_y = strtol(argument, &argument, 10);
                if (js_min_y < 0)
                {
                    js_min_y = 0;
                }

                js_timelimit = strtol(argument, &argument, 10);
                if (js_timelimit < 2)
                {
                    js_timelimit = 2;
                }

                c_open_joystick();
                break;
#endif

#ifdef KEYPAD_JOYSTICK
            case PRM_JOY_KPAD_CALIBRATE:
                joy_step = strtol(argument, &argument, 10);
                if (joy_step < 1)
                {
                    joy_step = 1;
                }
                else if (joy_step > 255)
                {
                    joy_step = 255;
                }

                auto_recenter = strtol(argument, &argument, 10);

                break;
#endif

            case PRM_ROM_PATH:
                strncpy(system_path, argument, SYSSIZE);
                break;
            }
        }

        fclose(config_file);
    }
}


/* Save the configuration */
bool save_settings(void)
{
    FILE *config_file = NULL;

    LOG("Saving preferences...");

#define ERROR_SUBMENU_H 9
#define ERROR_SUBMENU_W 40
    int ch = -1;
    char submenu[ERROR_SUBMENU_H][ERROR_SUBMENU_W+1] =
    //1.  5.  10.  15.  20.  25.  30.  35.  40.
    { "||||||||||||||||||||||||||||||||||||||||",
      "|                                      |",
      "|                                      |",
      "| OOPS, could not open or write to the |",
      "| .apple2/apple2.cfg file in your HOME |",
      "| directory ...                        |",
      "|                                      |",
      "|                                      |",
      "||||||||||||||||||||||||||||||||||||||||" };

    config_file = fopen(config_filename, "w");
    if (config_file == NULL)
    {
        printf(
            "Cannot open the .apple2/apple2.cfg system defaults file for writing.\n"
            "Make sure it has rw permission in your home directory.");
        c_interface_print_submenu_centered(submenu[0], ERROR_SUBMENU_W, ERROR_SUBMENU_H);
        while ((ch = c_mygetch(1)) == -1)
        {
        }

        return false;
    }

    bool anErr = false;
    int err = fprintf(config_file,
            "speed = %0.2lf\n"
            "altspeed = %0.2lf\n"
            "mode = %s\n"
            "disk path = %s\n"
            "color = %s\n"
            "volume = %s\n"
            "joystick = %s\n"
            "system path = %s\n",
            cpu_scale_factor,
            cpu_altscale_factor,
            reverse_match(modes_table, apple_mode),
            disk_path,
            reverse_match(color_table, color_mode),
            reverse_match(volume_table, sound_volume),
            reverse_match(joy_input_table, joy_mode),
            system_path);
    anErr = anErr || (err < 0);

#ifdef PC_JOYSTICK
    err = fprintf(config_file,
            "pc joystick parms = %d %d %d %d %d %d %ld\n",
            js_center_x, js_center_y, js_max_x, js_min_x,
            js_max_y, js_min_y, js_timelimit);
#endif
    anErr = anErr || (err < 0);

#ifdef KEYPAD_JOYSTICK
    err = fprintf(config_file,
            "keypad joystick parms = %d %u\n", joy_step, auto_recenter ? 1 : 0);
#endif
    anErr = anErr || (err < 0);

    if (anErr)
    {
        c_interface_print_submenu_centered(submenu[0], ERROR_SUBMENU_W, ERROR_SUBMENU_H);
        while ((ch = c_mygetch(1)) == -1)
        {
        }
        return false;
    }

    fclose(config_file);

    return true;
}

