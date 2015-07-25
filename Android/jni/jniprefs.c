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

