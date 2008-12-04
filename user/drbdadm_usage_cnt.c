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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#include "drbdadm.h"
#include "drbdtool_common.h"
#include "drbd_endian.h"
#include "linux/drbd.h"		/* only use DRBD_MAGIC from here! */

#define HTTP_PORT 80
#define HTTP_HOST "usage.drbd.org"
#define HTTP_ADDR "212.69.162.23"
#define DRBD_LIB_DIR "/var/lib/drbd"
#define NODE_ID_FILE DRBD_LIB_DIR"/node_id"
#define GIT_HASH_BYTE 20
#define SVN_STYLE_OD  16

struct vcs_rel {
	u32	svn_revision;
	char	git_hash[GIT_HASH_BYTE];
};

struct node_info {
	u64	node_uuid;
	struct vcs_rel rev;
};

struct node_info_od {
	u32 magic;
	struct node_info ni;
} __attribute((packed));

/* For our purpose (finding the revision) SLURP_SIZE is always enough.
 */
static char* slurp_proc_drbd()
{
	const int SLURP_SIZE = 4096;
	char* buffer;
	int rr, fd;

	fd = open("/proc/drbd",O_RDONLY);
	if( fd == -1) return 0;

	buffer = malloc(SLURP_SIZE);
	if(!buffer) return 0;

	rr = read(fd, buffer, SLURP_SIZE-1);
	if( rr == -1) {
		free(buffer);
		return 0;
	}

	buffer[rr]=0;
	close(fd);

	return buffer;
}

void read_hex(char* dst, char* src, int dst_size, int src_size)
{
	int dst_i, u, src_i=0;

	for(dst_i=0;dst_i<dst_size;dst_i++) {
		if(src[src_i] == 0) break;
		sscanf(src+src_i,"%2x",&u);
		dst[dst_i]=u;
		if(++src_i >= src_size) break;
		if(src[src_i] == 0) break;
		if(++src_i >= src_size) break;
	}
}

void vcs_from_str(struct vcs_rel *rel, const char *text)
{
	char token[80];
	int plus=0;
	enum { begin,f_svn,f_rev,f_git } ex=begin;

	while (sget_token(token, sizeof(token), &text) != EOF) {
		switch(ex) {
		case begin:
			if(!strcmp(token,"plus")) plus = 1;
			if(!strcmp(token,"SVN"))  ex = f_svn;
			if(!strcmp(token,"GIT-hash:"))  ex = f_git;
			break;
		case f_svn:
			if(!strcmp(token,"Revision:"))  ex = f_rev;
			break;
		case f_rev:
			rel->svn_revision = atol(token) * 10;
			if( plus ) rel->svn_revision += 1;
			memset(rel->git_hash, 0, GIT_HASH_BYTE);
			return;
		case f_git:
			read_hex(rel->git_hash, token, GIT_HASH_BYTE, strlen(token));
			rel->svn_revision = 0;
			return;
		}
	}
}

static void vcs_get_current(struct vcs_rel *rel)
{
	char* version_txt;

	version_txt = slurp_proc_drbd();
	if(version_txt) {
		vcs_from_str(rel, version_txt);
		free(version_txt);
	} else {
		vcs_from_str(rel, drbd_buildtag());
	}
}

static int vcs_eq(struct vcs_rel *rev1, struct vcs_rel *rev2)
{
	if( rev1->svn_revision || rev2->svn_revision ) {
		return rev1->svn_revision == rev2->svn_revision;
	} else {
		return !memcmp(rev1->git_hash,rev2->git_hash,GIT_HASH_BYTE);
	}
}

static char *vcs_to_str(struct vcs_rel *rev)
{
	static char buffer[80]; // Not generic, sufficent for the purpose.

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
		perror("Creation of "NODE_ID_FILE" failed.");
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
		perror("Write to "NODE_ID_FILE" failed.");
		exit(20);
	}

	close(fd);
}


