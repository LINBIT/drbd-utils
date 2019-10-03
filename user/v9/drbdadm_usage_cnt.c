/*
   drbdadm_usage_cnt.c

   This file is part of DRBD by Philipp Reisner and Lars Ellenberg.

   Copyright (C) 2006-2008, LINBIT Information Technologies GmbH
   Copyright (C) 2006-2008, Philipp Reisner <philipp.reisner@linbit.com>
   Copyright (C) 2006-2008, Lars Ellenberg  <lars.ellenberg@linbit.com>

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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <setjmp.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#include "config.h"
#include "drbdadm.h"
#include "drbdtool_common.h"
#include "drbd_endian.h"
#include "linux/drbd.h"		/* only use DRBD_MAGIC from here! */
#include "config_flags.h"

#define HTTP_PORT 80
#define HTTP_HOST "usage.drbd.org"
#define HTTP_ADDR "159.69.154.96"
#define NODE_ID_FILE DRBD_LIB_DIR"/node_id"


struct node_info {
	uint64_t node_uuid;
	struct version rev;
};

struct node_info_od {
	uint32_t magic;
	struct node_info ni;
} __packed;



void maybe_exec_legacy_drbdadm(char **argv)
{
	const struct version *driver_version = drbd_driver_version(FALLBACK_TO_UTILS);

	if (driver_version->version.major == 8 &&
	    driver_version->version.minor == 3) {
#ifdef DRBD_LEGACY_83
		/* This drbdadm warned already... */
		setenv("DRBD_DONT_WARN_ON_VERSION_MISMATCH", "1", 0);
		add_lib_drbd_to_path();
		execvp(drbdadm_83, argv);
		err("execvp() failed to exec %s: %m\n", drbdadm_83);
#else
		config_help_legacy("drbdadm", driver_version);
#endif
		exit(E_EXEC_ERROR);
	}
	if (driver_version->version.major == 8 &&
	    driver_version->version.minor == 4) {
#ifdef DRBD_LEGACY_84
		/* This drbdadm warned already... */
		setenv("DRBD_DONT_WARN_ON_VERSION_MISMATCH", "1", 0);
		add_lib_drbd_to_path();
		execvp(drbdadm_84, argv);
		err("execvp() failed to exec %s: %m\n", drbdadm_84);
#else
		config_help_legacy("drbdadm", driver_version);
#endif
		exit(E_EXEC_ERROR);
	}
}

static char *vcs_to_str(struct version *rev)
{
	static char buffer[80]; // Not generic, sufficient for the purpose.

	if( rev->svn_revision ) {
		snprintf(buffer,80,"nv="U32,rev->svn_revision);
	} else {
		int len=20,p;
		unsigned char *bytes;

		p = sprintf(buffer,"git=");
		bytes = (unsigned char*)rev->git_hash;
		while(len--) p += sprintf(buffer+p,"%02x",*bytes++);
	}
	return buffer;
}

static void write_node_id(struct node_info *ni)
{
	int fd;
	struct node_info_od on_disk;
	int size;

	fd = open(NODE_ID_FILE,O_WRONLY|O_CREAT,S_IRUSR|S_IWUSR);
	if( fd == -1 && errno == ENOENT) {
		mkdir(DRBD_LIB_DIR,S_IRWXU);
		fd = open(NODE_ID_FILE,O_WRONLY|O_CREAT,S_IRUSR|S_IWUSR);
	}

	if( fd == -1) {
		err("Creation of "NODE_ID_FILE" failed: %m\n");
		exit(20);
	}

	if(ni->rev.svn_revision != 0) { // SVN style (old)
		on_disk.magic               = cpu_to_be32(DRBD_MAGIC);
		on_disk.ni.node_uuid        = cpu_to_be64(ni->node_uuid);
		on_disk.ni.rev.svn_revision = cpu_to_be32(ni->rev.svn_revision);
		memset(on_disk.ni.rev.git_hash,0,GIT_HASH_BYTE);
		size = SVN_STYLE_OD;
	} else {
		on_disk.magic               = cpu_to_be32(DRBD_MAGIC+1);
		on_disk.ni.node_uuid        = cpu_to_be64(ni->node_uuid);
		on_disk.ni.rev.svn_revision = 0;
		memcpy(on_disk.ni.rev.git_hash,ni->rev.git_hash,GIT_HASH_BYTE);
		size = sizeof(on_disk);
	}

	if( write(fd,&on_disk, size) != size) {
		err("Write to "NODE_ID_FILE" failed: %m\n");
		exit(20);
	}

	close(fd);
}


