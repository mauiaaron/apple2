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

#define LAUNCH_WITHOUT_JAVA 0
#if LAUNCH_WITHOUT_JAVA

int main(int argc, char **argv) {
    for (unsigned int i=0; i<10; i++) {
        LOG("counter : %u", i);
        sleep(1);
    }
    LOG("%s", "finished...");
}

#else

void Java_org_deadc0de_apple2_Apple2Activity_nativeOnCreate(JNIEnv *env, jobject obj, jstring j_dataDir) {

    const char *dataDir = (*env)->GetStringUTFChars(env, j_dataDir, 0);
    data_dir = strdup(dataDir);
    (*env)->ReleaseStringUTFChars(env, j_dataDir, dataDir);

    LOG("nativeOnCreate(%s)...", data_dir);

#if !TESTING
    // TODO ...
#else
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

    exit(0);
#endif
}

void Java_org_deadc0de_apple2_Apple2Activity_nativeOnResume(JNIEnv *env, jobject obj) {
    LOG("%s", "native onResume...");
}

void Java_org_deadc0de_apple2_Apple2Activity_nativeOnPause(JNIEnv *env, jobject obj) {
    LOG("%s", "native onPause...");
}

#endif // LAUNCH_WITHOUT_JAVA
