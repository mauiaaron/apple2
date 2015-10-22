/*
 * Apple // emulator for *ix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 2015 Aaron Culliney
 *
 */

#include "common.h"

#include <jni.h>

typedef enum AndroidTouchJoystickButtonValues {
    //ANDROID_TOUCHJOY_NONE = 0,
    ANDROID_TOUCHJOY_BUTTON0 = 1,
    ANDROID_TOUCHJOY_BUTTON1,
    ANDROID_TOUCHJOY_BUTTON_BOTH,
} AndroidTouchJoystickButtonValues;

void Java_org_deadc0de_apple2ix_Apple2Preferences_nativeSetColor(JNIEnv *env, jclass cls, jint color) {
    LOG("color : %d", color);
#if TESTING
    color_mode = COLOR;
#else
    if (color < COLOR_NONE || color > COLOR_INTERP) {
        return;
    }
    color_mode = color;

    video_reset();
    video_setpage(!!(softswitches & SS_SCREEN));
    video_redraw();
#endif
}

jboolean Java_org_deadc0de_apple2ix_Apple2Preferences_nativeSetSpeakerEnabled(JNIEnv *env, jclass cls, jboolean enabled) {
    LOG("enabled : %d", true);
    // NO-OP ... speaker should always be enabled (but volume could be zero)
    return true;
}

void Java_org_deadc0de_apple2ix_Apple2Preferences_nativeSetSpeakerVolume(JNIEnv *env, jclass cls, jint goesToTen) {
    LOG("volume : %d", goesToTen);
    assert(goesToTen >= 0);
    sound_volume = goesToTen;
#warning FIXME TODO refactor/remove sound_volume ?
    vm_reinitializeAudio();
}

void Java_org_deadc0de_apple2ix_Apple2Preferences_nativeSetAudioLatency(JNIEnv *env, jclass cls, jfloat latencySecs) {
#if !TESTING
    LOG("audio latency : %fsecs", latencySecs);
    assert(cpu_isPaused());
    audio_setLatency(latencySecs);
    timing_reinitializeAudio();
#endif
}

jboolean Java_org_deadc0de_apple2ix_Apple2Preferences_nativeSetMockingboardEnabled(JNIEnv *env, jclass cls, jboolean enabled) {
#if !TESTING
    LOG("mockingboard enabled : %d", enabled);
    assert(cpu_isPaused());
    MB_SetEnabled(enabled);
    timing_reinitializeAudio();
#endif
    return enabled;
}

void Java_org_deadc0de_apple2ix_Apple2Preferences_nativeSetMockingboardVolume(JNIEnv *env, jclass cls, jint goesToTen) {
    LOG("mockingboard volume : %d", goesToTen);
    assert(goesToTen >= 0);
    MB_SetVolumeZeroToTen(goesToTen);
}

void Java_org_deadc0de_apple2ix_Apple2Preferences_nativeSetCurrentTouchDevice(JNIEnv *env, jclass cls, jint touchDevice) {
    LOG("current touch device : %d", touchDevice);
    assert(touchDevice >= 0 && touchDevice < TOUCH_DEVICE_DEVICE_MAX);
    switch (touchDevice) {
        case TOUCH_DEVICE_JOYSTICK:
            keydriver_setTouchKeyboardOwnsScreen(false);
            joydriver_setTouchJoystickOwnsScreen(true);
            joydriver_setTouchVariant(EMULATED_JOYSTICK);
            video_backend->animation_showTouchJoystick();
            break;

        case TOUCH_DEVICE_JOYSTICK_KEYPAD:
            keydriver_setTouchKeyboardOwnsScreen(false);
            joydriver_setTouchJoystickOwnsScreen(true);
            joydriver_setTouchVariant(EMULATED_KEYPAD);
            video_backend->animation_showTouchJoystick();
            break;

        case TOUCH_DEVICE_KEYBOARD:
            keydriver_setTouchKeyboardOwnsScreen(true);
            joydriver_setTouchJoystickOwnsScreen(false);
            video_backend->animation_showTouchKeyboard();
            break;

        case TOUCH_DEVICE_NONE:
        default:
            break;
    }
}