static int read_node_id(struct node_info *ni)
{
	int rr;
	int fd;
	struct node_info_od on_disk;

	fd = open(NODE_ID_FILE, O_RDONLY);
	if (fd == -1) {
		return 0;
	}

	rr = read(fd, &on_disk, sizeof(on_disk));
	if (rr != sizeof(on_disk) && rr != SVN_STYLE_OD) {
		close(fd);
		return 0;
	}

	switch (be32_to_cpu(on_disk.magic)) {
	case DRBD_MAGIC:
		ni->node_uuid = be64_to_cpu(on_disk.ni.node_uuid);
		ni->rev.svn_revision = be32_to_cpu(on_disk.ni.rev.svn_revision);
		memset(ni->rev.git_hash, 0, GIT_HASH_BYTE);
		break;
	case DRBD_MAGIC+1:
		ni->node_uuid = be64_to_cpu(on_disk.ni.node_uuid);
		ni->rev.svn_revision = 0;
		memcpy(ni->rev.git_hash, on_disk.ni.rev.git_hash, GIT_HASH_BYTE);
		break;
	default:
		return 0;
	}

	close(fd);
	return 1;
}

/* What we probably should do is use getaddrinfo_a(),
 * instead of alarm() and siglongjump limited gethostbyname(),
 * but I don't like implicit threads. */
/* to interrupt gethostbyname,
 * we not only need a signal,
 * but also the long jump:
 * gethostbyname would otherwise just restart the syscall
 * and timeout again. */
static sigjmp_buf timed_out;
static void gethostbyname_timeout(int __attribute((unused)) signo)
{
	siglongjmp(timed_out, 1);
}

#define DNS_TIMEOUT 3	/* seconds */
#define SOCKET_TIMEOUT 3 /* seconds */
struct hostent *my_gethostbyname(const char *name)
{
	struct sigaction sa;
	struct sigaction so;
	struct hostent *h;
	static int failed_once_already = 0;

	if (failed_once_already)
		return NULL;

	alarm(0);
	sa.sa_handler = &gethostbyname_timeout;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_NODEFER;

	sigaction(SIGALRM, &sa, &so);

	if (!sigsetjmp(timed_out, 1)) {
#ifdef HAVE_GETHOSTBYNAME_R
		struct hostent ret;
		char buf[2048];
		int my_h_errno;
#endif

		alarm(DNS_TIMEOUT);
		/* h = gethostbyname(name);
		 * If the resolver is unresponsive,
		 * we may siglongjmp out of a "critical section" of gethostbyname,
		 * still holding some glibc internal lock.
		 * Any later attempt to call gethostbyname() would then deadlock
		 * (last syscall would be futex(...))
		 *
		 * gethostbyname_r() apparently does not use any internal locks.
		 * Even if unnecessary in our case, it feels less dirty.
		 */
#ifdef HAVE_GETHOSTBYNAME_R
		gethostbyname_r(name, &ret, buf, sizeof(buf), &h, &my_h_errno);
#else
		h = gethostbyname(name);
#endif
	} else {
		/* timed out, longjmp of SIGALRM jumped here */
		h = NULL;
	}

	alarm(0);
	sigaction(SIGALRM, &so, NULL);

	if (h == NULL)
		failed_once_already = 1;
	return h;
}

/**
 * insert_usage_with_socket:
 *
 * Return codes:
 *
 * 0 - success
 * 1 - failed to create socket
 * 2 - unknown server
 * 3 - cannot connect to server
 * 5 - other error
 */
