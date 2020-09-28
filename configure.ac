dnl
dnl autoconf for DRBD
dnl
dnl License: GNU General Public License Version 2 (GPLv2)

dnl Minimum autoconf version we require
AC_PREREQ(2.53)

dnl Include cpp feature testing macro
dnl AC_CONFIG_MACRO_DIR([m4])
dnl Using AC_CONFIG_MACRO_DIR([m4]) would be the correct way, but RHEL6 ships a broken autoconf
m4_include([m4/AX_CXX_COMPILE_STDXX.m4])
m4_include([m4/AX_CXX_COMPILE_STDXX_11.m4])

dnl What we are, our version, who to bug in case of problems
AC_INIT(DRBD, 9.15.0, [drbd-dev@lists.linbit.com])

dnl Sanitize $prefix. Autoconf does this by itself, but so late in the
dnl generated configure script that the expansion does not occur until
dnl after our eval magic below.
if test "$prefix" = "NONE"; then
  prefix=$ac_default_prefix
fi
exec_prefix=$prefix

dnl Expand autoconf variables so that we dont end up with '${prefix}'
dnl in #defines
dnl Autoconf deliberately leaves them unexpanded to allow make
dnl exec_prefix=/foo install. DRBD supports only DESTDIR, KDIR and
dnl KVER to be invoked with make.
prefix="`eval echo ${prefix}`"
exec_prefix="`eval echo ${exec_prefix}`"
bindir="`eval echo ${bindir}`"
sbindir="`eval echo ${sbindir}`"
libexecdir="`eval echo ${libexecdir}`"
datarootdir="`eval echo ${datarootdir}`"
datadir="`eval echo ${datadir}`"
sysconfdir="`eval echo ${sysconfdir}`"
sharedstatedir="`eval echo ${sharedstatedir}`"
localstatedir="`eval echo ${localstatedir}`"
libdir="`eval echo ${libdir}`"
includedir="`eval echo ${includedir}`"
oldincludedir="`eval echo ${oldincludedir}`"
infodir="`eval echo ${infodir}`"
mandir="`eval echo ${mandir}`"
docdir="`eval echo ${docdir}`"

dnl "--with-<foo>" options (all except rgm enabled by default, pass --without-<foo> to disable)
WITH_83_SUPPORT=yes
WITH_84_SUPPORT=yes
WITH_UDEV=yes
WITH_XEN=yes
WITH_PACEMAKER=yes
WITH_HEARTBEAT=yes
WITH_RGMANAGER=no
WITH_BASHCOMPLETION=yes
WITH_NOARCH_SUBPACKAGES=no
WITH_MANUAL=yes
WITH_DRBDMON=yes
WITH_WINDRBD=no
WITH_PREBUILTMAN=no
AC_ARG_WITH([83support],
	[AS_HELP_STRING([--without-83support],
			[Do not include support for drbd driver/module <= 8.3])],
	[WITH_83_SUPPORT=$withval])
AC_ARG_WITH([84support],
	[AS_HELP_STRING([--without-84support],
			[Do not include support for drbd driver/module 8.4])],
	[WITH_84_SUPPORT=$withval])
AC_ARG_WITH([udev],
	[AS_HELP_STRING([--with-udev],
			[Enable udev integration])],
	[WITH_UDEV=$withval])
AC_ARG_WITH([xen],
	[AS_HELP_STRING([--with-xen],
			[Enable Xen integration])],
	[WITH_XEN=$withval])
AC_ARG_WITH([pacemaker],
	[AS_HELP_STRING([--with-pacemaker],
			[Enable Pacemaker integration])],
	[WITH_PACEMAKER=$withval])
AC_ARG_WITH([heartbeat],
	[AS_HELP_STRING([--with-heartbeat],
			[Enable Heartbeat v1 haresources integration scripts])],
	[WITH_HEARTBEAT=$withval])
AC_ARG_WITH([rgmanager],
	[AS_HELP_STRING([--with-rgmanager],
			[Enable Red Hat Cluster Suite integration])],
	[WITH_RGMANAGER=$withval])