void Java_org_deadc0de_apple2ix_Apple2Preferences_nativeSetTouchJoystickVisibility(JNIEnv *env, jclass cls, jboolean visibility) {
    LOG("visibility: %d", visibility);
    joydriver_setShowControls(visibility);
}

jint Java_org_deadc0de_apple2ix_Apple2Preferences_nativeGetCurrentTouchDevice(JNIEnv *env, jclass cls) {
    LOG("%s", "");
    if (joydriver_ownsScreen()) {
        touchjoy_variant_t variant = joydriver_getTouchVariant();
        if (variant == EMULATED_JOYSTICK) {
            return TOUCH_DEVICE_JOYSTICK;
        } else if (variant == EMULATED_KEYPAD) {
            return TOUCH_DEVICE_JOYSTICK_KEYPAD;
        }
    } else if (keydriver_ownsScreen()) {
        return TOUCH_DEVICE_KEYBOARD;
    }
    return TOUCH_DEVICE_NONE;
}

void Java_org_deadc0de_apple2ix_Apple2Preferences_nativeSetTouchMenuEnabled(JNIEnv *env, jclass cls, jboolean enabled) {
    LOG("enabled : %d", enabled);
    interface_setTouchMenuEnabled(enabled);
}

void Java_org_deadc0de_apple2ix_Apple2Preferences_nativeSetTouchKeyboardLowercaseEnabled(JNIEnv *env, jclass cls, jboolean enabled) {
    LOG("enabled : %d", enabled);
    keydriver_setLowercaseEnabled(enabled);
}

void Java_org_deadc0de_apple2ix_Apple2Preferences_nativeSetTouchMenuVisibility(JNIEnv *env, jclass cls, jfloat alpha) {
    LOG("visibility : %f", alpha);
    interface_setTouchMenuVisibility(alpha);
}

void Java_org_deadc0de_apple2ix_Apple2Preferences_nativeSetTouchKeyboardVisibility(JNIEnv *env, jclass cls, jfloat inactiveAlpha, jfloat activeAlpha) {
    LOG("inactive:%f active:%f", inactiveAlpha, activeAlpha);
    keydriver_setVisibilityWhenOwnsScreen(inactiveAlpha, activeAlpha);
}

void Java_org_deadc0de_apple2ix_Apple2Preferences_nativeSetTouchJoystickButtonTypes(JNIEnv *env, jclass cls, jint touchDownButton, jint northButton, jint southButton) {
    LOG(": %d,%d,%d", touchDownButton, northButton, southButton);

    touchDownButton -= 1;
    northButton -= 1;
    southButton -= 1;
    if (touchDownButton < TOUCH_NONE || touchDownButton > TOUCH_BOTH) {
        ERRLOG("OOPS, invalid parameter!");
        return;
    }
    if (northButton < TOUCH_NONE || northButton > TOUCH_BOTH) {
        ERRLOG("OOPS, invalid parameter!");
        return;
    }
    if (southButton < TOUCH_NONE || southButton > TOUCH_BOTH) {
        ERRLOG("OOPS, invalid parameter!");
        return;
    }

    uint8_t rosetteChars[ROSETTE_COLS * ROSETTE_ROWS];
    int rosetteScancodes[ROSETTE_COLS * ROSETTE_ROWS];
    rosetteChars[ROSETTE_NORTHWEST] = ' ';                      rosetteScancodes[ROSETTE_NORTHWEST] = -1;
    rosetteChars[ROSETTE_NORTH]     = (uint8_t)MOUSETEXT_UP;    rosetteScancodes[ROSETTE_NORTH]     = -1;
    rosetteChars[ROSETTE_NORTHEAST] = ' ';                      rosetteScancodes[ROSETTE_NORTHEAST] = -1;
    rosetteChars[ROSETTE_WEST]      = (uint8_t)MOUSETEXT_LEFT;  rosetteScancodes[ROSETTE_WEST]      = -1;
    rosetteChars[ROSETTE_CENTER]    = '+';                      rosetteScancodes[ROSETTE_CENTER]    = -1;
    rosetteChars[ROSETTE_EAST]      = (uint8_t)MOUSETEXT_RIGHT; rosetteScancodes[ROSETTE_EAST]      = -1;
    rosetteChars[ROSETTE_SOUTHWEST] = ' ';                      rosetteScancodes[ROSETTE_SOUTHWEST] = -1;
    rosetteChars[ROSETTE_SOUTH]     = (uint8_t)MOUSETEXT_DOWN;  rosetteScancodes[ROSETTE_SOUTH]     = -1;
    rosetteChars[ROSETTE_SOUTHEAST] = ' ';                      rosetteScancodes[ROSETTE_SOUTHEAST] = -1;
    joydriver_setTouchAxisTypes(rosetteChars, rosetteScancodes);
    joydriver_setTouchButtonTypes((touchjoy_button_type_t)touchDownButton, -1, (touchjoy_button_type_t)northButton, -1, (touchjoy_button_type_t)southButton, -1);
}

