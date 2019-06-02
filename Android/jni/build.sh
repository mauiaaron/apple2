#!/bin/bash

package_id="org.deadc0de.apple2ix.basic"
apple2_src_path=apple2ix-src
glue_srcs="$apple2_src_path/cpu-supp.c $apple2_src_path/disk.c $apple2_src_path/display.c $apple2_src_path/vm.c $apple2_src_path/audio/speaker.c $apple2_src_path/audio/mockingboard.c"

usage() {
    if test "$(basename $0)" = "clean" ; then
        echo "$0"
        echo "	# cleans NDK build of $package_id"
    elif test "$(basename $0)" = "uninstall" ; then
        echo "$0"
        echo "	# uninstalls $package_id"
    else
        echo "$0 [build|release] [load|debug]"
        echo "	# default builds $package_id and then load or debug"
    fi
    exit 0
}

export EMBEDDED_STACKWALKER=1

while test "x$1" != "x"; do
    case "$1" in
        "build")
            do_build=1
            ;;

        "release")
            do_release=1
            ;;

        "debug")
            do_debug=1
            ;;

        "load")
            do_load=1
            ;;

        "-h")
            usage
            ;;

        "--help")
            usage
            ;;

        *)
            usage
            ;;
    esac
    shift
done

if test "x$do_build" = "x1" -a "x$do_release" = "x1" ; then
    echo "Must specify either build or release"
    usage
fi

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

if test "$(basename $0)" = "testcpu" ; then
    ln -s testcpu.mk Android.mk
elif test "$(basename $0)" = "testdisk" ; then
    ln -s testdisk.mk Android.mk
elif test "$(basename $0)" = "testdisplay" ; then
    ln -s testdisplay.mk Android.mk
elif test "$(basename $0)" = "testprefs" ; then
    ln -s testprefs.mk Android.mk
elif test "$(basename $0)" = "testtrace" ; then
    ln -s testtrace.mk Android.mk
elif test "$(basename $0)" = "testui" ; then
    ln -s testui.mk Android.mk
elif test "$(basename $0)" = "testvm" ; then
    ln -s testvm.mk Android.mk
elif test "$(basename $0)" = "apple2ix" ; then
    ln -s apple2ix.mk Android.mk
else
    echo "OOPS, unsure of what to build"
    exit 1
fi

###############################################################################
# build native sources

if test "x$do_build" = "x1" -o "x$do_release" = "x1" ; then

    #CC=`which clang`
    CC=`which gcc`
    CFLAGS="-std=gnu11"

    # ROMz
    $CC $CFLAGS -o $apple2_src_path/genrom $apple2_src_path/genrom.c && \
        $apple2_src_path/genrom $apple2_src_path/rom/apple_IIe.rom $apple2_src_path/rom/slot6.rom > $apple2_src_path/rom.c

    # font
    $CC $CFLAGS -o $apple2_src_path/genfont $apple2_src_path/genfont.c && \
        $apple2_src_path/genfont < $apple2_src_path/font.txt > $apple2_src_path/font.c

    # bridge trampoline generation
    TARGET_ARCH=x86 $apple2_src_path/genglue.sh $glue_srcs > $apple2_src_path/x86/glue.S
    TARGET_ARCH=arm $apple2_src_path/genglue.sh $glue_srcs > $apple2_src_path/arm/glue.S

    if test "x$do_build" = "x1" ; then
        export BUILD_MODE=debug
        ndk-build V=1 NDK_MODULE_PATH=. NDK_DEBUG=1 NDK_TOOLCHAIN_VERSION=4.9
        ret=$?
        if test "x$ret" != "x0" ; then
            exit $ret
        fi
    else
        export BUILD_MODE=release
        ndk-build V=1 NDK_MODULE_PATH=. NDK_DEBUG=0 NDK_TOOLCHAIN_VERSION=4.9
        ret=$?
        if test "x$ret" != "x0" ; then
            exit $ret
        fi
    fi

    # Symbolicate and move symbols file into location to be deployed on device

    SYMFILE=libapple2ix.so.sym
    ARCHES_TO_SYMBOLICATE='armeabi-v7a x86 x86_64'

    for arch in $ARCHES_TO_SYMBOLICATE ; do
        SYMDIR=../assets/symbols/$arch/libapple2ix.so

        # remove old symbols (if any)
        /bin/rm -rf $SYMDIR

        # Run Breakpad's dump_syms
        host_arch=`uname -s`
        ../../externals/bin/$host_arch/dump_syms ../obj/local/$arch/libapple2ix.so > $SYMFILE

        ret=$?
        if test "x$ret" != "x0" ; then
            echo "OOPS, dump_syms failed for $arch"
            exit $ret
        fi

        # strip to the just the numeric id in the .sym header and verify it makes sense
        sym_id=$(head -1 $SYMFILE  | cut -d ' ' -f 4)
        sym_id_check=$(echo $sym_id | wc -c | tr -d ' 	' )
        if test "x$sym_id_check" != "x34" ; then
            echo "OOPS symbol header not expected size, meat-space intervention needed =P"
            exit 1
        fi
        sym_id_check=$(echo $sym_id | tr -d 'A-Fa-f0-9' | wc -c | tr -d ' 	' )
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

fi

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
    export RUNNING_GDB=1
    cd ..
    ndk-gdb --nowait --force --verbose --launch=org.deadc0de.apple2ix.Apple2Activity
elif test "x$do_load" = "x1" ; then
    adb shell am start -a android.intent.action.MAIN -n org.deadc0de.apple2ix.basic/org.deadc0de.apple2ix.Apple2Activity
fi

set +x

