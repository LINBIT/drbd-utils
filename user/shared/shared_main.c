/*
   shared_main.c

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

#define _GNU_SOURCE
#define _XOPEN_SOURCE 600
#define _FILE_OFFSET_BITS 64

#include "config.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <search.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/sockios.h>
#include <linux/netdevice.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <time.h>

#include "drbd_endian.h"
#include "shared_main.h"
#include "shared_tool.h"

extern struct ifreq *ifreq_list;

struct d_globals global_options = {

	.cmd_timeout_short = CMD_TIMEOUT_SHORT_DEF,
	.cmd_timeout_medium = CMD_TIMEOUT_MEDIUM_DEF,
	.cmd_timeout_medium = CMD_TIMEOUT_LONG_DEF,

	.dialog_refresh = 1,
	.usage_count = UC_ASK,
};

void chld_sig_hand(int __attribute((unused)) unused)
{
	// do nothing. But interrupt systemcalls :)
}

unsigned minor_by_id(const char *id)
{
	if (strncmp(id, "minor-", 6))
		return -1U;
	return m_strtoll(id + 6, 1);
}

/*
 * I'd really rather parse the output of
 *   ip -o a s
 * once, and be done.
 * But anyways....
 */

struct ifreq *get_ifreq(void)
{
	int sockfd, num_ifaces;
	struct ifreq *ifr;
	struct ifconf ifc;
	size_t buf_size;

	if (0 > (sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))) {
		perror("Cannot open socket");
		exit(EXIT_FAILURE);
	}

	num_ifaces = 0;
	ifc.ifc_req = NULL;

	/* realloc buffer size until no overflow occurs  */
	do {
		num_ifaces += 16;	/* initial guess and increment */
		buf_size = ++num_ifaces * sizeof(struct ifreq);
		ifc.ifc_len = buf_size;
		if (NULL == (ifc.ifc_req = realloc(ifc.ifc_req, ifc.ifc_len))) {
			fprintf(stderr, "Out of memory.\n");
			return NULL;
		}
		if (ioctl(sockfd, SIOCGIFCONF, &ifc)) {
			perror("ioctl SIOCFIFCONF");
			free(ifc.ifc_req);
			return NULL;
		}
	} while (buf_size <= (size_t) ifc.ifc_len);

	num_ifaces = ifc.ifc_len / sizeof(struct ifreq);
	/* Since we allocated at least one more than necessary,
	 * this serves as a stop marker for the iteration in
	 * have_ip() */
	ifc.ifc_req[num_ifaces].ifr_name[0] = 0;
	for (ifr = ifc.ifc_req; ifr->ifr_name[0] != 0; ifr++) {
		/* we only want to look up the presence or absence of a certain address
		 * here. but we want to skip "down" interfaces.  if an interface is down,
		 * we store an invalid sa_family, so the lookup will skip it.
		 */
		struct ifreq ifr_for_flags = *ifr;	/* get a copy to work with */
		if (ioctl(sockfd, SIOCGIFFLAGS, &ifr_for_flags) < 0) {
			perror("ioctl SIOCGIFFLAGS");
			ifr->ifr_addr.sa_family = -1;	/* what's wrong here? anyways: skip */
			continue;
		}
		if (!(ifr_for_flags.ifr_flags & IFF_UP)) {
			ifr->ifr_addr.sa_family = -1;	/* is not up: skip */
			continue;
		}
	}
	close(sockfd);
	return ifc.ifc_req;
}

int have_ip_ipv4(const char *ip)
{
	struct ifreq *ifr;
	struct in_addr query_addr;

	query_addr.s_addr = inet_addr(ip);

	if (!ifreq_list)
		ifreq_list = get_ifreq();

	for (ifr = ifreq_list; ifr && ifr->ifr_name[0] != 0; ifr++) {
		/* SIOCGIFCONF only supports AF_INET */
		struct sockaddr_in *list_addr =
		    (struct sockaddr_in *)&ifr->ifr_addr;
		if (ifr->ifr_addr.sa_family != AF_INET)
			continue;
		if (query_addr.s_addr == list_addr->sin_addr.s_addr)
			return 1;
	}
	return 0;
}

