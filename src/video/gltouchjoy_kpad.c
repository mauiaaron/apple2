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

#define KEY_REPEAT_THRESHOLD_NANOS (NANOSECONDS_PER_SECOND / 4)
#define CALLBACK_LOCK_VALUE (1000)

// WARNING : this does not match the rosette left-right-down layout, but what is returned by atan2f()
typedef enum keypad_octant_t {
    OCTANT_WEST = 0,
    OCTANT_NORTHWEST,
    OCTANT_NORTH,
    OCTANT_NORTHEAST,
    OCTANT_EAST,
    OCTANT_SOUTHEAST,
    OCTANT_SOUTH,
    OCTANT_SOUTHWEST,
    ORIGIN,
} keypad_octant_t;

// Assuming radian value between -PI <-> PI
#define RADIANS_NORTHWEST ((-3.f * M_PI) / 4.f)
#define RADIANS_NORTH     ((-1.f * M_PI) / 2.f)
#define RADIANS_NORTHEAST ((-1.f * M_PI) / 4.f)
#define RADIANS_WEST_NEG  (-M_PI)
#define RADIANS_EAST      (0.f)
#define RADIANS_SOUTHWEST (( 3.f * M_PI) / 4.f)
#define RADIANS_SOUTH     (( 1.f * M_PI) / 2.f)
#define RADIANS_SOUTHEAST (( 1.f * M_PI) / 4.f)

typedef enum keypad_fire_t {
    REPEAT_AXIS = 0,
    REPEAT_AXIS_ALT,
    REPEAT_BUTTON,
    MAX_REPEATING,
} keypad_fire_t;

static struct {
    keypad_octant_t axisCurrentOctant;

    uint8_t currButtonDisplayChar;
    void (*buttonDrawCallback)(char newChar);

    // index of repeating scancodes to fire
    keypad_fire_t fireIdx;

    volatile int axisLock; // modified by both CPU thread and touch handling thread
    volatile int buttonLock; // modified by both CPU thread and touch handling thread

    int scancodes[MAX_REPEATING];
    struct timespec timingBegins[MAX_REPEATING];
    bool buttonBegan;
    bool axisBegan;
    int lastScancode;

    float repeatThresholdNanos;

} kpad = { 0 };

static GLTouchJoyVariant kpadJoy = { 0 };

// ----------------------------------------------------------------------------
// repeat key callback scheduling and unscheduling
//
// Assumptions :
//  - All touch sources run on a single thread
//  - callback itself runs on CPU thread

// unlock callback section
static inline void _callback_sourceUnlock(volatile int *source) {
    int val = __sync_add_and_fetch(source, (int)(CALLBACK_LOCK_VALUE));
    assert(val >= 0 && "inconsistent lock state for callback detected");
}

// attempt to lock the critical section where we unschedule the function pointer callback
// there should be no outstanding touch sources being tracked
static inline bool _callback_sourceTryLock(volatile int *source) {
    int val = __sync_sub_and_fetch(source, (int)(CALLBACK_LOCK_VALUE));
    if (val == -CALLBACK_LOCK_VALUE) {
        return true;
    } else {
        _callback_sourceUnlock(source);
        return false;
    }
}

// touch source has ended
static inline void _touch_sourceEnd(volatile int *source) {
    __sync_sub_and_fetch(source, 1);
}

// touch source has begun
static inline void _touch_sourceBegin(volatile int *source) {
    do {
        int val = __sync_add_and_fetch(source, 1);
        if (val >= 0) {
            assert(val > 0 && "inconsistent lock state for touch source detected");
            return;
        }
        // spin waiting on callback critical
        _touch_sourceEnd(source);
    } while (1);
}

