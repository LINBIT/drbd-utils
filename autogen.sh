#!/bin/sh

# for those that expect an autogen.sh,
# here it is.

aclocal
autoheader
autoconf

echo "
suggested configure parameters:
# prepare for rpmbuild, only generate spec files
./configure --enable-spec
# or prepare for direct build
./configure --prefix=/usr --localstatedir=/var --sysconfdir=/etc
"