AC_ARG_WITH([bashcompletion],
	[AS_HELP_STRING([--with-bashcompletion],
			[Enable programmable bash completion])],
	[WITH_BASHCOMPLETION=$withval])
AC_ARG_WITH([distro],
	[AS_HELP_STRING([--with-distro],
			[Configure for a specific distribution (supported values: generic, redhat, suse, debian; default is to autodetect)])],
	[DISTRO=$withval])
AC_ARG_WITH([initdir],
	[AS_HELP_STRING([--with-initdir],
			[Override directory for init scripts (default is distribution-specific)])],
	[INITDIR=$withval])
AC_ARG_WITH([noarchsubpkg],
	[AS_HELP_STRING([--with-noarchsubpkg],
			[Build subpackages that support it for the "noarch" architecture (makes sense only with --enable-spec, supported by RPM from 4.6.0 forward)])],
	[WITH_NOARCH_SUBPACKAGES=$withval])
AC_ARG_ENABLE([spec],
	[AS_HELP_STRING([--enable-spec],
			[Rather than creating Makefiles, create an RPM spec file only])],
	[SPECMODE=$enableval],
	[SPECMODE=""])
AC_ARG_WITH([manual],
	[AS_HELP_STRING([--without-manual],
			[Do not include manual pages])],
	[WITH_MANUAL=$withval])
AC_ARG_WITH([prebuiltman],
	[AS_HELP_STRING([--with-prebuiltman],
			[Do not try to (re)generate man pages])],
	[WITH_PREBUILTMAN=$withval])

AC_ARG_WITH([windrbd],
	[AS_HELP_STRING([--with-windrbd],
			[Include windrbd support (requires Cygwin)])],
	[WITH_WINDRBD=$withval])
AC_SUBST(WITH_WINDRBD)

if test x"$WITH_WINDRBD" = x"yes" ; then
   if test x"$WITH_83_SUPPORT" = x"yes" ; then
      AC_MSG_WARN([DRBD 8.3 (and earlier) not supported with WinDRBD, disabling drbd 8.3 userland tools build.])
      WITH_83_SUPPORT=no
   fi
   if test x"$WITH_84_SUPPORT" = x"yes" ; then
      AC_MSG_WARN([DRBD 8.4 (and earlier) not supported with WinDRBD, disabling drbd 8.4 userland tools build.])
      WITH_84_SUPPORT=no
   fi
fi

AC_SUBST(WITH_83_SUPPORT)
AC_SUBST(WITH_84_SUPPORT)
AC_SUBST(WITH_UDEV)
AC_SUBST(WITH_XEN)
AC_SUBST(WITH_PACEMAKER)
AC_SUBST(WITH_HEARTBEAT)
AC_SUBST(WITH_RGMANAGER)
AC_SUBST(WITH_BASHCOMPLETION)
AC_SUBST(WITH_MANUAL)
AC_SUBST(WITH_PREBUILTMAN)

dnl Check for functions
AC_CHECK_FUNCS([getentropy])
AC_CHECK_FUNCS([gethostbyname_r])

dnl Do we know where systemd unit files go?
PKG_PROG_PKG_CONFIG
AC_ARG_WITH([systemdunitdir],
	[AC_HELP_STRING([--with-systemdunitdir=DIR],
        	        [Directory for systemd service files [Auto]])],
	[WITH_SYSTEMD_UNIT_DIR=$withval])
if test x"$with_systemdunitdir" = x || \
   test x"$with_systemdunitdir" = xyes ; then
	if test x"$PKG_CONFIG" != x; then
		systemdunitdir=$($PKG_CONFIG --variable=systemdsystemunitdir systemd)
	fi
	if test x"$systemdunitdir" = x; then
		AC_MSG_NOTICE([Could not detect systemd unit directory])
	fi
else
	systemdunitdir=$with_systemdunitdir
fi
AC_SUBST(systemdunitdir)
AC_MSG_RESULT([Using systemd unit directory: $systemdunitdir])

