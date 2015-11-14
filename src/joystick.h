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

#if INTERFACE_TOUCH

typedef enum touchjoy_variant_t {
    EMULATED_NONE = 0,
    EMULATED_JOYSTICK, // touch interface emulates a physical joystick device
    EMULATED_KEYPAD,   // touch interface generates key presses
} touchjoy_variant_t;

typedef enum touchjoy_button_type_t {
    TOUCH_NONE = -1,
    TOUCH_BUTTON0 = 0,
    TOUCH_BUTTON1,
    TOUCH_BOTH,
    // --or-- an ASCII/fonttext value ...
} touchjoy_button_type_t;

#define ROSETTE_ROWS 3
#define ROSETTE_COLS 3

enum {
    ROSETTE_NORTHWEST=0,
    ROSETTE_NORTH,
    ROSETTE_NORTHEAST,
    ROSETTE_WEST,
    ROSETTE_CENTER,
    ROSETTE_EAST,
    ROSETTE_SOUTHWEST,
    ROSETTE_SOUTH,
    ROSETTE_SOUTHEAST,
};

// is the touch joystick available
extern bool (*joydriver_isTouchJoystickAvailable)(void);

// enable/disable touch joystick
extern void (*joydriver_setTouchJoystickEnabled)(bool enabled);

// grant/remove ownership of touch screen
extern void (*joydriver_setTouchJoystickOwnsScreen)(bool pwnd);

// query touch screen ownership
extern bool (*joydriver_ownsScreen)(void);

/*
 * set the joystick button types/visuals (scancodes are fired for EMULATED_KEYPAD variant)
 *
 *  - for EMULATED_JOYSTICK, there is an implicit extra layer-of-indirection for the touchjoy_button_type_t, which maps
 *  to the open apple, closed apple, or "both" visual keys
 *
 *  - for EMULATED_KEYPAD, the touchjoy_button_type_t is the displayed visual (as ASCII value and lookup into font
 *  table)
 */
extern void (*joydriver_setTouchButtonTypes)(
        touchjoy_button_type_t touchDownChar, int downScancode,
        touchjoy_button_type_t northChar, int northScancode,
        touchjoy_button_type_t southChar, int southScancode);

// set the button tap delay (to differentiate between single tap and north/south/etc swipe)
extern void (*joydriver_setTapDelay)(float secs);

// set the sensitivity multiplier
extern void (*joydriver_setTouchAxisSensitivity)(float multiplier);

// set the touch button switch threshold
extern void (*joydriver_setButtonSwitchThreshold)(int delta);

// set the joystick variant
extern void (*joydriver_setTouchVariant)(touchjoy_variant_t variant);

// get the joystick variant
extern touchjoy_variant_t (*joydriver_getTouchVariant)(void);

// set the axis visuals (scancodes are fired for EMULATED_KEYPAD variant)
extern void (*joydriver_setTouchAxisTypes)(uint8_t rosetteChars[(ROSETTE_ROWS * ROSETTE_COLS)], int rosetteScancodes[(ROSETTE_ROWS * ROSETTE_COLS)]);

// set screen divide between axis and buttons
extern void (*joydriver_setScreenDivision)(float division);

// swap axis and buttons sides
extern void (*joydriver_setAxisOnLeft)(bool axisIsOnLeft);

// begin calibration mode
extern void (*joydriver_beginCalibration)(void);

// end calibration mode
extern void (*joydriver_endCalibration)(void);

// end calibration mode
extern bool (*joydriver_isCalibrating)(void);

// set controls visibility
extern void (*joydriver_setShowControls)(bool showControls);

// set key repeat threshold (keypad joystick)
extern void (*joydriver_setKeyRepeatThreshold)(float repeatThresholdSecs);

#endif // INTERFACE_TOUCH

#endif // whole file
