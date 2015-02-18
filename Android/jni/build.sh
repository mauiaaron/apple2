#!/bin/sh

set -x

apple2_src_path=../../src
glue_srcs="$apple2_src_path/disk.c $apple2_src_path/misc.c $apple2_src_path/display.c $apple2_src_path/vm.c $apple2_src_path/cpu-supp.c"

if test "$(basename $0)" = "clean" ; then
    /bin/rm $apple2_src_path/rom.c
    /bin/rm $apple2_src_path/font.c
    /bin/rm $apple2_src_path/x86/glue.S
    /bin/rm $apple2_src_path/arm/glue.S
    ndk-build clean
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

ndk-build V=1 NDK_DEBUG=1 && ant -f ../build.xml

if test "$(basename $0)" = "run" ; then
    ant -f ../build.xml debug install && \
        adb shell am start -a android.intent.action.MAIN -n org.deadc0de.apple2/.Apple2Activity
fi

if test "$(basename $0)" = "debug" ; then
    echo "TODO FIXME ..."
fi

set +x

