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

typedef enum keypad_press_t {
    REPEAT_IDX_A = 0,
    REPEAT_IDX_B,
    MAX_REPEATING,
} keypad_press_t;

typedef struct subvariant_s {
    // state variables
    keypad_octant_t currentOctant;
    keypad_press_t autoPressIdx;
    unsigned long frameCount;
    interface_touch_event_t currEventType;
    unsigned int keyPressCount;
    long scancodes[MAX_REPEATING];
    bool isShifted[MAX_REPEATING];
    uint8_t strobeShifter;

    // semi-static configured rosette
    long rosetteScans[ROSETTE_ROWS * ROSETTE_COLS];
    uint8_t rosetteChars[ROSETTE_ROWS * ROSETTE_COLS];
    bool rosetteShift[ROSETTE_ROWS * ROSETTE_COLS];
} subvariant_s;

static struct {
    video_frame_callback_fn frameCallback;

    unsigned long spinlock;

    subvariant_s axis;
    subvariant_s butt;

    bool autostrobeDelay; // pref for autostrobe delay

} kpad = { 0 };

static GLTouchJoyVariant kpadVariant = { 0 };

// ----------------------------------------------------------------------------

static inline void _reset_subvariant_state(subvariant_s *subvariant) {
    subvariant->frameCount = 0;
    subvariant->keyPressCount = 0;
    subvariant->strobeShifter = 0;
    subvariant->currentOctant = ORIGIN;
    subvariant->autoPressIdx = REPEAT_IDX_A;
    subvariant->scancodes[REPEAT_IDX_A] = -1;
    subvariant->scancodes[REPEAT_IDX_B] = -1;
    subvariant->isShifted[REPEAT_IDX_A] = false;
    subvariant->isShifted[REPEAT_IDX_B] = false;
    subvariant->currEventType = TOUCH_UP;
}

static inline void _advance_press_index(keypad_press_t *idx) {
    *idx = (*idx == REPEAT_IDX_A) ? REPEAT_IDX_B : REPEAT_IDX_A;
}

static inline void _press_key(subvariant_s *subvariant, keypad_press_t pressIdx) {
    int scancode = subvariant->scancodes[pressIdx];
    bool isShifted = subvariant->isShifted[pressIdx];

#if DEBUG_TOUCH_JOY
    {
        int c = keys_scancode2ASCII(scancode, /*is_shifted:*/true, /*is_ctrl:*/false);
        if (c >= 0) {
            TOUCH_JOY_LOG("\t\t\tPRESS KEY: %c", (uint8_t)c);
        }
    }
#endif

    if (isShifted) {
        keys_handleInput(SCODE_L_SHIFT, /*is_pressed:*/true, /*is_ascii:*/false);
    }

    keys_handleInput(scancode, /*is_pressed:*/true, /*is_ascii:*/false);
    keys_handleInput(scancode, /*is_pressed:*/false, /*is_ascii:*/false);

    if (isShifted) {
        keys_handleInput(SCODE_L_SHIFT, /*is_pressed:*/false, /*is_ascii:*/false);
    }

    _advance_press_index(&subvariant->autoPressIdx);

    ++(subvariant->keyPressCount);
}

// No-op callback
static void touchkpad_nopCallback(uint8_t textFlashCounter) {
    (void)textFlashCounter;
}