static int read_node_id(struct node_info *ni)
{
	int rr,fd;
	struct node_info_od on_disk;

	fd = open(NODE_ID_FILE,O_RDONLY);
	if( fd == -1) {
		return 0;
	}

	rr = read(fd,&on_disk, sizeof(on_disk)); 
	if( rr != sizeof(on_disk) && rr != SVN_STYLE_OD ) {
		close(fd);
		return 0;
	}

	switch(be32_to_cpu(on_disk.magic)) {
	case DRBD_MAGIC:
		ni->node_uuid    = be64_to_cpu(on_disk.ni.node_uuid);
		ni->rev.svn_revision = be32_to_cpu(on_disk.ni.rev.svn_revision);
		memset(ni->rev.git_hash,0,GIT_HASH_BYTE);
		break;
	case DRBD_MAGIC+1:
		ni->node_uuid    = be64_to_cpu(on_disk.ni.node_uuid);
		ni->rev.svn_revision = 0;
		memcpy(ni->rev.git_hash,on_disk.ni.rev.git_hash,GIT_HASH_BYTE);
		break;
	default:
		return 0;
	}

	close(fd);
	return 1;
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
static int make_get_request(char *req_buf) {
	struct sockaddr_in server;
	struct hostent *host_info;
	unsigned long addr;
	int sock;
	char *http_host = HTTP_HOST;
	int buf_len = 1024;
	char buffer[buf_len];
	FILE *sockfd;
	int writeit;
	sock = socket( PF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		return 1;
	}
	memset (&server, 0, sizeof(server));

	/* convert host name to ip */
	host_info = gethostbyname(http_host);
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
			fprintf(stderr,"%s", buffer);
		}
	}
	fclose(sockfd);
	close(sock);
	return 0;
}

static void url_encode(char* in, char* out)
{
	char *h = "0123456789abcdef";
	char c;

	while( (c = *in++) != 0 ) {
		if( c == '\n' ) break;
		if( ( 'a' <= c && c <= 'z' )
		    || ( 'A' <= c && c <= 'Z' )
		    || ( '0' <= c && c <= '9' )
		    || c == '-' || c == '_' || c == '.' )
			*out++ = c;
		else if( c == ' ' )
			*out++ = '+';
		else {
			*out++ = '%';
			*out++ = h[c >> 4];
			*out++ = h[c & 0x0f];
		}
	}
	*out = 0;
}

/* Ensure that the node is counted on http://usage.drbd.org
 */
#define ANSWER_SIZE 80

void uc_node(enum usage_count_type type)
{
	struct node_info ni;
	char *req_buf;
	struct vcs_rel current;
	int send = 0;
	int update = 0;
	char answer[ANSWER_SIZE];
	char n_comment[ANSWER_SIZE*3];

	if( type == UC_NO ) return;
	if( getuid() != 0 ) return;

	/* not when running directly from init,
	 * or if stdout is no tty.
	 * you do not want to have the "user information message"
	 * as output from `drbdadm sh-resources all`
	 */
	if (getenv("INIT_VERSION")) return;
	if (no_tty) return;

	vcs_get_current(&current);

	if( ! read_node_id(&ni) ) {
		get_random_bytes(&ni.node_uuid,sizeof(ni.node_uuid));
		ni.rev = current;
		send = 1;
	} else {
		// read_node_id() was successull
		if (!vcs_eq(&ni.rev,&current)) {
			ni.rev = current;
			update = 1;
			send = 1;
		}
	}

	if(!send) return;

	n_comment[0]=0;
	if (type == UC_ASK ) {
		fprintf(stderr,
"\n"
"\t\t--== This is %s of DRBD ==--\n"
"Please take part in the global DRBD usage count at http://"HTTP_HOST".\n\n"
"The counter works anonymously. It creates a random number to identify\n"
"your machine and sends that random number, along with \n"
"DRBD's version number, to "HTTP_HOST".\n\n"
"The benefits for you are:\n"
" * In response to your submission, the server ("HTTP_HOST") will tell you\n"
"   how many users before you have installed this version (%s).\n"
" * With a high counter LINBIT has a strong motivation to\n"
"   continue funding DRBD's development.\n\n"
"http://"HTTP_HOST"/cgi-bin/insert_usage.pl?nu="U64"&%s\n\n"
"In case you want to participate but know that this machine is firewalled,\n"
"simply issue the query string with your favorite web browser or wget.\n"
"You can control all of this by setting 'usage-count' in your drbd.conf.\n\n"
"* You may enter a free form comment about your machine, that gets\n"
"  used on "HTTP_HOST" instead of the big random number.\n"
"* If you wish to opt out entirely, simply enter 'no'.\n"
"* To count this node without comment, just press [RETURN]\n",
			update ? "an update" : "a new installation",
			REL_VERSION,ni.node_uuid, vcs_to_str(&ni.rev));
		fgets(answer,ANSWER_SIZE,stdin);
		if(!strcmp(answer,"no\n")) send = 0;
		url_encode(answer,n_comment);
	}

	ssprintf(req_buf,"GET http://"HTTP_HOST"/cgi-bin/insert_usage.pl?"
		 "nu="U64"&%s%s%s HTTP/1.0\n\n",
		 ni.node_uuid, vcs_to_str(&ni.rev),
		 n_comment[0] ? "&nc=" : "", n_comment);

	if (send) {
		write_node_id(&ni);

		fprintf(stderr,
"\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
"  --==  Thank you for participating in the global usage survey  ==--\n"
"The server's response is:\n\n");
		make_get_request(req_buf);
		if (type == UC_ASK) {
			fprintf(stderr,
"\n"
"From now on, drbdadm will contact "HTTP_HOST" only when you update\n"
"DRBD or when you use 'drbdadm create-md'. Of course it will continue\n"
"to ask you for confirmation as long as 'usage-count' is at its default\n"
"value of 'ask'.\n\n"
"Just press [RETURN] to continue: ");
			fgets(answer,9,stdin);
		}
	}
}

