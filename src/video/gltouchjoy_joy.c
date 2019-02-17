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

#define BUTTON_TAP_DELAY_NANOS_DEFAULT   (NANOSECONDS_PER_SECOND/20)     // 0.2 secs
#define BUTTON_TAP_DELAY_NANOS_MIN       (NANOSECONDS_PER_SECOND/10000)  // 0.0001 secs

typedef struct touch_event_s {
    struct touch_event_s *next;
    interface_touch_event_t event;
    uint8_t currJoyButtonValue0;
    uint8_t currJoyButtonValue1;
    uint8_t currButtonDisplayChar;
} touch_event_s;

static touch_event_s *touchEventQ = NULL;

static GLTouchJoyVariant joyVariant = { 0 };

static struct {
    void (*buttonDrawCallback)(char newChar);

    pthread_t tapDelayThreadId;
    pthread_mutex_t tapDelayMutex;
    pthread_cond_t tapDelayCond;
    unsigned int tapDelayNanos;

    touchjoy_button_type_t touchDownChar;
    touchjoy_button_type_t northChar;
    touchjoy_button_type_t southChar;
} joys = { 0 };

// ----------------------------------------------------------------------------

static interface_device_t touchjoy_variant(void) {
    return TOUCH_DEVICE_JOYSTICK;
}

static inline void _reset_axis_state(void) {
    joy_x = HALF_JOY_RANGE;
    joy_y = HALF_JOY_RANGE;
}

static inline void _reset_buttons_state(void) {
    run_args.joy_button0 = 0x0;
    run_args.joy_button1 = 0x0;
}

static void touchjoy_resetState(void) {
    _reset_axis_state();
    //_reset_buttons_state(); -- do not reset button state here, it may interfere with other code performing reboot/reset
}

// ----------------------------------------------------------------------------

// Tap Delay Thread : implements a gesture recognizer that differentiates between a "long touch", "tap", and "swipe
// down/up" gestures.  Necessarily delays processing of initial touch down event to make a proper determination.  Also
// delays resetting joystick button state after touch up event to avoid resetting joystick button state too soon.
//
//  * long touch and tap are interpreted as one (configurable) joystick button fire event
//  * swipe up and swipe down are the other two (configurable) joystick button fire events
//

static struct timespec *_tap_wait(void) {
    static struct timespec wait = { 0 };
    clock_gettime(CLOCK_REALTIME, &wait);
    wait = timespec_add(wait, joys.tapDelayNanos);
    return &wait;
}

