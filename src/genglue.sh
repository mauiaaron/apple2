#!/bin/sh

echo "#include \"$TARGET_ARCH/glue-prologue.h\""
echo "#include \"$TARGET_ARCH/glue-offsets.h\""

while test "x$1" != "x" ; do
    grep -E -h '(GLUE_)|(#[ 	]*if)|(#[ 	]*endif)|(#[ 	]*else)|(#[ 	]*elif)' "$1"
    shift
done

