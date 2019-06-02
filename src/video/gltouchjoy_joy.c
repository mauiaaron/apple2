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

#include "video/gltouchjoy.h"

#if !INTERFACE_TOUCH
#error this is a touch interface module, possibly you mean to not compile this at all?
#endif

static GLTouchJoyVariant joyVariant = { 0 };

static struct {
    video_frame_callback_fn frameCallback;
    volatile unsigned long spinlock;
    unsigned long tapDelayFrameCount;

    interface_touch_event_t currEventType;

    touchjoy_button_type_t touchDownChar;
    touchjoy_button_type_t northChar;
    touchjoy_button_type_t southChar;

    uint8_t currJoyButtonValue0;
    uint8_t currJoyButtonValue1;
    uint8_t currButtonDisplayChar;
    uint8_t lastButtonDisplayChar;

    bool buttonThresholdExceeded;
    bool justTapConfigured;
} joys = { 0 };

// ----------------------------------------------------------------------------

static interface_device_t touchjoy_variant(void) {
    return TOUCH_DEVICE_JOYSTICK;
}

static inline void _reset_button_state(void) {
    run_args.joy_button0 = 0;
    run_args.joy_button1 = 0;
}

static inline void _reset_axis_state(void) {
    joy_x = HALF_JOY_RANGE;
    joy_y = HALF_JOY_RANGE;
}

static void touchjoy_resetState(void) {
    TOUCH_JOY_LOG("\t\tjoy resetState");

    _reset_axis_state();
    //NOTE : do not reset button state here, it may interfere with other code performing reboot/reset
    joys.spinlock = SPINLOCK_INIT;
    joys.frameCallback = NULL;
    joys.tapDelayFrameCount = 0;
}

static void touchjoy_setup(void) {
    // ...
}

static void touchjoy_shutdown(void) {
    // ...
}

// ----------------------------------------------------------------------------
// axis state machine

static void touchjoy_axisDown(void) {
    _reset_axis_state();
}

static void touchjoy_axisMove(int x, int y) {
    x = (int) ((float)x * joyglobals.axisMultiplier);
    y = (int) ((float)y * joyglobals.axisMultiplier);

    x += 0x80;
    y += 0x80;

    if (x < 0) {
        x = 0;
    }
    else if (x > 0xff) {
        x = 0xff;
    }
    if (y < 0) {
        y = 0;
    }
    else if (y > 0xff) {
        y = 0xff;
    }

    joy_x = x;
    joy_y = y;
}

static void touchjoy_axisUp(int x, int y) {
    //x = (int) ((float)x * joyglobals.axisMultiplier);
    //y = (int) ((float)y * joyglobals.axisMultiplier);
    (void)x;
    (void)y;
    _reset_axis_state();
}

// ----------------------------------------------------------------------------
// button state machine

static void _fire_current_buttons(void) {
    TOUCH_JOY_LOG("\t\t\tfire buttons 0:%02x 1:%02X char:%02x", joys.currJoyButtonValue0, joys.currJoyButtonValue1, joys.currButtonDisplayChar);
    run_args.joy_button0 = joys.currJoyButtonValue0;
    run_args.joy_button1 = joys.currJoyButtonValue1;
    joys.lastButtonDisplayChar = joys.currButtonDisplayChar;
}

static void _signal_tap_delay_event(interface_touch_event_t eventType, touchjoy_button_type_t theButtonChar) {

    joys.currEventType = eventType;

    if (theButtonChar == TOUCH_BUTTON1) {
        joys.currJoyButtonValue0 = 0x80;
        joys.currJoyButtonValue1 = 0;
        joys.currButtonDisplayChar = MOUSETEXT_OPENAPPLE;
    } else if (theButtonChar == TOUCH_BUTTON2) {
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
        //joys.currButtonDisplayChar = ' '; -- avoid truncating normal render cycle
    }
}

// End-of-video-frame callback (handling tap delay and auto-fire)
static void touchjoy_frameCallback(uint8_t textFlashCounter) {
    (void)textFlashCounter;

    TOUCH_JOY_LOG("\t\t+++joy frameCallback");

    // When activated, this is called every video frame -- ~16.688 millis

    ASSERT_ON_CPU_THREAD();

    SPIN_LOCK_FULL(&joys.spinlock);
    do {
        ++joys.tapDelayFrameCount;
        TOUCH_JOY_LOG("\t\t+++joy frameCallback acquire (%lu)", joys.tapDelayFrameCount);
        if (joys.tapDelayFrameCount < joyglobals.tapDelayFrames) {
            break;
        }

        _fire_current_buttons();

        if (joys.currEventType == TOUCH_UP) {
            // unregister callback ...
            joys.frameCallback = NULL;
            TOUCH_JOY_LOG("\t\t+++joy callback done");
        }
    } while (0);

    TOUCH_JOY_LOG("\t\t+++joy frameCallback release");
    SPIN_UNLOCK_FULL(&joys.spinlock);
}

static void touchjoy_buttonDown(void) {
    TOUCH_JOY_LOG("\t\tjoy buttonDown");

    SPIN_LOCK_FULL(&joys.spinlock);
    TOUCH_JOY_LOG("\t\tjoy buttonDown acquire");

    joys.buttonThresholdExceeded = false;
    joys.tapDelayFrameCount = 0UL;

    _reset_button_state();
    _signal_tap_delay_event(TOUCH_DOWN, joys.touchDownChar);

    if (joyglobals.tapDelayFrames == 0 || joys.justTapConfigured) {
        // unambiguous intent : no tap delay or only the tap button is configured
        _fire_current_buttons();
    }

    // hook into end-of-video-frame callback
    joys.frameCallback = &touchjoy_frameCallback;

    TOUCH_JOY_LOG("\t\tjoy buttonDown release");
    SPIN_UNLOCK_FULL(&joys.spinlock);
}

