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

enum {
    ANDROID_ACTION_DOWN = 0x0,
    ANDROID_ACTION_UP = 0x1,
    ANDROID_ACTION_MOVE = 0x2,
    ANDROID_ACTION_CANCEL = 0x3,
    ANDROID_ACTION_POINTER_DOWN = 0x5,
    ANDROID_ACTION_POINTER_UP = 0x6,
};

static bool nativePaused = false;
static bool nativeRequestsShowMainMenu = false;

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

static void _nativeRequestsShowMainMenu(void) {
    nativeRequestsShowMainMenu = true;
}

// ----------------------------------------------------------------------------
// JNI functions

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeOnCreate(JNIEnv *env, jobject obj, jstring j_dataDir) {
    const char *dataDir = (*env)->GetStringUTFChars(env, j_dataDir, 0);

    // Android lifecycle can call onCreate() multiple times...
    if (data_dir) {
        LOG("IGNORING multiple calls to nativeOnCreate ...");
        return;
    }

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
    LOG("native graphicsInitialized width:%d height:%d", width, height);
    static bool graphicsPreviouslyInitialized = false;
    if (graphicsPreviouslyInitialized) {
        video_backend->shutdown();
    }
    graphicsPreviouslyInitialized = true;

    video_backend->reshape(width, height);
    video_backend->init((void *)0);
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeOnResume(JNIEnv *env, jobject obj, jboolean isSystemResume) {
    if (!nativePaused) {
        return;
    }
    LOG("%s", "native onResume...");
    if (isSystemResume) {
        if (video_backend->animation_showPaused) {
            video_backend->animation_showPaused();
        }
    } else {
        nativePaused = false;
        pthread_mutex_unlock(&interface_mutex);
    }
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
    if (!nativePaused) {
        c_keys_handle_input(-1, 0, 0);
    }

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
    if (!nativePaused) {
        _vid_dirty = true;// HACK HACK HACK FIXME TODO : efficiency and battery life gains if we can fix this ...
    }
    video_backend->render();
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeReboot(JNIEnv *env, jobject obj) {
    LOG("%s", "native reboot...");
    cpu65_reboot();
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeOnQuit(JNIEnv *env, jobject obj) {
    LOG("%s", "nativeOnQuit...");

    c_eject_6(0);
    c_eject_6(1);

    emulator_shutting_down = true;
    video_shutdown();

    pthread_mutex_unlock(&interface_mutex);
    if (pthread_join(cpu_thread_id, NULL)) {
        ERRLOG("OOPS: pthread_join of CPU thread ...");
    }
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

    if (nativePaused) {
        LOG("UNPAUSING NATIVE CPU THREAD");
        nativePaused = false;
        pthread_mutex_unlock(&interface_mutex);
        return true;
    }

    jfloat *x_coords = (*env)->GetFloatArrayElements(env, xCoords, 0);
    jfloat *y_coords = (*env)->GetFloatArrayElements(env, yCoords, 0);

    int joyaction = _androidTouchEvent2JoystickEvent(action);

    //for (unsigned int i=0; i<pointerCount; i++) {
    //  LOG("\t[%f,%f]", x_coords[i], y_coords[i]);
    //}

    interface_onTouchEvent(joyaction, pointerCount, pointerIndex, x_coords, y_coords);

    bool consumed = true; // default assume that native can handle touch event (except for explicit request to show main menu)
    if (nativeRequestsShowMainMenu) {
        nativeRequestsShowMainMenu = false;
        consumed = false;
    }

    (*env)->ReleaseFloatArrayElements(env, xCoords, x_coords, 0);
    (*env)->ReleaseFloatArrayElements(env, yCoords, y_coords, 0);
    return consumed;
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeSetColor(JNIEnv *env, jobject obj, jint color) {
    LOG("native set color : %d", color);
    if (color < COLOR_NONE || color > COLOR_INTERP) {
        return;
    }
    color_mode = color;

    video_reset();
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
            char *diskImageUnreadable = "Disk Image Unreadable";
            unsigned int cols = strlen(diskImageUnreadable);
            video_backend->animation_showMessage(diskImageUnreadable, cols, 1);
        } else {
            video_backend->animation_showDiskChosen(drive);
        }
        FREE(gzPath);
    } else {
        video_backend->animation_showDiskChosen(drive);
    }
    (*env)->ReleaseStringUTFChars(env, jPath, path);
}

// ----------------------------------------------------------------------------
// Constructor

__attribute__((constructor(CTOR_PRIORITY_LATE)))
static void _init_jnihooks(void) {
    video_backend->hostenv_showMainMenu = &_nativeRequestsShowMainMenu;
}