AC_ARG_WITH(tmpfilesdir,
	AC_HELP_STRING([--with-tmpfilesdir=DIR],
		[install configuration files for management of volatile files and directories in DIR [[PREFIX/lib/tmpfiles.d]]]),
		tmpfilesdir=$withval, tmpfilesdir='${prefix}/lib/tmpfiles.d')
AC_SUBST(tmpfilesdir)

# set default early
default_udevdir=/lib/udev
if test x"$with_udev" = x || \
   test x"$with_udev" = xyes ; then
	if test x"$PKG_CONFIG" != x; then
		udevdir=$($PKG_CONFIG --variable=udevdir udev)
	fi
	if test x"$udevdir" = x; then
		AC_MSG_NOTICE([Could not detect udev rules directory, using default])
		udevdir=$default_udevdir
	fi
	AC_MSG_RESULT([Using udev rules directory: $udevdir])
else
	udevdir=$default_udevdir
fi
dnl always replace, even if not used
AC_SUBST(udevdir)
udevrulesdir=$udevdir/rules.d
AC_SUBST(udevrulesdir)

AC_ARG_WITH([initscripttype],
	    [AS_HELP_STRING([--with-initscripttype=INIT_SCRIPT_TYPE],
			    [Type of init script to install (sysv|systemd|both|none). [auto]])
	    ])
case "$with_initscripttype" in
	"")
		if grep -ql systemd /sbin/init ; then
			initscripttype=systemd
		else
			initscripttype=sysv
		fi ;;
	sysv|systemd|both|none)
		initscripttype=$with_initscripttype ;;
	*)
		AC_MSG_ERROR([Illegal value -$with_initscripttype- for option --with-initscripttype]) ;;
esac
AC_SUBST(initscripttype)

AC_ARG_WITH([drbdmon],
	[AS_HELP_STRING([--without-drbdmon],
			[Do not include the DRBD resource reporting utility])],
	[WITH_DRBDMON=$withval])
AC_SUBST(WITH_DRBDMON)

dnl Checks for programs
AC_PROG_CC
AC_PROG_LN_S
AC_PATH_PROG(SED, sed)
AC_PATH_PROG(GREP, grep)
AC_PATH_PROG(FLEX, flex)
AC_PATH_PROG(RPMBUILD, rpmbuild)
AC_PATH_PROG(XSLTPROC, xsltproc)
AC_PATH_PROG(CLITEST, clitest)
AC_PATH_PROG(TAR, tar)
AC_PATH_PROG(GIT, git)
AC_PATH_PROG(PO4A_TRANSLATE, po4a-translate)
AC_PATH_PROG(PO4A_GETTEXTIZE, po4a-gettextize)
AC_PATH_PROG(DPKG_BUILDPACKAGE, dpkg-buildpackage)
AC_PATH_PROG(UDEVADM, udevadm, [false], [/sbin$PATH_SEPARATOR$PATH])
AC_PATH_PROG(UDEVINFO, udevinfo, [false], [/sbin$PATH_SEPARATOR$PATH])

if test -z "$CC"; then
   AC_MSG_ERROR([Cannot build utils without a C compiler.])
fi

if test -z $FLEX; then
   AC_MSG_ERROR([Cannot build utils without flex.])
fi

if [[ $WITH_DRBDMON == "yes" ]] ; then
   AC_PROG_CXX
   AX_CXX_COMPILE_STDCXX_11(, optional)
   dnl RCK: c++ testing is "special", CXX might be guessed to g++ even if not installed
   dnl we have to check the undocumented "$ac_ct_CXX"
   dnl https://lists.gnu.org/archive/html/bug-autoconf/2010-05/msg00001.html
   if test -z "$ac_ct_CXX" -o "$HAVE_CXX11" = "0"; then
      AC_MSG_WARN([No C++11 compiler found, disabling drbdmon build.])
      WITH_DRBDMON=no
   fi
   AC_CHECK_LIB([rt], [clock_gettime, timer_create, timer_settime, timer_delete], [],
                [AC_MSG_WARN([Could not find required librt functions, disabling drbdmon build.])
                 WITH_DRBDMON=no])
fi

if test x"$WITH_WINDRBD" = x"yes" ; then
   AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
