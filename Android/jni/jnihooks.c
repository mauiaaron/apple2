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
#include "androidkeys.h"

#include <jni.h>
#include <math.h>

enum {
    ANDROID_ACTION_DOWN = 0x0,
    ANDROID_ACTION_UP = 0x1,
    ANDROID_ACTION_MOVE = 0x2,
    ANDROID_ACTION_CANCEL = 0x3,
    ANDROID_ACTION_POINTER_DOWN = 0x5,
    ANDROID_ACTION_POINTER_UP = 0x6,
};

static bool nativePaused = false;

#if TESTING
static bool _run_tests(void) {
    char *local_argv[] = {
        "-f",
        NULL
    };
    int local_argc = 0;
    for (char **p = &local_argv[0]; *p != NULL; p++) {
        ++local_argc;
    }
#if defined(TEST_CPU)
    extern int test_cpu(int, char *[]);
    test_cpu(local_argc, local_argv);
#elif defined(TEST_VM)
    extern int test_vm(int, char *[]);
    test_vm(local_argc, local_argv);
#elif defined(TEST_DISPLAY)
    extern int test_display(int, char *[]);
    test_display(local_argc, local_argv);
#elif defined(TEST_DISK)
    extern int test_disk(int, char *[]);
    test_disk(local_argc, local_argv);
#else
#   error "OOPS, no tests specified"
#endif
}
#endif

