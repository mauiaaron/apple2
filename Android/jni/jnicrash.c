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

// Keep these in sync with the Java side
enum {
    CRASH_JAVA=0,
    CRASH_NULL_DEREF,
    CRASH_STACKCALL_OVERFLOW,
    CRASH_STACKBUF_OVERFLOW,
    CRASH_SIGABRT,
    CRASH_SIGFPE,
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

static void _crash_sigabrt(void) {
    kill(getpid(), SIGABRT);
    __builtin_unreachable();
}

static void _crash_sigfpe(void) {
    static volatile int foo = 2;
    static volatile int bar = 0;
    while (foo >= 0) {
        --foo;
        bar = 2/foo;
    }
    __builtin_unreachable();
}

void Java_org_deadc0de_apple2ix_Apple2CrashHandler_nativePerformCrash(JNIEnv *env, jclass cls, jint crashType) {
#warning FIXME TODO ... we should turn off test codepaths in release build =D
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

        case CRASH_SIGABRT:
            _crash_sigabrt();
            break;

        case CRASH_SIGFPE:
            _crash_sigfpe();
            break;

        default:
            // unknown crasher, just abort ...
            abort();
            break;
    }
}

void Java_org_deadc0de_apple2ix_Apple2CrashHandler_nativeProcessCrash(JNIEnv *env, jclass cls, jstring jCrashPath, jstring jOutputPath) {
    if (!(crashHandler && crashHandler->processCrash)) {
        return;
    }

    LOG("...");

    const char *crashPath  = (*env)->GetStringUTFChars(env, jCrashPath,  NULL);
    const char *outputPath = (*env)->GetStringUTFChars(env, jOutputPath, NULL);
    FILE *outputFILE = NULL;
    char *symbolsPath = NULL;

    do {
        outputFILE = TEMP_FAILURE_RETRY_FOPEN(fopen(outputPath, "w"));
        if (!outputFILE) {
            ERRLOG("could not open %s", outputPath);
            break;
        }

        if (android_armArchV7A) {
            ASPRINTF(&symbolsPath, "%s/symbols/armeabi-v7a", data_dir);
        } else if (android_x86) {
            ASPRINTF(&symbolsPath, "%s/symbols/x86", data_dir);
        } else /*if (android_armArch)*/ {
            ASPRINTF(&symbolsPath, "%s/symbols/armeabi", data_dir);
        } /*else { moar archs ... } */

        bool success = crashHandler->processCrash(crashPath, symbolsPath, outputFILE);
        if (!success) {
            LOG("CRASH REPORT PROCESSING FAILED ...");
        }
    } while (0);

    if (outputFILE) {
        TEMP_FAILURE_RETRY(fflush(outputFILE));
        TEMP_FAILURE_RETRY(fclose(outputFILE));
    }

    if (symbolsPath) {
        FREE(symbolsPath);
    }

    (*env)->ReleaseStringUTFChars(env, jCrashPath,  crashPath);
    (*env)->ReleaseStringUTFChars(env, jOutputPath, outputPath);
}