#ifndef __CYGWIN__
# error Not a cygwin compiler
#endif
]])], [AC_MSG_RESULT([This is a cygwin compiler, which is good])], [AC_MSG_ERROR([--with-windrbd requires a Cygwin gcc for being built])])
fi

if test -z $RPMBUILD; then
   AC_MSG_WARN([No rpmbuild found, building RPM packages is disabled.])
fi

if test -z $DPKG_BUILDPACKAGE; then
   AC_MSG_WARN([No dpkg-buildpackage found, building Debian packages is disabled.])
fi

if test -z $XSLTPROC; then
   AC_MSG_WARN([Cannot build man pages without xsltproc. You may safely ignore this warning when building from a tarball.])
   dnl default to some sane value at least,
   dnl so the error message about command not found makes sense
   dnl otherwise you get "--xinclude ... command not found" :-/
   XSLTPROC=xsltproc
fi

if test -z $CLITEST; then
   AC_MSG_WARN([Cannot run tests without clitest, disabling test target.])
   WITH_CLITEST=no
else
   WITH_CLITEST=yes
fi

if test -z $PO4A_TRANSLATE; then
   AC_MSG_WARN([Cannot build man pages without po4a-translage. You may safely ignore this warning when building from a tarball.])
   dnl default to some sane value at least,
   dnl so the error message about command not found makes sense
   PO4A_TRANSLATE=po4a-translate
fi

if test -z $PO4A_GETTEXTIZE; then
   AC_MSG_WARN([Cannot build man pages without po4a-gettextize. You may safely ignore this warning when building from a tarball.])
   dnl default to some sane value at least,
   dnl so the error message about command not found makes sense
   PO4A_GETTEXTIZE=po4a-gettextize
fi

if test -z $GIT; then
   AC_MSG_WARN(Cannot update buildtag without git. You may safely ignore this warning when building from a tarball.)
fi

if test $UDEVADM = false && test $UDEVINFO = false; then
   if test "$WITH_UDEV" = "yes"; then
     AC_MSG_WARN([udev support enabled, but neither udevadm nor udevinfo found on this system.])
   fi
fi

dnl special header checks

if test "$WITH_WINDRBD" = "no"; then
AC_LANG(C)
AC_CHECK_HEADERS([linux/genetlink.h],
   [
      AC_COMPILE_IFELSE([AC_LANG_SOURCE[
         #include <linux/genetlink.h>
         static unsigned int i = CTRL_ATTR_VERSION;
         ]], [AC_DEFINE([HAVE_CTRL_ATTR_VERSION],[],[CTRL_ATTR_VERSION defined in linux/genetlink.h])]),
      AC_COMPILE_IFELSE([AC_LANG_SOURCE[
         #include <linux/genetlink.h>
         static unsigned int i = CTRL_ATTR_HDRSIZE;
         ]], [AC_DEFINE([HAVE_CTRL_ATTR_HDRSIZE],[],[CTRL_ATTR_HDRSIZE defined in linux/genetlink.h])]),
      AC_COMPILE_IFELSE([AC_LANG_SOURCE[
         #include <linux/genetlink.h>
         static unsigned int i = CTRL_ATTR_MCAST_GROUPS;
         ]], [AC_DEFINE([HAVE_CTRL_ATTR_MCAST_GROUPS],[],[CTRL_ATTR_MCAST_GROUPS defined in linux/genetlink.h])]),
      AC_COMPILE_IFELSE([AC_LANG_SOURCE[
         #include <linux/genetlink.h>
         static unsigned int i = CTRL_CMD_DELMCAST_GRP;
         ]], [AC_DEFINE([HAVE_CTRL_CMD_DELMCAST_GRP],[1],[Does genetlink provide CTRL_CMD_DELMCAST_GRP already])]),
   ],
   [AC_MSG_ERROR([Could not find linux/genetlink.h])])
fi

dnl Checks for system services

