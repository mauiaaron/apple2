dnl ---------------------------------------------------------------------------

AC_DEFINE(PACKAGE_URL, "https://github.com/mauiaaron/apple2", [apple2ix project URL])

AC_PREREQ([2.69])
AC_INIT([apple2ix], [0.8])

AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE([foreign subdir-objects])

dnl AM_CONFIG_HEADER(src/config.h) -- disable config.h because it makes it difficult/impossible to do modular builds for the test suite

AC_PROG_CC([clang gcc])
AM_PROG_CC_C_O      dnl apparently required for custom font.c target?
AM_PROG_AS
AC_PROG_INSTALL

dnl ---------------------------------------------------------------------------
dnl Arch checks

ASM_O="src/x86/glue.o src/x86/cpu.o"
dnl HACK there's gotta be a better way ... without this verbosity, CFLAGS are not correct (lacking -DTESTING=1 , etc) if we don't specify specific obj files for test binaries
testcpu_ASM_O="src/x86/testcpu-glue.o src/x86/testcpu-cpu.o"
testdisk_ASM_O="src/x86/testdisk-glue.o src/x86/testdisk-cpu.o"
testdisplay_ASM_O="src/x86/testdisplay-glue.o src/x86/testdisplay-cpu.o"
testprefs_ASM_O="src/x86/testprefs-glue.o src/x86/testprefs-cpu.o"
testtrace_ASM_O="src/x86/testtrace-glue.o src/x86/testtrace-cpu.o"
testvm_ASM_O="src/x86/testvm-glue.o src/x86/testvm-cpu.o"
arch=''
case $target in
    x86_64-*-*)
        arch='x64'
        ;;
    i?86-*-*)
        arch='x86'
        ;;
    x86*)
        dnl support shorthand ./configure --target=x86
        arch='x86'
        ;;
    *)
        ASM_O=""
        AC_MSG_ERROR([emulator does not presently support architecture $target])
        ;;
esac

AM_CFLAGS="-std=gnu11 -Wall"

dnl double-check compilation for x86 target
if test "$arch" = "x86" ; then
    my_save_cflags="$CFLAGS"
    AC_MSG_CHECKING([whether compiler supports x86 target])
    case $host in
        i?86-*-*)
            dnl building on an actual x86 machine presumably works
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])], [
                AC_MSG_RESULT([32bit w00t!])
            ], [
                AC_MSG_RESULT([oops])
                AC_MSG_ERROR([world is b0rken])
            ])
            ;;
        *)
            dnl check x86 compilation on x86_64 (or other) host
            CFLAGS="-m32 -Xassembler --32"
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])], [
                AC_MSG_RESULT([w00t!])
                AC_MSG_WARN([compiling for x86 architecture appears to work, but emulator may fail to link if x86 versions of required libraries are not present...])
                dnl HACK FIXME TODO --------------------------------------------------------- ^^^^^^^^^^^^ should check for this (and check linking against all x86 versions of needed libraries
                AM_CFLAGS="$AM_CFLAGS $CFLAGS"
                dnl -------- ARCHOS_HACK_LDFLAGS="-L/usr/lib/i386-linux-gnu -L/lib/i386-linux-gnu"
            ], [
                AC_MSG_RESULT([oops])
                AC_MSG_ERROR([build system does not support building for $arch architecture])
            ])
            ;;
    esac
    CFLAGS="$my_save_cflags"
fi

AC_SUBST(ASM_O)
AC_SUBST(testcpu_ASM_O)
AC_SUBST(testdisk_ASM_O)
AC_SUBST(testdisplay_ASM_O)
AC_SUBST(testprefs_ASM_O)
AC_SUBST(testtrace_ASM_O)
AC_SUBST(testvm_ASM_O)

AC_SUBST([AM_CFLAGS])


