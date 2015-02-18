#!/bin/sh

set -x

apple2_src_path=../../src
glue_srcs="$apple2_src_path/disk.c $apple2_src_path/misc.c $apple2_src_path/display.c $apple2_src_path/vm.c $apple2_src_path/cpu-supp.c"

if test "$(basename $0)" = "clean" ; then
    /bin/rm $apple2_src_path/rom.c
    /bin/rm $apple2_src_path/font.c
    /bin/rm $apple2_src_path/x86/glue.S
    /bin/rm $apple2_src_path/arm/glue.S

    ln -s apple2ix.mk Android.mk
    ndk-build -f apple2ix.mk clean
    /bin/rm Android.mk

    ln -s testcpu.mk Android.mk
    ndk-build -f testcpu.mk clean
    /bin/rm Android.mk

    ln -s testvm.mk Android.mk
    ndk-build -f testvm.mk clean
    /bin/rm Android.mk

    ln -s testdisplay.mk Android.mk
    ndk-build -f testdisplay.mk clean
    /bin/rm Android.mk

    ln -s testdisk.mk Android.mk
    ndk-build -f testdisk.m clean
    /bin/rm Android.mk

    exit 0
fi

#CC=`which clang`
CC=`which gcc`
CFLAGS="-std=gnu11"

# ROMz
$CC $CFLAGS -o $apple2_src_path/genrom $apple2_src_path/genrom.c && \
    $apple2_src_path/genrom $apple2_src_path/rom/apple_IIe.rom $apple2_src_path/rom/slot6.rom > $apple2_src_path/rom.c

# font
$CC $CFLAGS -o $apple2_src_path/genfont $apple2_src_path/genfont.c && \
    $apple2_src_path/genfont < $apple2_src_path/font.txt > $apple2_src_path/font.c

# glue
$apple2_src_path/x86/genglue $glue_srcs > $apple2_src_path/x86/glue.S
$apple2_src_path/arm/genglue $glue_srcs > $apple2_src_path/arm/glue.S

if test "$(basename $0)" = "testcpu" ; then
    ln -s testcpu.mk Android.mk
    ndk-build V=1 NDK_DEBUG=1 && ant -f ../build.xml debug
elif test "$(basename $0)" = "testvm" ; then
    ln -s testvm.mk Android.mk
    ndk-build V=1 NDK_DEBUG=1 && ant -f ../build.xml debug
elif test "$(basename $0)" = "testdisplay" ; then
    ln -s testdisplay.mk Android.mk
    ndk-build V=1 NDK_DEBUG=1 && ant -f ../build.xml debug
elif test "$(basename $0)" = "testdisk" ; then
    ln -s testdisk.mk Android.mk
    ndk-build V=1 NDK_DEBUG=1 && ant -f ../build.xml debug
else
    ln -s apple2ix.mk Android.mk
    ndk-build V=1 NDK_DEBUG=1 && ant -f ../build.xml debug
fi

if test "$(basename $0)" = "run" ; then
    ant -f ../build.xml install && \
        adb shell am start -a android.intent.action.MAIN -n org.deadc0de.apple2/.Apple2Activity
fi

if test "$(basename $0)" = "debug" ; then
    echo "TODO FIXME ..."
fi

/bin/rm Android.mk

set +x

