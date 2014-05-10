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

uint16_t joy_x;
uint16_t joy_y;
uint8_t joy_button0;
uint8_t joy_button1;
uint8_t joy_button2;

void c_open_joystick();
void c_close_joystick();
void c_calibrate_joystick();
void c_joystick_reset();

#endif // whole file