static void *_button_tap_delayed_thread(void *dummyptr) {
    TRACE_INTERFACE_MARK("_button_tap_delayed_thread ...");
    LOG(">>> [DELAYEDTAP] thread start ...");

    pthread_mutex_lock(&joys.tapDelayMutex);

    int timedOut = ETIMEDOUT;
    for (;;) {
        if (UNLIKELY(emulator_isShuttingDown())) {
            break;
        }

        if (timedOut) {
            // reset state and deep sleep waiting for touch down
            _reset_buttons_state();
            TOUCH_JOY_GESTURE_LOG(">>> [DELAYEDTAP] deep sleep ...");
            pthread_cond_wait(&joys.tapDelayCond, &joys.tapDelayMutex);
        } else {
            // delays reset of button state while remaining ready to process a touch down
            TOUCH_JOY_GESTURE_LOG(">>> [DELAYEDTAP] event looping ...");
            timedOut = pthread_cond_timedwait(&joys.tapDelayCond, &joys.tapDelayMutex, _tap_wait()); // wait and possibly consume event
            assert((!timedOut || timedOut == ETIMEDOUT) && "should not fail any other way");
            if (timedOut) {
                // reset state and go into deep sleep
                continue;
            }
            _reset_buttons_state();
        }

        if (UNLIKELY(emulator_isShuttingDown())) {
            break;
        }

        TOUCH_JOY_GESTURE_LOG(">>> [DELAYEDTAP] touch down ...");

        touch_event_s *touchCurrEvent = NULL;
        touch_event_s *touchPrevEvent = touchEventQ;
        assert(touchPrevEvent && "should be a touch event ready to consume");
        touchEventQ = touchEventQ->next;
        assert(touchPrevEvent->event == TOUCH_DOWN && "event queue head should be a touch down event");

        for (;;) {
            // delay processing of touch down to perform simple gesture recognition
            timedOut = pthread_cond_timedwait(&joys.tapDelayCond, &joys.tapDelayMutex, _tap_wait());
            assert((!timedOut || timedOut == ETIMEDOUT) && "should not fail any other way");

            if (UNLIKELY(emulator_isShuttingDown())) {
                break;
            }

            touchCurrEvent = touchEventQ;
            if (!touchCurrEvent) {
                assert(timedOut);
                // touch-down-and-hold
                TOUCH_JOY_GESTURE_LOG(">>> [DELAYEDTAP] long touch ...");
                run_args.joy_button0 = touchPrevEvent->currJoyButtonValue0;
                run_args.joy_button1 = touchPrevEvent->currJoyButtonValue1;
                joys.buttonDrawCallback(touchPrevEvent->currButtonDisplayChar);
                continue;
            }
            touchEventQ = touchEventQ->next;

            if (touchCurrEvent->event == TOUCH_MOVE) {
                // dragging ...
                TOUCH_JOY_GESTURE_LOG(">>> [DELAYEDTAP] move ...");
                run_args.joy_button0 = touchCurrEvent->currJoyButtonValue0;
                run_args.joy_button1 = touchCurrEvent->currJoyButtonValue1;
                joys.buttonDrawCallback(touchCurrEvent->currButtonDisplayChar);
                FREE(touchPrevEvent);
                touchPrevEvent = touchCurrEvent;
            } else if (touchCurrEvent->event == TOUCH_UP) {
                // tap
                TOUCH_JOY_GESTURE_LOG(">>> [DELAYEDTAP] touch up ...");
                run_args.joy_button0 = touchPrevEvent->currJoyButtonValue0;
                run_args.joy_button1 = touchPrevEvent->currJoyButtonValue1;
                joys.buttonDrawCallback(touchPrevEvent->currButtonDisplayChar);
                timedOut = 0;
                break;
            } else if (touchCurrEvent->event == TOUCH_DOWN) {
                LOG("WHOA : unexpected touch down, are you spamming the touchscreen?!");
                FREE(touchPrevEvent);
                touchPrevEvent = touchCurrEvent;
                continue;
            } else {
                __builtin_unreachable();
            }
        }

        FREE(touchPrevEvent);
        FREE(touchCurrEvent);
    }

    // clear out event queue
    touch_event_s *p = touchEventQ;
    while (p) {
        touch_event_s *dead = p;
        p = p->next;
        FREE(dead);
    }

    pthread_mutex_unlock(&joys.tapDelayMutex);

    joys.tapDelayMutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
    joys.tapDelayCond = (pthread_cond_t)PTHREAD_COND_INITIALIZER;

    LOG(">>> [DELAYEDTAP] thread exit ...");

    return NULL;
}

static void touchjoy_setup(void (*buttonDrawCallback)(char newChar)) {
    joys.buttonDrawCallback = buttonDrawCallback;
    if (joys.tapDelayThreadId == 0) {
        pthread_create(&joys.tapDelayThreadId, NULL, (void *)&_button_tap_delayed_thread, (void *)NULL);
    }
}