// End-of-video-frame callback (handling tap delay and auto-strobe)
static void touchkpad_frameCallback(uint8_t textFlashCounter) {

    // When activated, this is called every video frame -- ~16.688 millis

    ASSERT_ON_CPU_THREAD();

    SPIN_LOCK_FULL(&kpad.spinlock);
    TOUCH_JOY_LOG("\t\t+++kpad frameCallback acquire %c%c%c%c %x %x",
            (textFlashCounter & 0x8 ? '1' : '0'),
            (textFlashCounter & 0x4 ? '1' : '0'),
            (textFlashCounter & 0x2 ? '1' : '0'),
            (textFlashCounter & 0x1 ? '1' : '0'), kpad.axis.strobeShifter, kpad.butt.strobeShifter);

    ++(kpad.axis.frameCount);
    ++(kpad.butt.frameCount);

    subvariant_s *subvariant = ((textFlashCounter & 0x1) == 0x0) ? &kpad.axis : &kpad.butt;
    int scancode = subvariant->scancodes[subvariant->autoPressIdx];
    if (scancode < 0) {
        _advance_press_index(&subvariant->autoPressIdx);
        scancode = subvariant->scancodes[subvariant->autoPressIdx];
    }

    do {
        if (kpad.autostrobeDelay) {
            // perform autostrobe delay counting ...

            if (textFlashCounter == 0x8) {
                // This occurs every 16 video frames ... textFlashCounter : 1000
                // UtAIIe 3-13 : "The duration of the television scan is 262 horizontal scans.  This is [16.688 milliseconds]"
                // UtAIIe 3-17 : "[bit3] of the flash counter is the clockpulse for generating the delay before auto repeat"
                // UtAIIe 3-17 : "When a key is held for 534-801 millis, the AUTOSTRB starts to alternate at 15 Hz"
                // NOTE : 16.688 * 16 == ~267 millis ... 267 * 2 == 534 millis, 267 * 3 == 801 millis
                // Fastest scenario:
                //      key down 0@ 0x08 => 001 , t = 0ms
                //               1@ 0x08 => 011 , t = 267ms
                //               2@ 0x08 => 111 , t = 534ms
                //
                // Slowest scenario:
                //      key down 0@ 0x09 => 000 , t = 0ms
                //               1@ 0x08 => 001 , t = 267ms
                //               2@ 0x08 => 011 , t = 534ms
                //               3@ 0x08 => 111 , t = 801ms
                if (kpad.axis.frameCount >= joyglobals.tapDelayFrames) {
                    kpad.axis.strobeShifter = kpad.axis.strobeShifter << 1;
                    kpad.axis.strobeShifter |= 0x1;
                    kpad.axis.strobeShifter &= 0x3;
                }
                if (kpad.butt.frameCount >= joyglobals.tapDelayFrames) {
                    kpad.butt.strobeShifter = kpad.butt.strobeShifter << 1;
                    kpad.butt.strobeShifter |= 0x1;
                    kpad.butt.strobeShifter &= 0x3;
                }
            }

            bool notYetPressed = subvariant->keyPressCount == 0;
            bool twoKeys2Press = (subvariant->scancodes[REPEAT_IDX_A] >= 0 && subvariant->scancodes[REPEAT_IDX_B] >= 0);
            if (notYetPressed || twoKeys2Press) {
                // do not apply autostrobe delay ...
            } else if ((subvariant->strobeShifter & 0x3) != 0x3) {
                // still waiting for the autostrobe delay ...
                break;
            }
        }

        if (subvariant->frameCount < joyglobals.tapDelayFrames) {
            break;
        }

        // AUTOSTROBE PRESS CONFIGURED KEY
        // UtAIIe 3-17 : "[after the key press delay ...] the AUTOSTRB starts to alternate at 15 Hz"
        //
        // NOTE: Our implementation here is sometimes faster by 2x ...
        //  - We are simulating at least two simultaneous keypresses (one for the axis and one for the button side/subvariant)
        //  - If there are two keys to press for a particular side/subvariant, each one is pressed at 15 Hz
        //  - If there is only one key for the particular side/subvariant, it is pressed every two frames (7.5 Hz)

        if (scancode >= 0) {
            int idx = subvariant->autoPressIdx;
            _press_key(subvariant, idx);

            // some key was pressed ...
            if (subvariant->currEventType == TOUCH_UP) {
                TOUCH_JOY_LOG("\t\t+++resetting axis index %d ...", idx);
                subvariant->scancodes[idx] = -1;
            }
        } else {
            // no keys were pressed ...
        }

    } while (0);

    if ((kpad.axis.currEventType == TOUCH_UP) && (kpad.butt.currEventType == TOUCH_UP)) {
        // all touch up occurred -- unlatch the callback
        TOUCH_JOY_LOG("\t\t+++kpad callback done");
        kpad.frameCallback = touchkpad_nopCallback;
        kpad.axis.scancodes[REPEAT_IDX_A] = -1;
        kpad.axis.scancodes[REPEAT_IDX_B] = -1;
        kpad.butt.scancodes[REPEAT_IDX_A] = -1;
        kpad.butt.scancodes[REPEAT_IDX_B] = -1;
    }

    TOUCH_JOY_LOG("\t\t+++kpad frameCallback release");
    SPIN_UNLOCK_FULL(&kpad.spinlock);
}