static int make_get_request(char *uri) {
	struct sockaddr_in server;
	struct hostent *host_info;
	unsigned long addr;
	int sock;
	char *req_buf;
	char *http_host = HTTP_HOST;
	int buf_len = 1024;
	char buffer[buf_len];
	FILE *sockfd;
	int writeit;
	struct timeval timeout = { .tv_sec = SOCKET_TIMEOUT };
	struct utsname nodeinfo;

	sock = socket( PF_INET, SOCK_STREAM, 0);
	if (sock < 0)
		return 1;

	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
	setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

	memset (&server, 0, sizeof(server));

	/* convert host name to ip */
	host_info = my_gethostbyname(http_host);
	if (host_info == NULL) {
		/* unknown host, try with ip */
		if ((addr = inet_addr( HTTP_ADDR )) != INADDR_NONE)
			memcpy((char *)&server.sin_addr, &addr, sizeof(addr));
		else {
			close(sock);
			return 2;
		}
	} else {
		memcpy((char *)&server.sin_addr, host_info->h_addr,
			host_info->h_length);
	}

	uname(&nodeinfo);
	req_buf = ssprintf("GET %s HTTP/1.0\r\n"
			   "Host: "HTTP_HOST"\r\n"
			   "User-Agent: drbdadm/"PACKAGE_VERSION" (%s; %s; %s; %s)\r\n"
			   "\r\n",
			   uri,
			   nodeinfo.sysname, nodeinfo.release,
			   nodeinfo.version, nodeinfo.machine);

	server.sin_family = AF_INET;
	server.sin_port = htons(HTTP_PORT);

	if (connect(sock, (struct sockaddr*)&server, sizeof(server))<0) {
		/* cannot connect to server */
		close(sock);
		return 3;
	}

	if ((sockfd = fdopen(sock, "r+")) == NULL) {
		close(sock);
		return 5;
	}

	if (fputs(req_buf, sockfd) == EOF) {
		fclose(sockfd);
		close(sock);
		return 5;
	}

	writeit = 0;
	while (fgets(buffer, buf_len, sockfd) != NULL) {
		/* ignore http headers */
		if (writeit == 0) {
			if (buffer[0] == '\r' || buffer[0] == '\n')
				writeit = 1;
		} else {
			err("%s", buffer);
		}
	}
	fclose(sockfd);
	close(sock);
	return 0;
}

static void url_encode(char *in, char *out)
{
	char *h = "0123456789abcdef";
	unsigned char c;

	while ((c = *in++) != 0) {
		if (c == '\n')
			break;
		if (('a' <= c && c <= 'z') ||
		    ('A' <= c && c <= 'Z') ||
		    ('0' <= c && c <= '9') ||
		    c == '-' || c == '_' || c == '.')
			*out++ = c;
		else if (c == ' ')
			*out++ = '+';
		else {
			*out++ = '%';
			*out++ = h[c >> 4];
			*out++ = h[c & 0x0f];
		}
	}
	*out = 0;
}

/* returns 1, if either svn_revision or git_hash are known,
 * returns 0 otherwise.  */
int have_vcs_hash(const struct version *v)
{
	int i;
	if (v->svn_revision)
		return 1;
	for (i = 0; i < GIT_HASH_BYTE; i++)
		if (v->git_hash[i])
			return 1;
	return 0;
}

/* Ensure that the node is counted on http://usage.drbd.org
 */
#define ANSWER_SIZE 80