static void touchkpad_keyboardReadCallback(void) {
    assert(pthread_self() == cpu_thread_id);

    // HACK FIXME TODO :
    //
    // There are a number of cases where the emulated software is reading the keyboard state but not using the value.
    // This is quite noticeable in a number of games that take keyboard input.
    //
    // This indicates that we are incorrectly emulating the keyboard hardware.  The proper fix for this touch keypad
    // joystick will be to properly emulate the original hardware timing, using the existing facility to count 65c02 CPU
    // machine cyles.
#warning FIXME TODO : implement proper keyboard repeat callback timing

    if (kpad.lastScancode >= 0) {
        c_keys_handle_input(kpad.lastScancode, /*pressed:*/false, /*ASCII:*/false);
        kpad.lastScancode = -1;
    }

    struct timespec now = { 0 };
    clock_gettime(CLOCK_MONOTONIC, &now);

    int fired = -1;
    for (unsigned int i=0; i<MAX_REPEATING; i++) {
        int scancode = kpad.scancodes[kpad.fireIdx];
        if (scancode >= 0) {
            struct timespec deltat = timespec_diff(kpad.timingBegins[kpad.fireIdx], now, NULL);
            if (deltat.tv_sec || deltat.tv_nsec > kpad.repeatThresholdNanos) {
                LOG("ACTIVE(%d,%d) REPEAT #%d/%lu/%lu: %d", kpad.axisLock, kpad.buttonLock, kpad.fireIdx, deltat.tv_sec, deltat.tv_nsec, scancode);
                c_keys_handle_input(scancode, /*pressed:*/true, /*ASCII:*/false);
                kpad.lastScancode = scancode;
                fired = kpad.fireIdx;
            }
        }
        ++kpad.fireIdx;
        if (kpad.fireIdx >= MAX_REPEATING) {
            kpad.fireIdx = 0;
        }
        if (fired >= 0) {
            break;
        }
    }

    if (fired == REPEAT_BUTTON) {
        kpad.buttonDrawCallback(kpad.currButtonDisplayChar);
    }

    bool lockedAxis = _callback_sourceTryLock(&kpad.axisLock);
    if (lockedAxis) {
        if (fired == REPEAT_AXIS || fired == REPEAT_AXIS_ALT) {
            LOG("RESETTING AXIS INDEX %d ...", fired);
            kpad.scancodes[fired] = -1;
        }
    }

    bool lockedButton = _callback_sourceTryLock(&kpad.buttonLock);
    if (lockedButton) {
        if (fired == REPEAT_BUTTON) {
            LOG("RESETTING BUTTON INDEX %d ...", fired);
            kpad.scancodes[fired] = -1;
        }
    }

    if (lockedButton && lockedAxis && fired < 0) {
        LOG("REPEAT KEY CALLBACK DONE ...");
        keydriver_keyboardReadCallback = NULL;
    }

    if (lockedAxis) {
        _callback_sourceUnlock(&kpad.axisLock);
    }
    if (lockedButton) {
        _callback_sourceUnlock(&kpad.buttonLock);
    }
}

// ----------------------------------------------------------------------------

static touchjoy_variant_t touchkpad_variant(void) {
    return EMULATED_KEYPAD;
}

static void touchkpad_resetState(void) {
    LOG("RESET STATE ..........................................");
    kpad.axisLock = 0;
    kpad.buttonLock = 0;
    keydriver_keyboardReadCallback = NULL;

    kpad.axisCurrentOctant = ORIGIN;
    kpad.lastScancode = -1;

    for (unsigned int i=0; i<MAX_REPEATING; i++) {
        kpad.scancodes[i] = -1;
        kpad.timingBegins[i] = (struct timespec){ 0 };
    }

    kpad.currButtonDisplayChar = ' ';
    kpad.axisBegan = false;
    kpad.buttonBegan = false;

    for (unsigned int i=0; i<ROSETTE_COLS; i++) {
        for (unsigned int j=0; j<ROSETTE_ROWS; j++) {
            c_keys_handle_input(axes.rosetteScancodes[i], /*pressed:*/false, /*ASCII:*/false);
        }
    }

    c_keys_handle_input(buttons.touchDownScancode, /*pressed:*/false, /*ASCII:*/false);
    c_keys_handle_input(buttons.northScancode,     /*pressed:*/false, /*ASCII:*/false);
    c_keys_handle_input(buttons.southScancode,     /*pressed:*/false, /*ASCII:*/false);
}

static void touchkpad_setup(void (*buttonDrawCallback)(char newChar)) {
    kpad.buttonDrawCallback = buttonDrawCallback;
}

static void touchkpad_shutdown(void) {
    // ...
}

// ----------------------------------------------------------------------------
// axis key(s) state

static void touchkpad_axisDown(void) {
    LOG("%s", "");
    if (!kpad.axisBegan) {
        // avoid multiple locks on extra axisDown()
        kpad.axisBegan = true;
        _touch_sourceBegin(&kpad.axisLock);
    }

    keydriver_keyboardReadCallback = &touchkpad_keyboardReadCallback;
    struct timespec now = { 0 };
    clock_gettime(CLOCK_MONOTONIC, &now);
    kpad.timingBegins[REPEAT_AXIS] = now;
    kpad.timingBegins[REPEAT_AXIS_ALT] = now;

    kpad.axisCurrentOctant = ORIGIN;
    if (axes.rosetteScancodes[ROSETTE_CENTER] >= 0) {
        kpad.scancodes[REPEAT_AXIS] = axes.rosetteScancodes[ROSETTE_CENTER];
        kpad.scancodes[REPEAT_AXIS_ALT] = -1;
    }
}