dnl OS Check
AC_EGREP_CPP(unsupported_, [
#if defined(__ANDROID__)
unsupported_for_now
#elif __APPLE__
unsupported_for_now
    #include "TargetConditionals.h"
    #if TARGET_IPHONE_SIMULATOR
    #elif TARGET_OS_IPHONE
    #elif TARGET_OS_MAC
    #else
    #endif
#elif __linux
linux
#elif __unix
unix
#elif __posix
posix
#else
unknown
#endif
], [
    AC_MSG_CHECKING([Operating System ])
    AC_MSG_RESULT([unsupported])
    AC_MSG_ERROR([Apparently you have an unsupported OS, build aborted])
], [
    AC_MSG_CHECKING([Operating System ])
    AC_MSG_RESULT([supported])
])

dnl ASM underscore linking test
AC_TRY_LINK([asm("_glibc_foobar:");], [glibc_foobar()], [
    AC_MSG_NOTICE([Underscores in assembly linkage allowed...])
], [
    AC_MSG_NOTICE([Underscores in assembly linkage not allowed...])
    AC_DEFINE(NO_UNDERSCORES, 1, [Underscores allowed in assembly linkage])
])


dnl ---------------------------------------------------------------------------

# Sometimes Flex is installed as Lex, e.g., NetBSD.
AC_CHECK_PROG([FLEX], [flex lex], [flex])
# Force the use of `missing' to wrap Flex invocations.
AM_MISSING_PROG([LEX], [$FLEX])
# Perform all the tests Automake and Autoconf need.
AM_PROG_LEX

dnl POSIX high-precision clock
AC_SEARCH_LIBS(clock_gettime, rt, [], [
    AC_MSG_ERROR([Emulator needs realtime clocks (-lrt) to build...])
], [])

AC_CHECK_HEADER(zlib.h, [], [
    AC_MSG_ERROR([Emulator requires zlib headers to build...])
])
AC_SEARCH_LIBS(gzopen, z, [], [
    AC_MSG_ERROR([Emulator requires zlib library to build...])
], [])

AC_CHECK_HEADER(pthread.h, [], [
    AC_MSG_ERROR([Emulator requires pthread headers to build...])
])
AC_SEARCH_LIBS(pthread_create, pthread, [], [
    AC_MSG_ERROR([Emulator requires pthread library to build...])
], [])

AC_SEARCH_LIBS(sqrtf, m, [], [
    AC_MSG_ERROR([Emulator requires math library to build...])
], [])


dnl ---------------------------------------------------------------------------
dnl Video ...

AC_PATH_XTRA

