#!/bin/sh

# for those that expect an autogen.sh,
# here it is.

aclocal
autoheader
autoconf

echo "
suggested configure parameters:
# for windrbd (on a cygwin host, assuming C:\windrbd is your windrbd root)
./configure --prefix=/cygdrive/c/windrbd/usr --localstatedir=/cygdrive/c/windrbd/var --sysconfdir=/cygdrive/c/windrbd/etc --without-83support --without-84support --without-drbdmon --with-windrbd
# prepare for rpmbuild, only generate spec files
./configure --enable-spec
# or prepare for direct build
./configure --prefix=/usr --localstatedir=/var --sysconfdir=/etc
"
