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

    // scancodes to fire upon axisUp, buttonPress, or as key-repeat
    keypad_fire_t fireIdx;
    int scancodes[MAX_REPEATING];
    struct timespec timingBegins[MAX_REPEATING];

    bool axisTiming;
    bool buttonTiming;

} kpad = { 0 };

static GLTouchJoyVariant kpadJoy = { 0 };

// ----------------------------------------------------------------------------
// repeat key callback

static void touchkpad_keyboardReadCallback(void) {

    assert(pthread_self() == cpu_thread_id);

    struct timespec now = { 0 };
    clock_gettime(CLOCK_MONOTONIC, &now);

    for (unsigned int i=0; i<MAX_REPEATING; i++) {
        bool fired = false;
        int scancode = kpad.scancodes[kpad.fireIdx];
        if (scancode >= 0) {
            struct timespec deltat = timespec_diff(kpad.timingBegins[kpad.fireIdx], now, NULL);
            if (deltat.tv_sec || deltat.tv_nsec > KEY_REPEAT_THRESHOLD_NANOS) {
                LOG("REPEAT #%d/%lu/%lu: %d", kpad.fireIdx, deltat.tv_sec, deltat.tv_nsec, scancode);
                c_keys_handle_input(scancode, /*pressed:*/true, /*ASCII:*/false);
                fired = true;
            }
        }
        ++kpad.fireIdx;
        if (kpad.fireIdx >= MAX_REPEATING) {
            kpad.fireIdx = 0;
        }
        if (fired) {
            break;
        }
    }

    if (!kpad.axisTiming && !kpad.buttonTiming) {
        keydriver_keyboardReadCallback = NULL;// unschedule callback
    }
}

// ----------------------------------------------------------------------------

static touchjoy_variant_t touchkpad_variant(void) {
    return EMULATED_KEYPAD;
}

static void touchkpad_resetState(void) {
    kpad.axisCurrentOctant = ORIGIN;

    for (unsigned int i=0; i<MAX_REPEATING; i++) {
        kpad.scancodes[i] = -1;
    }

    kpad.axisTiming = false;
    kpad.buttonTiming = false;

    kpad.currButtonDisplayChar = ' ';

    for (unsigned int i=0; i<ROSETTE_COLS; i++) {
        for (unsigned int j=0; j<ROSETTE_ROWS; j++) {
            c_keys_handle_input(axes.rosetteScancodes[i], /*pressed:*/false, /*ASCII:*/false);
        }
    }

    c_keys_handle_input(buttons.touchDownScancode, /*pressed:*/false, /*ASCII:*/false);
    c_keys_handle_input(buttons.northScancode,     /*pressed:*/false, /*ASCII:*/false);
    c_keys_handle_input(buttons.southScancode,     /*pressed:*/false, /*ASCII:*/false);
}

// ----------------------------------------------------------------------------
// axis key(s) state

static void touchkpad_axisDown(void) {
    kpad.axisCurrentOctant = ORIGIN;
    if (axes.rosetteScancodes[ROSETTE_CENTER] >= 0) {
        kpad.scancodes[REPEAT_AXIS] = axes.rosetteScancodes[ROSETTE_CENTER];
        kpad.scancodes[REPEAT_AXIS_ALT] = -1;
        clock_gettime(CLOCK_MONOTONIC, &kpad.timingBegins[REPEAT_AXIS]);
        kpad.axisTiming = true;
        keydriver_keyboardReadCallback = &touchkpad_keyboardReadCallback;
    }
}

