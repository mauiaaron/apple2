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
#   define TOUCH_JOY_LOG(...) LOG(__VA_ARGS__)
#else
#   define TOUCH_JOY_LOG(...)
#endif

#define ROSETTE_ROWS 3
#define ROSETTE_COLS 3

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

    int centerX;
    int centerY;
    int trackingIndex;
    struct timespec timingBegin;

} GLTouchJoyButtons;
extern GLTouchJoyButtons buttons;

typedef struct GLTouchJoyVariant {
    interface_device_t (*variant)(void);
    void (*resetState)(void);
    void (*setup)(void (*buttonDrawCallback)(char newChar));
    void (*shutdown)(void);

    void (*prefsChanged)(const char *domain);

    void (*buttonDown)(void);
    void (*buttonMove)(int dx, int dy);
    void (*buttonUp)(int dx, int dy);

    void (*axisDown)(void);
    void (*axisMove)(int dx, int dy);
    void (*axisUp)(int dx, int dy);

    uint8_t *(*rosetteChars)(void);

} GLTouchJoyVariant;

// registers a touch joystick variant with manager
void gltouchjoy_registerVariant(interface_device_t variant, GLTouchJoyVariant *touchJoyVariant);

#endif // whole file
