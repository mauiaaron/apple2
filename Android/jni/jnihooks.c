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

#include <jni.h>
#include "common.h"
#include "video/renderer.h"
#include "androidkeys.h"

static bool nativePaused = false;

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
    video_driver_reshape(width, height);
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeGraphicsInitialized(JNIEnv *env, jobject obj, jint width, jint height) {
    LOG("%s", "native graphicsInitialized...");
    video_driver_reshape(width, height);

#if !TESTING
    static bool graphicsPreviouslyInitialized = false;
    if (graphicsPreviouslyInitialized) {
        video_driver_shutdown();
    }
    graphicsPreviouslyInitialized = true;
    video_driver_init((void *)0);

#else
    char *local_argv[] = {
        "-f",
        NULL
    };
    int local_argc = 0;
    for (char **p = &local_argv[0]; *p != NULL; p++) {
        ++local_argc;
    }
#   if defined(TEST_CPU)
    extern int test_cpu(int, char *[]);
    test_cpu(local_argc, local_argv);
#   elif defined(TEST_VM)
    extern int test_vm(int, char *[]);
    test_vm(local_argc, local_argv);
#   elif defined(TEST_DISPLAY)
    extern int test_display(int, char *[]);
    test_display(local_argc, local_argv);
#   elif defined(TEST_DISK)
    extern int test_disk(int, char *[]);
    test_disk(local_argc, local_argv);
#   else
#       error "OOPS, no tests specified"
#   endif
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
    video_driver_render();
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
