# Makefile in documentation directory
#
# This file is part of DRBD by Philipp Reisner and Lars Ellenberg.
#
# drbd is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# drbd is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with drbd; see the file COPYING.  If not, write to
# the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

include ../../common/Makefile_v9_com

#
# WITH_DRBDMON is defined in Makefile_v9_com so need to be checked
# after Makefile_v9_com is included. The others are referenced in
# Makefile_v9_com_post.
#
TOPDIR = ../../..
MANLANG = ja

MANPAGES	= drbdsetup.8 drbd.conf.5 drbd.8 drbdadm.8 drbdmeta.8
ifeq ($(WITH_DRBDMON), yes)
MANPAGES	+= drbdmon.8
endif

FILES_IN_GIT_ADD  = drbd.conf.xml.in.po drbd.xml.po \
	drbdadm.xml.po drbdmeta.xml.po drbdsetup-options.xml.po \
	drbdsetup.xml.in.po drbdmon.xml.po

FILES_REMOVE_ADD = *.xml *.xml.in

include ../../common/Makefile_v9_com_post
