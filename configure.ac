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
TESTVM_ASM_O="src/x86/testvm-glue.o src/x86/testvm-cpu.o"
TESTDISK_ASM_O="src/x86/testdisk-glue.o src/x86/testdisk-cpu.o"
TESTPREFS_ASM_O="src/x86/testprefs-glue.o src/x86/testprefs-cpu.o"
TESTTRACE_ASM_O="src/x86/testtrace-glue.o src/x86/testtrace-cpu.o"
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
AC_SUBST(TESTVM_ASM_O)
AC_SUBST(TESTDISK_ASM_O)
AC_SUBST(TESTPREFS_ASM_O)
AC_SUBST(TESTTRACE_ASM_O)
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
        ], [
            AC_MSG_ERROR([Did not find OpenGL nor X11 libraries...])
        ], [-LX11])
    ], [
        AC_MSG_ERROR([Did not find OpenGL nor X11 headers...])
    ])
])

AC_SUBST(VIDEO_O)

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
])
AC_SUBST(META_O)

AC_DEFINE(INTERFACE_CLASSIC, 1, [Use the classic menu interface])

dnl ---------------------------------------------------------------------------
dnl Misc ...

AC_DEFINE(APPLE2IX, 1, [Denotes a section of code as Apple//ix sourced, used with external sources])
AC_DEFINE(KEYPAD_JOYSTICK, 1, [Joystick emulated on keyboard ... should not be true on mobile devices])
AC_DEFINE(CONFORMANT_TRACKS, 1, [Conformant to Applewin, and apparently also to the original //e disk timing, but hella-slow on low-end mobile devices])

dnl ---------------------------------------------------------------------------

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

