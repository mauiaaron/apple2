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
#include "cpu.h"

#define         PRM_NONE                        0
#define         PRM_SPEED                       1
#define         PRM_MODE                        2
#define         PRM_DISK_PATH                   3
#define         PRM_HIRES_COLOR                 4
#define         PRM_SOUND                       5
#define         PRM_JOY_INPUT                   6
#define         PRM_JOY_RANGE                   7
#define         PRM_JOY_OX                      8
#define         PRM_JOY_OY                      9
#define         PRM_JOY_PC_CALIBRATE            10
#define         PRM_JOY_KYBD_SENSITIVITY        11
#define         PRM_ROM_PATH                    12


char system_path[SYSSIZE];
char disk_path[DISKSIZE];

int apple_mode;
int sound_mode;
int color_mode;
short joy_mode;
short joy_step;
short joy_center_x;
short joy_center_y;
short joy_range;
short half_joy_range;

#ifdef PC_JOYSTICK
int js_center_x;
int js_center_y;
long js_timelimit;
int js_max_x;
int js_max_y;
int js_min_x;
int js_min_y;
#endif /* PC_JOYSTICK */

static char *config_filename;

struct match_table
{
    const char *tag;
    int value;
};

static const struct match_table prefs_table[] =
{
    { "speed", PRM_SPEED },
    { "mode", PRM_MODE },
    { "path", PRM_DISK_PATH },
    { "disk path", PRM_DISK_PATH },
    { "disk_path", PRM_DISK_PATH },
    { "path", PRM_DISK_PATH },
    { "color", PRM_HIRES_COLOR },
    { "sound", PRM_SOUND },
    { "joystick", PRM_JOY_INPUT },
    { "joy range", PRM_JOY_RANGE },
    { "joystick range", PRM_JOY_RANGE },
    { "joy_range", PRM_JOY_RANGE },
    { "origin_x", PRM_JOY_OX },
    { "origin_y", PRM_JOY_OY },
    { "pc joystick parms", PRM_JOY_PC_CALIBRATE },
    { "pc_joystick_parms", PRM_JOY_PC_CALIBRATE },
    { "sensitivity", PRM_JOY_KYBD_SENSITIVITY },
    { "system path", PRM_ROM_PATH },
    { "system_path", PRM_ROM_PATH },
    { 0, PRM_NONE }
};

static const struct match_table modes_table[] =
{
    { "][+", II_MODE },
    { "][+ undocumented", IIU_MODE },
#ifdef APPLE_IIE
    { "//e", IIE_MODE },
    { 0, IIE_MODE }
#else /* !APPLE_IIE */
    { 0, IIU_MODE }
#endif /* !APPLE_IIE */
};

static const struct match_table color_table[] =
{
    { "black/white", NO_COLOR },
    { "lazy color", LAZY_COLOR },
    { "color", COLOR },
    { "lazy interpolated", LAZY_INTERP },
    { "interpolated", INTERP },
    { "off", 0 },
    { "on", COLOR },
    { 0, COLOR }
};

static const struct match_table sound_table[] =
{
    { "off", 0 },
    { "pc_speaker", 1 },
    { "on", 1 },
    { 0, 1 },
};

static const struct match_table joy_input_table[] =
{
    { "off", JOY_OFF },
    { "keyboard", JOY_KYBD },
    { "linear", JOY_KYBD },
#ifdef PC_JOYSTICK
    { "pc joystick", JOY_PCJOY },
    { "pc_joystick", JOY_PCJOY },
#endif                          /* PC_JOYSTICK */
#if 0
    { "digital", JOY_DIGITAL },
#endif                          /* 0 */
    { 0, JOY_KYBD }
};

/* Find the number assigned to KEYWORD in a match table PARADIGM. If no match,
 * then the value associated with the terminating entry is used as a
 * default. */
