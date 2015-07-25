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

#include "common.h"

#include <jni.h>

typedef enum AndroidTouchDevice_t {
    // Maps to values in Apple2Preferences.java
    ANDROID_TOUCH_NONE = 0,
    ANDROID_TOUCH_JOYSTICK,
    ANDROID_TOUCH_KEYBOARD,
    ANDROID_TOUCH_DEVICE_MAX,
} AndroidTouchDevice_t;

void Java_org_deadc0de_apple2ix_Apple2Preferences_nativeEnableTouchJoystick(JNIEnv *env, jclass cls, jboolean enabled) {
    LOG("native enable touch joystick : %d", enabled);
}

void Java_org_deadc0de_apple2ix_Apple2Preferences_nativeEnableTiltJoystick(JNIEnv *env, jclass cls, jboolean enabled) {
    LOG("native enable tilt joystick : %d", enabled);
}

void Java_org_deadc0de_apple2ix_Apple2Preferences_nativeSetColor(JNIEnv *env, jclass cls, jint color) {
    LOG("native set hires color : %d", color);
    if (color < COLOR_NONE || color > COLOR_INTERP) {
        return;
    }
    color_mode = color;

    video_reset();
    video_setpage(!!(softswitches & SS_SCREEN));
    video_redraw();
}

jboolean Java_org_deadc0de_apple2ix_Apple2Preferences_nativeSetSpeakerEnabled(JNIEnv *env, jclass cls, jboolean enabled) {
    LOG("native set speaker enabled : %d", true);
    // NO-OP ... speaker should always be enabled (but volume could be zero)
    return true;
}

void Java_org_deadc0de_apple2ix_Apple2Preferences_nativeSetSpeakerVolume(JNIEnv *env, jclass cls, jint goesToTen) {
    LOG("native set speaker volume : %d", goesToTen);
    assert(goesToTen >= 0);
    sound_volume = goesToTen;
#warning FIXME TODO refactor/remove sound_volume ?
    speaker_setVolumeZeroToTen(goesToTen);
}

void Java_org_deadc0de_apple2ix_Apple2Preferences_nativeSetAudioLatency(JNIEnv *env, jclass cls, jfloat latencySecs) {
    LOG("native set audio latency : %f", latencySecs);
    //assert(cpu_isPaused());
    audio_setLatency(latencySecs);
}

jboolean Java_org_deadc0de_apple2ix_Apple2Preferences_nativeSetMockingboardEnabled(JNIEnv *env, jclass cls, jboolean enabled) {
    LOG("native set set mockingboard enabled : %d", enabled);
    //assert(cpu_isPaused());
#warning FIXME ^^^ this should be true
    MB_Destroy();
    if (enabled) {
        MB_Initialize();
    }
    return MB_ISEnabled();
}

void Java_org_deadc0de_apple2ix_Apple2Preferences_nativeSetMockingboardVolume(JNIEnv *env, jclass cls, jint goesToTen) {
    LOG("native set mockingboard volume : %d", goesToTen);
    assert(goesToTen >= 0);
    MB_SetVolumeZeroToTen(goesToTen);
}

void Java_org_deadc0de_apple2ix_Apple2Preferences_nativeSetDefaultTouchDevice(JNIEnv *env, jclass cls, jint touchDevice) {
    LOG("native set default touch device : %d", touchDevice);
    assert(touchDevice >= 0 && touchDevice < ANDROID_TOUCH_DEVICE_MAX);
    switch (touchDevice) {
        case ANDROID_TOUCH_JOYSTICK:
            keydriver_setTouchKeyboardOwnsScreen(false);
            joydriver_setTouchJoystickOwnsScreen(true);
            break;

        case ANDROID_TOUCH_KEYBOARD:
            joydriver_setTouchJoystickOwnsScreen(false);
            keydriver_setTouchKeyboardOwnsScreen(true);
            break;

        case ANDROID_TOUCH_NONE:
        default:
            break;
    }
}