BASH_COMPLETION_SUFFIX=""
UDEV_RULE_SUFFIX=""
RPM_DIST_TAG=""
RPM_BUILDREQ_DEFAULT="gcc flex glibc-devel make"
RPM_REQ_DRBDMON=""
RPM_SUBPACKAGE_NOARCH=""
RPM_REQ_PACEMAKER=""
RPM_REQ_HEARTBEAT=""
RPM_REQ_BASH_COMPLETION=""
RPM_REQ_XEN=""
RPM_REQ_CHKCONFIG_POST=""
RPM_REQ_CHKCONFIG_PREUN=""
dnl figure out the distribution we're running on, and set some variables accordingly
if test -z $DISTRO; then
   AC_CHECK_FILE(/etc/redhat-release, [DISTRO="redhat"])
   AC_CHECK_FILE(/etc/debian_version, [DISTRO="debian"])
   AC_CHECK_FILE(/etc/SuSE-release, [DISTRO="suse"])
fi
case "$DISTRO" in
     redhat)
	test -z $INITDIR && INITDIR="$sysconfdir/rc.d/init.d"
	RPM_DIST_TAG="%{?dist}"
	dnl Fedora/Red Hat packaging guidelines mandate that packages
	dnl belonging to the "minimal build system" should not be
	dnl listed in BuildRequires
	RPM_BUILDREQ_DEFAULT="flex"
	RPM_REQ_CHKCONFIG_POST="Requires(post): chkconfig"
	RPM_REQ_CHKCONFIG_PREUN="Requires(preun): chkconfig"
	AC_MSG_NOTICE([configured for Red Hat (includes Fedora, RHEL, CentOS).])

	AC_CHECK_FILE(/etc/fedora-release, [SUB_DISTRO="fedora"], [SUB_DISTRO="RHEL"])
	if test "$SUB_DISTRO" = "fedora"; then
		# pacemaker, heartbeat and bash-completion are not available in RHEL
		# Xen: Be relaxed on RHEL (hassle free update). Be strict on Fedora
		RPM_REQ_PACEMAKER="Requires: pacemaker"
		RPM_REQ_HEARTBEAT="Requires: heartbeat"
		RPM_REQ_BASH_COMPLETION="Requires: bash-completion"
		RPM_REQ_XEN="Requires: xen"
	fi
	;;
     debian)
	AC_MSG_NOTICE([configured for Debian (includes Ubuntu).])
        ;;
     suse)
	BASH_COMPLETION_SUFFIX=".sh"
	# RPM_REQ_CHKCONFIG_POST="" chkconfig is part of aaa_base on suse
	# RPM_REQ_CHKCONFIG_PREUN="" chkconfig is part of aaa_base on suse
	AC_MSG_NOTICE([configured for SUSE (includes openSUSE, SLES).])

	RPM_REQ_BASH_COMPLETION="Requires: bash"
	# The following are disabled for hassle free updates:
	# RPM_REQ_XEN="Requires: xen"
	# RPM_REQ_PACEMAKER="Requires: pacemaker"
	# RPM_REQ_HEARTBEAT="Requires: heartbeat"

	# Unfortunately gcc on SLES9 is broken with -O2. Works with -O1
	if grep -q 'VERSION = 9' /etc/SuSE-release; then
		CFLAGS="-g -O1"
	fi
	;;
     "")
	AC_MSG_WARN([Unable to determine what distribution we are running on. Distribution-specific features will be disabled.])
	;;
esac
dnl INITDIR may be set with --with-initdir, or set in the distro
dnl detection magic above. If unset down to here, use a sensible
dnl default.
test -z $INITDIR && INITDIR="$sysconfdir/init.d"

dnl Our udev rules file is known to work only with udev >= 85
if test "$WITH_UDEV" = "yes"; then
   udev_version=`$UDEVADM version 2>/dev/null` || udev_version=`$UDEVINFO -V | cut -d " " -f 3`
   if test -z $udev_version || test $udev_version -lt 85; then
      UDEV_RULE_SUFFIX=".disabled"
      AC_MSG_WARN([Obsolete or unknown udev version. Installing disabled udev rules.])
   fi
fi

dnl Our sub-packages can be built for noarch, but RPM only supports
dnl this from version 4.6.0 forward
if test "$WITH_NOARCH_SUBPACKAGES" = "yes"; then
   RPM_SUBPACKAGE_NOARCH="BuildArch: noarch"