static int
match(const struct match_table *paradigm, const char *keyword)
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
static char *
clean_string(char *x)
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
void
load_settings(void)
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

            switch (match(prefs_table, parameter))
            {
            case PRM_NONE:
                fprintf(stderr, "Unrecognized config parameter `%s'", parameter);
                break;

            case PRM_SPEED:
            {
                int x;

                x = strtol(argument, 0, 0);

                if (x < 0)
                {
                    x = 0;
                }

                cpu65_delay = MAX_APPLE_DELAY - x + 1;
                if (cpu65_delay < 1)
                {
                    cpu65_delay = 1;
                }
            }
            break;

            case PRM_MODE:
                apple_mode = match(modes_table, argument);
                break;

            case PRM_DISK_PATH:
                strncpy(disk_path, argument, DISKSIZE);
                break;

            case PRM_HIRES_COLOR:
                color_mode = match(color_table, argument);
                break;

            case PRM_SOUND:
                sound_mode = match(sound_table, argument);
                break;

            case PRM_JOY_INPUT:
                joy_mode = match(joy_input_table, argument);
                break;

            case PRM_JOY_RANGE:
                joy_range = strtol(argument, 0, 0);
                if (joy_range < 10)
                {
                    joy_range = 10;
                }
                else
                if (joy_range > 256)
                {
                    joy_range = 256;
                }

                half_joy_range = joy_range / 2;

                if (joy_center_x > joy_range)
                {
                    joy_center_x = half_joy_range;
                }

                if (joy_center_y > joy_range)
                {
                    joy_center_y = half_joy_range;
                }

                break;

            case PRM_JOY_OX:
                joy_center_x = strtol(argument, 0, 0);
                if (joy_center_x < 0)
                {
                    joy_center_x = 0;
                }
                else
                if (joy_center_x > 255)
                {
                    joy_center_x = 255;
                }

                if (joy_center_x > joy_range)
                {
                    joy_center_x = half_joy_range;
                }

                break;

            case PRM_JOY_OY:
                joy_center_y = strtol(argument, 0, 0);
                if (joy_center_y < 0)
                {
                    joy_center_y = 0;
                }
                else
                if (joy_center_y > 255)
                {
                    joy_center_y = 255;
                }

                if (joy_center_y > joy_range)
                {
                    joy_center_y = half_joy_range;
                }

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
                c_calculate_joystick_parms();   /* calculate the associated parms */
#endif

            case PRM_JOY_KYBD_SENSITIVITY:
                joy_step = strtol(argument, 0, 0);
                if (joy_step < 1)
                {
                    joy_step = 1;
                }
                else
                if (joy_step > 100)
                {
                    joy_step = 100;
                }

                break;

            case PRM_ROM_PATH:
                strncpy(system_path, argument, SYSSIZE);
                break;
            }
        }

        fclose(config_file);
    }
}


/* Save the configuration */
void
save_settings(void)
{
    FILE *config_file;

    config_file = fopen(config_filename, "w");
    if (config_file == NULL)
    {
        printf(
            "Cannot open the .apple2 system defaults file for writing.\n"
            "Make sure it has rw permission in your home directory.");
        return;
    }

    fprintf(config_file,
            "speed = %d\n"
            "mode = %s\n"
            "disk path = %s\n"
            "color = %s\n"
            "sound = %s\n"
            "joystick = %s\n"
            "joystick range = %d\n"
            "origin_x = %d\n"
            "origin_y = %d\n"
            "sensitivity = %d%%\n"
            "system path = %s\n",
            MAX_APPLE_DELAY + 1 - cpu65_delay,
            reverse_match(modes_table, apple_mode),
            disk_path,
            reverse_match(color_table, color_mode),
            reverse_match(sound_table, sound_mode),
            reverse_match(joy_input_table, joy_mode),
            joy_range,
            joy_center_x,
            joy_center_y,
            joy_step,
            system_path);

#ifdef PC_JOYSTICK
    fprintf(config_file,
            "pc joystick parms = %d %d %d %d %d %d %ld\n",
            js_center_x, js_center_y, js_max_x, js_min_x,
            js_max_y, js_min_y, js_timelimit);
#endif

    fclose(config_file);
}