static inline int _androidTouchEvent2JoystickEvent(jint action) {
    switch (action) {
        case ANDROID_ACTION_DOWN:
            return TOUCH_DOWN;
        case ANDROID_ACTION_MOVE:
            return TOUCH_MOVE;
        case ANDROID_ACTION_UP:
            return TOUCH_UP;
        case ANDROID_ACTION_POINTER_DOWN:
            return TOUCH_POINTER_DOWN;
        case ANDROID_ACTION_POINTER_UP:
            return TOUCH_POINTER_UP;
        case ANDROID_ACTION_CANCEL:
            return TOUCH_CANCEL;
        default:
            LOG("Unknown Android event : %d", action);
            return TOUCH_CANCEL;
    }
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeOnCreate(JNIEnv *env, jobject obj, jstring j_dataDir) {
    const char *dataDir = (*env)->GetStringUTFChars(env, j_dataDir, 0);
    data_dir = strdup(dataDir);
    (*env)->ReleaseStringUTFChars(env, j_dataDir, dataDir);
    LOG("nativeOnCreate(%s)...", data_dir);

#if TESTING
    _run_tests();
#else
    c_initialize_firsttime();
    pthread_create(&cpu_thread_id, NULL, (void *) &cpu_thread, (void *)NULL);
#endif
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeGraphicsChanged(JNIEnv *env, jobject obj, jint width, jint height) {
    LOG("%s", "native graphicsChanged...");
    video_backend->reshape(width, height);
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeGraphicsInitialized(JNIEnv *env, jobject obj, jint width, jint height) {
    LOG("%s", "native graphicsInitialized...");
    video_backend->reshape(width, height);

    static bool graphicsPreviouslyInitialized = false;
    if (graphicsPreviouslyInitialized) {
        video_backend->shutdown();
    }
    graphicsPreviouslyInitialized = true;
    video_backend->init((void *)0);
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeOnResume(JNIEnv *env, jobject obj) {
    if (!nativePaused) {
        return;
    }
    nativePaused = false;
    LOG("%s", "native onResume...");
    pthread_mutex_unlock(&interface_mutex);
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeOnPause(JNIEnv *env, jobject obj) {
    if (nativePaused) {
        return;
    }
    nativePaused = true;
    LOG("%s", "native onPause...");
    pthread_mutex_lock(&interface_mutex);
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeRender(JNIEnv *env, jobject obj) {
    if (nativePaused) {
        return;
    }
    c_keys_handle_input(-1, 0, 0);

#if FPS_LOG
    static uint32_t prevCount = 0;
    static uint32_t idleCount = 0;

    idleCount++;

    static struct timespec prev = { 0 };
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    if (now.tv_sec != prev.tv_sec) {
        LOG("native render() : %u", idleCount-prevCount);
        prevCount = idleCount;
        prev = now;
    }
#endif

    extern volatile bool _vid_dirty;
    _vid_dirty = true;
    video_backend->render();
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeReboot(JNIEnv *env, jobject obj) {
    LOG("%s", "native reboot...");
    cpu65_reboot();
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeOnQuit(JNIEnv *env, jobject obj) {
    LOG("%s", "native quit...");

    c_eject_6(0);
    c_eject_6(1);

#ifdef AUDIO_ENABLED
    speaker_destroy();
    MB_Destroy();
#endif

    video_shutdown();
    exit(0);
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeOnKeyDown(JNIEnv *env, jobject obj, jint keyCode, jint metaState) {
#if !TESTING
    android_keycode_to_emulator(keyCode, metaState, true);
#endif
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeOnKeyUp(JNIEnv *env, jobject obj, jint keyCode, jint metaState) {
#if !TESTING
    android_keycode_to_emulator(keyCode, metaState, false);
#endif
}

jboolean Java_org_deadc0de_apple2ix_Apple2Activity_nativeOnTouch(JNIEnv *env, jobject obj, jint action, jint pointerCount, jint pointerIndex, jfloatArray xCoords, jfloatArray yCoords) {
    //LOG("nativeOnTouch : %d/%d/%d :", action, pointerCount, pointerIndex);

    jfloat *x_coords = (*env)->GetFloatArrayElements(env, xCoords, 0);
    jfloat *y_coords = (*env)->GetFloatArrayElements(env, yCoords, 0);

    int joyaction = _androidTouchEvent2JoystickEvent(action);

    //for (unsigned int i=0; i<pointerCount; i++) {
    //  LOG("\t[%f,%f]", x_coords[i], y_coords[i]);
    //}

    bool consumed = joydriver_onTouchEvent(joyaction, pointerCount, pointerIndex, x_coords, y_coords);

    (*env)->ReleaseFloatArrayElements(env, xCoords, x_coords, 0);
    (*env)->ReleaseFloatArrayElements(env, yCoords, y_coords, 0);
    return consumed;
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeIncreaseCPUSpeed(JNIEnv *env, jobject obj) {
    pthread_mutex_lock(&interface_mutex);

    int percent_scale = (int)round(cpu_scale_factor * 100.0);
    if (percent_scale >= 100) {
        percent_scale += 25;
    } else {
        percent_scale += 5;
    }
    cpu_scale_factor = percent_scale/100.0;

    if (cpu_scale_factor > CPU_SCALE_FASTEST) {
        cpu_scale_factor = CPU_SCALE_FASTEST;
    }

    LOG("native set emulation percentage to %f", cpu_scale_factor);

    if (video_animation_show_cpuspeed) {
        video_animation_show_cpuspeed();
    }

#warning HACK TODO FIXME ... refactor timing stuff
    timing_toggle_cpu_speed();
    timing_toggle_cpu_speed();

    pthread_mutex_unlock(&interface_mutex);
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeDecreaseCPUSpeed(JNIEnv *env, jobject obj) {
    pthread_mutex_lock(&interface_mutex);

    int percent_scale = (int)round(cpu_scale_factor * 100.0);
    if (cpu_scale_factor == CPU_SCALE_FASTEST) {
        cpu_scale_factor = CPU_SCALE_FASTEST0;
        percent_scale = (int)round(cpu_scale_factor * 100);
    } else {
        if (percent_scale > 100) {
            percent_scale -= 25;
        } else {
            percent_scale -= 5;
        }
    }
    cpu_scale_factor = percent_scale/100.0;

    if (cpu_scale_factor < CPU_SCALE_SLOWEST) {
        cpu_scale_factor = CPU_SCALE_SLOWEST;
    }

    LOG("native set emulation percentage to %f", cpu_scale_factor);

    if (video_animation_show_cpuspeed) {
        video_animation_show_cpuspeed();
    }

#warning HACK TODO FIXME ... refactor timing stuff
    timing_toggle_cpu_speed();
    timing_toggle_cpu_speed();

    pthread_mutex_unlock(&interface_mutex);
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeSetColor(JNIEnv *env, jobject obj, jint color) {
    LOG("native set color : %d", color);
    if (color < COLOR_NONE || color > COLOR_INTERP) {
        return;
    }
    color_mode = color;

#warning HACK TODO FIXME need to refactor video resetting procedure
    video_set(0);
    video_setpage(!!(softswitches & SS_SCREEN));
    video_redraw();
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeChooseDisk(JNIEnv *env, jobject obj, jstring jPath, jboolean driveA, jboolean readOnly) {
    const char *path = (*env)->GetStringUTFChars(env, jPath, 0);
    int drive = driveA ? 0 : 1;
    int ro = readOnly ? 1 : 0;
    LOG("nativeChooseDisk(%s, %s, %s)", path, driveA ? "drive A" : "drive B", readOnly ? "read only" : "read/write");
    if (c_new_diskette_6(drive, path, ro)) {
        char *gzPath = NULL;
        asprintf(&gzPath, "%s.gz", path);
        if (c_new_diskette_6(drive, gzPath, ro)) {
            // TODO FIXME : show error message disk was unreadable ...
        } else {
            // TODO FIXME : show an OpenGL message that the disk was chosen ...
        }
        FREE(gzPath);
    } else {
        // TODO FIXME : show an OpenGL message that the disk was chosen ...
    }
    (*env)->ReleaseStringUTFChars(env, jPath, path);
}