// ----------------------------------------------------------------------------
// common subvariant touch lifecycle handling

static void _subvariant_touchDown(subvariant_s *subvariant) {

    _reset_subvariant_state(subvariant);
    subvariant->currEventType = TOUCH_DOWN;

    subvariant->scancodes[REPEAT_IDX_A] = subvariant->rosetteScans[ROSETTE_CENTER];
    subvariant->scancodes[REPEAT_IDX_B] = -1;
    subvariant->isShifted[REPEAT_IDX_A] = subvariant->rosetteShift[ROSETTE_CENTER];
    subvariant->isShifted[REPEAT_IDX_B] = false;

    if (joyglobals.tapDelayFrames == 0) {
        // unambiguous intent : no tap delay
        _press_key(subvariant, REPEAT_IDX_A);
    }

    kpad.frameCallback = &touchkpad_frameCallback;
}

static void _subvariant_touchMove(subvariant_s *subvariant, int dx, int dy, bool isTouchUp) {

    subvariant->currEventType = TOUCH_MOVE;

    int lastScancodes[MAX_REPEATING];
    lastScancodes[REPEAT_IDX_A] = subvariant->scancodes[REPEAT_IDX_A];
    lastScancodes[REPEAT_IDX_B] = subvariant->scancodes[REPEAT_IDX_B];

    keypad_octant_t lastOctant = subvariant->currentOctant;
    do {
        int c = (int)sqrtf(dx * dx + dy * dy);
        if (c < joyglobals.switchThreshold) {
            if (lastOctant != ORIGIN) {
                // unambiguous intent : user swiped beyond origin, then re-zeroed
                subvariant->currentOctant = ORIGIN;
                subvariant->scancodes[REPEAT_IDX_A] = subvariant->rosetteScans[ROSETTE_CENTER];
                subvariant->scancodes[REPEAT_IDX_B] = -1;
                subvariant->isShifted[REPEAT_IDX_A] = subvariant->rosetteShift[ROSETTE_CENTER];
                subvariant->isShifted[REPEAT_IDX_B] = -1;
            }
            break;
        }

        // unambiguous intent : user swiped beyond origin, determine octant of dx,dy

        const float radians = atan2f(dy, dx);   // -180-180deg range
        float radnorm = radians + M_PI;         // 0-360deg range
        if (UNLIKELY(radnorm < 0.f)) {
            radnorm = 0.f;                      // clamp positive
        }
        float octant = radnorm + (M_PI/8.f);    // rotate to correct octant (+45deg)
        octant /= (M_PI/4.f);                   // divide to octant (/90deg)
        subvariant->currentOctant = (keypad_octant_t) ((int)octant & 0x7); // integer modulo maps to enum

        if (subvariant->currentOctant == lastOctant) {
            break;
        }

        // CHANGED : moved to new octant, (and presumably scancode), so reset timings

        //TOUCH_JOY_LOG("\t\tradians:%f radnorm:%f octant:%f, currOctant:%d", radians, radnorm, octant, subvariant->currentOctant);

        // Current implementation NOTE : four cardinal directions are handled slightly different than the intercardinal
        // ones.
        //  - The intercardinals might generate 2 scanscodes (for example north and west scancodes for a northwest axis)
        //    if there is not a specific scancode to handle it (e.g., the northwest scancode).
        //  - The cardinals will only ever generate one scancode (the cardinal in question if it's set, or the scancode
        //    of the adjacent intercardinal where the point lies).

        long scanA = -1;
        long scanB = -1;
        bool shifA = false;
        bool shifB = false;

        switch (subvariant->currentOctant) {
            case OCTANT_NORTHWEST:
                if (subvariant->rosetteScans[ROSETTE_NORTHWEST] >= 0) {
                    TOUCH_JOY_LOG("\t\tXY : NORTHWEST, (%ld)", subvariant->rosetteScans[ROSETTE_WEST]);
                    scanA = subvariant->rosetteScans[ROSETTE_NORTHWEST];
                    shifA = subvariant->rosetteShift[ROSETTE_NORTHWEST];
                } else {
                    TOUCH_JOY_LOG("\t\tXY : WEST (%ld) & NORTH (%ld)", subvariant->rosetteScans[ROSETTE_WEST], subvariant->rosetteScans[ROSETTE_NORTH]);
                    scanA = subvariant->rosetteScans[ROSETTE_WEST];
                    shifA = subvariant->rosetteShift[ROSETTE_WEST];
                    scanB = subvariant->rosetteScans[ROSETTE_NORTH];
                    shifB = subvariant->rosetteShift[ROSETTE_NORTH];
                }
                break;

            case OCTANT_NORTH:
                if (subvariant->rosetteScans[ROSETTE_NORTH] >= 0) {
                    TOUCH_JOY_LOG("\t\tY : NORTH (%ld)", subvariant->rosetteScans[ROSETTE_NORTH]);
                    scanA = subvariant->rosetteScans[ROSETTE_NORTH];
                    shifA = subvariant->rosetteShift[ROSETTE_NORTH];
                } else if (radians < RADIANS_NORTH) {
                    TOUCH_JOY_LOG("\t\tXY : NORTHWEST (%ld)", subvariant->rosetteScans[ROSETTE_NORTHWEST]);
                    scanA = subvariant->rosetteScans[ROSETTE_NORTHWEST];
                    shifA = subvariant->rosetteShift[ROSETTE_NORTHWEST];
                } else {
                    TOUCH_JOY_LOG("\t\tXY : NORTHEAST (%ld)", subvariant->rosetteScans[ROSETTE_NORTHEAST]);
                    scanA = subvariant->rosetteScans[ROSETTE_NORTHEAST];
                    shifA = subvariant->rosetteShift[ROSETTE_NORTHEAST];
                }
                break;

            case OCTANT_NORTHEAST:
                if (subvariant->rosetteScans[ROSETTE_NORTHEAST] >= 0) {
                    TOUCH_JOY_LOG("\t\tXY : NORTHEAST (%ld)", subvariant->rosetteScans[ROSETTE_NORTHEAST]);
                    scanA = subvariant->rosetteScans[ROSETTE_NORTHEAST];
                    shifA = subvariant->rosetteShift[ROSETTE_NORTHEAST];
                } else {
                    TOUCH_JOY_LOG("\t\tXY : EAST (%ld) & NORTH (%ld)", subvariant->rosetteScans[ROSETTE_EAST], subvariant->rosetteScans[ROSETTE_NORTH]);
                    scanA = subvariant->rosetteScans[ROSETTE_EAST];
                    shifA = subvariant->rosetteShift[ROSETTE_EAST];
                    scanB = subvariant->rosetteScans[ROSETTE_NORTH];
                    shifB = subvariant->rosetteShift[ROSETTE_NORTH];
                }
                break;

            case OCTANT_WEST:
                if (subvariant->rosetteScans[ROSETTE_WEST] >= 0) {
                    TOUCH_JOY_LOG("\t\tY : WEST (%ld)", subvariant->rosetteScans[ROSETTE_WEST]);
                    scanA = subvariant->rosetteScans[ROSETTE_WEST];
                    shifA = subvariant->rosetteShift[ROSETTE_WEST];
                } else if (radians > RADIANS_WEST_NEG && radians < 0) {
                    TOUCH_JOY_LOG("\t\tXY : NORTHWEST (%ld)", subvariant->rosetteScans[ROSETTE_NORTHWEST]);
                    scanA = subvariant->rosetteScans[ROSETTE_NORTHWEST];
                    shifA = subvariant->rosetteShift[ROSETTE_NORTHWEST];
                } else {
                    TOUCH_JOY_LOG("\t\tXY : SOUTHWEST (%ld)", subvariant->rosetteScans[ROSETTE_SOUTHWEST]);
                    scanA = subvariant->rosetteScans[ROSETTE_SOUTHWEST];
                    shifA = subvariant->rosetteShift[ROSETTE_SOUTHWEST];
                }
                break;

            case OCTANT_EAST:
                if (subvariant->rosetteScans[ROSETTE_EAST] >= 0) {
                    TOUCH_JOY_LOG("\t\tY : EAST (%ld)", subvariant->rosetteScans[ROSETTE_EAST]);
                    scanA = subvariant->rosetteScans[ROSETTE_EAST];
                    shifA = subvariant->rosetteShift[ROSETTE_EAST];
                } else if (radians < RADIANS_EAST) {
                    TOUCH_JOY_LOG("\t\tXY : NORTHEAST (%ld)", subvariant->rosetteScans[ROSETTE_NORTHEAST]);
                    scanA = subvariant->rosetteScans[ROSETTE_NORTHEAST];
                    shifA = subvariant->rosetteShift[ROSETTE_NORTHEAST];
                } else {
                    TOUCH_JOY_LOG("\t\tXY : SOUTHEAST (%ld)", subvariant->rosetteScans[ROSETTE_SOUTHEAST]);
                    scanA = subvariant->rosetteScans[ROSETTE_SOUTHEAST];
                    shifA = subvariant->rosetteShift[ROSETTE_SOUTHEAST];
                }
                break;

            case OCTANT_SOUTHWEST:
                if (subvariant->rosetteScans[ROSETTE_SOUTHWEST] >= 0) {
                    TOUCH_JOY_LOG("\t\tXY : SOUTHWEST (%ld)", subvariant->rosetteScans[ROSETTE_SOUTHWEST]);
                    scanA = subvariant->rosetteScans[ROSETTE_SOUTHWEST];
                    shifA = subvariant->rosetteShift[ROSETTE_SOUTHWEST];
                } else {
                    TOUCH_JOY_LOG("\t\tXY : WEST (%ld) & SOUTH (%ld)", subvariant->rosetteScans[ROSETTE_WEST], subvariant->rosetteScans[ROSETTE_SOUTH]);
                    scanA = subvariant->rosetteScans[ROSETTE_WEST];
                    shifA = subvariant->rosetteShift[ROSETTE_WEST];
                    scanB = subvariant->rosetteScans[ROSETTE_SOUTH];
                    shifB = subvariant->rosetteShift[ROSETTE_SOUTH];
                }
                break;

            case OCTANT_SOUTH:
                if (subvariant->rosetteScans[ROSETTE_SOUTH] >= 0) {
                    TOUCH_JOY_LOG("\t\tY : SOUTH (%ld)", subvariant->rosetteScans[ROSETTE_SOUTH]);
                    scanA = subvariant->rosetteScans[ROSETTE_SOUTH];
                    shifA = subvariant->rosetteShift[ROSETTE_SOUTH];
                } else if (radians > RADIANS_SOUTH) {
                    TOUCH_JOY_LOG("\t\tXY : SOUTHWEST (%ld)", subvariant->rosetteScans[ROSETTE_SOUTHWEST]);
                    scanA = subvariant->rosetteScans[ROSETTE_SOUTHWEST];
                    shifA = subvariant->rosetteShift[ROSETTE_SOUTHWEST];
                } else {
                    TOUCH_JOY_LOG("\t\tXY : SOUTHEAST (%ld)", subvariant->rosetteScans[ROSETTE_SOUTHEAST]);
                    scanA = subvariant->rosetteScans[ROSETTE_SOUTHEAST];
                    shifA = subvariant->rosetteShift[ROSETTE_SOUTHEAST];
                }
                break;

            case OCTANT_SOUTHEAST:
                if (subvariant->rosetteScans[ROSETTE_SOUTHEAST] >= 0) {
                    TOUCH_JOY_LOG("\t\tXY : SOUTHEAST (%ld)", subvariant->rosetteScans[ROSETTE_SOUTHEAST]);
                    scanA = subvariant->rosetteScans[ROSETTE_SOUTHEAST];
                    shifA = subvariant->rosetteShift[ROSETTE_SOUTHEAST];
                } else {
                    TOUCH_JOY_LOG("\t\tXY : EAST (%ld) & SOUTH (%ld)", subvariant->rosetteScans[ROSETTE_EAST], subvariant->rosetteScans[ROSETTE_SOUTH]);
                    scanA = subvariant->rosetteScans[ROSETTE_EAST];
                    shifA = subvariant->rosetteShift[ROSETTE_EAST];
                    scanB = subvariant->rosetteScans[ROSETTE_SOUTH];
                    shifB = subvariant->rosetteShift[ROSETTE_SOUTH];
                }
                break;

            default:
                assert(false && "should not happen");
                break;
        }

        if (scanA < 0) {
            scanA = scanB;
            scanB = -1;
        }
        subvariant->scancodes[REPEAT_IDX_A] = scanA;
        subvariant->scancodes[REPEAT_IDX_B] = scanB;
        subvariant->isShifted[REPEAT_IDX_A] = shifA;
        subvariant->isShifted[REPEAT_IDX_B] = shifB;
    } while (0);

    if (subvariant->currentOctant != lastOctant) {
        // immediately press key upon threshold [re]-change ... and remove tap delay
        keypad_press_t pressIdx = subvariant->autoPressIdx;
        if (subvariant->scancodes[pressIdx] < 0) {
            _advance_press_index(&subvariant->autoPressIdx);
            pressIdx = subvariant->autoPressIdx;
        } else {
            _advance_press_index(&subvariant->autoPressIdx);
        }

        _press_key(subvariant, pressIdx);

        // remove tap delay
        subvariant->frameCount = joyglobals.tapDelayFrames;

        // reset strobeShifter if keys differ
        if (lastScancodes[pressIdx] != subvariant->scancodes[pressIdx]) {
            subvariant->strobeShifter = 0;
        }

        if (isTouchUp) {
            subvariant->scancodes[pressIdx] = -1;
        }
    }
}

