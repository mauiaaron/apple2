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

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeOnCreate(JNIEnv *env, jobject obj, jstring j_dataDir) {
    const char *dataDir = (*env)->GetStringUTFChars(env, j_dataDir, 0);
    data_dir = strdup(dataDir);
    (*env)->ReleaseStringUTFChars(env, j_dataDir, dataDir);
    LOG("nativeOnCreate(%s)...", data_dir);

    c_initialize_firsttime();
    pthread_create(&cpu_thread_id, NULL, (void *) &cpu_thread, (void *)NULL);
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeGraphicsChanged(JNIEnv *env, jobject obj, jint width, jint height) {
    LOG("%s", "native graphicsChanged...");
    video_backend->reshape(width, height);
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeGraphicsInitialized(JNIEnv *env, jobject obj, jint width, jint height) {
    LOG("%s", "native graphicsInitialized...");
    video_backend->reshape(width, height);

#if TESTING
    _run_tests();
#else
    static bool graphicsPreviouslyInitialized = false;
    if (graphicsPreviouslyInitialized) {
        video_backend->shutdown();
    }
    graphicsPreviouslyInitialized = true;
    video_backend->init((void *)0);
#endif
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

jboolean Java_org_deadc0de_apple2ix_Apple2Activity_nativeOnTouch(JNIEnv *env, jobject obj, jint action, jfloat keyCode, jfloat metaState) {
    return false;
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

