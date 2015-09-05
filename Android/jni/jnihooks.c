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

#include <cpu-features.h>
#include <jni.h>

unsigned long android_deviceSampleRateHz = 0;
unsigned long android_monoBufferSubmitSizeSamples = 0;
unsigned long android_stereoBufferSubmitSizeSamples = 0;
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

// ----------------------------------------------------------------------------
// JNI functions

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeOnCreate(JNIEnv *env, jobject obj, jstring j_dataDir, jint sampleRate, jint monoBufferSize, jint stereoBufferSize) {
    const char *dataDir = (*env)->GetStringUTFChars(env, j_dataDir, 0);

    // Android lifecycle can call onCreate() multiple times...
    if (data_dir) {
        LOG("IGNORING multiple calls to nativeOnCreate ...");
        return;
    }

    AndroidCpuFamily family = android_getCpuFamily();
    uint64_t features = android_getCpuFeatures();
    if (family == ANDROID_CPU_FAMILY_X86) {
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
    }

    data_dir = strdup(dataDir);
    (*env)->ReleaseStringUTFChars(env, j_dataDir, dataDir);
    LOG("data_dir : %s", data_dir);

    android_deviceSampleRateHz = (unsigned long)sampleRate;
    android_monoBufferSubmitSizeSamples = (unsigned long)monoBufferSize;
    android_stereoBufferSubmitSizeSamples = (unsigned long)stereoBufferSize;

#if TESTING
    _run_tests();
#else
    c_initialize_firsttime();
    pthread_create(&cpu_thread_id, NULL, (void *) &cpu_thread, (void *)NULL);
#endif
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeGraphicsChanged(JNIEnv *env, jobject obj, jint width, jint height) {
    LOG("%s", "");
    video_backend->reshape(width, height);
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeGraphicsInitialized(JNIEnv *env, jobject obj, jint width, jint height) {
    LOG("width:%d height:%d", width, height);
    static bool graphicsPreviouslyInitialized = false;
    if (graphicsPreviouslyInitialized) {
        LOG("shutting down previous context");
        video_backend->shutdown();
    }
    graphicsPreviouslyInitialized = true;

    video_backend->reshape(width, height);
    video_backend->init((void *)0);
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeOnResume(JNIEnv *env, jobject obj, jboolean isSystemResume) {
    if (!cpu_isPaused()) {
        return;
    }
    LOG("%s", "");
    if (!isSystemResume) {
        cpu_resume();
    }
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeOnPause(JNIEnv *env, jobject obj, jboolean isSystemPause) {
    if (cpu_isPaused()) {
        return;
    }
    LOG("%s", "");

    video_backend->animation_hideTouchMenu();

    if (isSystemPause) {
        // going to background
        cpu_pauseBackground();
    } else {
        // going to menu
        cpu_pause();
    }
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeRender(JNIEnv *env, jobject obj) {
    if (UNLIKELY(emulator_shutting_down)) {
        return;
    }

    if (!cpu_isPaused()) {
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
        LOG("FPS : %u", idleCount-prevCount);
        prevCount = idleCount;
        prev = now;
    }
#endif

    video_backend->render();
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeReboot(JNIEnv *env, jobject obj) {
    LOG("%s", "");
    cpu65_reboot();
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeOnQuit(JNIEnv *env, jobject obj) {
    LOG("%s", "");

    c_eject_6(0);
    c_eject_6(1);

    emulator_shutting_down = true;
    video_shutdown();

    cpu_resume();
    if (pthread_join(cpu_thread_id, NULL)) {
        ERRLOG("OOPS: pthread_join of CPU thread ...");
    }
}

#define _JAVA_CRASH_NAME "/jcrash.txt"
#define _HALF_PAGE_SIZE (PAGE_SIZE>>1)

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeOnUncaughtException(JNIEnv *env, jobject obj, jstring jhome, jstring jstr) {
    RELEASE_ERRLOG("Uncaught Java Exception ...");

    // Write to /data/data/org.deadc0de.apple2ix.basic/jcrash.txt
    const char *home = (*env)->GetStringUTFChars(env, jhome, NULL);
    char *q = (char *)home;
    char buf[_HALF_PAGE_SIZE] = { 0 };
    const char *p0 = &buf[0];
    char *p = (char *)p0;
    while (*q && (p-p0 < _HALF_PAGE_SIZE-1)) {
        *p++ = *q++;
    }
    (*env)->ReleaseStringUTFChars(env, jhome, home);
    q = &_JAVA_CRASH_NAME[0];
    while (*q && (p-p0 < _HALF_PAGE_SIZE-1)) {
        *p++ = *q++;
    }

    int fd = TEMP_FAILURE_RETRY(open(buf, (O_CREAT|O_APPEND|O_WRONLY), (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)));
    if (fd == -1) {
        RELEASE_ERRLOG("OOPS, could not create/write to java crash file");
        return;
    }

    const char *str = (*env)->GetStringUTFChars(env, jstr, NULL);
    jsize len = (*env)->GetStringUTFLength(env, jstr);
    TEMP_FAILURE_RETRY(write(fd, str, len));
    (*env)->ReleaseStringUTFChars(env, jstr, str);

    TEMP_FAILURE_RETRY(fsync(fd));
    TEMP_FAILURE_RETRY(close(fd));
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

jlong Java_org_deadc0de_apple2ix_Apple2Activity_nativeOnTouch(JNIEnv *env, jobject obj, jint action, jint pointerCount, jint pointerIndex, jfloatArray xCoords, jfloatArray yCoords) {
    //LOG(": %d/%d/%d :", action, pointerCount, pointerIndex);
    if (UNLIKELY(emulator_shutting_down)) {
        return 0x0LL;
    }

    jfloat *x_coords = (*env)->GetFloatArrayElements(env, xCoords, 0);
    jfloat *y_coords = (*env)->GetFloatArrayElements(env, yCoords, 0);

    int joyaction = _androidTouchEvent2JoystickEvent(action);

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

    LOG(": (%s, %s, %s)", path, driveA ? "drive A" : "drive B", readOnly ? "read only" : "read/write");
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

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeEjectDisk(JNIEnv *env, jobject obj, jboolean driveA) {
    LOG("%s", "");
    c_eject_6(!driveA);
}

// ----------------------------------------------------------------------------
// Constructor

__attribute__((constructor(CTOR_PRIORITY_LATE)))
static void _init_jnihooks(void) {
    // ...
}