static void touchkpad_axisMove(int dx, int dy) {
    LOG("%s", "");

    if ((dx > -joyglobals.switchThreshold) && (dx < joyglobals.switchThreshold) && (dy > -joyglobals.switchThreshold) && (dy < joyglobals.switchThreshold)) {
        kpad.scancodes[REPEAT_AXIS] = axes.rosetteScancodes[ROSETTE_CENTER];
        kpad.scancodes[REPEAT_AXIS_ALT] = -1;
        return;
    }

    if (dx == 0 && dy == 0) {
        return;
    }

    do {

        // determine the octant of this dx/dy

        keypad_octant_t axisLastOctant = kpad.axisCurrentOctant;

        const float radians = atan2f(dy, dx);   // -180-180deg range
        float radnorm = radians + M_PI;         // 0-360deg range
        if (UNLIKELY(radnorm < 0.f)) {
            radnorm = 0.f;                      // clamp positive
        }
        float octant = radnorm + (M_PI/8.f);    // rotate to correct octant (+45deg)
        octant /= (M_PI/4.f);                   // divide to octant (/90deg)
        kpad.axisCurrentOctant = (keypad_octant_t) ((int)octant & 0x7);// integer modulo maps to enum

        if (kpad.axisCurrentOctant == axisLastOctant) {
            break;
        }

        LOG("radians:%f radnorm:%f octant:%f, currOctant:%d", radians, radnorm, octant, kpad.axisCurrentOctant);

        // Current implementation NOTE : four cardinal directions are handled slightly different than the intercardinal
        // ones.
        //  - The intercardinals might generate 2 scanscodes (for example north and west scancodes for a northwest axis)
        //    if there is not a specific scancode to handle it (e.g., the northwest scancode).
        //  - The cardinals will only ever generate one scancode (the cardinal in question if it's set, or the scancode
        //    of the adjacent intercardinal where the point lies).
        kpad.scancodes[REPEAT_AXIS_ALT] = -1;
        switch (kpad.axisCurrentOctant) {
            case OCTANT_NORTHWEST:
                if (axes.rosetteScancodes[ROSETTE_NORTHWEST] >= 0) {
                    kpad.scancodes[REPEAT_AXIS] = axes.rosetteScancodes[ROSETTE_NORTHWEST];
                    LOG("XY : NORTHWEST, (%d)", axes.rosetteScancodes[ROSETTE_WEST]);
                } else {
                    LOG("XY : WEST (%d) & NORTH (%d)", axes.rosetteScancodes[ROSETTE_WEST], axes.rosetteScancodes[ROSETTE_NORTH]);
                    kpad.scancodes[REPEAT_AXIS] = axes.rosetteScancodes[ROSETTE_WEST];
                    kpad.scancodes[REPEAT_AXIS_ALT] = axes.rosetteScancodes[ROSETTE_NORTH];
                }
                break;

            case OCTANT_NORTH:
                if (axes.rosetteScancodes[ROSETTE_NORTH] >= 0) {
                    LOG("Y : NORTH (%d)", axes.rosetteScancodes[ROSETTE_NORTH]);
                    kpad.scancodes[REPEAT_AXIS] = axes.rosetteScancodes[ROSETTE_NORTH];
                } else if (radians < RADIANS_NORTH) {
                    LOG("XY : NORTHWEST (%d)", axes.rosetteScancodes[ROSETTE_NORTHWEST]);
                    kpad.scancodes[REPEAT_AXIS] = axes.rosetteScancodes[ROSETTE_NORTHWEST];
                } else {
                    LOG("XY : NORTHEAST (%d)", axes.rosetteScancodes[ROSETTE_NORTHEAST]);
                    kpad.scancodes[REPEAT_AXIS] = axes.rosetteScancodes[ROSETTE_NORTHEAST];
                }
                break;

            case OCTANT_NORTHEAST:
                if (axes.rosetteScancodes[ROSETTE_NORTHEAST] >= 0) {
                    LOG("XY : NORTHEAST (%d)", axes.rosetteScancodes[ROSETTE_NORTHEAST]);
                    kpad.scancodes[REPEAT_AXIS] = axes.rosetteScancodes[ROSETTE_NORTHEAST];
                } else {
                    LOG("XY : EAST (%d) & NORTH (%d)", axes.rosetteScancodes[ROSETTE_EAST], axes.rosetteScancodes[ROSETTE_NORTH]);
                    kpad.scancodes[REPEAT_AXIS] = axes.rosetteScancodes[ROSETTE_EAST];
                    kpad.scancodes[REPEAT_AXIS_ALT] = axes.rosetteScancodes[ROSETTE_NORTH];
                }
                break;

            case OCTANT_WEST:
                if (axes.rosetteScancodes[ROSETTE_WEST] >= 0) {
                    LOG("Y : WEST (%d)", axes.rosetteScancodes[ROSETTE_WEST]);
                    kpad.scancodes[REPEAT_AXIS] = axes.rosetteScancodes[ROSETTE_WEST];
                } else if (radians > RADIANS_WEST_NEG && radians < 0) {
                    LOG("XY : NORTHWEST (%d)", axes.rosetteScancodes[ROSETTE_NORTHWEST]);
                    kpad.scancodes[REPEAT_AXIS] = axes.rosetteScancodes[ROSETTE_NORTHWEST];
                } else {
                    LOG("XY : SOUTHWEST (%d)", axes.rosetteScancodes[ROSETTE_SOUTHWEST]);
                    kpad.scancodes[REPEAT_AXIS] = axes.rosetteScancodes[ROSETTE_SOUTHWEST];
                }
                break;

            case OCTANT_EAST:
                if (axes.rosetteScancodes[ROSETTE_EAST] >= 0) {
                    LOG("Y : EAST (%d)", axes.rosetteScancodes[ROSETTE_EAST]);
                    kpad.scancodes[REPEAT_AXIS] = axes.rosetteScancodes[ROSETTE_EAST];
                } else if (radians < RADIANS_EAST) {
                    LOG("XY : NORTHEAST (%d)", axes.rosetteScancodes[ROSETTE_NORTHEAST]);
                    kpad.scancodes[REPEAT_AXIS] = axes.rosetteScancodes[ROSETTE_NORTHEAST];
                } else {
                    LOG("XY : SOUTHEAST (%d)", axes.rosetteScancodes[ROSETTE_SOUTHEAST]);
                    kpad.scancodes[REPEAT_AXIS] = axes.rosetteScancodes[ROSETTE_SOUTHEAST];
                }
                break;

            case OCTANT_SOUTHWEST:
                if (axes.rosetteScancodes[ROSETTE_SOUTHWEST] >= 0) {
                    LOG("XY : SOUTHWEST (%d)", axes.rosetteScancodes[ROSETTE_SOUTHWEST]);
                    kpad.scancodes[REPEAT_AXIS] = axes.rosetteScancodes[ROSETTE_SOUTHWEST];
                } else {
                    LOG("XY : WEST (%d) & SOUTH (%d)", axes.rosetteScancodes[ROSETTE_WEST], axes.rosetteScancodes[ROSETTE_SOUTH]);
                    kpad.scancodes[REPEAT_AXIS] = axes.rosetteScancodes[ROSETTE_WEST];
                    kpad.scancodes[REPEAT_AXIS_ALT] = axes.rosetteScancodes[ROSETTE_SOUTH];
                }
                break;

            case OCTANT_SOUTH:
                if (axes.rosetteScancodes[ROSETTE_SOUTH] >= 0) {
                    LOG("Y : SOUTH (%d)", axes.rosetteScancodes[ROSETTE_SOUTH]);
                    kpad.scancodes[REPEAT_AXIS] = axes.rosetteScancodes[ROSETTE_SOUTH];
                } else if (radians > RADIANS_SOUTH) {
                    LOG("XY : SOUTHWEST (%d)", axes.rosetteScancodes[ROSETTE_SOUTHWEST]);
                    kpad.scancodes[REPEAT_AXIS] = axes.rosetteScancodes[ROSETTE_SOUTHWEST];
                } else {
                    LOG("XY : SOUTHEAST (%d)", axes.rosetteScancodes[ROSETTE_SOUTHEAST]);
                    kpad.scancodes[REPEAT_AXIS] = axes.rosetteScancodes[ROSETTE_SOUTHEAST];
                }
                break;

            case OCTANT_SOUTHEAST:
                if (axes.rosetteScancodes[ROSETTE_SOUTHEAST] >= 0) {
                    LOG("XY : SOUTHEAST (%d)", axes.rosetteScancodes[ROSETTE_SOUTHEAST]);
                    kpad.scancodes[REPEAT_AXIS] = axes.rosetteScancodes[ROSETTE_SOUTHEAST];
                } else {
                    LOG("XY : EAST (%d) & SOUTH (%d)", axes.rosetteScancodes[ROSETTE_EAST], axes.rosetteScancodes[ROSETTE_SOUTH]);
                    kpad.scancodes[REPEAT_AXIS] = axes.rosetteScancodes[ROSETTE_EAST];
                    kpad.scancodes[REPEAT_AXIS_ALT] = axes.rosetteScancodes[ROSETTE_SOUTH];
                }
                break;

            default:
                assert(false && "should not happen");
                break;
        }

    } while (0);
}