void Java_org_deadc0de_apple2ix_Apple2Preferences_nativeSetTouchJoystickTapDelay(JNIEnv *env, jclass cls, jfloat secs) {
    LOG("tap delay : %f", secs);
    joydriver_setTapDelay(secs);
}

void Java_org_deadc0de_apple2ix_Apple2Preferences_nativeSetTouchJoystickAxisSensitivity(JNIEnv *env, jclass cls, jfloat multiplier) {
    LOG("axis sensitivity : %f", multiplier);
    joydriver_setTouchAxisSensitivity(multiplier);
}

void Java_org_deadc0de_apple2ix_Apple2Preferences_nativeSetTouchJoystickButtonSwitchThreshold(JNIEnv *env, jclass cls, jint delta) {
    LOG("delta : %d", delta);
    joydriver_setButtonSwitchThreshold(delta);
}

void Java_org_deadc0de_apple2ix_Apple2Preferences_nativeTouchJoystickSetScreenDivision(JNIEnv *env, jclass cls, jfloat division) {
    LOG("division : %f", division);
    joydriver_setScreenDivision(division);
}

void Java_org_deadc0de_apple2ix_Apple2Preferences_nativeTouchJoystickSetAxisOnLeft(JNIEnv *env, jclass cls, jboolean axisIsOnLeft) {
    LOG("axis on left : %d", axisIsOnLeft);
    joydriver_setAxisOnLeft(axisIsOnLeft);
}

void Java_org_deadc0de_apple2ix_Apple2Preferences_nativeTouchJoystickSetKeypadTypes(JNIEnv *env, jclass cls,
        jintArray jRosetteChars, jintArray jRosetteScans, jintArray jButtonsChars, jintArray jButtonsScans)
{
    jint *rosetteChars = (*env)->GetIntArrayElements(env, jRosetteChars, 0);
    jint *rosetteScans = (*env)->GetIntArrayElements(env, jRosetteScans, 0);
    jint *buttonsChars = (*env)->GetIntArrayElements(env, jButtonsChars, 0);
    jint *buttonsScans = (*env)->GetIntArrayElements(env, jButtonsScans, 0);

    LOG("NW:%c/%d, N:%c/%d, NE:%c/%d, ... SWIPEUP:%c/%d",
            (char)rosetteChars[0], rosetteScans[0], (char)rosetteChars[1], rosetteScans[1], (char)rosetteChars[2], rosetteScans[2],
            (char)buttonsChars[1], buttonsScans[1]);
    LOG(" W:%c/%d, C:%c/%d,  E:%c/%d, ...     TAP:%c/%d",
            (char)rosetteChars[3], rosetteScans[3], (char)rosetteChars[4], rosetteScans[4], (char)rosetteChars[5], rosetteScans[5],
            (char)buttonsChars[0], buttonsScans[0]);
    LOG("SW:%c/%d, S:%c/%d, SE:%c/%d, ... SWIPEDN:%c/%d",
            (char)rosetteChars[6], rosetteScans[6], (char)rosetteChars[7], rosetteScans[7], (char)rosetteChars[8], rosetteScans[8],
            (char)buttonsChars[2], buttonsScans[2]);

    // we could just pass these as jcharArray ... but this isn't a tight loop =P
    uint8_t actualChars[ROSETTE_ROWS * ROSETTE_COLS];
    for (unsigned int i=0; i<(ROSETTE_ROWS * ROSETTE_COLS); i++) {
        actualChars[i] = (uint8_t)rosetteChars[i];
    }
    joydriver_setTouchAxisTypes(actualChars, rosetteScans);
    joydriver_setTouchButtonTypes(
            (touchjoy_button_type_t)buttonsChars[0], buttonsScans[0],
            (touchjoy_button_type_t)buttonsChars[1], buttonsScans[1],
            (touchjoy_button_type_t)buttonsChars[2], buttonsScans[2]);

    (*env)->ReleaseIntArrayElements(env, jRosetteChars, rosetteChars, 0);
    (*env)->ReleaseIntArrayElements(env, jRosetteScans, rosetteScans, 0);
    (*env)->ReleaseIntArrayElements(env, jButtonsChars, buttonsChars, 0);
    (*env)->ReleaseIntArrayElements(env, jButtonsScans, buttonsScans, 0);
}

