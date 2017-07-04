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
#include "json_parse_private.h"

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
static void _start_tests(void) {
    char *local_argv[] = {
        "-f",
        NULL
    };
    int local_argc = 0;
    for (char **p = &local_argv[0]; *p != NULL; p++) {
        ++local_argc;
    }

#if TEST_CPU
    // Currently this test is the only one that runs as a black screen
    extern int test_cpu(int, char *[]);
    test_cpu(local_argc, local_argv);
    kill(getpid(), SIGKILL); // and we're done ...
#endif

    cpu_pause();
    emulator_start();
    while (cpu_thread_id == 0) {
        sleep(1);
    }
    cpu_resume();

#if TEST_DISK
    extern void test_disk(int, char *[]);
    test_disk(local_argc, local_argv);
#elif TEST_DISPLAY
    extern void test_display(int, char *[]);
    test_display(local_argc, local_argv);
#elif TEST_PREFS
    extern void test_prefs(int, char *[]);
    test_prefs(local_argc, local_argv);
#elif TEST_TRACE
    extern void test_trace(int, char *[]);
    test_trace(local_argc, local_argv);
#elif TEST_UI
    extern void test_ui(int, char *[]);
    test_ui(local_argc, local_argv);
#elif TEST_VM
    extern void test_vm(int, char *[]);
    test_vm(local_argc, local_argv);
#elif TEST_CPU
    // handled above ...
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

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeOnCreate(JNIEnv *env, jclass cls, jstring j_dataDir, jint sampleRate, jint monoBufferSize, jint stereoBufferSize) {
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

    data_dir = STRDUP(dataDir);
    if (crashHandler && crashHandler->init) {
        crashHandler->init(data_dir);
    }
    char *home = NULL;
    ASPRINTF(&home, "HOME=%s", data_dir);
    if (home) {
        putenv(home);
        LEAK(home);
    }

    (*env)->ReleaseStringUTFChars(env, j_dataDir, dataDir);
    LOG("data_dir : %s", data_dir);

    android_deviceSampleRateHz = (unsigned long)sampleRate;
    android_monoBufferSubmitSizeSamples = (unsigned long)monoBufferSize;
    android_stereoBufferSubmitSizeSamples = (unsigned long)stereoBufferSize;

    joydriver_setClampBeyondRadius(true);

//#define DO_CPU65_TRACING 1
#if DO_CPU65_TRACING
#   warning !!!!!!!!!! this will quickly eat up disk space !!!!!!!!!!
    char *trfile = NULL;
    ASPRINTF(&trfile, "%s/%s", data_dir, "cpu_trace.txt");
    cpu65_trace_begin(trfile);
    FREE(trfile);
#endif

#if TESTING
    _start_tests();
#else
    cpu_pause();
    emulator_start();
#endif
}

void Java_org_deadc0de_apple2ix_Apple2View_nativeGraphicsInitialized(JNIEnv *env, jclass cls) {
    LOG("...");
    _video_setRenderThread(pthread_self()); // by definition, this method is called on the render thread ...
    video_shutdown();
    video_init();
}

jboolean Java_org_deadc0de_apple2ix_Apple2Activity_nativeEmulationResume(JNIEnv *env, jclass cls) {
    if (!cpu_isPaused()) {
        return false;
    }
    LOG("...");
    cpu_resume();

    return true;
}

jboolean Java_org_deadc0de_apple2ix_Apple2Activity_nativeEmulationPause(JNIEnv *env, jclass cls) {
    if (appState != APP_RUNNING) {
        return false;
    }

#if DO_CPU65_TRACING
    cpu65_trace_checkpoint();
#endif

    disk6_flush(0);
    disk6_flush(1);

    if (cpu_isPaused()) {
        return false;
    }
    LOG("...");

    cpu_pause();
    prefs_save();

    return true;
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

//#define FPS_LOG 1
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

    video_render();
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeReboot(JNIEnv *env, jclass cls, jint resetState) {
    LOG("...");
    if (resetState) {
        // joystick button settings should be balanced by c_joystick_reset() triggered on CPU thread
        if (resetState == 1) {
            joy_button0 = 0xff;
            joy_button1 = 0x0;
        } else {
            joy_button0 = 0x0;
            joy_button1 = 0xff;
        }
    }
    cpu65_interrupt(ResetSig);
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeOnQuit(JNIEnv *env, jclass cls) {
    appState = APP_REQUESTED_SHUTDOWN;

    LOG("...");

#if DO_CPU65_TRACING
    cpu65_trace_end();
#endif

    cpu_resume();
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeOnKeyDown(JNIEnv *env, jclass cls, jint keyCode, jint metaState) {
    if (UNLIKELY(appState != APP_RUNNING)) {
        return;
    }
    android_keycode_to_emulator(keyCode, metaState, true);
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeOnKeyUp(JNIEnv *env, jclass cls, jint keyCode, jint metaState) {
    if (UNLIKELY(appState != APP_RUNNING)) {
        return;
    }
    android_keycode_to_emulator(keyCode, metaState, false);
}

void Java_org_deadc0de_apple2ix_Apple2View_nativeOnJoystickMove(JNIEnv *env, jclass cls, jint x, jint y) {
    joydriver_setAxisValue((uint8_t)x, (uint8_t)y);
}

jlong Java_org_deadc0de_apple2ix_Apple2View_nativeOnTouch(JNIEnv *env, jclass cls, jint action, jint pointerCount, jint pointerIndex, jfloatArray xCoords, jfloatArray yCoords) {
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

jstring Java_org_deadc0de_apple2ix_Apple2DisksMenu_nativeChooseDisk(JNIEnv *env, jclass cls, jstring jJsonString) {
#if TESTING
    return NULL;
#endif

    assert(cpu_isPaused() && "considered dangerous to insert disk image when CPU thread is running");

    const char *jsonString = (*env)->GetStringUTFChars(env, jJsonString, NULL);

    JSON_ref jsonData = NULL;
    bool ret = json_createFromString(jsonString, &jsonData);
    assert(ret > 0);

    (*env)->ReleaseStringUTFChars(env, jJsonString, jsonString); jsonString = NULL;

    char *path = NULL;
    json_mapCopyStringValue(jsonData, "disk", &path);
    json_unescapeSlashes(&path);

    assert(path != NULL);
    assert(strlen(path) > 0);

    bool readOnly = true;
    json_mapParseBoolValue(jsonData, "readOnly", &readOnly);

    long fd = -1;
    if (!json_mapParseLongValue(jsonData, "fd", &fd, 10)) {
        TEMP_FAILURE_RETRY(fd = open(path, readOnly ? O_RDONLY : O_RDWR));
        if (fd == -1) {
            LOG("OOPS could not open disk path : %s", path);
        }
    } else {
        fd = dup(fd);
        if (fd == -1) {
            ERRLOG("OOPS could not dup file descriptor!");
        }
    }

    long drive = -1;
    json_mapParseLongValue(jsonData, "drive", &drive, 10);
    assert(drive == 0 || drive == 1);

    bool inserted = true;
    const char *err = disk6_insert(fd, drive, path, readOnly);
    if (err) {
        char *diskImageUnreadable = "Disk Image Unreadable";
        unsigned int cols = strlen(diskImageUnreadable);
        video_animations->animation_showMessage(diskImageUnreadable, cols, 1);
        inserted = false;
    } else {
        video_animations->animation_showDiskChosen(drive);
        // possibly override was_gzipped, if specified in args ...
        bool wasGzipped = false;
        if (json_mapParseBoolValue(jsonData, "wasGzipped", &wasGzipped)) {
            disk6.disk[drive].was_gzipped = wasGzipped;
        }
    }

    // remember if image was gzipped
    prefs_setBoolValue(PREF_DOMAIN_VM, drive == 0 ? PREF_DISK_DRIVEA_GZ : PREF_DISK_DRIVEB_GZ, disk6.disk[drive].was_gzipped); // HACK FIXME TODO ... refactor : this is erased on the Java side when we resume emulation 
    json_mapSetBoolValue(jsonData, "wasGzipped", disk6.disk[drive].was_gzipped);
    json_mapSetBoolValue(jsonData, "inserted", inserted);

    if (fd >= 0) {
        TEMP_FAILURE_RETRY(close(fd));
        fd = -1;
    }

    if (path) {
        FREE(path);
    }

    jsonString = ((JSON_s *)jsonData)->jsonString;
    jstring jstr = (*env)->NewStringUTF(env, jsonString);

    json_destroy(&jsonData);

    LOG(": (fd:%d, %s, %s, %s)", (int)fd, path, drive ? "drive A" : "drive B", readOnly ? "read only" : "read/write");

    return jstr;
}

void Java_org_deadc0de_apple2ix_Apple2DisksMenu_nativeEjectDisk(JNIEnv *env, jclass cls, jboolean driveA) {
#if TESTING
    return;
#endif

    LOG("...");
    disk6_eject(driveA ? 0 : 1);
}

static int _openFdFromJson(OUTPARM int *fdOut, JSON_ref jsonData, const char * const fdKey, const char * const pathKey, int flags, int mode) {

    long fd = -1;
    char *path = NULL;

    do {
        if (!json_mapParseLongValue(jsonData, fdKey, &fd, 10)) {
            json_mapCopyStringValue(jsonData, pathKey, &path);
            assert(path != NULL);

            json_unescapeSlashes(&path);
            if (strlen(path) <= 0) {
                break;
            }

            if (mode == 0) {
                TEMP_FAILURE_RETRY(fd = open(path, flags));
            } else {
                TEMP_FAILURE_RETRY(fd = open(path, flags, mode));
            }
            if (fd == -1) {
                LOG("OOPS could not open state file path %s", path);
            }
        } else {
            fd = dup(fd);
            if (fd == -1) {
                ERRLOG("OOPS could not dup file descriptor!");
            }
        }
    } while (0);

    FREE(path);

    *fdOut = (int)fd;
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeSaveState(JNIEnv *env, jclass cls, jstring jJsonString) {
    assert(cpu_isPaused() && "considered dangerous to save state when CPU thread is running");

    const char *jsonString = (*env)->GetStringUTFChars(env, jJsonString, NULL);
    LOG(": (%s)", jsonString);

    JSON_ref jsonData = NULL;
    bool ret = json_createFromString(jsonString, &jsonData);
    assert(ret > 0);

    (*env)->ReleaseStringUTFChars(env, jJsonString, jsonString); jsonString = NULL;

    int fdState = -1;
    _openFdFromJson(&fdState, jsonData, /*fdKey:*/"fdState", /*pathKey:*/"stateFile", O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR);

    if (!emulator_saveState(fdState)) {
        LOG("OOPS, could not save emulator state");
    }

    if (fdState >= 0) {
        TEMP_FAILURE_RETRY(close(fdState));
        fdState = -1;
    }

    json_destroy(&jsonData);
}

jstring Java_org_deadc0de_apple2ix_Apple2Activity_nativeLoadState(JNIEnv *env, jclass cls, jstring jJsonString) {
    assert(cpu_isPaused() && "considered dangerous to load state when CPU thread is running");

    const char *jsonString = (*env)->GetStringUTFChars(env, jJsonString, NULL);
    LOG(": %s", jsonString);

    JSON_ref jsonData = NULL;
    int ret = json_createFromString(jsonString, &jsonData);
    assert(ret > 0);

    (*env)->ReleaseStringUTFChars(env, jJsonString, jsonString); jsonString = NULL;

    int fdState = -1;
    _openFdFromJson(&fdState, jsonData, /*fdKey:*/"fdState", /*pathKey:*/"stateFile", O_RDONLY, 0);

    int fdA = -1;
    {
        bool readOnlyA = true;
        json_mapParseBoolValue(jsonData, "readOnlyA", &readOnlyA);
        _openFdFromJson(&fdA, jsonData, /*fdKey:*/"fdA", /*pathKey:*/"diskA", readOnlyA ? O_RDONLY : O_RDWR, 0);
    }

    int fdB = -1;
    {
        bool readOnlyB = true;
        json_mapParseBoolValue(jsonData, "readOnlyB", &readOnlyB);
        _openFdFromJson(&fdB, jsonData, /*fdKey:*/"fdB", /*pathKey:*/"diskB", readOnlyB ? O_RDONLY : O_RDWR, 0);
    }

    bool loadStateSuccess = true;
    if (emulator_loadState(fdState, (int)fdA, (int)fdB)) {
        json_mapSetBoolValue(jsonData, "wasGzippedA", disk6.disk[0].was_gzipped);
        json_mapSetBoolValue(jsonData, "wasGzippedB", disk6.disk[1].was_gzipped);
    } else {
        loadStateSuccess = false;
        LOG("OOPS, could not load emulator state");
        // FIXME TODO : should show invalid state animation here ...
    }

    if (fdState >= 0) {
        TEMP_FAILURE_RETRY(close(fdState));
        fdState = -1;
    }
    if (fdA >= 0) {
        TEMP_FAILURE_RETRY(close(fdA));
        fdA = -1;
    }
    if (fdB >= 0) {
        TEMP_FAILURE_RETRY(close(fdB));
        fdB = -1;
    }

    json_mapSetBoolValue(jsonData, "loadStateSuccess", loadStateSuccess);

    jsonString = ((JSON_s *)jsonData)->jsonString;
    jstring jstr = (*env)->NewStringUTF(env, jsonString);

    json_destroy(&jsonData);

    return jstr;
}

jstring Java_org_deadc0de_apple2ix_Apple2Activity_nativeStateExtractDiskPaths(JNIEnv *env, jclass cls, jstring jJsonString) {
    assert(cpu_isPaused() && "considered dangerous to save state when CPU thread is running");

    const char *jsonString = (*env)->GetStringUTFChars(env, jJsonString, NULL);
    LOG(": (%s)", jsonString);

    JSON_ref jsonData = NULL;
    bool ret = json_createFromString(jsonString, &jsonData);
    assert(ret > 0);

    (*env)->ReleaseStringUTFChars(env, jJsonString, jsonString); jsonString = NULL;

    int fdState = -1;
    _openFdFromJson(&fdState, jsonData, /*fdKey:*/"fdState", /*pathKey:*/"stateFile", O_RDONLY, 0);

    if (!emulator_stateExtractDiskPaths(fdState, jsonData)) {
        LOG("OOPS, could not extract disk paths from emulator state file");
    }

    jsonString = ((JSON_s *)jsonData)->jsonString;

    jstring jstr = (*env)->NewStringUTF(env, jsonString);

    json_destroy(&jsonData);

    if (fdState >= 0) {
        TEMP_FAILURE_RETRY(close(fdState));
        fdState = -1;
    }

    return jstr;
}

void Java_org_deadc0de_apple2ix_Apple2Preferences_nativePrefsSync(JNIEnv *env, jclass cls, jstring jDomain) {
    const char *domain = NULL;

    if (jDomain) {
        domain = (*env)->GetStringUTFChars(env, jDomain, 0);
    }

    LOG("... domain: %s", domain);
    prefs_load();
    prefs_sync(domain);

    if (jDomain) {
        (*env)->ReleaseStringUTFChars(env, jDomain, domain);
    }
}