int have_ip_ipv6(const char *ip)
{
	FILE *if_inet6;
	struct in6_addr addr6, query_addr;
	unsigned int b[4];
	char tmp_ip[INET6_ADDRSTRLEN+1];
	char name[20]; /* IFNAMSIZ aka IF_NAMESIZE is 16 */
	int i;

	/* don't want to do getaddrinfo lookup, but inet_pton get's confused by
	 * %eth0 link local scope specifiers. So we have a temporary copy
	 * without that part. */
	for (i=0; ip[i] && ip[i] != '%' && i < INET6_ADDRSTRLEN; i++)
		tmp_ip[i] = ip[i];
	tmp_ip[i] = 0;

	if (inet_pton(AF_INET6, tmp_ip, &query_addr) <= 0)
		return 0;

#define PROC_IF_INET6 "/proc/net/if_inet6"
	if_inet6 = fopen(PROC_IF_INET6, "r");
	if (!if_inet6) {
		if (errno != ENOENT)
			perror("open of " PROC_IF_INET6 " failed:");
#undef PROC_IF_INET6
		return 0;
	}

	while (fscanf
	       (if_inet6,
		X32(08) X32(08) X32(08) X32(08) " %*x %*x %*x %*x %s",
		b, b + 1, b + 2, b + 3, name) > 0) {
		for (i = 0; i < 4; i++)
			addr6.s6_addr32[i] = cpu_to_be32(b[i]);

		if (memcmp(&query_addr, &addr6, sizeof(struct in6_addr)) == 0) {
			fclose(if_inet6);
			return 1;
		}
	}
	fclose(if_inet6);
	return 0;
}

int have_ip(const char *af, const char *ip)
{
	if (!strcmp(af, "ipv4"))
		return have_ip_ipv4(ip);
	else if (!strcmp(af, "ipv6"))
		return have_ip_ipv6(ip);

	return 1;		/* SCI */
}

extern char *progname;
void substitute_deprecated_cmd(char **c, char *deprecated,
				      char *substitution)
{
	if (!strcmp(*c, deprecated)) {
		fprintf(stderr, "'%s %s' is deprecated, use '%s %s' instead.\n",
			progname, deprecated, progname, substitution);
		*c = substitution;
	}
}

pid_t my_fork(void)
{
	pid_t pid = -1;
	int try;
	for (try = 0; try < 10; try++) {
		errno = 0;
		pid = fork();
		if (pid != -1 || errno != EAGAIN)
			return pid;
		err("fork: retry: Resource temporarily unavailable\n");
		usleep(100 * 1000);
	}
	return pid;
}

