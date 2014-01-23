/*
 * Apple // emulator for Linux: Configuration defines
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

#ifndef PREFS_H
#define PREFS_H

#ifndef __ASSEMBLER__

typedef enum joystick_mode_t {
    JOY_OFF = 0,
#ifdef KEYPAD_JOYSTICK
    JOY_KPAD,
#endif
#ifdef LINUX_JOYSTICK
    JOY_PCJOY,
#endif
    NUM_JOYOPTS
} joystick_mode_t;

typedef enum color_mode_t {
    COLOR_NONE = 0,
    /*LAZY_COLOR, deprecated*/
    COLOR,
    /*LAZY_INTERP, deprecated*/
    COLOR_INTERP,
    NUM_COLOROPTS
} color_mode_t;

typedef enum a2_video_mode_t {
    VIDEO_FULLSCREEN = 0,
    VIDEO_1X,
    VIDEO_2X,
    NUM_VIDOPTS
} a2_video_mode_t;

#define SYSSIZE         4096
extern char system_path[SYSSIZE];
#define DISKSIZE        4096
extern char disk_path[DISKSIZE];

extern int apple_mode; /* undocumented instructions or //e mode */
extern int sound_volume;
extern color_mode_t color_mode;
extern a2_video_mode_t a2_video_mode;

/* generic joystick settings */
extern joystick_mode_t joy_mode;

#ifdef KEYPAD_JOYSTICK
extern short joy_step;
extern uint8_t auto_recenter;
#endif

#ifdef LINUX_JOYSTICK
/* real joystick settings */
extern int js_center_x;
extern int js_center_y;
extern int js_max_x;
extern int js_max_y;
extern int js_min_x;
extern int js_min_y;
#endif /* LINUX_JOYSTICK */

/* functions in prefs.c */
extern void load_settings(void);
extern bool save_settings(void);

#endif /* !__ASSEMBLER__ */

/* values for apple_mode */
#define IIE_MODE 2
#define IIU_MODE 1
#define II_MODE  0

#endif /* PREFS_H */
