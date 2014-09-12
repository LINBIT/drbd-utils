/*
   shared_main.h

   This file is part of DRBD by Philipp Reisner and Lars Ellenberg.

   Copyright (C) 2014, LINBIT HA Solutions GmbH.

   drbd is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   drbd is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with drbd; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

 */

#ifndef __SHARED_MAIN_H__
#define __SHARED_MAIN_H__

void alarm_handler(int __attribute((unused)) signo);
void chld_sig_hand(int __attribute((unused)) unused);

unsigned minor_by_id(const char *id);

void substitute_deprecated_cmd(char **c, char *deprecated,
				      char *substitution);

struct ifreq *get_ifreq(void);
int have_ip(const char *af, const char *ip);
int have_ip_ipv4(const char *ip);
int have_ip_ipv6(const char *ip);

#endif
