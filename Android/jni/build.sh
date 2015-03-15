#!/bin/sh

package_id="org.deadc0de.apple2ix"
apple2_src_path=apple2ix-src
glue_srcs="$apple2_src_path/disk.c $apple2_src_path/misc.c $apple2_src_path/display.c $apple2_src_path/vm.c $apple2_src_path/cpu-supp.c"
do_load=0
do_debug=0

usage() {
    echo "$0 [--load|--debug]"
    exit 0
}

while test "x$1" != "x"; do
    case "$1" in
        "--debug")
            do_load=1
            do_debug=1
            ;;

        "--load")
            do_load=1
            do_debug=0
            ;;

        "-h")
            usage
            ;;

        "--help")
            usage
            ;;

        *)
            package_id=$1
            ;;
    esac
    shift
done

set -x

/bin/rm Android.mk

if test "$(basename $0)" = "clean" ; then
    /bin/rm $apple2_src_path/rom.c
    /bin/rm $apple2_src_path/font.c
    /bin/rm $apple2_src_path/x86/glue.S
    /bin/rm $apple2_src_path/arm/glue.S

    # considered dangerous
    /bin/rm -rf ../bin
    /bin/rm -rf ../libs
    /bin/rm -rf ../gen
    /bin/rm -rf ../obj

    exit 0
fi

if test "$(basename $0)" = "uninstall" ; then
    adb uninstall $package_id
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
elif test "$(basename $0)" = "testvm" ; then
    ln -s testvm.mk Android.mk
elif test "$(basename $0)" = "testdisplay" ; then
    ln -s testdisplay.mk Android.mk
elif test "$(basename $0)" = "testdisk" ; then
    ln -s testdisk.mk Android.mk
else
    ln -s apple2ix.mk Android.mk
fi

ndk-build V=1 NDK_DEBUG=1 && \
    ant -f ../build.xml debug

if test "x$do_load" = "x1" ; then
    ant -f ../build.xml debug install
fi

if test "x$do_debug" = "x1" ; then
    ( cd .. && ndk-gdb.py --force --start )
elif test "x$do_load" = "x1" ; then
    adb shell am start -a android.intent.action.MAIN -n $package_id/.Apple2Activity
fi

set +x

