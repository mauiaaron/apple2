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
    EMULATED_JOYSTICK = 0, // touch interface emulates a physical joystick device
    EMULATED_KEYPAD,       // touch interface generates key presses
} touchjoy_variant_t;

typedef enum touchjoy_button_type_t {
    TOUCH_NONE = -1,
    TOUCH_BUTTON0 = 0,
    TOUCH_BUTTON1,
    TOUCH_BOTH,
} touchjoy_button_type_t;

// is the touch joystick available
extern bool (*joydriver_isTouchJoystickAvailable)(void);

// enable/disable touch joystick
extern void (*joydriver_setTouchJoystickEnabled)(bool enabled);

// grant/remove ownership of touch screen
extern void (*joydriver_setTouchJoystickOwnsScreen)(bool pwnd);

// query touch screen ownership
extern bool (*joydriver_ownsScreen)(void);

// set the joystick button visuals (these values are also fired for keyboard variant)
extern void (*joydriver_setTouchButtonValues)(char button0Val, char button1Val, char buttonBothVal);

// set the joystick button types (for joystick variant)
extern void (*joydriver_setTouchButtonTypes)(touchjoy_button_type_t down, touchjoy_button_type_t north, touchjoy_button_type_t south);

// set the tap delay (to differentiate between single tap and north/south/etc swipe)
extern void (*joydriver_setTapDelay)(float secs);

// set the touch axis sensitivity multiplier
extern void (*joydriver_setTouchAxisSensitivity)(float multiplier);

// set the touch button switch threshold
extern void (*joydriver_setButtonSwitchThreshold)(int delta);

// set the joystick variant
extern void (*joydriver_setTouchVariant)(touchjoy_variant_t variant);

// get the joystick variant
extern touchjoy_variant_t (*joydriver_getTouchVariant)(void);

// set the axis visuals (these key values are also fired for keyboard variant)
extern void (*joydriver_setTouchAxisValues)(char north, char west, char east, char south);

// set screen divide between axis and buttons
extern void (*joydriver_setScreenDivision)(float division);

// swap axis and buttons sides
extern void (*joydriver_setAxisOnLeft)(bool axisIsOnLeft);

// begin calibration mode
extern void (*joydriver_beginCalibration)(void);

// end calibration mode
extern void (*joydriver_endCalibration)(void);

#endif // INTERFACE_TOUCH

#endif // whole file