static void touchkpad_axisMove(int dx, int dy) {

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

        // handle a new octant

        struct timespec now = { 0 };
        clock_gettime(CLOCK_MONOTONIC, &now);
        kpad.timingBegins[REPEAT_AXIS] = now;
        kpad.timingBegins[REPEAT_AXIS_ALT] = now;
        keydriver_keyboardReadCallback = &touchkpad_keyboardReadCallback;

        kpad.scancodes[REPEAT_AXIS_ALT] = -1;
        switch (kpad.axisCurrentOctant) {
            case OCTANT_NORTHWEST:
                if (axes.rosetteScancodes[ROSETTE_NORTHWEST] >= 0) {
                    kpad.scancodes[REPEAT_AXIS] = axes.rosetteScancodes[ROSETTE_NORTHWEST];
                    LOG("XY : NORTHWEST, (%d)", axes.rosetteScancodes[ROSETTE_WEST]);
                } else if (axes.rosetteScancodes[ROSETTE_NORTH] < 0) {
                    if (radians > RADIANS_NORTHWEST) {
                        LOG("IGNORING Y (NORTH) ...");
                        kpad.scancodes[REPEAT_AXIS] = -1;
                    } else {
                        LOG("X : WEST (%d)", axes.rosetteScancodes[ROSETTE_WEST]);
                        kpad.scancodes[REPEAT_AXIS] = axes.rosetteScancodes[ROSETTE_WEST];
                    }
                } else if (axes.rosetteScancodes[ROSETTE_WEST] < 0) {
                    if (radians < RADIANS_NORTHWEST) {
                        LOG("IGNORING X (WEST) ...");
                        kpad.scancodes[REPEAT_AXIS] = -1;
                    } else {
                        LOG("Y : NORTH (%d)", axes.rosetteScancodes[ROSETTE_NORTH]);
                        kpad.scancodes[REPEAT_AXIS] = axes.rosetteScancodes[ROSETTE_NORTH];
                    }
                } else {
                    if (radians > RADIANS_NORTHWEST) {
                        LOG("XY : NORTH (%d) & WEST (%d)", axes.rosetteScancodes[ROSETTE_NORTH], axes.rosetteScancodes[ROSETTE_WEST]);
                        kpad.scancodes[REPEAT_AXIS]     = axes.rosetteScancodes[ROSETTE_NORTH];
                        kpad.scancodes[REPEAT_AXIS_ALT] = axes.rosetteScancodes[ROSETTE_WEST];
                    } else {
                        LOG("XY : WEST (%d) & NORTH (%d)", axes.rosetteScancodes[ROSETTE_WEST], axes.rosetteScancodes[ROSETTE_NORTH]);
                        kpad.scancodes[REPEAT_AXIS]     = axes.rosetteScancodes[ROSETTE_WEST];
                        kpad.scancodes[REPEAT_AXIS_ALT] = axes.rosetteScancodes[ROSETTE_NORTH];
                    }
                }
                break;

            case OCTANT_NORTH:
                LOG("Y : NORTH (%d)", axes.rosetteScancodes[ROSETTE_NORTH]);
                kpad.scancodes[REPEAT_AXIS] = axes.rosetteScancodes[ROSETTE_NORTH];
                break;

            case OCTANT_NORTHEAST:
                if (axes.rosetteScancodes[ROSETTE_NORTHEAST] >= 0) {
                    LOG("XY : NORTHEAST (%d)", axes.rosetteScancodes[ROSETTE_NORTHEAST]);
                    kpad.scancodes[REPEAT_AXIS] = axes.rosetteScancodes[ROSETTE_NORTHEAST];
                } else if (axes.rosetteScancodes[ROSETTE_NORTH] < 0) {
                    if (radians < RADIANS_NORTHEAST) {
                        LOG("IGNORING Y (NORTH) ...");
                        kpad.scancodes[REPEAT_AXIS] = -1;
                    } else {
                        LOG("X : EAST (%d)", axes.rosetteScancodes[ROSETTE_EAST]);
                        kpad.scancodes[REPEAT_AXIS] = axes.rosetteScancodes[ROSETTE_EAST];
                    }
                } else if (axes.rosetteScancodes[ROSETTE_EAST] < 0) {
                    if (radians > RADIANS_NORTHEAST) {
                        LOG("IGNORING X (EAST) ...");
                        kpad.scancodes[REPEAT_AXIS] = -1;
                    } else {
                        LOG("Y : NORTH (%d)", axes.rosetteScancodes[ROSETTE_NORTH]);
                        kpad.scancodes[REPEAT_AXIS] = axes.rosetteScancodes[ROSETTE_NORTH];
                    }
                } else {
                    if (radians < RADIANS_NORTHEAST) {
                        LOG("XY : NORTH (%d) & EAST (%d)", axes.rosetteScancodes[ROSETTE_NORTH], axes.rosetteScancodes[ROSETTE_EAST]);
                        kpad.scancodes[REPEAT_AXIS]     = axes.rosetteScancodes[ROSETTE_NORTH];
                        kpad.scancodes[REPEAT_AXIS_ALT] = axes.rosetteScancodes[ROSETTE_EAST];
                    } else {
                        LOG("XY : EAST (%d) & NORTH (%d)", axes.rosetteScancodes[ROSETTE_EAST], axes.rosetteScancodes[ROSETTE_NORTH]);
                        kpad.scancodes[REPEAT_AXIS]     = axes.rosetteScancodes[ROSETTE_EAST];
                        kpad.scancodes[REPEAT_AXIS_ALT] = axes.rosetteScancodes[ROSETTE_NORTH];
                    }
                }
                break;

            case OCTANT_WEST:
                LOG("Y : WEST (%d)", axes.rosetteScancodes[ROSETTE_WEST]);
                kpad.scancodes[REPEAT_AXIS] = axes.rosetteScancodes[ROSETTE_WEST];
                break;

            case OCTANT_EAST:
                LOG("Y : EAST (%d)", axes.rosetteScancodes[ROSETTE_EAST]);
                kpad.scancodes[REPEAT_AXIS] = axes.rosetteScancodes[ROSETTE_EAST];
                break;

            case OCTANT_SOUTHWEST:
                if (axes.rosetteScancodes[ROSETTE_SOUTHWEST] >= 0) {
                    LOG("XY : SOUTHWEST (%d)", axes.rosetteScancodes[ROSETTE_SOUTHWEST]);
                    kpad.scancodes[REPEAT_AXIS] = axes.rosetteScancodes[ROSETTE_SOUTHWEST];
                } else if (axes.rosetteScancodes[ROSETTE_SOUTH] < 0) {
                    if (radians < RADIANS_SOUTHWEST) {
                        kpad.scancodes[REPEAT_AXIS] = -1;
                        LOG("IGNORING Y (SOUTH) ...");
                    } else {
                        LOG("X : WEST (%d)", axes.rosetteScancodes[ROSETTE_WEST]);
                        kpad.scancodes[REPEAT_AXIS] = axes.rosetteScancodes[ROSETTE_WEST];
                    }
                } else if (axes.rosetteScancodes[ROSETTE_WEST] < 0) {
                    if (radians > RADIANS_SOUTHWEST) {
                        kpad.scancodes[REPEAT_AXIS] = -1;
                        LOG("IGNORING X (WEST) ...");
                    } else {
                        LOG("Y : SOUTH (%d)", axes.rosetteScancodes[ROSETTE_SOUTH]);
                        kpad.scancodes[REPEAT_AXIS] = axes.rosetteScancodes[ROSETTE_SOUTH];
                    }
                } else {
                    if (radians < RADIANS_SOUTHWEST) {
                        LOG("XY : SOUTH (%d) & WEST (%d)", axes.rosetteScancodes[ROSETTE_SOUTH], axes.rosetteScancodes[ROSETTE_WEST]);
                        kpad.scancodes[REPEAT_AXIS]     = axes.rosetteScancodes[ROSETTE_SOUTH];
                        kpad.scancodes[REPEAT_AXIS_ALT] = axes.rosetteScancodes[ROSETTE_WEST];
                    } else {
                        LOG("XY : WEST (%d) & SOUTH (%d)", axes.rosetteScancodes[ROSETTE_WEST], axes.rosetteScancodes[ROSETTE_SOUTH]);
                        kpad.scancodes[REPEAT_AXIS]     = axes.rosetteScancodes[ROSETTE_WEST];
                        kpad.scancodes[REPEAT_AXIS_ALT] = axes.rosetteScancodes[ROSETTE_SOUTH];
                    }
                }
                break;

            case OCTANT_SOUTH:
                LOG("Y : SOUTH (%d)", axes.rosetteScancodes[ROSETTE_SOUTH]);
                kpad.scancodes[REPEAT_AXIS] = axes.rosetteScancodes[ROSETTE_SOUTH];
                break;

            case OCTANT_SOUTHEAST:
                if (axes.rosetteScancodes[ROSETTE_SOUTHEAST] >= 0) {
                    LOG("XY : SOUTHEAST (%d)", axes.rosetteScancodes[ROSETTE_SOUTHEAST]);
                    kpad.scancodes[REPEAT_AXIS] = axes.rosetteScancodes[ROSETTE_SOUTHEAST];
                } else if (axes.rosetteScancodes[ROSETTE_SOUTH] < 0) {
                    if (radians > RADIANS_SOUTHEAST) {
                        LOG("IGNORING Y (SOUTH) ...");
                        kpad.scancodes[REPEAT_AXIS] = -1;
                    } else {
                        LOG("X : EAST (%d)", axes.rosetteScancodes[ROSETTE_EAST]);
                        kpad.scancodes[REPEAT_AXIS] = axes.rosetteScancodes[ROSETTE_EAST];
                    }
                } else if (axes.rosetteScancodes[ROSETTE_EAST] < 0) {
                    if (radians < RADIANS_SOUTHEAST) {
                        LOG("IGNORING X (EAST) ...");
                        kpad.scancodes[REPEAT_AXIS] = -1;
                    } else {
                        LOG("Y : SOUTH (%d)", axes.rosetteScancodes[ROSETTE_SOUTH]);
                        kpad.scancodes[REPEAT_AXIS] = axes.rosetteScancodes[ROSETTE_SOUTH];
                    }
                } else {
                    if (radians > RADIANS_SOUTHEAST) {
                        LOG("XY : SOUTH (%d) & EAST (%d)", axes.rosetteScancodes[ROSETTE_SOUTH], axes.rosetteScancodes[ROSETTE_EAST]);
                        kpad.scancodes[REPEAT_AXIS]     = axes.rosetteScancodes[ROSETTE_SOUTH];
                        kpad.scancodes[REPEAT_AXIS_ALT] = axes.rosetteScancodes[ROSETTE_EAST];
                    } else {
                        LOG("XY : EAST (%d) & SOUTH (%d)", axes.rosetteScancodes[ROSETTE_EAST], axes.rosetteScancodes[ROSETTE_SOUTH]);
                        kpad.scancodes[REPEAT_AXIS]     = axes.rosetteScancodes[ROSETTE_EAST];
                        kpad.scancodes[REPEAT_AXIS_ALT] = axes.rosetteScancodes[ROSETTE_SOUTH];
                    }
                }
                break;

            default:
                assert(false && "should not happen");
                break;
        }

    } while (0);

    kpad.axisTiming = true;
}