void uc_node(enum usage_count_type type)
{
	struct node_info ni;
	char *uri;
	int update = 0;
	char answer[ANSWER_SIZE];
	char n_comment[ANSWER_SIZE*3];
	char *r;
	const struct version *driver_version = drbd_driver_version(STRICT);

	if( type == UC_NO ) return;
	if( getuid() != 0 ) return;

	/* not when running directly from init,
	 * or if stdout is no tty and type is ask.
	 * you do not want to have the "user information message"
	 * as output from `drbdadm sh-resources all`
	 */
	if (getenv("INIT_VERSION")) return;
	if (no_tty && type == UC_ASK) return;

	/* If we don't know the current version control system hash,
	 * we found the version via "modprobe -F version drbd",
	 * and did not find a /proc/drbd to read it from.
	 * Avoid flipping between "hash-some-value" and "hash-all-zero",
	 * Re-registering every time...
	 */
	if (!driver_version || !have_vcs_hash(driver_version))
		return;

	memset(&ni, 0, sizeof(ni));

	if( ! read_node_id(&ni) ) {
		get_random_bytes(&ni.node_uuid,sizeof(ni.node_uuid));
		ni.rev = *driver_version;
	} else if (driver_version && !version_equal(&ni.rev, driver_version)) {
		ni.rev = *driver_version;
		update = 1;
	} else return;

	n_comment[0]=0;
	if (type == UC_ASK ) {
		err("\n"
		    "\t\t--== This is %s of DRBD ==--\n"
		    "Please take part in the global DRBD usage count at http://"HTTP_HOST".\n\n"
		    "The counter works anonymously. It creates a random number to identify\n"
		    "your machine and sends that random number, along with the kernel and\n"
		    "DRBD version, to "HTTP_HOST".\n\n"
		    "The benefits for you are:\n"
		    " * In response to your submission, the server ("HTTP_HOST") will tell you\n"
		    "   how many users before you have installed this version (%s).\n"
		    " * With a high counter LINBIT has a strong motivation to\n"
		    "   continue funding DRBD's development.\n\n"
		    "http://"HTTP_HOST"/cgi-bin/insert_usage.pl?nu="U64"&%s\n\n"
		    "In case you want to participate but know that this machine is firewalled,\n"
		    "simply issue the query string with your favorite web browser or wget.\n"
		    "You can control all of this by setting 'usage-count' in your global_common.conf.\n\n"
		    "* You may enter a free form comment about your machine, that gets\n"
		    "  used on "HTTP_HOST" instead of the big random number.\n"
		    "* If you wish to opt out entirely, simply enter 'no'.\n"
		    "* To count this node without comment, just press [RETURN]\n",
		    update ? "an update" : "a new installation",
		    PACKAGE_VERSION, ni.node_uuid, vcs_to_str(&ni.rev));
		r = fgets(answer, ANSWER_SIZE, stdin);
		if(r && !strcmp(answer,"no\n")) return;
		url_encode(answer,n_comment);
	}

	uri = ssprintf("http://"HTTP_HOST"/cgi-bin/insert_usage.pl?nu="U64"&%s%s%s",
		       ni.node_uuid, vcs_to_str(&ni.rev),
		       n_comment[0] ? "&nc=" : "", n_comment);

	write_node_id(&ni);

	err("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
			"  --==  Thank you for participating in the global usage survey  ==--\n"
			"The server's response is:\n\n");
	make_get_request(uri);
	if (type == UC_ASK) {
		err("\n"
				"From now on, drbdadm will contact "HTTP_HOST" only when you update\n"
				"DRBD or when you use 'drbdadm create-md'. Of course it will continue\n"
				"to ask you for confirmation as long as 'usage-count' is set to 'ask'\n\n"
				"Just press [RETURN] to continue: ");
		if (fgets(answer, 9, stdin) == NULL)
			err("Could not read answer\n");
	}
}

/* For our purpose (finding the revision) SLURP_SIZE is always enough.
 */
static char* run_adm_drbdmeta(const struct cfg_ctx *ctx, const char *arg_override)
{
	const int SLURP_SIZE = 4096;
	int rr,pipes[2];
	char* buffer;
	pid_t pid;

	buffer = malloc(SLURP_SIZE);
	if(!buffer) return 0;

	if(pipe(pipes)) return 0;

	pid = fork();
	if(pid == -1) {
		err("Can not fork\n");
		exit(E_EXEC_ERROR);
	}
	if(pid == 0) {
		struct adm_cmd local_cmd = *ctx->cmd;
		struct cfg_ctx local_ctx = *ctx;
		// child
		close(pipes[0]); // close reading end
		dup2(pipes[1],1); // 1 = stdout
		close(pipes[1]);
		local_cmd.name = arg_override;
		local_ctx.cmd = &local_cmd;
		rr = _adm_drbdmeta(&local_ctx,
				   SLEEPS_VERY_LONG|
				   DONT_REPORT_FAILED,
				   NULL);
		exit(rr);
	}
	close(pipes[1]); // close writing end

	rr = read(pipes[0], buffer, SLURP_SIZE-1);
	if( rr == -1) {
		free(buffer);
		// FIXME cleanup
		return 0;
	}
	buffer[rr]=0;
	close(pipes[0]);

	waitpid(pid,0,0);

	return buffer;
}

struct d_name *find_backend_option(const char *opt_name)
{
	struct d_name *b_opt;
	const int str_len = strlen(opt_name);

	STAILQ_FOREACH(b_opt, &backend_options, link) {
		if (!strncmp(b_opt->name, opt_name, str_len))
			return b_opt;
	}
	return NULL;
}

static bool peer_completely_diskless(struct d_host_info *peer)
{
	struct d_volume *vol;

	for_each_volume(vol, &peer->volumes) {
		if (vol->disk)
			return false;
	}

	return true;
}