opengl_selected='yes'
AC_ARG_ENABLE([opengl], AS_HELP_STRING([--disable-opengl], [Disable OpenGL video driver (uses regular X11)]), [
    opengl_selected='no'
], [
    AC_CHECK_HEADER(GL/glew.h, [
        AC_CHECK_HEADER(GL/freeglut.h, [
            AC_SEARCH_LIBS(glCreateProgram, [GL], [
                AC_SEARCH_LIBS(glutMainLoop, [glut freeglut], [
                    AC_SEARCH_LIBS(glewInit, [GLEW glew], [
                        opengl_supported='yes'
                        AC_DEFINE(VIDEO_OPENGL, 1, [Use OpenGL])
                        AC_DEFINE(USE_GLUT, 1, [Use GLUT library])
                        VIDEO_O="src/video/glvideo.o src/video/glnode.o src/video/glalert.o src/video/glhudmodel.o src/video/glutinput.o src/video_util/matrixUtil.o src/video_util/modelUtil.o src/video_util/sourceUtil.o src/video_util/vectorUtil.o"
                        dnl HACK there's gotta be a better way ... without this verbosity, CFLAGS are not correct (lacking -DTESTING=1 , etc) if we don't specify specific obj files for test binaries
                        testcpu_VIDEO_O="src/video/testcpu-glvideo.o src/video/testcpu-glnode.o src/video/testcpu-glalert.o src/video/testcpu-glhudmodel.o src/video/testcpu-glutinput.o src/video_util/testcpu-matrixUtil.o src/video_util/testcpu-modelUtil.o src/video_util/testcpu-sourceUtil.o src/video_util/testcpu-vectorUtil.o"
                        testdisk_VIDEO_O="src/video/testdisk-glvideo.o src/video/testdisk-glnode.o src/video/testdisk-glalert.o src/video/testdisk-glhudmodel.o src/video/testdisk-glutinput.o src/video_util/testdisk-matrixUtil.o src/video_util/testdisk-modelUtil.o src/video_util/testdisk-sourceUtil.o src/video_util/testdisk-vectorUtil.o"
                        testdisplay_VIDEO_O="src/video/testdisplay-glvideo.o src/video/testdisplay-glnode.o src/video/testdisplay-glalert.o src/video/testdisplay-glhudmodel.o src/video/testdisplay-glutinput.o src/video_util/testdisplay-matrixUtil.o src/video_util/testdisplay-modelUtil.o src/video_util/testdisplay-sourceUtil.o src/video_util/testdisplay-vectorUtil.o"
                        testprefs_VIDEO_O="src/video/testprefs-glvideo.o src/video/testprefs-glnode.o src/video/testprefs-glalert.o src/video/testprefs-glhudmodel.o src/video/testprefs-glutinput.o src/video_util/testprefs-matrixUtil.o src/video_util/testprefs-modelUtil.o src/video_util/testprefs-sourceUtil.o src/video_util/testprefs-vectorUtil.o"
                        testtrace_VIDEO_O="src/video/testtrace-glvideo.o src/video/testtrace-glnode.o src/video/testtrace-glalert.o src/video/testtrace-glhudmodel.o src/video/testtrace-glutinput.o src/video_util/testtrace-matrixUtil.o src/video_util/testtrace-modelUtil.o src/video_util/testtrace-sourceUtil.o src/video_util/testtrace-vectorUtil.o"
                        testvm_VIDEO_O="src/video/testvm-glvideo.o src/video/testvm-glnode.o src/video/testvm-glalert.o src/video/testvm-glhudmodel.o src/video/testvm-glutinput.o src/video_util/testvm-matrixUtil.o src/video_util/testvm-modelUtil.o src/video_util/testvm-sourceUtil.o src/video_util/testvm-vectorUtil.o"
                        AC_MSG_RESULT([Building emulator with OpenGL support, w00t!])
                    ], [
                        AC_MSG_WARN([Did not find OpenGL GLEW library...])
                    ], [-lGL -lGLEW -lglut])
                ], [
                    AC_MSG_WARN([Did not find glut library...])
                ], [-lGL -lGLEW -lglut])
            ], [
                AC_MSG_WARN([Did not find OpenGL library...])
            ], [-lGL])
        ], [
            AC_MSG_WARN([Did not find GL/freeglut.h header ...])
        ])
    ], [
        AC_MSG_WARN([Did not find GL/glew.h header ...])
    ])
])

AS_IF([test "x$opengl_supported" = "xyes"], [
], [
    dnl OpenGL not supported
    AS_IF([test "x$opengl_selected" = "xyes"], [
        AC_MSG_WARN([!!! DID NOT FIND OPENGL LIBRARIES !!! will attempt to build legacy X11 variant (this is OKAY but LIMITED) ...])
    ], [])

    AC_CHECK_HEADER(X11/XKBlib.h, [
        AC_SEARCH_LIBS(XPutImage, [X11], [
            AC_SEARCH_LIBS(XShmAttach, Xext, [
                AC_DEFINE(HAVE_X11_SHM, 1, [Enable X11 MIT SHM extension])
            ], [
                AC_MSG_WARN([Building emulator without support of X11 MITSHM extension...])
            ], [-lX11])
            VIDEO_O="src/video/xvideo.o"
            dnl HACK there's gotta be a better way ... without this verbosity, CFLAGS are not correct (lacking -DTESTING=1 , etc) if we don't specify specific obj files for test binaries
            testcpu_VIDEO_O="src/video/testcpu-xvideo.o"
            testdisk_VIDEO_O="src/video/testdisk-xvideo.o"
            testdisplay_VIDEO_O="src/video/testdisplay-xvideo.o"
            testprefs_VIDEO_O="src/video/testprefs-xvideo.o"
            testtrace_VIDEO_O="src/video/testtrace-xvideo.o"
            testvm_VIDEO_O="src/video/testvm-xvideo.o"
        ], [
            AC_MSG_ERROR([Did not find OpenGL nor X11 libraries...])
        ], [-LX11])
    ], [
        AC_MSG_ERROR([Did not find OpenGL nor X11 headers...])
    ])
])

