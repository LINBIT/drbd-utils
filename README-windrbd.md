Drbd-utils for windrbd
======================

This is the WinDRBD branch of drbd-utils. If you want to build
for Linux, please do a

	git checkout master

now.

To clone this, do a

	git clone --branch windrbd --recursive https://github.com/LINBIT/drbd-utils.git

Build instructions
==================

You need a Windows (at least Windows 7) host with cygwin installed
to build this. To install CygWin, go to:

	https://cygwin.com/install.html

and follow the instructions there. Be sure to install the development
tools (such as gcc, autotools, make, ..) including flex (this is normally
not installed).

Once you are done installing Cygwin, do

	./autogen.sh
	./configure --prefix=/cygdrive/c/windrbd/usr --localstatedir=/cygdrive/c/windrbd/var --sysconfdir=/cygdrive/c/windrbd/etc --without-83support --without-84support --without-drbdmon --with-windrbd
	make
	make install

If you are not already running bash as Administrator user, run the last
command (make install) as Administrator, else make install will fail.

Support
=======

If you need help with drbd and windrbd please contact Linbit at
sales@linbit.com

The windrbd branch is currently maintained by Johannes Thoma.