static void _subvariant_touchUp(subvariant_s *subvariant, int dx, int dy) {

    _subvariant_touchMove(subvariant, dx, dy, /*isTouchUp:*/true);

    subvariant->currEventType = TOUCH_UP;

    // remove tap delay ...
    subvariant->frameCount = joyglobals.tapDelayFrames;

    if (subvariant->keyPressCount == 0) {
        // nothing previously pressed, immediately press origin key
        assert(subvariant->currentOctant == ORIGIN);
        _press_key(subvariant, REPEAT_IDX_A);
        subvariant->scancodes[REPEAT_IDX_A] = -1;
    }
}

// ----------------------------------------------------------------------------
// axis key(s) state

static void touchkpad_axisDown(void) {

    SPIN_LOCK_FULL(&kpad.spinlock);
    TOUCH_JOY_LOG("\t\tkpad axisDown acquire");

    _subvariant_touchDown(&kpad.axis);

    TOUCH_JOY_LOG("\t\tkpad axisDown release");
    SPIN_UNLOCK_FULL(&kpad.spinlock);
}

static void touchkpad_axisMove(int dx, int dy) {

    SPIN_LOCK_FULL(&kpad.spinlock);
    TOUCH_JOY_LOG("\t\tkpad axisMove acquire");

    _subvariant_touchMove(&kpad.axis, dx, dy, /*isTouchUp:*/false);

    TOUCH_JOY_LOG("\t\tkpad axisMove release");
    SPIN_UNLOCK_FULL(&kpad.spinlock);
}

