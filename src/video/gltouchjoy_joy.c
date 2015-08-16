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

#include "video/gltouchjoy.h"

#if !INTERFACE_TOUCH
#error this is a touch interface module, possibly you mean to not compile this at all?
#endif

static GLTouchJoyVariant happyHappyJoyJoy = { 0 };

static struct {
    uint8_t currJoyButtonValue0;
    uint8_t currJoyButtonValue1;
    uint8_t currButtonDisplayChar;
} joys;

static touchjoy_variant_t touchjoy_variant(void) {
    return EMULATED_JOYSTICK;
}

static inline void _reset_axis_state(void) {
    joy_x = HALF_JOY_RANGE;
    joy_y = HALF_JOY_RANGE;
}

static inline void _reset_buttons_state(void) {
    joy_button0 = 0x0;
    joy_button1 = 0x0;
    joys.currJoyButtonValue0 = 0;
    joys.currJoyButtonValue1 = 0;
    joys.currButtonDisplayChar = ' ';
}

static void touchjoy_resetState(void) {
    _reset_axis_state();
    _reset_buttons_state();
}

// ----------------------------------------------------------------------------
// axis state

static void touchjoy_axisDown(void) {
    _reset_axis_state();
}

static void touchjoy_axisMove(int x, int y) {
    if (axes.multiplier != 1.f) {
        x = (int) ((float)x * axes.multiplier);
        y = (int) ((float)y * axes.multiplier);
    }

    x += 0x80;
    y += 0x80;

    if (x < 0) {
        x = 0;
    }
    if (x > 0xff) {
        x = 0xff;
    }
    if (y < 0) {
        y = 0;
    }
    if (y > 0xff) {
        y = 0xff;
    }

    joy_x = x;
    joy_y = y;
}

static void touchjoy_axisUp(int x, int y) {
    _reset_axis_state();
}

// ----------------------------------------------------------------------------
// button state

static void touchjoy_setCurrButtonValue(touchjoy_button_type_t theButtonChar, int theButtonScancode) {
    if (theButtonChar == TOUCH_BUTTON0) {
        joys.currJoyButtonValue0 = 0x80;
        joys.currJoyButtonValue1 = 0;
        joys.currButtonDisplayChar = MOUSETEXT_OPENAPPLE;
    } else if (theButtonChar == TOUCH_BUTTON1) {
        joys.currJoyButtonValue0 = 0;
        joys.currJoyButtonValue1 = 0x80;
        joys.currButtonDisplayChar = MOUSETEXT_CLOSEDAPPLE;
    } else if (theButtonChar == TOUCH_BOTH) {
        joys.currJoyButtonValue0 = 0x80;
        joys.currJoyButtonValue1 = 0x80;
        joys.currButtonDisplayChar = '+';
    } else {
        joys.currJoyButtonValue0 = 0;
        joys.currJoyButtonValue1 = 0;
        joys.currButtonDisplayChar = ' ';
    }
}

static uint8_t touchjoy_buttonPress(void) {
    joy_button0 = joys.currJoyButtonValue0;
    joy_button1 = joys.currJoyButtonValue1;
    return joys.currButtonDisplayChar;
}

static void touchjoy_buttonRelease(void) {
    _reset_buttons_state();
}

// ----------------------------------------------------------------------------

__attribute__((constructor(CTOR_PRIORITY_EARLY)))
static void _init_gltouchjoy_joy(void) {
    LOG("Registering OpenGL software touch joystick (joystick variant)");

    happyHappyJoyJoy.variant = &touchjoy_variant,
    happyHappyJoyJoy.resetState = &touchjoy_resetState,

    happyHappyJoyJoy.setCurrButtonValue = &touchjoy_setCurrButtonValue,
    happyHappyJoyJoy.buttonPress = &touchjoy_buttonPress,
    happyHappyJoyJoy.buttonRelease = &touchjoy_buttonRelease,

    happyHappyJoyJoy.axisDown = &touchjoy_axisDown,
    happyHappyJoyJoy.axisMove = &touchjoy_axisMove,
    happyHappyJoyJoy.axisUp = &touchjoy_axisUp,

    gltouchjoy_registerVariant(EMULATED_JOYSTICK, &happyHappyJoyJoy);
}

