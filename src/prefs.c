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

#define _GNU_SOURCE

#include "common.h"

#define         PRM_NONE                        0
#define         PRM_SPEED                       1
#define         PRM_ALTSPEED                    101
#define         PRM_MODE                        2
#define         PRM_DISK_PATH                   3
#define         PRM_HIRES_COLOR                 4
#define         PRM_VOLUME                      5
#define         PRM_JOY_INPUT                   6
#define         PRM_VIDEO_MODE                  7
#define         PRM_JOY_KPAD_CALIBRATE          11
#define         PRM_ROM_PATH                    12
#define         PRM_CAPSLOCK                    102


char system_path[PATH_MAX] = { 0 };
char disk_path[PATH_MAX] = { 0 };

#warning FIXME TODO : completely excise deprecated apple_mode stuff
int apple_mode = 2/*IIE_MODE*/;
a2_video_mode_t a2_video_mode = VIDEO_1X;
joystick_mode_t joy_mode = JOY_PCJOY;

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
    { "video", PRM_VIDEO_MODE },
    { "volume", PRM_VOLUME },
    { "caps_lock", PRM_CAPSLOCK },
    { "caps lock", PRM_CAPSLOCK },
    { "joystick", PRM_JOY_INPUT },
    { "keypad joystick parms", PRM_JOY_KPAD_CALIBRATE },
    { "keypad_joystick_parms", PRM_JOY_KPAD_CALIBRATE },
    { "system path", PRM_ROM_PATH },
    { "system_path", PRM_ROM_PATH },
    { 0, PRM_NONE }
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

static const struct match_table video_table[] =
{
    { "1X", VIDEO_1X },
    { "2X", VIDEO_2X },
    { "Fullscreen", VIDEO_FULLSCREEN },
    { 0, VIDEO_1X }
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

static const struct match_table capslock_table[] =
{
    { "0", 0 },
    { "1", 1 },
};

static const struct match_table joy_input_table[] =
{
    { "pc joystick", JOY_PCJOY },
#ifdef KEYPAD_JOYSTICK
    { "joy keypad", JOY_KPAD },
    { "joy_keypad", JOY_KPAD },
#endif
    { 0, JOY_PCJOY }
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

    const char *apple2cfg = getenv("APPLE2IXCFG");
    if (apple2cfg) {
        config_filename = strdup(apple2cfg);
    } else {
        const char *homedir;

        homedir = getenv("HOME");
        config_filename = malloc(strlen(homedir) + 9);
        strcpy(config_filename, homedir);
        strcat(config_filename, "/.apple2");

        /* config_filename is left allocated for convinence in
         * save_settings */
    }

    {
        char *buffer = 0;
        size_t size = 0;

        FILE *config_file = fopen(config_filename, "r");
        if (config_file == NULL)
        {
            ERRLOG(
                "Warning. Cannot open the .apple2 system defaults file.\n"
                "Make sure it's readable in your home directory.");
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
                ERRLOG("Unrecognized config parameter `%s'", parameter);
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

            case PRM_DISK_PATH:
                strncpy(disk_path, argument, PATH_MAX-1);
                disk_path[PATH_MAX-1] = '\0';
                break;

            case PRM_HIRES_COLOR:
                color_mode = match(color_table, argument);
                break;

            case PRM_VIDEO_MODE:
                a2_video_mode = match(video_table, argument);
                break;

            case PRM_VOLUME:
                sound_volume = match(volume_table, argument);
                break;

            case PRM_CAPSLOCK:
                caps_lock = match(capslock_table, argument) ? true : false;
                break;

            case PRM_JOY_INPUT:
                joy_mode = match(joy_input_table, argument);
                break;

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

                joy_auto_recenter = strtol(argument, &argument, 10);

                break;
#endif

            case PRM_ROM_PATH:
                strncpy(system_path, argument, PATH_MAX-1);
                system_path[PATH_MAX-1] = '\0';
                break;
            }
        }

        GETLINE_FREE(buffer);

        fclose(config_file);
    }
}


/* Save the configuration */
bool save_settings(void)
{
    FILE *config_file = NULL;

    LOG("Saving preferences...");

#define PREFS_ERRPRINT() \
        ERRLOG( \
            "Cannot open the .apple2/apple2.cfg system defaults file for writing.\n" \
            "Make sure it has rw permission in your home directory.")

#define ERROR_SUBMENU_H 9
#define ERROR_SUBMENU_W 40
#ifdef INTERFACE_CLASSIC
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
#endif

    config_file = fopen(config_filename, "w");
    if (config_file == NULL)
    {
        PREFS_ERRPRINT();
#ifdef INTERFACE_CLASSIC
        c_interface_print_submenu_centered(submenu[0], ERROR_SUBMENU_W, ERROR_SUBMENU_H);
        while ((ch = c_mygetch(1)) == -1)
        {
        }
#endif
        return false;
    }

    int err = 0;
    err |= fprintf(config_file, "speed = %0.2lf\n", cpu_scale_factor);
    err |= fprintf(config_file, "altspeed = %0.2lf\n", cpu_altscale_factor);
    err |= fprintf(config_file, "disk path = %s\n", disk_path);
    err |= fprintf(config_file, "color = %s\n", reverse_match(color_table, color_mode));
    err |= fprintf(config_file, "video = %s\n", reverse_match(video_table, a2_video_mode));
    err |= fprintf(config_file, "volume = %s\n", reverse_match(volume_table, sound_volume));
    err |= fprintf(config_file, "caps lock = %s\n", reverse_match(capslock_table, (int)caps_lock));
    err |= fprintf(config_file, "joystick = %s\n", reverse_match(joy_input_table, joy_mode));
    err |= fprintf(config_file, "system path = %s\n", system_path);

#ifdef KEYPAD_JOYSTICK
    err |= fprintf(config_file, "keypad joystick parms = %d %u\n", joy_step, (joy_auto_recenter ? 1 : 0));
#endif

    if (err < 0) {
        PREFS_ERRPRINT();
#ifdef INTERFACE_CLASSIC
        c_interface_print_submenu_centered(submenu[0], ERROR_SUBMENU_W, ERROR_SUBMENU_H);
        while ((ch = c_mygetch(1)) == -1) {  }
#endif
        return false;
    }

    fclose(config_file);

    return true;
}