static void touchkpad_axisUp(int dx, int dy) {
    LOG("%s", "");
    touchkpad_axisMove(dx, dy);
    kpad.axisCurrentOctant = ORIGIN;
    kpad.timingBegins[REPEAT_AXIS] = (struct timespec){ 0 };
    kpad.timingBegins[REPEAT_AXIS_ALT] = (struct timespec){ 0 };
    if (kpad.axisBegan) {
        kpad.axisBegan = false;
        _touch_sourceEnd(&kpad.axisLock);
    }
}

// ----------------------------------------------------------------------------
// button key state

static void _set_current_button_state(touchjoy_button_type_t theButtonChar, int theButtonScancode) {
    LOG("%s", "");
    if (theButtonChar >= 0) {
        kpad.currButtonDisplayChar = theButtonChar;
        kpad.scancodes[REPEAT_BUTTON] = theButtonScancode;
    } else {
        kpad.currButtonDisplayChar = ' ';
        kpad.scancodes[REPEAT_BUTTON] = -1;
    }
}

static void touchkpad_buttonDown(void) {
    if (!kpad.buttonBegan) {
        kpad.buttonBegan = true;
        _touch_sourceBegin(&kpad.buttonLock);
    }
    _set_current_button_state(buttons.touchDownChar, buttons.touchDownScancode);
    if (kpad.scancodes[REPEAT_BUTTON] >= 0) {
        LOG("->BUTT : %d/'%c'", kpad.scancodes[REPEAT_BUTTON], kpad.currButtonDisplayChar);
        clock_gettime(CLOCK_MONOTONIC, &kpad.timingBegins[REPEAT_BUTTON]);
        keydriver_keyboardReadCallback = &touchkpad_keyboardReadCallback;
    }
}