/* For our purpose (finding the revision) SLURP_SIZE is always enough.
 */
char* run_admm_generic(struct d_resource* res ,const char* cmd)
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
		fprintf(stderr,"Can not fork\n");
		exit(E_exec_error);
	}
	if(pid == 0) {
		// child
		close(pipes[0]); // close reading end
		dup2(pipes[1],1); // 1 = stdout
		close(pipes[1]);
		exit(_admm_generic(res,cmd,
				   SLEEPS_VERY_LONG|SUPRESS_STDERR|
				   DONT_REPORT_FAILED));
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

int adm_create_md(struct d_resource* res ,const char* cmd)
{
	char answer[ANSWER_SIZE];
	struct node_info ni;
	u64 device_uuid=0;
	u64 device_size=0;
	char *req_buf;
	int send=0;
	char *tb;
	int rv,fd;
	int soi_tmp;
	char *setup_opts_0_tmp;

	tb = run_admm_generic(res, "read-dev-uuid");
	device_uuid = strto_u64(tb,NULL,16);
	free(tb);

	rv = _admm_generic(res, cmd, SLEEPS_VERY_LONG); // cmd is "create-md".

	if(rv || dry_run) return rv;

	fd = open(res->me->disk,O_RDONLY);
	if( fd != -1) {
		device_size = bdev_size(fd);
		close(fd);
	}

	if( read_node_id(&ni) && device_size && !device_uuid) {
		get_random_bytes(&device_uuid, sizeof(u64));

		if( global_options.usage_count == UC_YES ) send = 1;
		if( global_options.usage_count == UC_ASK ) {
			fprintf(stderr,
"\n"
"\t\t--== Creating metadata ==--\n"
"As with nodes, we count the total number of devices mirrored by DRBD at\n"
"at http://"HTTP_HOST".\n\n"
"The counter works anonymously. It creates a random number to identify\n"
"the device and sends that random number, along with \n"
"DRBD's version number, to "HTTP_HOST".\n\n"
"http://"HTTP_HOST"/cgi-bin/insert_usage.pl?nu="U64"&ru="U64"&rs="U64"\n\n"
"* If you wish to opt out entirely, simply enter 'no'.\n"
"* To continue, just press [RETURN]\n",
				ni.node_uuid,device_uuid,device_size
				);
		fgets(answer,ANSWER_SIZE,stdin);
		if(strcmp(answer,"no\n")) send = 1;
		}
	}

	if(!device_uuid) {
		get_random_bytes(&device_uuid, sizeof(u64));
	}

	if (send) {
		ssprintf(req_buf,"GET http://"HTTP_HOST"/cgi-bin/insert_usage.pl?"
			 "nu="U64"&ru="U64"&rs="U64" HTTP/1.0\n\n",
			 ni.node_uuid, device_uuid, device_size);
		make_get_request(req_buf);
	}

	/* HACK */
	soi_tmp = soi;
	setup_opts_0_tmp = setup_opts[0];

	setup_opts[0] = NULL;
	ssprintf( setup_opts[0], X64(016), device_uuid);
	soi=1;
	_admm_generic(res, "write-dev-uuid", SLEEPS_VERY_LONG);

	setup_opts[0] = setup_opts_0_tmp;
	soi = soi_tmp;

	return rv;
}

