/*
 * Apple // emulator for *nix
 *
 * This software package is subject to the GNU General Public License
 * version 2 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * THERE ARE NO WARRANTIES WHATSOEVER.
 *
 */

/*
 * 65c02 CPU Timing Support.
 *
 * Copyleft 2013 Aaron Culliney
 *
 */

#ifndef _JOYSTICK_H_
#define _JOYSTICK_H_

#define JOY_RANGE 0x100
#define HALF_JOY_RANGE 0x80

extern uint16_t joy_x;
extern uint16_t joy_y;
extern uint8_t joy_button0;
extern uint8_t joy_button1;
extern uint8_t joy_button2;
extern bool joy_clip_to_radius;

#ifdef KEYPAD_JOYSTICK
extern uint8_t joy_auto_recenter;
extern short joy_step;
#endif

void c_joystick_reset(void);
#ifdef INTERFACE_CLASSIC
void c_calibrate_joystick(void);
#endif

#endif // whole file
