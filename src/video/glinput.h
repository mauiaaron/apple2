/*
 * Apple // emulator for *ix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 2013-2015 Aaron Culliney
 *
 */

//void gldriver_mouse(int button, int state, int x, int y);

//void gldriver_mouse_drag(int x, int y);

// regular key down function
void gldriver_on_key_down(unsigned char key, int x, int y);

// regular key up function
void gldriver_on_key_up(unsigned char key, int x, int y);

// special key down function
void gldriver_on_key_special_down(int key, int x, int y);

// special key up function
void gldriver_on_key_special_up(int key, int x, int y);

// reset joystick function
void _glutJoystickReset(void);

