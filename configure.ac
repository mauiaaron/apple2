dnl ---------------------------------------------------------------------------

AC_PREREQ([2.69])
AC_INIT([apple2ix], [0.8])

AM_INIT_AUTOMAKE([foreign subdir-objects])

dnl AM_CONFIG_HEADER(src/config.h) -- disable config.h because it makes it difficult/impossible to do modular builds for the test suite

AC_PROG_CC([clang gcc])
AM_PROG_CC_C_O      dnl apparently required for custom font.c target?
AM_PROG_AS
AC_PROG_INSTALL


dnl ---------------------------------------------------------------------------
dnl Arch i386 checks, sigh... The plan is to eventually support x64 and certain
dnl ARM targets, but we only handle i386 at the moment thus this hackishness

dnl arch check (currently must be i386)
my_save_cflags="$CFLAGS"
CFLAGS="-m32 -Xassembler --32"
AC_MSG_CHECKING([whether CC supports compiling for i386 ])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])], [
    AC_MSG_RESULT([w00t!])
    AC_MSG_WARN([compiling for i386 architecture appears to work, but emulator may fail to link if i386 versions of required libraries are not present...])
    AM_CFLAGS="-std=gnu11 -Wall $CFLAGS"
], [
    AC_MSG_RESULT([oops])
    AC_MSG_ERROR([emulator currently supports (cross-)compilation for i386 architecture only])
])

CFLAGS="$my_save_cflags"
CCASFLAGS="$CCASFLAGS -m32 -Xassembler --32"

AC_SUBST([AM_CFLAGS])

dnl OS Check (currently must be Linux until we sort the arch crap out) ...
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
unsupported_for_now
#elif __posix
unsupported_for_now
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

dnl at this point we believe arch/os is good ...

ARCHOS_HACK_LDFLAGS="-L/usr/lib/i386-linux-gnu -L/lib/i386-linux-gnu"
AC_SUBST(ARCHOS_HACK_LDFLAGS)

VM_O="src/asm386/glue.o src/asm386/cpu.o src/asm386/display.o src/asm386/memory.o"
AC_SUBST(VM_O)


dnl ---------------------------------------------------------------------------

AM_PROG_LEX

dnl AS_IF([test "x$LEX" = "xno"], [
dnl     AC_MSG_ERROR([Emulator needs lex/flex to build source...])
dnl ], [
dnl     AC_MSG_WARN([Found lex $LEX])
dnl ])

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


dnl ---------------------------------------------------------------------------
dnl Video ...

dnl currently X11 is required ...
AC_PATH_XTRA

AC_CHECK_HEADER(X11/XKBlib.h, [], [
    AC_MSG_ERROR([Emulator needs X11 headers to build this software...])
])
AC_SEARCH_LIBS(XPutImage, [X11], [], [
    AC_MSG_ERROR([Emulator need X11 libraries to build the emulator...])
], [-LX11])

AC_SEARCH_LIBS(XShmAttach, Xext, [
    AC_DEFINE(HAVE_X11_SHM, 1, [Enable X11 MIT SHM extension])
], [
    AC_MSG_WARN([Building emulator without support of X11 MITSHM extension...])
], [-lX11])

AC_DEFINE(VIDEO_X11, 1, [Enable common X11 video])

VIDEO_O="src/video/xvideo.o"
AC_SUBST(VIDEO_O)


dnl ---------------------------------------------------------------------------
dnl Sound ...

AC_ARG_ENABLE([audio], AS_HELP_STRING([--disable-audio], [Disable emulator audio output]), [], [
    AC_CHECK_HEADER(AL/al.h, [
        AC_CHECK_HEADER(AL/alc.h, [
            AC_CHECK_HEADER(AL/alext.h, [
                AC_SEARCH_LIBS(alcOpenDevice, openal, [
                    dnl found OpenAL ...
                    AC_DEFINE(AUDIO_OPENAL, 1, [Enable OpenAL audio output])
                    AC_DEFINE(AUDIO_ENABLED, 1, [Enable sound module])
                    AUDIO_GLUE_C="src/audio/mockingboard.c"
                    AUDIO_O="src/audio/soundcore.o src/audio/soundcore-openal.o src/audio/speaker.o src/audio/win-shim.o src/audio/alhelpers.o src/audio/mockingboard.o src/audio/AY8910.o"
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

dnl AS_IF([test "x$audio_disabled" = "xno"], [
dnl     ...
dnl ])


dnl ---------------------------------------------------------------------------
dnl Debugger & classic interface ...
AC_ARG_ENABLE([debugger], AS_HELP_STRING([--disable-debugger], [Disable 6502 debugging console]), [], [
    AC_DEFINE(DEBUGGER, 1, [Enable 6502 debugger module])
    META_O="src/meta/debug.o src/meta/debugger.o src/meta/opcodes.o"

    AC_CHECK_HEADER(openssl/sha.h, [
        AC_SEARCH_LIBS(SHA1, crypto, [
            AC_DEFINE(HAVE_OPENSSL, 1, [Use OpenSSL])
        ], [
            AC_MSG_WARN([Could not find OpenSSL libraries, certain functionality will be disabled])
        ], [])
    ], [
        AC_MSG_WARN([Could not find openssl/sha.h header])
    ], [
#include <openssl/sha.h>
    ])
])
AC_SUBST(META_O)

INTERFACE_O="src/interface.o"
AC_SUBST(INTERFACE_O)
AC_DEFINE(INTERFACE_CLASSIC, 1, [Use the classic menu interface])

dnl ---------------------------------------------------------------------------
dnl Misc ...

AC_DEFINE(APPLE2IX, 1, [Denotes a section of code as Apple//ix sourced, used with external sources])
AC_DEFINE(KEYPAD_JOYSTICK, 1, [Joystick emulated on keyboard ... should not be true on mobile devices])

dnl Joystick device
AC_CHECK_HEADER(linux/joystick.h, [
    AC_DEFINE(LINUX_JOYSTICK, 1, [Enable Linux joystick device])
], [
    AC_MSG_WARN([Could not find Linux PC Joystick/GamePad header...])
    AC_MSG_WARN([Emulator will be built without Joystick device support, but this may diminish the experience!])
])


dnl ---------------------------------------------------------------------------

AC_DEFINE(PACKAGE_URL, "https://github.com/mauiaaron/apple2", [apple2ix project URL])
AC_DEFINE(WEB_RESOURCES, "ftp://ftp.apple.asimov.net", [Apple II Web Resources])
AC_DEFINE(PACKAGE_MANPAGE, "man apple2ix", [manpage info])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

