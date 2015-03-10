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

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeOnCreate(JNIEnv *env, jobject obj, jstring j_dataDir) {
    const char *dataDir = (*env)->GetStringUTFChars(env, j_dataDir, 0);
    data_dir = strdup(dataDir);
    (*env)->ReleaseStringUTFChars(env, j_dataDir, dataDir);
    LOG("nativeOnCreate(%s)...", data_dir);
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeGraphicsInitialized(JNIEnv *env, jobject obj, jint width, jint height) {
    LOG("%s", "native graphicsInitialized...");
    video_driver_reshape(width, height);
#if !TESTING
    c_initialize_firsttime();
    pthread_create(&cpu_thread_id, NULL, (void *) &cpu_thread, (void *)NULL);
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
    LOG("%s", "native onResume...");
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeOnPause(JNIEnv *env, jobject obj) {
    LOG("%s", "native onPause...");
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativeRender(JNIEnv *env, jobject obj) {
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

