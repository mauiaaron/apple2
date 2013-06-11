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

#define	SYSSIZE		4096
extern char system_path[SYSSIZE];
#define	DISKSIZE	4096
extern char disk_path[DISKSIZE];

extern int apple_mode; /* undocumented instructions or //e mode */
extern int sound_mode; /* PC speaker or OFF */
extern int color_mode;

/* generic joystick settings */
extern short joy_mode;
extern short joy_step;
extern short joy_center_x;
extern short joy_center_y;
extern short joy_range;
extern short half_joy_range;

#ifdef PC_JOYSTICK
/* real joystick settings */
extern int js_center_x;
extern int js_center_y;
extern long js_timelimit;
extern int js_max_x;
extern int js_max_y;
extern int js_min_x;
extern int js_min_y;
#endif /* PC_JOYSTICK */

/* functions in prefs.c */
extern void load_settings(void);
extern void save_settings(void);

#endif /* !__ASSEMBLER__ */

/* values for apple_mode */
#define IIE_MODE 2
#define IIU_MODE 1
#define II_MODE  0

/* values for color_mode */
#define NO_COLOR 0
#define LAZY_COLOR 1
#define COLOR 2
#define LAZY_INTERP 3
#define INTERP 4

/* values for joy_mode */
#define JOY_OFF     0
#define JOY_KYBD    1
#define JOY_DIGITAL 2
#define JOY_PCJOY   3

#endif /* PREFS_H */