static void touchkpad_axisUp(int dx, int dy) {
    touchkpad_axisMove(dx, dy);
    kpad.axisCurrentOctant = ORIGIN;

    int scancode = kpad.scancodes[REPEAT_AXIS];
    kpad.scancodes[REPEAT_AXIS] = -1;
    if (scancode < 0) {
        scancode = kpad.scancodes[REPEAT_AXIS_ALT];
        kpad.scancodes[REPEAT_AXIS_ALT] = -1;
    }

    c_keys_handle_input(scancode, /*pressed:*/true,  /*ASCII:*/false);
    c_keys_handle_input(scancode, /*pressed:*/false, /*ASCII:*/false);
    kpad.axisTiming = false;

    // if no other scancodes, REPEAT_AXIS_ALT (if non-negative) will be fired once remaining in callback
}

// ----------------------------------------------------------------------------
// button key state

static void touchkpad_setCurrButtonValue(touchjoy_button_type_t theButtonChar, int theButtonScancode) {
    if (theButtonChar >= 0) {
        kpad.currButtonDisplayChar = theButtonChar;
        kpad.scancodes[REPEAT_BUTTON] = theButtonScancode;
    } else {
        kpad.currButtonDisplayChar = ' ';
        kpad.scancodes[REPEAT_BUTTON] = -1;
    }
}

