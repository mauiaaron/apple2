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
#include "androidkeys.h"

#include <cpu-features.h>
#include <jni.h>

unsigned long android_deviceSampleRateHz = 0;
unsigned long android_monoBufferSubmitSizeSamples = 0;
unsigned long android_stereoBufferSubmitSizeSamples = 0;

bool android_armArch = false;
bool android_armArchV7A = false;
bool android_arm64Arch = false;

bool android_x86 = false;
bool android_x86_64 = false;

bool android_armNeonEnabled = false;
bool android_x86SSSE3Enabled = false;

enum {
    ANDROID_ACTION_DOWN = 0x0,
    ANDROID_ACTION_UP = 0x1,
    ANDROID_ACTION_MOVE = 0x2,
    ANDROID_ACTION_CANCEL = 0x3,
    ANDROID_ACTION_POINTER_DOWN = 0x5,
    ANDROID_ACTION_POINTER_UP = 0x6,
};

typedef enum lifecycle_seq_t {
    APP_RUNNING = 0,
    APP_REQUESTED_SHUTDOWN,
    APP_FINISHED,
} lifecycle_seq_t;

static lifecycle_seq_t appState = APP_RUNNING;

#if TESTING
static bool running_tests = false;
static void _run_tests(void) {
    char *local_argv[] = {
        "-f",
        NULL
    };
    int local_argc = 0;
    for (char **p = &local_argv[0]; *p != NULL; p++) {
        ++local_argc;
    }
#if defined(TEST_CPU)
    // Currently this test is the only one that runs as a black screen
    extern int test_cpu(int, char *[]);
    test_cpu(local_argc, local_argv);
    tkill(getpid(), SIGKILL); // and we're done ...
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

static inline int _androidTouchEvent2InterfaceEvent(jint action) {
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

static void discover_cpu_family(void) {
    LOG("Discovering CPU family...");

    AndroidCpuFamily family = android_getCpuFamily();
    uint64_t features = android_getCpuFeatures();
    if (family == ANDROID_CPU_FAMILY_X86) {
        android_x86 = true;
        if (features & ANDROID_CPU_X86_FEATURE_SSSE3) {
            LOG("nANDROID_CPU_X86_FEATURE_SSSE3");
            android_x86SSSE3Enabled = true;
        }
        if (features & ANDROID_CPU_X86_FEATURE_MOVBE) {
            LOG("ANDROID_CPU_X86_FEATURE_MOVBE");
        }
        if (features & ANDROID_CPU_X86_FEATURE_POPCNT) {
            LOG("ANDROID_CPU_X86_FEATURE_POPCNT");
        }
    } else if (family == ANDROID_CPU_FAMILY_ARM) {
        if (features & ANDROID_CPU_ARM_FEATURE_ARMv7) {
            LOG("ANDROID_CPU_ARM_FEATURE_ARMv7");
            android_armArchV7A = true;
        } else {
            LOG("!!! NOT ANDROID_CPU_ARM_FEATURE_ARMv7");
            android_armArch = true;
        }

        if (features & ANDROID_CPU_ARM_FEATURE_VFPv3) {
            LOG("ANDROID_CPU_ARM_FEATURE_VFPv3");
        }
        if (features & ANDROID_CPU_ARM_FEATURE_NEON) {
            LOG("ANDROID_CPU_ARM_FEATURE_NEON");
            android_armNeonEnabled = true;
        }
        if (features & ANDROID_CPU_ARM_FEATURE_LDREX_STREX) {
            LOG("ANDROID_CPU_ARM_FEATURE_LDREX_STREX");
        }
    } else if (family == ANDROID_CPU_FAMILY_ARM64) {
#warning FIXME TODO ...
        //android_arm64Arch = true;
        android_armArchV7A = true;
    }
}

// ----------------------------------------------------------------------------
// JNI functions

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeOnCreate(JNIEnv *env, jobject obj, jstring j_dataDir, jint sampleRate, jint monoBufferSize, jint stereoBufferSize) {
    const char *dataDir = (*env)->GetStringUTFChars(env, j_dataDir, 0);

    // Android lifecycle can call onCreate() multiple times...
    if (data_dir) {
        LOG("IGNORING multiple calls to nativeOnCreate ...");
        return;
    }

    discover_cpu_family();

    // Do not remove this deadc0de ... it forces a runtime load-library/link error on Gingerbread devices if we have
    // incorrectly compiled the app against a later version of the NDK!!!
    int pagesize = getpagesize();
    LOG("PAGESIZE IS : %d", pagesize);

    data_dir = strdup(dataDir);
    if (crashHandler && crashHandler->init) {
        crashHandler->init(data_dir);
    }

    (*env)->ReleaseStringUTFChars(env, j_dataDir, dataDir);
    LOG("data_dir : %s", data_dir);

    android_deviceSampleRateHz = (unsigned long)sampleRate;
    android_monoBufferSubmitSizeSamples = (unsigned long)monoBufferSize;
    android_stereoBufferSubmitSizeSamples = (unsigned long)stereoBufferSize;

#if !TESTING
    cpu_pause();
    emulator_start();
#endif
}

void Java_org_deadc0de_apple2ix_Apple2View_nativeGraphicsChanged(JNIEnv *env, jclass cls, jint width, jint height) {
    // WARNING : this can happen on non-GL thread
    LOG("...");
    video_backend->reshape(width, height);
}

void Java_org_deadc0de_apple2ix_Apple2View_nativeGraphicsInitialized(JNIEnv *env, jclass cls, jint width, jint height) {
    // WARNING : this needs to happen on the GL thread only
    LOG("width:%d height:%d", width, height);
    video_shutdown();
    video_backend->reshape(width, height);
    video_backend->init((void *)0);
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeEmulationResume(JNIEnv *env, jobject obj) {
#if TESTING
    // test driver thread is managing CPU
    if (!running_tests) {
        running_tests = true;
        assert(cpu_thread_id == 0 && "CPU thread must not be initialized yet...");
        _run_tests();
    }
#else
    if (!cpu_isPaused()) {
        return;
    }
    LOG("...");
    cpu_resume();
#endif
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeEmulationPause(JNIEnv *env, jobject obj) {
    if (appState != APP_RUNNING) {
        return;
    }

    disk6_flush(0);
    disk6_flush(1);

    if (cpu_isPaused()) {
        return;
    }
    LOG("...");

#if TESTING
    // test driver thread is managing CPU
#else
    cpu_pause();
#endif
}

void Java_org_deadc0de_apple2ix_Apple2View_nativeRender(JNIEnv *env, jclass cls) {
    SCOPE_TRACE_VIDEO("nativeRender");

    if (UNLIKELY(appState != APP_RUNNING)) {
        if (appState == APP_REQUESTED_SHUTDOWN) {
            appState = APP_FINISHED;
            emulator_shutdown();
        }
        return;
    }

#if FPS_LOG
    static uint32_t prevCount = 0;
    static uint32_t idleCount = 0;

    idleCount++;

    static struct timespec prev = { 0 };
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    if (now.tv_sec != prev.tv_sec) {
        LOG("FPS : %u", idleCount-prevCount);
        prevCount = idleCount;
        prev = now;
    }
#endif

    video_backend->render();
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeReboot(JNIEnv *env, jobject obj) {
    LOG("...");
    cpu65_reboot();
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeOnQuit(JNIEnv *env, jobject obj) {
#if TESTING
    // test driver thread is managing CPU
#else
    appState = APP_REQUESTED_SHUTDOWN;

    LOG("...");

    disk6_eject(0);
    disk6_eject(1);

    cpu_resume();
#endif
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeOnKeyDown(JNIEnv *env, jobject obj, jint keyCode, jint metaState) {
    if (UNLIKELY(appState != APP_RUNNING)) {
        return;
    }
    android_keycode_to_emulator(keyCode, metaState, true);
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeOnKeyUp(JNIEnv *env, jobject obj, jint keyCode, jint metaState) {
    if (UNLIKELY(appState != APP_RUNNING)) {
        return;
    }
    android_keycode_to_emulator(keyCode, metaState, false);
}

jlong Java_org_deadc0de_apple2ix_Apple2Activity_nativeOnTouch(JNIEnv *env, jobject obj, jint action, jint pointerCount, jint pointerIndex, jfloatArray xCoords, jfloatArray yCoords) {
    //LOG(": %d/%d/%d :", action, pointerCount, pointerIndex);

    SCOPE_TRACE_TOUCH("nativeOnTouch");

    if (UNLIKELY(appState != APP_RUNNING)) {
        return 0x0LL;
    }

    jfloat *x_coords = (*env)->GetFloatArrayElements(env, xCoords, 0);
    jfloat *y_coords = (*env)->GetFloatArrayElements(env, yCoords, 0);

    int joyaction = _androidTouchEvent2InterfaceEvent(action);

    //for (unsigned int i=0; i<pointerCount; i++) {
    //  LOG("\t[%f,%f]", x_coords[i], y_coords[i]);
    //}

    int64_t flags = interface_onTouchEvent(joyaction, pointerCount, pointerIndex, x_coords, y_coords);

    (*env)->ReleaseFloatArrayElements(env, xCoords, x_coords, 0);
    (*env)->ReleaseFloatArrayElements(env, yCoords, y_coords, 0);
    return flags;
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeChooseDisk(JNIEnv *env, jobject obj, jstring jPath, jboolean driveA, jboolean readOnly) {
    const char *path = (*env)->GetStringUTFChars(env, jPath, NULL);
    int drive = driveA ? 0 : 1;
    int ro = readOnly ? 1 : 0;

    assert(cpu_isPaused() && "considered dangerous to insert disk image when CPU thread is running");

    LOG(": (%s, %s, %s)", path, driveA ? "drive A" : "drive B", readOnly ? "read only" : "read/write");
    if (disk6_insert(drive, path, ro)) {
        char *gzPath = NULL;
        asprintf(&gzPath, "%s.gz", path);
        if (disk6_insert(drive, gzPath, ro)) {
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

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeEjectDisk(JNIEnv *env, jobject obj, jboolean driveA) {
    LOG("...");
    disk6_eject(!driveA);
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeSaveState(JNIEnv *env, jobject obj, jstring jPath) {
    const char *path = (*env)->GetStringUTFChars(env, jPath, NULL);

    assert(cpu_isPaused() && "considered dangerous to save state CPU thread is running");

    LOG(": (%s)", path);
    if (!emulator_saveState(path)) {
        LOG("OOPS, could not save emulator state");
    }

    (*env)->ReleaseStringUTFChars(env, jPath, path);
}

jstring Java_org_deadc0de_apple2ix_Apple2Activity_nativeLoadState(JNIEnv *env, jobject obj, jstring jPath) {
    const char *path = (*env)->GetStringUTFChars(env, jPath, NULL);

    assert(cpu_isPaused() && "considered dangerous to save state CPU thread is running");

    LOG(": (%s)", path);
    if (!emulator_loadState(path)) {
        LOG("OOPS, could not load emulator state");
    }

    (*env)->ReleaseStringUTFChars(env, jPath, path);

    // restoring state may cause a change in disk paths, so we need to notify the Java/Android menu system of the change
    // (normally we drive state from the Java/menu side...)
    char *disk1 = disk6.disk[0].file_name;
    bool readOnly1 = disk6.disk[0].is_protected;
    char *disk2 = disk6.disk[1].file_name;
    bool readOnly2 = disk6.disk[1].is_protected;
    char *str = NULL;
    jstring jstr = NULL;
    asprintf(&str, "{ disk1 = \"%s\"; readOnly1 = %s; disk2 = \"%s\"; readOnly2 = %s }", (disk1 ?: ""), readOnly1 ? "true" : "false", (disk2 ?: ""), readOnly2 ? "true" : "false");
    if (str) {
        jstr = (*env)->NewStringUTF(env, str);
        FREE(str);
    }

    return jstr;
}

// ----------------------------------------------------------------------------
// Constructor

__attribute__((constructor(CTOR_PRIORITY_LATE)))
static void _init_jnihooks(void) {
    // ...
}

