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

typedef enum AndroidTouchDevice {
    // Maps to values in Apple2Preferences.java
    ANDROID_TOUCH_NONE = 0,
    ANDROID_TOUCH_JOYSTICK,
    ANDROID_TOUCH_JOYSTICK_KEYPAD,
    ANDROID_TOUCH_KEYBOARD,
    ANDROID_TOUCH_DEVICE_MAX,
} AndroidTouchDevice;

typedef enum AndroidTouchJoystickButtonValues {
    //ANDROID_TOUCH_NONE = 0,
    ANDROID_TOUCHJOY_BUTTON0 = 1,
    ANDROID_TOUCHJOY_BUTTON1,
    ANDROID_TOUCHJOY_BUTTON_BOTH,
} AndroidTouchJoystickButtonValues;

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
            joydriver_setTouchVariant(EMULATED_JOYSTICK);
            break;

        case ANDROID_TOUCH_JOYSTICK_KEYPAD:
            keydriver_setTouchKeyboardOwnsScreen(false);
            joydriver_setTouchJoystickOwnsScreen(true);
            joydriver_setTouchVariant(EMULATED_KEYPAD);
            break;

        case ANDROID_TOUCH_KEYBOARD:
            keydriver_setTouchKeyboardOwnsScreen(true);
            joydriver_setTouchJoystickOwnsScreen(false);
            break;

        case ANDROID_TOUCH_NONE:
        default:
            break;
    }
}

jint Java_org_deadc0de_apple2ix_Apple2Preferences_nativeGetCurrentTouchDevice(JNIEnv *env, jclass cls) {
    LOG("nativeGetCurrentTouchDevice() ...");
    if (joydriver_ownsScreen()) {
        touchjoy_variant_t variant = joydriver_getTouchVariant();
        if (variant == EMULATED_JOYSTICK) {
            return ANDROID_TOUCH_JOYSTICK;
        } else if (variant == EMULATED_JOYSTICK) {
            return ANDROID_TOUCH_JOYSTICK_KEYPAD;
        }
    } else if (keydriver_ownsScreen()) {
        return ANDROID_TOUCH_KEYBOARD;
    }
    return ANDROID_TOUCH_NONE;
}

void Java_org_deadc0de_apple2ix_Apple2Preferences_nativeSetTouchMenuEnabled(JNIEnv *env, jclass cls, jboolean enabled) {
    LOG("native set touch menu enabled : %d", enabled);
    interface_setTouchMenuEnabled(enabled);
}

void Java_org_deadc0de_apple2ix_Apple2Preferences_nativeSetTouchMenuVisibility(JNIEnv *env, jclass cls, jfloat alpha) {
    LOG("native set touch menu visibility : %f", alpha);
    interface_setTouchMenuVisibility(alpha);
}

void Java_org_deadc0de_apple2ix_Apple2Preferences_nativeSetTouchJoystickButtonTypes(JNIEnv *env, jclass cls, jint touchDownButton, jint northButton, jint southButton) {
    LOG("nativeSetTouchJoystickButtonTypes() : %d,%d,%d", touchDownButton, northButton, southButton);

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

    joydriver_setTouchButtonTypes((touchjoy_button_type_t)touchDownButton, (touchjoy_button_type_t)northButton, (touchjoy_button_type_t)southButton);
}

void Java_org_deadc0de_apple2ix_Apple2Preferences_nativeSetTouchJoystickTapDelay(JNIEnv *env, jclass cls, jfloat secs) {
    LOG("nativeSetTouchJoystickTapDelay() : %f", secs);
    joydriver_setTapDelay(secs);
}

void Java_org_deadc0de_apple2ix_Apple2Preferences_nativeSetTouchJoystickAxisSensitivity(JNIEnv *env, jclass cls, jfloat multiplier) {
    LOG("nativeSetTouchJoystickAxisSensitivity() : %f", multiplier);
    joydriver_setTouchAxisSensitivity(multiplier);
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