static void touchkpad_axisUp(int dx, int dy) {

    SPIN_LOCK_FULL(&kpad.spinlock);
    TOUCH_JOY_LOG("\t\tkpad axisUp acquire");

    _subvariant_touchUp(&kpad.axis, dx, dy);

    TOUCH_JOY_LOG("\t\tkpad axisUp release");
    SPIN_UNLOCK_FULL(&kpad.spinlock);
}

// ----------------------------------------------------------------------------
// button key(s) state

static void touchkpad_buttonDown(void) {

    SPIN_LOCK_FULL(&kpad.spinlock);
    TOUCH_JOY_LOG("\t\tkpad buttonDown acquire");

    _subvariant_touchDown(&kpad.butt);

    TOUCH_JOY_LOG("\t\tkpad buttonDown release");
    SPIN_UNLOCK_FULL(&kpad.spinlock);
}

static void touchkpad_buttonMove(int dx, int dy) {

    SPIN_LOCK_FULL(&kpad.spinlock);
    TOUCH_JOY_LOG("\t\tkpad buttonMove acquire");

    _subvariant_touchMove(&kpad.butt, dx, dy, /*isTouchUp:*/false);

    TOUCH_JOY_LOG("\t\tkpad buttonMove release");
    SPIN_UNLOCK_FULL(&kpad.spinlock);
}

