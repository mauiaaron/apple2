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

#ifndef _GLTOUCHJOY_H_
#define _GLTOUCHJOY_H_

#include "video/glhudmodel.h"
#include "video/glnode.h"

#define DEBUG_TOUCH_JOY 0
#if DEBUG_TOUCH_JOY
#   define TOUCH_JOY_LOG(...) _SIMPLE_LOG(__VA_ARGS__)
#else
#   define TOUCH_JOY_LOG(...)
#endif

#define ROSETTE_ROWS 3
#define ROSETTE_COLS 3
#define ROSETTE_COUNT (ROSETTE_ROWS*ROSETTE_COLS)

#define BUTTON_TAP_DELAY_FRAMES_DEFAULT 12L // 12 * 16.688 millis == ~0.2sec
#define BUTTON_TAP_DELAY_FRAMES_MAX     30L // 30 * 16.688 millis == ~0.5sec

// TOUCH EVENT LIFECYCLE MAPPING TO APPLE ][ JOYSTICK BUTTONS OR KEYS
// ------------------------------------------------------------------
//
// The touch screen event lifecycle (touch down, touch move, touch up) has a direct 1:1 correspondence to the Apple ][
// joystick axis.  Touch down zeros the joystick axis, touch move will convert the screen coordinates to x,y joystick
// axis value, touch up will re-zero the joystick axis.  The only user configuration here is to normalize and scale
// between the touchscreen coordinates to the 0-255 Apple ][ joystick axes.
//
// Difficulty arises in the mapping between the touch screen events and which button/key should be fired.  The gltouchjoy
// implementation (both traditional joystick and keypad joystick) allows tap/long-press and swipe gestures to fire
// different configured buttons/keys.
//
// BUTTON TAP DELAY RATIONALE
// --------------------------
//
// A touch-down can be an ambiguous event ... does the user intend to tap/long-press or swipe/swipe-and-hold?  Our
// mapping of the touch lifecycle to the joystick buttons/keys uses a user-configurable delay before firing the
// appropriate button/key.
//
// BUTTON TAP DELAY STATE MACHINE
// ------------------------------
//
//  - On touch-down, we need to delay for a certain amount of time (video frames) to determine the user's intent
//      + This latches the end-of-video-frame callback (on CPU thread).  The amount of frames to delay is a configurable
//        preference -- (who are we to hardcode this determination?)
//          * Users likely want a custom twitch response setting, (at smaller thresholds, running the risk of firing a
//            tap when they intended just a swipe)
//          * Some games may not make use of the second joystick button, and so may be more effectively played with only
//            tapping (no swipes) and a `tapDelayFrames` configured to zero.  (In which case the button down event is
//            non-ambiguous) and should immediately fire the configured tap button.
//  - Ambiguity about user's intent is also resolved the moment there is a touch-up or a touch-move beyond the `switchThreshold`
//      + Touch-up will immediately fire the tap button (if not yet fired)
//      + Touch-move beyond the configured `switchThreshold` will immediately fire the appropriate configured swipe button
//      + Touch-move beneath the `switchThreshold` is considered ambiguous "jitter"
//  - The end-of-video-frame callback on the CPU thread...
//      + ...handles tap delay counting
//      + ...handles continual fire of current button (during touch-and-hold, or swipe-and-hold)
//      + ...will unregister itself after touch-up event
//
// SCENARIOS:
//  - SINGLE TAP:
//      + down ... (move <  `switchThreshold`)* ... up ... [cbX]
//          * up: fires configured tap button on UI thread, resets `tapDelayFrames`
//          * cbX: after `tapDelayFrames`, resets joystick button state and unregisters itself from the e-o-f callback
//
//  - SINGLE TOUCH-AND-HOLD:
//      + down ... ([cb], move < `switchThreshold`)* ... up ... [cbX]
//      + down ... (move < `switchThreshold`, [cb])* ... up ... [cbX]
//          * [cb] fires configured tap button on CPU thread end-of-frame callback
//
//  - down, move >= `switchThreshold`, up
//  - down, (move >= `switchThreshold`, move back to < `switchThreshold`)+, up
//  - down, ... cancel/reset

typedef enum touchjoy_button_type_t {
    TOUCH_NONE = 0,
    TOUCH_BUTTON1 = 1,
    TOUCH_BUTTON2,
    TOUCH_BOTH,
    // --or-- an ASCII/fonttext value ...
} touchjoy_button_type_t;

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

// globals

typedef struct GLTouchJoyGlobals {

    bool prefsChanged;
    bool isAvailable;       // Were there any OpenGL/memory errors on gltouchjoy initialization?
    bool isCalibrating;     // Are we running in calibration mode?
    bool ownsScreen;        // Does the touchjoy currently own the screen?
    bool showControls;      // Are controls visible?
    bool showAzimuth;       // Is joystick azimuth shown?
    float minAlphaWhenOwnsScreen;
    float minAlpha;
    float screenDivider;
    float axisMultiplier;   // multiplier for joystick axis
    bool axisIsOnLeft;
    long switchThreshold;   // threshold over which a move is in one of the octants
    long tapDelayFrames;    // tap/swipe discrimination delay

} GLTouchJoyGlobals;
extern GLTouchJoyGlobals joyglobals;

typedef struct GLTouchJoyVariant {
    interface_device_t (*variant)(void);
    void (*resetState)(void);
    void (*setup)(void);
    void (*shutdown)(void);

    void (*prefsChanged)(const char *domain);

    void (*buttonDown)(void);
    void (*buttonMove)(int dx, int dy);
    void (*buttonUp)(int dx, int dy);

    void (*axisDown)(void);
    void (*axisMove)(int dx, int dy);
    void (*axisUp)(int dx, int dy);

    uint8_t *(*axisRosetteChars)(void);
    uint8_t *(*buttRosetteChars)(void);
    uint8_t (*buttActiveChar)(void);

} GLTouchJoyVariant;

// registers a touch joystick variant with manager
void gltouchjoy_registerVariant(interface_device_t variant, GLTouchJoyVariant *touchJoyVariant);

#endif // whole file
