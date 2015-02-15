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
#include <android/log.h>

#define LOG(fmt, ...) __android_log_print(ANDROID_LOG_ERROR, "apple2ix", fmt, __VA_ARGS__)

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

void Java_org_deadc0de_apple2_Apple2Activity_nativeOnCreate(JNIEnv *env, jobject obj) {
    LOG("%s", "native onCreate...");
}

void Java_org_deadc0de_apple2_Apple2Activity_nativeOnResume(JNIEnv *env, jobject obj) {
    LOG("%s", "native onResume...");
}

void Java_org_deadc0de_apple2_Apple2Activity_nativeOnPause(JNIEnv *env, jobject obj) {
    LOG("%s", "native onPause...");
}

#endif