fi
AC_SUBST(DISTRO)
AC_SUBST(INITDIR)
AC_SUBST(BASH_COMPLETION_SUFFIX)
AC_SUBST(UDEV_RULE_SUFFIX)
AC_SUBST(RPM_DIST_TAG)
AC_SUBST(RPM_BUILDREQ_DEFAULT)
AC_SUBST(RPM_SUBPACKAGE_NOARCH)
AC_SUBST(RPM_REQ_PACEMAKER)
AC_SUBST(RPM_REQ_HEARTBEAT)
AC_SUBST(RPM_REQ_BASH_COMPLETION)
AC_SUBST(RPM_REQ_XEN)
AC_SUBST(RPM_REQ_CHKCONFIG_POST)
AC_SUBST(RPM_REQ_CHKCONFIG_PREUN)
AC_SUBST(WITH_CLITEST)

AH_TEMPLATE(DRBD_LIB_DIR, [Local state directory. Commonly
			  /var/lib/drbd or /usr/local/var/lib/drbd])
AH_TEMPLATE(DRBD_RUN_DIR, [Runtime state directory. Commonly
			  /var/run/drbd or /usr/local/var/run/drbd])
AH_TEMPLATE(DRBD_LOCK_DIR, [Local lock directory. Commonly
			   /var/lock or
			   /usr/local/var/lock])
AH_TEMPLATE(DRBD_CONFIG_DIR, [Local configuration directory. Commonly
			   /etc or
			   /usr/local/etc])
AH_TEMPLATE(DRBD_BIN_DIR, [Installation directory. Commonly
			   /usr/bin])
AH_TEMPLATE(DRBD_LEGACY_83, [Include support for drbd-8.3 kernel code])
AH_TEMPLATE(DRBD_LEGACY_84, [Include support for drbd-8.4 kernel code])
AH_TEMPLATE(WINDRBD, [Include support for windrbd (do not use this under Linux)])

AC_SUBST(DRBD_LIB_DIR, [$localstatedir/lib/$PACKAGE_TARNAME])
AC_SUBST(DRBD_RUN_DIR, [$localstatedir/run/$PACKAGE_TARNAME])
AC_SUBST(DRBD_LOCK_DIR, [$localstatedir/lock])
AC_SUBST(DRBD_CONFIG_DIR, [$sysconfdir])
AC_SUBST(DRBD_BIN_DIR, [$exec_prefix/bin])

AC_DEFINE_UNQUOTED(DRBD_LIB_DIR, ["$DRBD_LIB_DIR"])
AC_DEFINE_UNQUOTED(DRBD_RUN_DIR, ["$DRBD_RUN_DIR"])
AC_DEFINE_UNQUOTED(DRBD_LOCK_DIR, ["$DRBD_LOCK_DIR"])
AC_DEFINE_UNQUOTED(DRBD_CONFIG_DIR, ["$DRBD_CONFIG_DIR"])
AC_DEFINE_UNQUOTED(DRBD_BIN_DIR, ["$DRBD_BIN_DIR"])

if test "$WITH_83_SUPPORT" = "yes"; then
   AC_DEFINE(DRBD_LEGACY_83, [1])
fi

if test "$WITH_84_SUPPORT" = "yes"; then
   AC_DEFINE(DRBD_LEGACY_84, [1])
fi

if test "$WITH_WINDRBD" = "yes"; then
   AC_DEFINE(WINDRBD, [1])
fi

dnl The configuration files we create (from their .in template)
if test -z $SPECMODE; then
   AC_CONFIG_FILES(Makefile user/shared/Makefile user/v9/Makefile user/v83/Makefile user/v84/Makefile scripts/Makefile documentation/v83/Makefile scripts/drbd.rules user/windrbd/Makefile user/drbdmon/Makefile documentation/common/Makefile_v84_com documentation/common/Makefile_v9_com)
   AC_CONFIG_HEADERS(user/shared/config.h)
else
   AC_CONFIG_FILES(drbd.spec)
fi

dnl output
AC_OUTPUT
