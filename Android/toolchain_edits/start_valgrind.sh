#!/usr/bin/env bash

# This script assumes that Android Valgrind has already been installed on your Android device at /data/local/Inst/

set -x

PACKAGE="org.deadc0de.apple2ix.basic"

# Callgrind tool
#VGPARAMS='-v --error-limit=no --trace-children=yes --log-file=/sdcard/valgrind.log.%p --tool=callgrind --callgrind-out-file=/sdcard/callgrind.out.%p'

# Memcheck tool
VGPARAMS='--num-callers=16 --error-limit=no -v --error-limit=no --default-suppressions=yes --suppressions=/data/local/Inst/lib/valgrind/default.supp --trace-children=yes --log-file=/sdcard/valgrind.log.%p --tool=memcheck --leak-check=full --show-reachable=yes'

# Generate suppressions
VGPARAMS="--gen-suppressions=all $VGPARAMS"

cat << EOF > ./start_valgrind.sh
    #!/system/bin/sh
    # WARNING : these \$ variables need to be defined above and outside this bundled script
    set -x
    PACKAGE=$PACKAGE
    export TMPDIR=/data/data/$PACKAGE
    exec /data/local/Inst/bin/valgrind $VGPARAMS \$*
EOF

DATALOCAL=/data/local

adb push ./start_valgrind.sh $DATALOCAL
ret=$?
if test "x$ret" != "x0" ; then
    exit 1
fi

adb shell chmod 755 $DATALOCAL/start_valgrind.sh
ret=$?
if test "x$ret" != "x0" ; then
    exit 1
fi

adb root
ret=$?
if test "x$ret" != "x0" ; then
    exit 1
fi

# ugh, properties have a 31 character limit (presumably plus \0)
WRAPPCK=$(echo wrap.$PACKAGE | cut -c 1-31 )

adb shell setprop $WRAPPCK "'logwrapper $DATALOCAL/start_valgrind.sh'"
ret=$?
if test "x$ret" != "x0" ; then
    exit 1
fi

echo "WRAPPCK: $(adb shell getprop WRAPPCK)"

adb shell am force-stop $PACKAGE
ret=$?
if test "x$ret" != "x0" ; then
    exit 1
fi

adb shell am start -a android.intent.action.MAIN -n $PACKAGE/org.deadc0de.apple2ix.Apple2Activity
ret=$?
if test "x$ret" != "x0" ; then
    exit 1
fi


# https://code.google.com/p/android/issues/detail?id=93752
echo "hacking around SELinux ... (so this is why we can't have nice thingies)"
adb shell setenforce 0

set +x

exit 0