static void touchkpad_buttonUp(int dx, int dy) {

    SPIN_LOCK_FULL(&kpad.spinlock);
    TOUCH_JOY_LOG("\t\tkpad buttonUp acquire");

    _subvariant_touchUp(&kpad.butt, dx, dy);

    TOUCH_JOY_LOG("\t\tkpad buttonUp release");
    SPIN_UNLOCK_FULL(&kpad.spinlock);
}

// ----------------------------------------------------------------------------
// prefs handling

static void _subvariant_prefsChanged(subvariant_s *subvariant, const char *domain, const char *prefKey) {
    long lVal = 0;
    bool bVal = false;

    // ASCII : "kp{Axis,Butt}Rosette" : [ { "scan" : 16, "isShifted" : false, "ch" : 81 }, ... ]
    JSON_ref array = NULL;
    do {
        if (!prefs_copyJSONValue(domain, prefKey, &array)) {
            LOG("could not parse touch keypad rosette for domain %s", domain);
            break;
        }
        long count = 0;
        if (!json_arrayCount(array, &count)) {
            LOG("rosette is not an array!");
            break;
        }
        if (count != ROSETTE_COUNT) {
            LOG("rosette count unexpected : %lu!", count);
            break;
        }
        for (unsigned long i=0; i<ROSETTE_COUNT; i++) {
            JSON_ref map = NULL;

            if (json_arrayCopyJSONAtIndex(array, i, &map) <= 0) {
                LOG("could not parse touch keypad rosette data at index %lu!", i);
                break;
            }

            assert(map != NULL);
            if (!json_isMap(map)) {
                LOG("touch keypad rosette at index %lu is not a map!", i);
                break;
            }

            subvariant->rosetteShift[i] = json_mapParseBoolValue(map, "isShifted", &bVal) ? bVal : false;

            subvariant->rosetteChars[i] = json_mapParseLongValue(map, "ch", &lVal, /*base:*/10) ? (uint8_t)lVal : ' ';
            subvariant->rosetteScans[i] = json_mapParseLongValue(map, "scan", &lVal, /*base:*/10) ? lVal : -1;
            if (subvariant->rosetteScans[i] > 0x7f) {
                subvariant->rosetteScans[i] = -1;
            }

            json_destroy(&map);
        }
    } while (0);

    json_destroy(&array);
}

