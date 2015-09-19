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

// Keep these in sync with the Java side
enum {
    CRASH_JAVA=0,
    CRASH_NULL_DEREF,
    CRASH_STACKCALL_OVERFLOW,
    CRASH_STACKBUF_OVERFLOW,
    // MOAR!
};

#include <jni.h>

static volatile int __attribute__((noinline)) _crash_null_deref(void) {
    static volatile uintptr_t *ptr = NULL;
    while ((ptr+1)) {
        *ptr++ = 0xAA;
    }
    return (int)ptr[0];
}

static volatile int (*funPtr0)(void) = NULL;
static volatile int __attribute__((noinline)) _crash_stackcall_overflow(void) {
    if (funPtr0) {
        funPtr0();
        funPtr0 = NULL;
    } else {
        funPtr0 = &_crash_stackcall_overflow;
        funPtr0();
    }
    return getpid();
}

static volatile int (*funPtr1)(unsigned int) = NULL;
static volatile int __attribute__((noinline)) _crash_stackbuf_overflow0(unsigned int smashSize) {
    volatile char buf[32];
    memset((char *)buf, 0x55, smashSize);
    return (int)&buf[0];
}

static volatile int __attribute__((noinline)) _crash_stackbuf_overflow(void) {
    static volatile unsigned int smashSize = 0;
    while (1) {
        if (funPtr1) {
            funPtr1(smashSize);
            funPtr1 = NULL;
        } else {
            funPtr1 = &_crash_stackbuf_overflow0;
            funPtr1(smashSize);
        }

        smashSize += 32;
        if (!smashSize) {
            break;
        }
    }
    return getpid();
}

void Java_org_deadc0de_apple2ix_Apple2Activity_nativePerformCrash(JNIEnv *env, jobject obj, jint crashType) {
    LOG("... performing crash of type : %d", crashType);

    switch (crashType) {
        case CRASH_NULL_DEREF:
            _crash_null_deref();
            break;

        case CRASH_STACKCALL_OVERFLOW:
            _crash_stackcall_overflow();
            break;

        case CRASH_STACKBUF_OVERFLOW:
            _crash_stackbuf_overflow();
            break;

        default:
            // unknown crasher, just abort ...
            abort();
            break;
    }
}

