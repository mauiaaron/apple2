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
    bool trackingButtonMove;
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

    bool deepSleep = false;
    uint8_t currJoyButtonValue0 = 0x0;
    uint8_t currJoyButtonValue1 = 0x0;
    uint8_t currButtonDisplayChar = ' ';
    for (;;) {
        if (UNLIKELY(joyglobals.isShuttingDown)) {
            break;
        }

        struct timespec wait;
        clock_gettime(CLOCK_REALTIME, &wait);
        wait = timespec_add(wait, joys.tapDelayNanos);
        int timedOut = pthread_cond_timedwait(&joys.tapDelayCond, &joys.tapDelayMutex, &wait); // wait and possibly consume event
        assert((!timedOut || timedOut == ETIMEDOUT) && "should not fail any other way");

        if (!timedOut) {
            if (!deepSleep) {
                if (joys.trackingButtonMove) {
                    // dragging
                    currJoyButtonValue0 = 0x0;
                    currJoyButtonValue1 = 0x0;
                    currButtonDisplayChar = joys.currButtonDisplayChar;
                } else if (joys.trackingButton) {
                    // touch down -- delay consumption to determine if tap or drag
                    currJoyButtonValue0 = joys.currJoyButtonValue0;
                    currJoyButtonValue1 = joys.currJoyButtonValue1;
                    currButtonDisplayChar = joys.currButtonDisplayChar;
                    joys.buttonDrawCallback(currButtonDisplayChar);
                    // zero the buttons before delay
                    _reset_buttons_state();
                    continue;
                } else {
                    // touch up becomes tap
                    joys.currJoyButtonValue0 = currJoyButtonValue0;
                    joys.currJoyButtonValue1 = currJoyButtonValue1;
                    joys.currButtonDisplayChar = currButtonDisplayChar;
                }
            }
        }

        joy_button0 = joys.currJoyButtonValue0;
        joy_button1 = joys.currJoyButtonValue1;
        joys.buttonDrawCallback(joys.currButtonDisplayChar);

        deepSleep = false;
        if (timedOut && !joys.trackingButton) {
            deepSleep = true;
            _reset_buttons_state();
            TOUCH_JOY_LOG(">>> [DELAYEDTAP] end ...");
            pthread_cond_wait(&joys.tapDelayCond, &joys.tapDelayMutex); // consume initial touch down
            TOUCH_JOY_LOG(">>> [DELAYEDTAP] begin ...");
        } else {
            TOUCH_JOY_LOG(">>> [DELAYEDTAP] event looping ...");
        }
    }

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

static void _set_current_button_state(touchjoy_button_type_t theButtonChar) {
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
    _set_current_button_state(buttons.touchDownChar);
    joys.trackingButton = true;
    _signal_tap_delay();
}

static void touchjoy_buttonMove(int dx, int dy) {
    if ((dy < -joyglobals.switchThreshold) || (dy > joyglobals.switchThreshold)) {

        touchjoy_button_type_t theButtonChar = -1;
        if (dy < 0) {
            theButtonChar = buttons.northChar;
        } else {
            theButtonChar = buttons.southChar;
        }

        _set_current_button_state(theButtonChar);
        _signal_tap_delay();
    }
    joys.trackingButtonMove = true;
}

static void touchjoy_buttonUp(int dx, int dy) {
    joys.trackingButton = false;
    joys.trackingButtonMove = false;
    _signal_tap_delay();
}

static void gltouchjoy_setTapDelay(float secs) {
    if (UNLIKELY(secs < 0.f)) {
        ERRLOG("Clamping tap delay to 0.0 secs");
    }
    if (UNLIKELY(secs > 1.f)) {
        ERRLOG("Clamping tap delay to 1.0 secs");
    }
    unsigned int tapDelayNanos = (unsigned int)((float)NANOSECONDS_PER_SECOND * secs);
#define MIN_WAIT_NANOS 1000000
    if (tapDelayNanos < MIN_WAIT_NANOS) {
        tapDelayNanos = MIN_WAIT_NANOS;
    }
    joys.tapDelayNanos = tapDelayNanos;
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