void Java_org_deadc0de_apple2ix_Apple2Preferences_nativeTouchDeviceBeginCalibrationMode(JNIEnv *env, jclass cls) {
    LOG("%s", "");
    if (joydriver_ownsScreen()) {
        joydriver_beginCalibration();
    } else if (keydriver_ownsScreen()) {
        keydriver_beginCalibration();
    }
}

void Java_org_deadc0de_apple2ix_Apple2Preferences_nativeTouchDeviceEndCalibrationMode(JNIEnv *env, jclass cls) {
    LOG("%s", "");
    if (joydriver_ownsScreen()) {
        joydriver_endCalibration();
    } else if (keydriver_ownsScreen()) {
        keydriver_endCalibration();
    }
}

void Java_org_deadc0de_apple2ix_Apple2Preferences_nativeSetTouchDeviceKeyRepeatThreshold(JNIEnv *env, jclass cls, jfloat threshold) {
    LOG("threshold : %f", threshold);
    joydriver_setKeyRepeatThreshold(threshold);
}

jint Java_org_deadc0de_apple2ix_Apple2Preferences_nativeGetCPUSpeed(JNIEnv *env, jclass cls) {
    LOG("%s", "");
    return (jint)round(cpu_scale_factor * 100.0);
}

void Java_org_deadc0de_apple2ix_Apple2Preferences_nativeSetCPUSpeed(JNIEnv *env, jclass cls, jint percentSpeed) {
    LOG("percentSpeed : %d%%", percentSpeed);
#if TESTING
    cpu_scale_factor = CPU_SCALE_FASTEST;
    cpu_altscale_factor = CPU_SCALE_FASTEST;
    timing_initialize();
#else
    bool wasPaused = cpu_isPaused();

    if (!wasPaused) {
        cpu_pause();
    }

    cpu_scale_factor = percentSpeed/100.0;
    if (cpu_scale_factor > CPU_SCALE_FASTEST) {
        cpu_scale_factor = CPU_SCALE_FASTEST;
    }
    if (cpu_scale_factor < CPU_SCALE_SLOWEST) {
        cpu_scale_factor = CPU_SCALE_SLOWEST;
    }

    if (video_backend->animation_showCPUSpeed) {
        video_backend->animation_showCPUSpeed();
    }

    timing_initialize();

    if (!wasPaused) {
        cpu_resume();
    }
#endif
}

void Java_org_deadc0de_apple2ix_Apple2Preferences_nativeLoadTouchKeyboardJSON(JNIEnv *env, jclass cls, jstring j_jsonPath) {
    const char *jsonPath = (*env)->GetStringUTFChars(env, j_jsonPath, 0);
    LOG("jsonPath: %s", jsonPath);
    keydriver_loadAltKbd(jsonPath);
    (*env)->ReleaseStringUTFChars(env, j_jsonPath, jsonPath);
}

