#! /bin/sh
echo Running autotools...

mkdir -p build-aux
aclocal -I m4
autoheader
automake --foreign --add-missing
autoconf