static void touchkpad_prefsChanged(const char *domain) {
    assert(video_isRenderThread());

    long lVal = 0;
    bool bVal = false;

    kpad.autostrobeDelay = !(prefs_parseBoolValue(domain, PREF_KPAD_FAST_AUTOREPEAT, &bVal) ? bVal : true);

    do {
        const bool rosetteShift[ROSETTE_ROWS*ROSETTE_COLS] = { false };
        const uint8_t rosetteChars[ROSETTE_ROWS*ROSETTE_COLS] = {
            ICONTEXT_NONACTIONABLE,           'I',          ICONTEXT_NONACTIONABLE,
            'J',                    ICONTEXT_NONACTIONABLE,          'K',
            ICONTEXT_NONACTIONABLE,           'M',          ICONTEXT_NONACTIONABLE,
        };
        const int rosetteScans[ROSETTE_ROWS*ROSETTE_COLS] = {
                     -1,             keys_ascii2Scancode('I'),          -1,
            keys_ascii2Scancode('J'),          -1,             keys_ascii2Scancode('K'),
                     -1,             keys_ascii2Scancode('M'),          -1,
        };
        for (unsigned long i=0; i<ROSETTE_COUNT; i++) {
            kpad.axis.rosetteShift[i] = rosetteShift[i];
            kpad.axis.rosetteChars[i] = rosetteChars[i];
            kpad.axis.rosetteScans[i] = rosetteScans[i];
        }

        _subvariant_prefsChanged(&kpad.axis, domain, PREF_KPAD_AXIS_ROSETTE);
    } while (0);

    do {
        const bool rosetteShift[ROSETTE_ROWS*ROSETTE_COLS] = { false };
        const uint8_t rosetteChars[ROSETTE_ROWS*ROSETTE_COLS] = {
            ICONTEXT_NONACTIONABLE, ICONTEXT_NONACTIONABLE, ICONTEXT_NONACTIONABLE,
            ICONTEXT_NONACTIONABLE, ICONTEXT_SPACE_VISUAL , ICONTEXT_NONACTIONABLE,
            ICONTEXT_NONACTIONABLE, ICONTEXT_NONACTIONABLE, ICONTEXT_NONACTIONABLE,
        };
        const int rosetteScans[ROSETTE_ROWS*ROSETTE_COLS] = {
            keys_ascii2Scancode(' '), keys_ascii2Scancode(' '), keys_ascii2Scancode(' '),
            keys_ascii2Scancode(' '), keys_ascii2Scancode(' '), keys_ascii2Scancode(' '),
            keys_ascii2Scancode(' '), keys_ascii2Scancode(' '), keys_ascii2Scancode(' '),
        };
        for (unsigned long i=0; i<ROSETTE_COUNT; i++) {
            kpad.butt.rosetteShift[i] = rosetteShift[i];
            kpad.butt.rosetteChars[i] = rosetteChars[i];
            kpad.butt.rosetteScans[i] = rosetteScans[i];
        }

        _subvariant_prefsChanged(&kpad.butt, domain, PREF_KPAD_BUTT_ROSETTE);
    } while (0);
}

