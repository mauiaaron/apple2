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

#define BUTTON_TAP_DELAY_NANOS_DEFAULT 50000000

static GLTouchJoyVariant happyHappyJoyJoy = { 0 };

static struct {
    uint8_t currJoyButtonValue0;
    uint8_t currJoyButtonValue1;
    uint8_t currButtonDisplayChar;
    void (*buttonDrawCallback)(char newChar);

    bool trackingButton;
    pthread_t tapDelayThreadId;
    pthread_mutex_t tapDelayMutex;
    pthread_cond_t tapDelayCond;
    unsigned int tapDelayNanos;
} joys;

// ----------------------------------------------------------------------------

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

// Tap Delay Thread : delays processing of touch-down so that a different joystick button can be fired

static inline void _signal_tap_delay(void) {
    pthread_mutex_lock(&joys.tapDelayMutex);
    pthread_cond_signal(&joys.tapDelayCond);
    pthread_mutex_unlock(&joys.tapDelayMutex);
}

static void *_button_tap_delayed_thread(void *dummyptr) {
    LOG(">>> [DELAYEDTAP] thread start ...");

    pthread_mutex_lock(&joys.tapDelayMutex);

    do {
        pthread_cond_wait(&joys.tapDelayCond, &joys.tapDelayMutex);
        TOUCH_JOY_LOG(">>> [DELAYEDTAP] begin ...");

        if (UNLIKELY(joyglobals.isShuttingDown)) {
            break;
        }

        struct timespec ts = { .tv_sec=0, .tv_nsec=joys.tapDelayNanos };

        // sleep for the configured delay time
        pthread_mutex_unlock(&joys.tapDelayMutex);
        nanosleep(&ts, NULL);
        pthread_mutex_lock(&joys.tapDelayMutex);

        // wait until touch up/cancel
        do {

            // now set the joystick button values
            joy_button0 = joys.currJoyButtonValue0;
            joy_button1 = joys.currJoyButtonValue1;
            joys.buttonDrawCallback(joys.currButtonDisplayChar);

            if (!joys.trackingButton || joyglobals.isShuttingDown) {
                break;
            }

            pthread_cond_wait(&joys.tapDelayCond, &joys.tapDelayMutex);

            if (!joys.trackingButton || joyglobals.isShuttingDown) {
                break;
            }
            TOUCH_JOY_LOG(">>> [DELAYEDTAP] looping ...");
        } while (1);

        if (UNLIKELY(joyglobals.isShuttingDown)) {
            break;
        }

        // delay the ending of button tap or touch/move event by the configured delay time
        pthread_mutex_unlock(&joys.tapDelayMutex);
        nanosleep(&ts, NULL);
        pthread_mutex_lock(&joys.tapDelayMutex);

        _reset_buttons_state();

        TOUCH_JOY_LOG(">>> [DELAYEDTAP] end ...");
    } while (1);

    pthread_mutex_unlock(&joys.tapDelayMutex);

    LOG(">>> [DELAYEDTAP] thread exit ...");

    return NULL;
}

static void touchjoy_setup(void (*buttonDrawCallback)(char newChar)) {
    assert((joys.tapDelayThreadId == 0) && "setup called multiple times!");
    joys.buttonDrawCallback = buttonDrawCallback;
    pthread_create(&joys.tapDelayThreadId, NULL, (void *)&_button_tap_delayed_thread, (void *)NULL);
}

static void touchjoy_shutdown(void) {
    pthread_cond_signal(&joys.tapDelayCond);
    if (pthread_join(joys.tapDelayThreadId, NULL)) {
        ERRLOG("OOPS: pthread_join tap delay thread ...");
    }
    joys.tapDelayThreadId = 0;
    joys.tapDelayMutex = (pthread_mutex_t){ 0 };
    joys.tapDelayCond = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
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

static void _set_current_button_state(touchjoy_button_type_t theButtonChar, int theButtonScancode) {
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

static void touchjoy_buttonDown(void) {
    _set_current_button_state(buttons.touchDownChar, buttons.touchDownScancode);
    joys.trackingButton = true;
    _signal_tap_delay();
}

static void touchjoy_buttonMove(int dx, int dy) {
    if ((dy < -joyglobals.switchThreshold) || (dy > joyglobals.switchThreshold)) {
        touchjoy_button_type_t theButtonChar = -1;
        int theButtonScancode = -1;
        if (dy < 0) {
            theButtonChar = buttons.northChar;
            theButtonScancode = buttons.northScancode;
        } else {
            theButtonChar = buttons.southChar;
            theButtonScancode = buttons.southScancode;
        }

        _set_current_button_state(theButtonChar, theButtonScancode);
        _signal_tap_delay();
    }
}

static void touchjoy_buttonUp(int dx, int dy) {
    joys.trackingButton = false;
    _signal_tap_delay();
}

static void gltouchjoy_setTapDelay(float secs) {
    if (UNLIKELY(secs < 0.f)) {
        ERRLOG("Clamping tap delay to 0.0 secs");
    }
    if (UNLIKELY(secs > 1.f)) {
        ERRLOG("Clamping tap delay to 1.0 secs");
    }
    joys.tapDelayNanos = (unsigned int)((float)NANOSECONDS_PER_SECOND * secs);
}

// ----------------------------------------------------------------------------

__attribute__((constructor(CTOR_PRIORITY_EARLY)))
static void _init_gltouchjoy_joy(void) {
    LOG("Registering OpenGL software touch joystick (joystick variant)");

    happyHappyJoyJoy.variant = &touchjoy_variant,
    happyHappyJoyJoy.resetState = &touchjoy_resetState,
    happyHappyJoyJoy.setup = &touchjoy_setup,
    happyHappyJoyJoy.shutdown = &touchjoy_shutdown,

    happyHappyJoyJoy.buttonDown = &touchjoy_buttonDown,
    happyHappyJoyJoy.buttonMove = &touchjoy_buttonMove,
    happyHappyJoyJoy.buttonUp = &touchjoy_buttonUp,

    happyHappyJoyJoy.axisDown = &touchjoy_axisDown,
    happyHappyJoyJoy.axisMove = &touchjoy_axisMove,
    happyHappyJoyJoy.axisUp = &touchjoy_axisUp,

    joys.tapDelayMutex = (pthread_mutex_t){ 0 };
    joys.tapDelayCond = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
    joys.tapDelayNanos = BUTTON_TAP_DELAY_NANOS_DEFAULT;

    joydriver_setTapDelay = &gltouchjoy_setTapDelay;

    gltouchjoy_registerVariant(EMULATED_JOYSTICK, &happyHappyJoyJoy);
}

