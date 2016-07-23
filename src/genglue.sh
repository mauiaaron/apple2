#!/bin/sh

echo "#include \"$TARGET_ARCH/glue-prologue.h\""
grep -E -h '(GLUE_)|(#[ 	]*if)|(#[ 	]*endif)|(#[ 	]*else)|(#[ 	]*elif)' $*
exit 0
