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

#ifndef _JOYSTICK_H_
#define _JOYSTICK_H_

#define JOY_RANGE 0x100
#define HALF_JOY_RANGE (JOY_RANGE>>1)
#define QUARTER_JOY_RANGE (HALF_JOY_RANGE>>1)

typedef enum joystick_mode_t {
    JOY_PCJOY = 0,
#ifdef KEYPAD_JOYSTICK
    JOY_KPAD,
#endif
    NUM_JOYOPTS
} joystick_mode_t;

#define JOY_MODE_DEFAULT JOY_PCJOY
static inline joystick_mode_t getJoyMode(long lVal) {
    if (lVal < 0 || lVal >= NUM_JOYOPTS) {
        lVal = JOY_MODE_DEFAULT;
    }
    return (joystick_mode_t)lVal;
}

extern joystick_mode_t joy_mode;

extern uint16_t joy_x;
extern uint16_t joy_y;
extern bool joy_clip_to_radius;

#ifdef KEYPAD_JOYSTICK
extern bool joy_auto_recenter;
extern short joy_step;
#endif

void joystick_reset(void);

#ifdef INTERFACE_CLASSIC
void c_calibrate_joystick(void);
#endif

// enable/disable gamepad clamping to joystick corners
void joydriver_setClampBeyondRadius(bool clamp);

// set joystick axis values
void joydriver_setAxisValue(uint8_t x, uint8_t y);

// return X axis value
uint8_t joydriver_getAxisX(void);

// return Y axis value
uint8_t joydriver_getAxisY(void);

// set button 0 pressed
void joydriver_setButton0Pressed(bool pressed);

// set button 1 pressed
void joydriver_setButton1Pressed(bool pressed);

// backend joystick driver reset procedure
extern void (*joydriver_resetJoystick)(void);

#endif // whole file