AC_SUBST(VIDEO_O)
AC_SUBST(testcpu_VIDEO_O)
AC_SUBST(testdisk_VIDEO_O)
AC_SUBST(testdisplay_VIDEO_O)
AC_SUBST(testprefs_VIDEO_O)
AC_SUBST(testtrace_VIDEO_O)
AC_SUBST(testvm_VIDEO_O)

dnl ---------------------------------------------------------------------------
dnl Sound ...

openal_selected='yes'
AC_ARG_ENABLE([audio], AS_HELP_STRING([--disable-audio], [Disable emulator audio output]), [
    openal_selected='no'
], [
    AC_CHECK_HEADER(AL/al.h, [
        AC_CHECK_HEADER(AL/alc.h, [
            AC_CHECK_HEADER(AL/alext.h, [
                AC_SEARCH_LIBS(alcOpenDevice, openal, [
                    dnl found OpenAL ...
                    openal_supported='yes'
                    AC_DEFINE(AUDIO_ENABLED, 1, [Enable sound module])
                    AUDIO_GLUE_C="src/audio/speaker.c src/audio/mockingboard.c src/audio/playqueue.c"
                    AUDIO_O="src/audio/soundcore.o src/audio/soundcore-openal.o src/audio/speaker.o src/audio/playqueue.o src/audio/alhelpers.o src/audio/mockingboard.o src/audio/AY8910.o"
                    dnl HACK there's gotta be a better way ... without this verbosity, CFLAGS are not correct (lacking -DTESTING=1 , etc) if we don't specify specific obj files for test binaries
                    testcpu_AUDIO_O="src/audio/testcpu-soundcore.o src/audio/testcpu-soundcore-openal.o src/audio/testcpu-speaker.o src/audio/testcpu-playqueue.o src/audio/testcpu-alhelpers.o src/audio/testcpu-mockingboard.o src/audio/testcpu-AY8910.o"
                    testdisk_AUDIO_O="src/audio/testdisk-soundcore.o src/audio/testdisk-soundcore-openal.o src/audio/testdisk-speaker.o src/audio/testdisk-playqueue.o src/audio/testdisk-alhelpers.o src/audio/testdisk-mockingboard.o src/audio/testdisk-AY8910.o"
                    testdisplay_AUDIO_O="src/audio/testdisplay-soundcore.o src/audio/testdisplay-soundcore-openal.o src/audio/testdisplay-speaker.o src/audio/testdisplay-playqueue.o src/audio/testdisplay-alhelpers.o src/audio/testdisplay-mockingboard.o src/audio/testdisplay-AY8910.o"
                    testprefs_AUDIO_O="src/audio/testprefs-soundcore.o src/audio/testprefs-soundcore-openal.o src/audio/testprefs-speaker.o src/audio/testprefs-playqueue.o src/audio/testprefs-alhelpers.o src/audio/testprefs-mockingboard.o src/audio/testprefs-AY8910.o"
                    testtrace_AUDIO_O="src/audio/testtrace-soundcore.o src/audio/testtrace-soundcore-openal.o src/audio/testtrace-speaker.o src/audio/testtrace-playqueue.o src/audio/testtrace-alhelpers.o src/audio/testtrace-mockingboard.o src/audio/testtrace-AY8910.o"
                    testvm_AUDIO_O="src/audio/testvm-soundcore.o src/audio/testvm-soundcore-openal.o src/audio/testvm-speaker.o src/audio/testvm-playqueue.o src/audio/testvm-alhelpers.o src/audio/testvm-mockingboard.o src/audio/testvm-AY8910.o"
                ], [
                    AC_MSG_WARN([Could not find OpenAL libraries, sound will be disabled])
                ], [])
            ], [
                AC_MSG_WARN([Could not find OpenAL headers, sound will be disabled])
            ], [
#include <AL/al.h>
#include <AL/alc.h>
            ])
        ], [
            AC_MSG_WARN([Could not find OpenAL headers, sound will be disabled])
        ])
    ], [
        AC_MSG_WARN([Could not find OpenAL headers, sound will be disabled])
    ])
])
AC_SUBST(AUDIO_GLUE_C)
AC_SUBST(AUDIO_O)
AC_SUBST(testcpu_AUDIO_O)
AC_SUBST(testdisk_AUDIO_O)
AC_SUBST(testdisplay_AUDIO_O)
AC_SUBST(testprefs_AUDIO_O)
AC_SUBST(testtrace_AUDIO_O)
AC_SUBST(testvm_AUDIO_O)