static void _touchjoy_buttonMove(int dx, int dy) {

    bool shouldFire = false;
    touchjoy_button_type_t theButtonChar = joys.touchDownChar;

    int delta = abs(dy);
    if (!joys.buttonThresholdExceeded) {
        if (delta >= joyglobals.switchThreshold) {
            // unambiguous intent : user swiped beyond threshold
            joys.buttonThresholdExceeded = true;
            shouldFire = true;
            theButtonChar = (dy < 0) ? joys.northChar : joys.southChar;
        }
    } else {

        if (delta < joyglobals.switchThreshold) {
            // 2019/04/20 NOTE: originally we did not re-zero back to touchDownChar ... this allowed a progression between
            // southChar/northChar (or vice-versa)
            //
            // touchDownChar should be fired on a tap or long-press (once we swipe beyond the threshold, we should only switch
            // between northChar and southChar)

            //shouldFire = true;
            joys.buttonThresholdExceeded = false;
        }
    }

    if (shouldFire) {
        // immediately fire current button(s) upon threshold [re]-change ... and remove delay for auto-fire
        _signal_tap_delay_event(TOUCH_MOVE, theButtonChar);
        _fire_current_buttons();
    }
}

static void touchjoy_buttonMove(int dx, int dy) {
    TOUCH_JOY_LOG("\t\tjoy buttonMove");

    SPIN_LOCK_FULL(&joys.spinlock);
    TOUCH_JOY_LOG("\t\tjoy buttonMove acquire");

    _touchjoy_buttonMove(dx, dy);

    TOUCH_JOY_LOG("\t\tjoy buttonMove release");
    SPIN_UNLOCK_FULL(&joys.spinlock);
}

static void touchjoy_buttonUp(int dx, int dy) {
    TOUCH_JOY_LOG("\t\tjoy buttonUp");

    SPIN_LOCK_FULL(&joys.spinlock);
    TOUCH_JOY_LOG("\t\tjoy buttonUp acquire");

    bool ignored = false;
    _touchjoy_buttonMove(dx, dy);

    _fire_current_buttons();

    // force CPU thread callback into recount before unsetting button(s) state ... this allows time for CPU thread to consume current button(s)
    joys.tapDelayFrameCount = 0UL;

    _signal_tap_delay_event(TOUCH_UP, TOUCH_NONE);

    TOUCH_JOY_LOG("\t\tjoy buttonUp release");
    SPIN_UNLOCK_FULL(&joys.spinlock);
}

static void touchjoy_prefsChanged(const char *domain) {
    assert(video_isRenderThread());

    long lVal = 0;

    joys.touchDownChar = prefs_parseLongValue(domain, PREF_JOY_TOUCHDOWN_CHAR, &lVal, /*base:*/10) ? lVal : TOUCH_BUTTON1;
    joys.northChar = prefs_parseLongValue(domain, PREF_JOY_SWIPE_NORTH_CHAR, &lVal, /*base:*/10) ? lVal : TOUCH_BOTH;
    joys.southChar = prefs_parseLongValue(domain, PREF_JOY_SWIPE_SOUTH_CHAR, &lVal, /*base:*/10) ? lVal : TOUCH_BUTTON2;

    joys.justTapConfigured = (joys.touchDownChar != TOUCH_NONE) &&
                             (joys.northChar     == TOUCH_NONE) &&
                             (joys.southChar     == TOUCH_NONE);
}

static uint8_t *touchjoy_axisRosetteChars(void) {
    static uint8_t rosetteChars[ROSETTE_ROWS * ROSETTE_COLS] = {
        ' ', MOUSETEXT_UP, ' ',
        MOUSETEXT_LEFT, ICONTEXT_MENU_TOUCHJOY, MOUSETEXT_RIGHT,
        ' ', MOUSETEXT_DOWN, ' ',
    };
    return rosetteChars;
}

static uint8_t *touchjoy_buttRosetteChars(void) {
    return NULL;
}

static uint8_t touchjoy_buttActiveChar(void) {
    return joys.lastButtonDisplayChar;
}

// ----------------------------------------------------------------------------

static void _init_gltouchjoy_joy(void) {
    LOG("Registering OpenGL software touch joystick (joystick variant)");

    joyVariant.variant = &touchjoy_variant;
    joyVariant.resetState = &touchjoy_resetState;
    joyVariant.setup = &touchjoy_setup;
    joyVariant.shutdown = &touchjoy_shutdown;

    joyVariant.prefsChanged = &touchjoy_prefsChanged;

    joyVariant.buttonDown = &touchjoy_buttonDown;
    joyVariant.buttonMove = &touchjoy_buttonMove;
    joyVariant.buttonUp = &touchjoy_buttonUp;

    joyVariant.axisDown = &touchjoy_axisDown;
    joyVariant.axisMove = &touchjoy_axisMove;
    joyVariant.axisUp = &touchjoy_axisUp;

    joyVariant.axisRosetteChars = &touchjoy_axisRosetteChars;
    joyVariant.buttRosetteChars = &touchjoy_buttRosetteChars;
    joyVariant.buttActiveChar = &touchjoy_buttActiveChar;

    gltouchjoy_registerVariant(TOUCH_DEVICE_JOYSTICK, &joyVariant);

    video_registerFrameCallback(&joys.frameCallback);
}

static __attribute__((constructor)) void __init_gltouchjoy_joy(void) {
    emulator_registerStartupCallback(CTOR_PRIORITY_EARLY, &_init_gltouchjoy_joy);
}