static void touchkpad_buttonMove(int dx, int dy) {
    // Currently this is the same logic as the "regular" joystick variant ... but we could likely be more creative here
    // in a future revision, like having a full octant of key possibilities ... (for example, playing Bolo with a friend
    // on the same tablet, one driving, the other shooting and controlling the turret)
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
    }
}

static void touchkpad_buttonUp(int dx, int dy) {
    LOG("%s", "");
    touchkpad_buttonMove(dx, dy);
    kpad.timingBegins[REPEAT_BUTTON] = (struct timespec){ 0 };
    if (kpad.buttonBegan) {
        kpad.buttonBegan = false;
        _touch_sourceEnd(&kpad.buttonLock);
    }
}

static void touchkpad_setKeyRepeatThreshold(float repeatThresholdSecs) {
    kpad.repeatThresholdNanos = repeatThresholdSecs * NANOSECONDS_PER_SECOND;
}

// ----------------------------------------------------------------------------

__attribute__((constructor(CTOR_PRIORITY_EARLY)))
static void _init_gltouchjoy_kpad(void) {
    LOG("Registering OpenGL software touch joystick (keypad variant)");

    for (unsigned int i=0; i<MAX_REPEATING; i++) {
        kpad.scancodes[i] = -1;
    }

    kpad.currButtonDisplayChar = ' ';

    kpad.repeatThresholdNanos = KEY_REPEAT_THRESHOLD_NANOS;

    kpadJoy.variant = &touchkpad_variant,
    kpadJoy.resetState = &touchkpad_resetState,
    kpadJoy.setup = &touchkpad_setup,
    kpadJoy.shutdown = &touchkpad_shutdown,

    kpadJoy.buttonDown = &touchkpad_buttonDown,
    kpadJoy.buttonMove = &touchkpad_buttonMove,
    kpadJoy.buttonUp = &touchkpad_buttonUp,

    kpadJoy.axisDown = &touchkpad_axisDown,
    kpadJoy.axisMove = &touchkpad_axisMove,
    kpadJoy.axisUp = &touchkpad_axisUp,

    joydriver_setKeyRepeatThreshold = &touchkpad_setKeyRepeatThreshold;

    gltouchjoy_registerVariant(EMULATED_KEYPAD, &kpadJoy);
}