static uint8_t touchkpad_buttonPress(void) {
    if (kpad.scancodes[REPEAT_BUTTON] >= 0) {
        LOG("->BUTT : %d/'%c'", kpad.scancodes[REPEAT_BUTTON], kpad.currButtonDisplayChar);
        clock_gettime(CLOCK_MONOTONIC, &kpad.timingBegins[REPEAT_BUTTON]);
        c_keys_handle_input(kpad.scancodes[REPEAT_BUTTON], /*pressed:*/true, /*ASCII:*/false);
        kpad.buttonTiming = true;
        keydriver_keyboardReadCallback = &touchkpad_keyboardReadCallback;
    }
    return kpad.currButtonDisplayChar;
}

static void touchkpad_buttonRelease(void) {
    kpad.scancodes[REPEAT_BUTTON] = -1;
    kpad.buttonTiming = false;
    c_keys_handle_input(kpad.scancodes[REPEAT_BUTTON], /*pressed:*/false, /*ASCII:*/false);
}

// ----------------------------------------------------------------------------

__attribute__((constructor(CTOR_PRIORITY_EARLY)))
static void _init_gltouchjoy_kpad(void) {
    LOG("Registering OpenGL software touch joystick (keypad variant)");

    for (unsigned int i=0; i<MAX_REPEATING; i++) {
        kpad.scancodes[i] = -1;
    }

    kpad.currButtonDisplayChar = ' ';

    kpadJoy.variant = &touchkpad_variant,
    kpadJoy.resetState = &touchkpad_resetState,

    kpadJoy.setCurrButtonValue = &touchkpad_setCurrButtonValue,
    kpadJoy.buttonPress = &touchkpad_buttonPress,
    kpadJoy.buttonRelease = &touchkpad_buttonRelease,

    kpadJoy.axisDown = &touchkpad_axisDown,
    kpadJoy.axisMove = &touchkpad_axisMove,
    kpadJoy.axisUp = &touchkpad_axisUp,

    gltouchjoy_registerVariant(EMULATED_KEYPAD, &kpadJoy);
}

