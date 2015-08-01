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
    assert(cpu_isPaused());
    audio_setLatency(latencySecs);
    timing_reinitializeAudio();
}

jboolean Java_org_deadc0de_apple2ix_Apple2Preferences_nativeSetMockingboardEnabled(JNIEnv *env, jclass cls, jboolean enabled) {
    LOG("native set set mockingboard enabled : %d", enabled);
    assert(cpu_isPaused());
    MB_SetEnabled(enabled);
    timing_reinitializeAudio();
    return MB_ISEnabled();
}

void Java_org_deadc0de_apple2ix_Apple2Preferences_nativeSetMockingboardVolume(JNIEnv *env, jclass cls, jint goesToTen) {
    LOG("native set mockingboard volume : %d", goesToTen);
    assert(goesToTen >= 0);
    MB_SetVolumeZeroToTen(goesToTen);
}

void Java_org_deadc0de_apple2ix_Apple2Preferences_nativeSetCurrentTouchDevice(JNIEnv *env, jclass cls, jint touchDevice) {
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

void Java_org_deadc0de_apple2ix_Apple2Preferences_nativeSetTouchMenuEnabled(JNIEnv *env, jclass cls, jboolean enabled) {
    LOG("native set touch menu enabled : %d", enabled);
    interface_setTouchMenuEnabled(enabled);
}

void Java_org_deadc0de_apple2ix_Apple2Preferences_nativeSetTouchMenuVisibility(JNIEnv *env, jclass cls, jfloat alpha) {
    LOG("native set touch menu visibility : %f", alpha);
    interface_setTouchMenuVisibility(alpha);
}

jboolean Java_org_deadc0de_apple2ix_Apple2Preferences_nativeIsTouchKeyboardScreenOwner(JNIEnv *env, jclass cls) {
    LOG("nativeIsTouchKeyboardScreenOwner() ...");
    return keydriver_ownsScreen();
}

jboolean Java_org_deadc0de_apple2ix_Apple2Preferences_nativeIsTouchJoystickScreenOwner(JNIEnv *env, jclass cls) {
    LOG("nativeIsTouchJoystickScreenOwner() ...");
    return joydriver_ownsScreen();
}

jint Java_org_deadc0de_apple2ix_Apple2Preferences_nativeGetCPUSpeed(JNIEnv *env, jclass cls) {
    LOG("nativeGetCPUSpeed() ...");
    return (jint)round(cpu_scale_factor * 100.0);
}

void Java_org_deadc0de_apple2ix_Apple2Preferences_nativeSetCPUSpeed(JNIEnv *env, jclass cls, jint percentSpeed) {
    LOG("nativeSetCPUSpeed() : %d%%", percentSpeed);
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
}

