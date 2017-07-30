dnl ---------------------------------------------------------------------------

AC_DEFINE(PACKAGE_URL, "https://deadc0de.org/apple2ix", [apple2ix project URL])

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
testui_ASM_O="src/x86/testui-glue.o src/x86/testui-cpu.o"
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
AC_SUBST(testui_ASM_O)
AC_SUBST(testvm_ASM_O)

AC_SUBST([AM_CFLAGS])


dnl OS Check
AC_EGREP_CPP(unsupported_, [
#if defined(__ANDROID__)
unsupported_for_now
#elif __APPLE__
unsupported_for_now
    #include "TargetConditionals.h"
    #if TARGET_OS_SIMULATOR
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


AC_ARG_ENABLE([opengl], AS_HELP_STRING([--enable-opengl], [Enable OpenGL graphics output (autodetected)]))
AC_CHECK_HEADER(GL/glew.h, [
    AC_CHECK_HEADER(GL/freeglut.h, [
        AC_SEARCH_LIBS(glCreateProgram, [GL], [
            AC_SEARCH_LIBS(glutMainLoop, [glut freeglut], [
                AC_SEARCH_LIBS(glewInit, [GLEW glew], [

                    AS_IF([test "x$enable_opengl" != "xno"], [

                        AC_DEFINE(VIDEO_OPENGL, 1, [Building with OpenGL support])
                        AC_DEFINE(USE_GLUT, 1, [Use GLUT library])
                        VIDEO_O="src/video/glvideo.o src/video/glnode.o src/video/glalert.o src/video/glhudmodel.o src/video/glutinput.o src/video_util/matrixUtil.o src/video_util/modelUtil.o src/video_util/sourceUtil.o src/video_util/vectorUtil.o"
                        dnl HACK there's gotta be a better way ... without this verbosity, CFLAGS are not correct (lacking -DTESTING=1 , etc) if we don't specify specific obj files for test binaries
                        testcpu_VIDEO_O="src/video/testcpu-glvideo.o src/video/testcpu-glnode.o src/video/testcpu-glalert.o src/video/testcpu-glhudmodel.o src/video/testcpu-glutinput.o src/video_util/testcpu-matrixUtil.o src/video_util/testcpu-modelUtil.o src/video_util/testcpu-sourceUtil.o src/video_util/testcpu-vectorUtil.o"
                        testdisk_VIDEO_O="src/video/testdisk-glvideo.o src/video/testdisk-glnode.o src/video/testdisk-glalert.o src/video/testdisk-glhudmodel.o src/video/testdisk-glutinput.o src/video_util/testdisk-matrixUtil.o src/video_util/testdisk-modelUtil.o src/video_util/testdisk-sourceUtil.o src/video_util/testdisk-vectorUtil.o"
                        testdisplay_VIDEO_O="src/video/testdisplay-glvideo.o src/video/testdisplay-glnode.o src/video/testdisplay-glalert.o src/video/testdisplay-glhudmodel.o src/video/testdisplay-glutinput.o src/video_util/testdisplay-matrixUtil.o src/video_util/testdisplay-modelUtil.o src/video_util/testdisplay-sourceUtil.o src/video_util/testdisplay-vectorUtil.o"
                        testprefs_VIDEO_O="src/video/testprefs-glvideo.o src/video/testprefs-glnode.o src/video/testprefs-glalert.o src/video/testprefs-glhudmodel.o src/video/testprefs-glutinput.o src/video_util/testprefs-matrixUtil.o src/video_util/testprefs-modelUtil.o src/video_util/testprefs-sourceUtil.o src/video_util/testprefs-vectorUtil.o"
                        testtrace_VIDEO_O="src/video/testtrace-glvideo.o src/video/testtrace-glnode.o src/video/testtrace-glalert.o src/video/testtrace-glhudmodel.o src/video/testtrace-glutinput.o src/video_util/testtrace-matrixUtil.o src/video_util/testtrace-modelUtil.o src/video_util/testtrace-sourceUtil.o src/video_util/testtrace-vectorUtil.o"
                        testui_VIDEO_O="src/video/testui-glvideo.o src/video/testui-glnode.o src/video/testui-glalert.o src/video/testui-glhudmodel.o src/video/testui-glutinput.o src/video_util/testui-matrixUtil.o src/video_util/testui-modelUtil.o src/video_util/testui-sourceUtil.o src/video_util/testui-vectorUtil.o"
                        testvm_VIDEO_O="src/video/testvm-glvideo.o src/video/testvm-glnode.o src/video/testvm-glalert.o src/video/testvm-glhudmodel.o src/video/testvm-glutinput.o src/video_util/testvm-matrixUtil.o src/video_util/testvm-modelUtil.o src/video_util/testvm-sourceUtil.o src/video_util/testvm-vectorUtil.o"
                        AC_MSG_RESULT([Building emulator with OpenGL support, w00t!])
                    ])
                ], [
                    AS_IF([test "x$enable_opengl" != "xno"], [
                        AC_MSG_WARN([Did not find OpenGL GLEW library, OpenGL support is disabled ...])
                    ])
                ], [-lGL -lGLEW -lglut])
            ], [
                AS_IF([test "x$enable_opengl" != "xno"], [
                    AC_MSG_WARN([Did not find glut library, OpenGL support is disabled ...])
                ])
            ], [-lGL -lGLEW -lglut])
        ], [
            AS_IF([test "x$enable_opengl" != "xno"], [
                AC_MSG_WARN([Did not find OpenGL library, OpenGL support is disabled ...])
            ])
        ], [-lGL])
    ], [
        AS_IF([test "x$enable_opengl" != "xno"], [
            AC_MSG_WARN([Did not find GL/freeglut.h header, OpenGL support is disabled ...])
        ])
    ])
], [
    AS_IF([test "x$enable_opengl" != "xno"], [
        AC_MSG_WARN([Did not find GL/glew.h header, OpenGL support is disabled ...])
    ])
])


AC_ARG_ENABLE([x11], AS_HELP_STRING([--enable-x11], [Enable X11 graphics output (autodetected)]))
AC_CHECK_HEADER(X11/XKBlib.h, [
    AC_SEARCH_LIBS(XPutImage, [X11], [
        AS_IF([test "x$enable_x11" != "xno"], [
            AC_SEARCH_LIBS(XShmAttach, Xext, [
                AC_DEFINE(HAVE_X11_SHM, 1, [Enable X11 MIT SHM extension])
            ], [
                AC_MSG_WARN([Building emulator without support of X11 MITSHM extension...])
            ], [-lX11])
            VIDEO_O="$VIDEO_O src/video/xvideo.o"
            dnl HACK there's gotta be a better way ... without this verbosity, CFLAGS are not correct (lacking -DTESTING=1 , etc) if we don't specify specific obj files for test binaries
            testcpu_VIDEO_O="$testcpu_VIDEO_O src/video/testcpu-xvideo.o"
            testdisk_VIDEO_O="$testdisk_VIDEO_O src/video/testdisk-xvideo.o"
            testdisplay_VIDEO_O="$testdisplay_VIDEO_O src/video/testdisplay-xvideo.o"
            testprefs_VIDEO_O="$testprefs_VIDEO_O src/video/testprefs-xvideo.o"
            testtrace_VIDEO_O="$testtrace_VIDEO_O src/video/testtrace-xvideo.o"
            testui_VIDEO_O="$testui_VIDEO_O src/video/testui-xvideo.o"
            testvm_VIDEO_O="$testvm_VIDEO_O src/video/testvm-xvideo.o"
            AC_MSG_RESULT([Building emulator with X11 support])
        ])
    ], [
        AS_IF([test "x$enable_x11" != "xno"], [
            AC_MSG_WARN([Did not find X11 libraries, X11 support is disabled ...])
        ])
    ], [-LX11])
], [
    AS_IF([test "x$enable_x11" != "xno"], [
        AC_MSG_WARN([Did not find X11 headers, X11 support is disabled ...])
    ])
])


AC_SUBST(VIDEO_O)
AC_SUBST(testcpu_VIDEO_O)
AC_SUBST(testdisk_VIDEO_O)
AC_SUBST(testdisplay_VIDEO_O)
AC_SUBST(testprefs_VIDEO_O)
AC_SUBST(testtrace_VIDEO_O)
AC_SUBST(testui_VIDEO_O)
AC_SUBST(testvm_VIDEO_O)


dnl ---------------------------------------------------------------------------
dnl Sound ...

AUDIO_GLUE_C="src/audio/speaker.c src/audio/mockingboard.c"
AUDIO_O="src/audio/soundcore.o src/audio/speaker.o src/audio/mockingboard.o src/audio/AY8910.o"
testcpu_AUDIO_O="src/audio/testcpu-soundcore.o src/audio/testcpu-speaker.o src/audio/testcpu-mockingboard.o src/audio/testcpu-AY8910.o"
testdisk_AUDIO_O="src/audio/testdisk-soundcore.o src/audio/testdisk-speaker.o src/audio/testdisk-mockingboard.o src/audio/testdisk-AY8910.o"
testdisplay_AUDIO_O="src/audio/testdisplay-soundcore.o src/audio/testdisplay-speaker.o src/audio/testdisplay-mockingboard.o src/audio/testdisplay-AY8910.o"
testprefs_AUDIO_O="src/audio/testprefs-soundcore.o src/audio/testprefs-speaker.o src/audio/testprefs-mockingboard.o src/audio/testprefs-AY8910.o"
testtrace_AUDIO_O="src/audio/testtrace-soundcore.o src/audio/testtrace-speaker.o src/audio/testtrace-mockingboard.o src/audio/testtrace-AY8910.o"
testui_AUDIO_O="src/audio/testui-soundcore.o src/audio/testui-speaker.o src/audio/testui-mockingboard.o src/audio/testui-AY8910.o"
testvm_AUDIO_O="src/audio/testvm-soundcore.o src/audio/testvm-speaker.o src/audio/testvm-mockingboard.o src/audio/testvm-AY8910.o"

AC_ARG_ENABLE([openal], AS_HELP_STRING([--enable-openal], [Enable OpenAL audio output (autodetected)]))

AC_CHECK_HEADER(AL/al.h, [
    AC_CHECK_HEADER(AL/alc.h, [
        AC_CHECK_HEADER(AL/alext.h, [
            AC_SEARCH_LIBS(alcOpenDevice, openal, [
                AS_IF([test "x$enable_openal" != "xno"], [
                    dnl found OpenAL ...
                    AUDIO_O="$AUDIO_O src/audio/soundcore-openal.o src/audio/playqueue.o src/audio/alhelpers.o"
                    dnl HACK there's gotta be a better way ... without this verbosity, CFLAGS are not correct (lacking -DTESTING=1 , etc) if we don't specify specific obj files for test binaries
                    testcpu_AUDIO_O="$testcpu_AUDIO_O src/audio/testcpu-soundcore-openal.o src/audio/testcpu-playqueue.o src/audio/testcpu-alhelpers.o"
                    testdisk_AUDIO_O="$testdisk_AUDIO_O src/audio/testdisk-soundcore-openal.o src/audio/testdisk-playqueue.o src/audio/testdisk-alhelpers.o"
                    testdisplay_AUDIO_O="$testdisplay_AUDIO_O src/audio/testdisplay-soundcore-openal.o src/audio/testdisplay-playqueue.o src/audio/testdisplay-alhelpers.o"
                    testprefs_AUDIO_O="$testprefs_AUDIO_O src/audio/testprefs-soundcore-openal.o src/audio/testprefs-playqueue.o src/audio/testprefs-alhelpers.o"
                    testtrace_AUDIO_O="$testtrace_AUDIO_O src/audio/testtrace-soundcore-openal.o src/audio/testtrace-playqueue.o src/audio/testtrace-alhelpers.o"
                    testui_AUDIO_O="$testui_AUDIO_O src/audio/testui-soundcore-openal.o src/audio/testui-playqueue.o src/audio/testui-alhelpers.o"
                    testvm_AUDIO_O="$testvm_AUDIO_O src/audio/testvm-soundcore-openal.o src/audio/testvm-playqueue.o src/audio/testvm-alhelpers.o"
                ])
            ], [
                AS_IF([test "x$enable_openal" != "xno"], [
                    AC_MSG_WARN([Did not find OpenAL libraries, OpenAL sound output is disabled ...])
                ])
            ], [])
        ], [
            AS_IF([test "x$enable_openal" != "xno"], [
                AC_MSG_WARN([Could not find OpenAL headers, OpenAL sound output is disabled ...])
            ])
        ], [
#include <AL/al.h>
#include <AL/alc.h>
        ])
    ], [
        AS_IF([test "x$enable_openal" != "xno"], [
            AC_MSG_WARN([Could not find OpenAL headers, OpenAL sound output is disabled ...])
        ])
    ])
], [
    AS_IF([test "x$enable_openal" != "xno"], [
        AC_MSG_WARN([Could not find OpenAL headers, OpenAL sound output is disabled ...])
    ])
])

AC_SUBST(AUDIO_GLUE_C)
AC_SUBST(AUDIO_O)
AC_SUBST(testcpu_AUDIO_O)
AC_SUBST(testdisk_AUDIO_O)
AC_SUBST(testdisplay_AUDIO_O)
AC_SUBST(testprefs_AUDIO_O)
AC_SUBST(testtrace_AUDIO_O)
AC_SUBST(testui_AUDIO_O)
AC_SUBST(testvm_AUDIO_O)


dnl ---------------------------------------------------------------------------
dnl Misc ...

AC_DEFINE(APPLE2IX, 1, [Denotes a section of code as Apple//ix sourced, used with external sources])
AC_DEFINE(INTERFACE_CLASSIC, 1, [Use the classic menu interface])
AC_DEFINE(KEYPAD_JOYSTICK, 1, [Joystick emulated on keyboard ... should not be true on mobile devices])
AC_DEFINE(CONFORMANT_TRACKS, 1, [Conformant to Applewin, and apparently also to the original //e disk timing, but hella-slow on low-end mobile devices])


dnl ---------------------------------------------------------------------------

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

