#!/bin/sh

package_id="org.deadc0de.apple2ix.basic"
apple2_src_path=apple2ix-src
glue_srcs="$apple2_src_path/disk.c $apple2_src_path/misc.c $apple2_src_path/display.c $apple2_src_path/vm.c $apple2_src_path/cpu-supp.c $apple2_src_path/audio/speaker.c $apple2_src_path/audio/mockingboard.c"
target_arch=armeabi-v7a

usage() {
    echo "$0 [--build-release] [--load] [--debug] [--armeabi | --armeabi-v7a]"
    exit 0
}

export EMBEDDED_STACKWALKER=1

while test "x$1" != "x"; do
    case "$1" in
        "--debug")
            do_debug=1
            ;;

        "--load")
            do_load=1
            ;;

        "--armeabi")
            target_arch=armeabi
            ;;

        "--v7a")
            target_arch=armeabi-v7a
            ;;

        "--build-release")
            do_release=1
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

    ndk-build V=1 NDK_MODULE_PATH=. clean
    ##cd ..
    ##ant clean

    exit 0
fi

/bin/rm Android.mk

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

###############################################################################
# build native sources
if test "x$do_release" = "x1" ; then
    ndk-build V=1 NDK_MODULE_PATH=. # NDK_TOOLCHAIN_VERSION=clang
else
    ndk-build V=1 NDK_MODULE_PATH=. NDK_DEBUG=1 # NDK_TOOLCHAIN_VERSION=clang
fi
ret=$?
if test "x$ret" != "x0" ; then
    exit $ret
fi

###############################################################################
# Symbolicate and move symbols file into location to be deployed on device

SYMFILE=libapple2ix.so.sym
ARCHES_TO_SYMBOLICATE='armeabi armeabi-v7a'

for arch in $ARCHES_TO_SYMBOLICATE ; do
    SYMDIR=../assets/symbols/$arch/libapple2ix.so

    # remove old symbols (if any)
    /bin/rm -rf $SYMDIR

    # Run Breakpad's dump_syms
    ../../externals/bin/dump_syms ../obj/local/$arch/libapple2ix.so > $SYMFILE

    ret=$?
    if test "x$ret" != "x0" ; then
        echo "OOPS, dump_syms failed for $arch"
        exit $ret
    fi

    # strip to the just the numeric id in the .sym header and verify it makes sense
    sym_id=$(head -1 $SYMFILE  | cut -d ' ' -f 4)
    sym_id_check=$(echo $sym_id | wc -c)
    if test "x$sym_id_check" != "x34" ; then
        echo "OOPS symbol header not expected size, meat-space intervention needed =P"
        exit 1
    fi
    sym_id_check=$(echo $sym_id | tr -d 'A-Fa-f0-9' | wc -c)
    if test "x$sym_id_check" != "x1" ; then
        echo "OOPS unexpected characters in symbol header, meat-space intervention needed =P"
        exit 1
    fi

    mkdir -p $SYMDIR/$sym_id
    ret=$?
    if test "x$ret" != "x0" ; then
        echo "OOPS, could not create symbols directory for arch:$arch and sym_id:$sym_id"
        exit $ret
    fi

    /bin/mv $SYMFILE $SYMDIR/$sym_id/
    ret=$?
    if test "x$ret" != "x0" ; then
        echo "OOPS, could not move $SYMFILE to $SYMDIR/$sym_id/"
        exit $ret
    fi
done

###############################################################################
# usually we should build the Java stuff from within Android Studio
if test "x$do_load" = "x1" ; then
    ant -f ../build.xml debug install
    ret=$?
    if test "x$ret" != "x0" ; then
        exit $ret
    fi
fi

###############################################################################
if test "x$do_debug" = "x1" ; then
    cd ..
    /bin/rm ./libs/gdbserver
    /bin/ln -s $target_arch/gdbserver libs/gdbserver
    ##/bin/rm ./libs/gdb.setup
    ##/bin/ln -s $target_arch/gdb.setup libs/gdb.setup
    ##ndk-gdb --verbose --force --launch=org.deadc0de.apple2ix.Apple2Activity
    ndk-gdb --nowait --verbose --force --launch=org.deadc0de.apple2ix.Apple2Activity
elif test "x$do_load" = "x1" ; then
    adb shell am start -a android.intent.action.MAIN -n org.deadc0de.apple2ix.basic/org.deadc0de.apple2ix.Apple2Activity
fi

set +x