AS_IF([test "x$openal_supported" = "xyes"], [
], [
    dnl OpenAL not supported
    AS_IF([test "x$openal_selected" = "xyes"], [
        AC_MSG_WARN([!!! DID NOT FIND OPENAL LIBRARIES !!! audio will be disabled (this is OKAY but LIMITED) ...])
    ], [])
])


dnl ---------------------------------------------------------------------------
dnl Debugger & classic interface ...
AC_ARG_ENABLE([debugger], AS_HELP_STRING([--disable-debugger], [Disable 6502 debugging console]), [], [
    AC_DEFINE(DEBUGGER, 1, [Enable 6502 debugger module])
    META_O="src/meta/debug.o src/meta/debugger.o src/meta/opcodes.o src/test/sha1.o"
    dnl HACK there's gotta be a better way ... without this verbosity, CFLAGS are not correct (lacking -DTESTING=1 , etc) if we don't specify specific obj files for test binaries
    testcpu_META_O="src/meta/testcpu-debug.o src/meta/testcpu-debugger.o src/meta/testcpu-opcodes.o src/test/testcpu-sha1.o"
    testdisk_META_O="src/meta/testdisk-debug.o src/meta/testdisk-debugger.o src/meta/testdisk-opcodes.o src/test/testdisk-sha1.o"
    testdisplay_META_O="src/meta/testdisplay-debug.o src/meta/testdisplay-debugger.o src/meta/testdisplay-opcodes.o src/test/testdisplay-sha1.o"
    testprefs_META_O="src/meta/testprefs-debug.o src/meta/testprefs-debugger.o src/meta/testprefs-opcodes.o src/test/testprefs-sha1.o"
    testtrace_META_O="src/meta/testtrace-debug.o src/meta/testtrace-debugger.o src/meta/testtrace-opcodes.o src/test/testtrace-sha1.o"
    testvm_META_O="src/meta/testvm-debug.o src/meta/testvm-debugger.o src/meta/testvm-opcodes.o src/test/testvm-sha1.o"
])
AC_SUBST(META_O)
AC_SUBST(testcpu_META_O)
AC_SUBST(testdisk_META_O)
AC_SUBST(testdisplay_META_O)
AC_SUBST(testprefs_META_O)
AC_SUBST(testtrace_META_O)
AC_SUBST(testvm_META_O)

AC_DEFINE(INTERFACE_CLASSIC, 1, [Use the classic menu interface])

dnl ---------------------------------------------------------------------------
dnl Misc ...

AC_DEFINE(APPLE2IX, 1, [Denotes a section of code as Apple//ix sourced, used with external sources])
AC_DEFINE(KEYPAD_JOYSTICK, 1, [Joystick emulated on keyboard ... should not be true on mobile devices])
AC_DEFINE(CONFORMANT_TRACKS, 1, [Conformant to Applewin, and apparently also to the original //e disk timing, but hella-slow on low-end mobile devices])

dnl ---------------------------------------------------------------------------

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

