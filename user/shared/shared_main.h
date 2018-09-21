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

#include <netinet/in.h>		/* for IFNAMSIZ */
#include <stdio.h>		/* for NULL */

#define CMD_TIMEOUT_SHORT_DEF 5
#define CMD_TIMEOUT_MEDIUM_DEF 121
#define CMD_TIMEOUT_LONG_DEF 600
extern struct d_globals global_options;

void alarm_handler(int __attribute((unused)) signo);
void chld_sig_hand(int __attribute((unused)) unused);

unsigned minor_by_id(const char *id);

void substitute_deprecated_cmd(char **c, char *deprecated,
				      char *substitution);

struct ifreq *get_ifreq(void);
int have_ip(const char *af, const char *ip);
int have_ip_ipv4(const char *ip);
int have_ip_ipv6(const char *ip);

const char *drbd_buildtag(void);

#define E_USAGE		  1
#define E_SYNTAX	  2
#define E_CONFIG_INVALID 10
#define E_EXEC_ERROR     20
#define E_THINKO	 42 /* :) */

enum {
	SLEEPS_FINITE        = 1,
	SLEEPS_SHORT         = 2+1,
	SLEEPS_LONG          = 4+1,
	SLEEPS_VERY_LONG     = 8+1,
	SLEEPS_MASK          = 15,

	RETURN_PID           = 2,
	SLEEPS_FOREVER       = 4,

	SUPRESS_STDERR       = 0x10,
	RETURN_STDOUT_FD     = 0x20,
	RETURN_STDERR_FD     = 0x40,
	DONT_REPORT_FAILED   = 0x80,
};

/* for check_uniq(): Check for uniqueness of certain values...
 * comment out if you want to NOT choke on the first conflict */
#define EXIT_ON_CONFLICT 1

/* for verify_ips(): are not verifyable ips fatal? */
#define INVALID_IP_IS_INVALID_CONF 1

enum usage_count_type {
	UC_YES,
	UC_NO,
	UC_ASK,
};

enum pp_flags {
	MATCH_ON_PROXY = 1,
	DRBDSETUP_SHOW = 2,
};

struct d_globals
{
	unsigned int cmd_timeout_short;
	unsigned int cmd_timeout_medium;
	unsigned int cmd_timeout_long;
	int disable_ip_verification;
	int udev_always_symlink_vnr;
	int minor_count;
	int dialog_refresh;
	enum usage_count_type usage_count;
};

#define IFI_HADDR 8
#define IFI_ALIAS 1

struct ifi_info {
	char ifi_name[IFNAMSIZ];      /* interface name, nul terminated */
	uint8_t ifi_haddr[IFI_HADDR]; /* hardware address */
	uint16_t ifi_hlen;            /* bytes in hardware address, 0, 6, 8 */
	short ifi_flags;              /* IFF_xxx constants from <net/if.h> */
	short ifi_myflags;            /* our own IFI_xxx flags */
	struct sockaddr *ifi_addr;    /* primary address */
	struct ifi_info *ifi_next;    /* next ifi_info structure */
};

extern int dry_run;
extern int verbose;
extern int adjust_with_progress;
extern char *sh_varname;

extern void m__system(char **argv, int flags, const char *res_name, pid_t *kid, int *fd, int *ex);
static inline int m_system_ex(char **argv, int flags, const char *res_name)
{
	int ex = -1;
	m__system(argv, flags, res_name, NULL, NULL, &ex);
	return ex;
}

#endif