int adm_create_md(const struct cfg_ctx *ctx)
{
	char answer[ANSWER_SIZE];
	struct node_info ni;
	uint64_t device_uuid=0;
	uint64_t device_size=0;
	char *uri;
	int send=0;
	char *tb;
	int rv, fd, verbose_tmp;
	char *r, *max_peers_str = NULL;
	struct d_name *b_opt_max_peers;
	const char *opt_max_peers = "--max-peers=";

	b_opt_max_peers = find_backend_option(opt_max_peers);
	if (b_opt_max_peers) {
		max_peers_str = ssprintf("%s", b_opt_max_peers->name + strlen(opt_max_peers));
	} else {
		struct connection *conn;
		int max_peers = 0;

		set_peer_in_resource(ctx->res, true);

		for_each_connection(conn, &ctx->res->connections) {
			if (conn->ignore)
				continue;

			if (!peer_completely_diskless(conn->peer))
				max_peers++;
		}

		if (max_peers == 0)
			max_peers = 1;

		max_peers_str = ssprintf("%d", max_peers);
	}

	/* drbdmeta does not understand "--max-peers=",
	 * so we drop it from the option list here... */
	if (b_opt_max_peers)
		STAILQ_REMOVE(&backend_options, b_opt_max_peers, d_name, link);

	verbose_tmp = verbose;
	verbose = 0;
	tb = run_adm_drbdmeta(ctx, "read-dev-uuid");
	verbose = verbose_tmp;
	device_uuid = strto_u64(tb,NULL,16);
	free(tb);

	/* This is "drbdmeta ... create-md".
	 * It implicitly adds all backend_options to the command line. */
	rv = _adm_drbdmeta(ctx, SLEEPS_VERY_LONG, max_peers_str);
	/* ... now add back "--max-peers=", if any,
	 * in case the caller loops over several volumes. */
	if (b_opt_max_peers)
		insert_head(&backend_options, b_opt_max_peers);

	if(rv || dry_run) return rv;

	fd = open(ctx->vol->disk,O_RDONLY);
	if( fd != -1) {
		device_size = bdev_size(fd);
		close(fd);
	}

	if( read_node_id(&ni) && device_size && !device_uuid) {
		get_random_bytes(&device_uuid, sizeof(uint64_t));

		if( global_options.usage_count == UC_YES ) send = 1;
		if( global_options.usage_count == UC_ASK ) {
			err("\n"
			    "\t\t--== Creating metadata ==--\n"
			    "As with nodes, we count the total number of devices mirrored by DRBD\n"
			    "at http://"HTTP_HOST".\n\n"
			    "The counter works anonymously. It creates a random number to identify\n"
			    "the device and sends that random number, along with the kernel and\n"
			    "DRBD version, to "HTTP_HOST".\n\n"
			    "http://"HTTP_HOST"/cgi-bin/insert_usage.pl?nu="U64"&ru="U64"&rs="U64"\n\n"
			    "* If you wish to opt out entirely, simply enter 'no'.\n"
			    "* To continue, just press [RETURN]\n",
			    ni.node_uuid, device_uuid, device_size);
			r = fgets(answer, ANSWER_SIZE, stdin);
			if(r && strcmp(answer,"no\n")) send = 1;
		}
	}

	if(!device_uuid) {
		get_random_bytes(&device_uuid, sizeof(uint64_t));
	}

	if (send) {
		uri = ssprintf("http://"HTTP_HOST"/cgi-bin/insert_usage.pl?"
			       "nu="U64"&ru="U64"&rs="U64,
			       ni.node_uuid, device_uuid, device_size);
		make_get_request(uri);
	}

	/* HACK */
	{
		struct adm_cmd local_cmd = *ctx->cmd;
		struct cfg_ctx local_ctx = *ctx;
		struct names old_backend_options;
		char *opt;

		opt = ssprintf(X64(016), device_uuid);
		old_backend_options = backend_options;
		STAILQ_INIT(&backend_options);
		insert_tail(&backend_options, names_from_str(opt));

		local_cmd.name = "write-dev-uuid";
		local_ctx.cmd = &local_cmd;
		local_cmd.drbdsetup_ctx = &wildcard_ctx;
		_adm_drbdmeta(&local_ctx, SLEEPS_VERY_LONG, NULL);

		free_names(&backend_options);
		backend_options = old_backend_options;
	}
	return rv;
}

