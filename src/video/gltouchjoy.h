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

#include "common.h"
#include "video/glvideo.h"
#include "video/glhudmodel.h"
#include "video/glnode.h"

#define DEBUG_TOUCH_JOY 0
#if DEBUG_TOUCH_JOY
#   define TOUCH_JOY_LOG(...) LOG(__VA_ARGS__)
#else
#   define TOUCH_JOY_LOG(...)
#endif

// globals

typedef struct GLTouchJoyGlobals {

    bool isAvailable;       // Were there any OpenGL/memory errors on gltouchjoy initialization?
    bool isShuttingDown;
    bool isCalibrating;     // Are we running in calibration mode?
    bool isEnabled;         // Does player want touchjoy enabled?
    bool ownsScreen;        // Does the touchjoy currently own the screen?
    bool showControls;      // Are controls visible?
    bool showAzimuth;       // Is joystick azimuth shown?
    float minAlphaWhenOwnsScreen;
    float minAlpha;
    float screenDivider;
    bool axisIsOnLeft;

    int switchThreshold;

} GLTouchJoyGlobals;
extern GLTouchJoyGlobals joyglobals;

// touch axis variables

typedef struct GLTouchJoyAxes {

    // origin model/texture
    GLModel *model;
    bool modelDirty;

    // azimuth model
    GLModel *azimuthModel;

    uint8_t rosetteChars[ROSETTE_ROWS * ROSETTE_COLS];
    int rosetteScancodes[ROSETTE_ROWS * ROSETTE_COLS];

    int centerX;
    int centerY;
    float multiplier;
    int trackingIndex;
    struct timespec timingBegin;

} GLTouchJoyAxes;
extern GLTouchJoyAxes axes;

// button object variables

typedef struct GLTouchJoyButtons {
    GLModel *model;
    uint8_t activeChar;
    bool modelDirty;

    touchjoy_button_type_t touchDownChar;
    int                    touchDownScancode;
    touchjoy_button_type_t northChar;
    int                    northScancode;
    touchjoy_button_type_t southChar;
    int                    southScancode;

    int centerX;
    int centerY;
    int trackingIndex;
    struct timespec timingBegin;

} GLTouchJoyButtons;
extern GLTouchJoyButtons buttons;

typedef struct GLTouchJoyVariant {
    touchjoy_variant_t (*variant)(void);
    void (*resetState)(void);
    void (*setup)(void (*buttonDrawCallback)(char newChar));
    void (*shutdown)(void);

    void (*buttonDown)(void);
    void (*buttonMove)(int dx, int dy);
    void (*buttonUp)(int dx, int dy);

    void (*axisDown)(void);
    void (*axisMove)(int dx, int dy);
    void (*axisUp)(int dx, int dy);

} GLTouchJoyVariant;

// registers a touch joystick variant with manager
void gltouchjoy_registerVariant(touchjoy_variant_t variant, GLTouchJoyVariant *touchJoyVariant);

#endif // whole file
