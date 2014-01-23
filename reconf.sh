#!/bin/sh

set -x

rm -f config.cache

aclocal
autoconf
autoheader
automake -a --warnings=all

set +x