// ----------------------------------------------------------------------------

static interface_device_t touchkpad_variant(void) {
    return TOUCH_DEVICE_JOYSTICK_KEYPAD;
}

static void touchkpad_resetState(void) {
    TOUCH_JOY_LOG("\t\tkpad resetState");

    kpad.spinlock = SPINLOCK_INIT;
    kpad.frameCallback = NULL;

    _reset_subvariant_state(&kpad.axis);
    _reset_subvariant_state(&kpad.butt);
}

static void touchkpad_setup(void) {
    // ...
}

static void touchkpad_shutdown(void) {
    // ...
}

static uint8_t *touchkpad_axisRosetteChars(void) {
    return kpad.axis.rosetteChars;
}

static uint8_t *touchkpad_buttRosetteChars(void) {
    return kpad.butt.rosetteChars;
}

static uint8_t touchkpad_buttActiveChar(void) {
    return ICONTEXT_NONACTIONABLE;
}

// ----------------------------------------------------------------------------

static void _init_gltouchjoy_kpad(void) {
    LOG("Registering OpenGL software touch joystick (keypad variant)");

    for (unsigned int i=0; i<MAX_REPEATING; i++) {
        kpad.axis.scancodes[i] = -1;
        kpad.axis.isShifted[i] = false;
        kpad.butt.scancodes[i] = -1;
        kpad.butt.isShifted[i] = false;
    }

    kpadVariant.variant = &touchkpad_variant;
    kpadVariant.resetState = &touchkpad_resetState;
    kpadVariant.setup = &touchkpad_setup;
    kpadVariant.shutdown = &touchkpad_shutdown;

    kpadVariant.prefsChanged = &touchkpad_prefsChanged;

    kpadVariant.buttonDown = &touchkpad_buttonDown;
    kpadVariant.buttonMove = &touchkpad_buttonMove;
    kpadVariant.buttonUp = &touchkpad_buttonUp;

    kpadVariant.axisDown = &touchkpad_axisDown;
    kpadVariant.axisMove = &touchkpad_axisMove;
    kpadVariant.axisUp = &touchkpad_axisUp;

    kpadVariant.axisRosetteChars = &touchkpad_axisRosetteChars;
    kpadVariant.buttRosetteChars = &touchkpad_buttRosetteChars;
    kpadVariant.buttActiveChar = &touchkpad_buttActiveChar;

    gltouchjoy_registerVariant(TOUCH_DEVICE_JOYSTICK_KEYPAD, &kpadVariant);

    video_registerFrameCallback(&kpad.frameCallback);
}

static __attribute__((constructor)) void __init_gltouchjoy_kpad(void) {
    emulator_registerStartupCallback(CTOR_PRIORITY_EARLY, &_init_gltouchjoy_kpad);
}