static void touchjoy_shutdown(void) {
    if (joys.tapDelayThreadId && emulator_isShuttingDown()) {
        pthread_mutex_lock(&joys.tapDelayMutex);
        pthread_cond_signal(&joys.tapDelayCond);
        pthread_mutex_unlock(&joys.tapDelayMutex);
        pthread_join(joys.tapDelayThreadId, NULL);
        joys.tapDelayThreadId = 0;
    }
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

static void _signal_tap_delay_event(interface_touch_event_t type, touchjoy_button_type_t theButtonChar) {
    touch_event_s *touchEvent = MALLOC(sizeof(*touchEvent));
    touchEvent->next = NULL;
    touchEvent->event = type;
    if (theButtonChar == TOUCH_BUTTON1) {
        touchEvent->currJoyButtonValue0 = 0x80;
        touchEvent->currJoyButtonValue1 = 0;
        touchEvent->currButtonDisplayChar = MOUSETEXT_OPENAPPLE;
    } else if (theButtonChar == TOUCH_BUTTON2) {
        touchEvent->currJoyButtonValue0 = 0;
        touchEvent->currJoyButtonValue1 = 0x80;
        touchEvent->currButtonDisplayChar = MOUSETEXT_CLOSEDAPPLE;
    } else if (theButtonChar == TOUCH_BOTH) {
        touchEvent->currJoyButtonValue0 = 0x80;
        touchEvent->currJoyButtonValue1 = 0x80;
        touchEvent->currButtonDisplayChar = '+';
    } else {
        touchEvent->currJoyButtonValue0 = 0;
        touchEvent->currJoyButtonValue1 = 0;
        touchEvent->currButtonDisplayChar = ' ';
    }

    pthread_mutex_lock(&joys.tapDelayMutex);
    touch_event_s *p0 = NULL;
    touch_event_s *p = touchEventQ;
    while (p) {
        p0 = p;
        p = p->next;
    }
    if (p0) {
        p0->next = touchEvent;
    } else {
        touchEventQ = touchEvent;
    }
    pthread_cond_signal(&joys.tapDelayCond);
    pthread_mutex_unlock(&joys.tapDelayMutex);
}

static void touchjoy_buttonDown(void) {
    _signal_tap_delay_event(TOUCH_DOWN, joys.touchDownChar);
}

static void touchjoy_buttonMove(int dx, int dy) {
    if ((dy < -joyglobals.switchThreshold) || (dy > joyglobals.switchThreshold)) {

        touchjoy_button_type_t theButtonChar = -1;
        if (dy < 0) {
            theButtonChar = joys.northChar;
        } else {
            theButtonChar = joys.southChar;
        }

        _signal_tap_delay_event(TOUCH_MOVE, theButtonChar);
    }
}

static void touchjoy_buttonUp(int dx, int dy) {
    _signal_tap_delay_event(TOUCH_UP, ' ');
}

static void touchjoy_prefsChanged(const char *domain) {
    assert(video_isRenderThread());

    long lVal = 0;

    joys.touchDownChar = prefs_parseLongValue(domain, PREF_JOY_TOUCHDOWN_CHAR, &lVal, /*base:*/10) ? lVal : TOUCH_BUTTON1;
    joys.northChar = prefs_parseLongValue(domain, PREF_JOY_SWIPE_NORTH_CHAR, &lVal, /*base:*/10) ? lVal : TOUCH_BOTH;
    joys.southChar = prefs_parseLongValue(domain, PREF_JOY_SWIPE_SOUTH_CHAR, &lVal, /*base:*/10) ? lVal : TOUCH_BUTTON2;

    float fVal = 0.f;
    joys.tapDelayNanos = prefs_parseFloatValue(domain, PREF_JOY_TAP_DELAY, &fVal) ? (fVal * NANOSECONDS_PER_SECOND) : BUTTON_TAP_DELAY_NANOS_DEFAULT;
    if (joys.tapDelayNanos < BUTTON_TAP_DELAY_NANOS_MIN) {
        joys.tapDelayNanos = BUTTON_TAP_DELAY_NANOS_MIN;
    }
}

static uint8_t *touchjoy_rosetteChars(void) {
    static uint8_t rosetteChars[ROSETTE_ROWS * ROSETTE_COLS] = { 0 };
    if (rosetteChars[0] == 0x0)  {
        rosetteChars[0]     = ' ';
        rosetteChars[1]     = MOUSETEXT_UP;
        rosetteChars[2]     = ' ';

        rosetteChars[3]     = MOUSETEXT_LEFT;
        rosetteChars[4]     = ICONTEXT_MENU_TOUCHJOY;
        rosetteChars[5]     = MOUSETEXT_RIGHT;

        rosetteChars[6]     = ' ';
        rosetteChars[7]     = MOUSETEXT_DOWN;
        rosetteChars[8]     = ' ';
    }
    return rosetteChars;
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

    joyVariant.rosetteChars = &touchjoy_rosetteChars;

    joys.tapDelayMutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
    joys.tapDelayCond = (pthread_cond_t)PTHREAD_COND_INITIALIZER;

    gltouchjoy_registerVariant(TOUCH_DEVICE_JOYSTICK, &joyVariant);
}

static __attribute__((constructor)) void __init_gltouchjoy_joy(void) {
    emulator_registerStartupCallback(CTOR_PRIORITY_EARLY, &_init_gltouchjoy_joy);
}