void m__system(char **argv, int flags, const char *res_name, pid_t *kid, int *fd, int *ex)
{
	pid_t pid;
	int status, rv = -1;
	int timeout = 0;
	char **cmdline = argv;
	int pipe_fds[2];

	struct sigaction so;
	struct sigaction sa;

	if (flags & (RETURN_STDERR_FD | RETURN_STDOUT_FD))
		assert(fd);

	sa.sa_handler = &alarm_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	if (dry_run || verbose) {
		if (sh_varname && *cmdline)
			printf("%s=%s\n", sh_varname,
					res_name ? shell_escape(res_name) : "");
		while (*cmdline) {
			printf("%s", shell_escape(*cmdline++));
			if (*cmdline) putchar(' ');
		}
		printf("\n");
		if (dry_run) {
			if (kid)
				*kid = -1;
			if (fd)
				*fd = -1;
			if (ex)
				*ex = 0;
			return;
		}
	}

	/* flush stdout and stderr, so output of drbdadm
	 * and helper binaries is reported in order! */
	fflush(stdout);
	fflush(stderr);

	if (adjust_with_progress && !(flags & RETURN_STDERR_FD))
		flags |= SUPRESS_STDERR;

	/* create the pipe in any case:
	 * it helps the analyzer and later we have:
	 * '*fd = pipe_fds[0];' */
	if (pipe(pipe_fds) < 0) {
		perror("pipe");
		fprintf(stderr, "Error in pipe, giving up.\n");
		exit(E_EXEC_ERROR);
	}

	pid = my_fork();
	if (pid == -1) {
		fprintf(stderr, "Can not fork\n");
		exit(E_EXEC_ERROR);
	}
	if (pid == 0) {
		prctl(PR_SET_PDEATHSIG, SIGKILL);
		/* Child: close reading end. */
		close(pipe_fds[0]);
		if (flags & RETURN_STDOUT_FD) {
			dup2(pipe_fds[1], STDOUT_FILENO);
		}
		if (flags & RETURN_STDERR_FD) {
			dup2(pipe_fds[1], STDERR_FILENO);
		}
		close(pipe_fds[1]);

		if (flags & SUPRESS_STDERR) {
			FILE *f = freopen("/dev/null", "w", stderr);
			if (!f)
				fprintf(stderr, "freopen(/dev/null) failed\n");
		}
		if (argv[0])
			execvp(argv[0], argv);
		fprintf(stderr, "Can not exec\n");
		exit(E_EXEC_ERROR);
	}

	/* Parent process: close writing end. */
	close(pipe_fds[1]);

	if (flags & SLEEPS_FINITE) {
		sigaction(SIGALRM, &sa, &so);
		alarm_raised = 0;
		switch (flags & SLEEPS_MASK) {
		case SLEEPS_SHORT:
			timeout = global_options.cmd_timeout_short;
			break;
		case SLEEPS_LONG:
			timeout = global_options.cmd_timeout_medium;
			break;
		case SLEEPS_VERY_LONG:
			timeout = global_options.cmd_timeout_long;
			break;
		default:
			fprintf(stderr, "logic bug in %s:%d\n", __FILE__,
				__LINE__);
			exit(E_THINKO);
		}
		alarm(timeout);
	}

	if (kid)
		*kid = pid;

	if (flags & (RETURN_STDOUT_FD | RETURN_STDERR_FD)
			||  flags == RETURN_PID) {
		if (fd)
			*fd = pipe_fds[0];

		return;
	}

	while (1) {
		if (waitpid(pid, &status, 0) == -1) {
			if (errno != EINTR)
				break;
			if (alarm_raised) {
				alarm(0);
				sigaction(SIGALRM, &so, NULL);
				rv = 0x100;
				break;
			} else {
				fprintf(stderr, "logic bug in %s:%d\n",
					__FILE__, __LINE__);
				exit(E_EXEC_ERROR);
			}
		} else {
			if (WIFEXITED(status)) {
				rv = WEXITSTATUS(status);
				break;
			}
		}
	}

	/* Do not close earlier, else the child gets EPIPE. */
	close(pipe_fds[0]);

	if (flags & SLEEPS_FINITE) {
		if (rv >= 10
		    && !(flags & (DONT_REPORT_FAILED | SUPRESS_STDERR))) {
			fprintf(stderr, "Command '");
			for (cmdline = argv; *cmdline; cmdline++) {
				fprintf(stderr, "%s", *cmdline);
				if (cmdline[1])
					fputc(' ', stderr);
			}
			if (alarm_raised) {
				fprintf(stderr,
					"' did not terminate within %u seconds\n",
					timeout);
				exit(E_EXEC_ERROR);
			} else {
				fprintf(stderr,
					"' terminated with exit code %d\n", rv);
			}
		}
	}
	fflush(stdout);
	fflush(stderr);

	if (ex)
		*ex = rv;
}
