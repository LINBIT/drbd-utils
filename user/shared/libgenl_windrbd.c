#include "config.h"

#include "libgenl.h"

#include <asm/byteorder.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

#include <windrbd/windrbd_ioctl.h>
#include <windows.h>
#include "shared_windrbd.h"

/* This file contains the 'netlink' interface for WinDRBD. It uses
 * ioctl's (DeviceIoControl()) to talk to the kernel, since there
 * is no such thing as AF_NETLINK in Windows. Using TCP/IP instead
 * (like WDRBD does it) is a severe security hole, since with TCP/IP
 * there is no permission check on the caller (and even can be easily
 * exploited remotely).
 */

static void fill_in_header(struct genl_sock *s, struct msg_buff *msg)
{
	struct nlmsghdr *n = (struct nlmsghdr *)msg->data;

	n->nlmsg_len = msg->tail - msg->data;
	n->nlmsg_flags |= NLM_F_REQUEST;
	n->nlmsg_seq = s->s_seq_expect = s->s_seq_next++;
	n->nlmsg_pid = getpid();
}

static int verify_header(struct genl_sock *s, struct iovec *iov, size_t c, char ** err_desc)
{
	struct nlmsghdr *nlh;

	nlh = (struct nlmsghdr*)iov->iov_base;
	if (!nlmsg_ok(nlh, c)) {
		if (err_desc)
			*err_desc = "truncated message in netlink reply";
		return -E_RCV_MSG_TRUNC;
	}
	if (s->s_seq_expect && nlh->nlmsg_seq != s->s_seq_expect) {
		dbg(2, "sequence mismatch: 0x%x != 0x%x, type:%x flags:%x sportid:%x\n",
			nlh->nlmsg_seq, s->s_seq_expect, nlh->nlmsg_type, nlh->nlmsg_flags, nlh->nlmsg_pid);

		if (err_desc)
			*err_desc = "sequence mismatch in netlink reply";
		return -E_RCV_SEQ_MISMATCH;
	}

	if (nlh->nlmsg_type == NLMSG_NOOP ||
	    nlh->nlmsg_type == NLMSG_OVERRUN) {
		if (err_desc)
			*err_desc = "unexpected message type in reply";
		return -E_RCV_UNEXPECTED_TYPE;
	}
	if (nlh->nlmsg_type == NLMSG_DONE)
		return -E_RCV_NLMSG_DONE;

	if (nlh->nlmsg_type == NLMSG_ERROR) {
		struct nlmsgerr *e = nlmsg_data(nlh);
		errno = -e->error;
		if (!errno)
			/* happens if you request NLM_F_ACK */
			dbg(3, "got a positive ACK message for seq:%u",
					s->s_seq_expect);
		else {
			dbg(3, "got a NACK message for seq:%u, error:%d",
					s->s_seq_expect, e->error);
			if (err_desc)
				*err_desc = strerror(errno);
		}
		return -E_RCV_ERROR_REPLY;
	}
	return c;
}


int genl_join_mc_group(struct genl_sock *s, const char *name)
{
	int err;
	unsigned int unused;
	struct windrbd_ioctl_genl_portid_and_multicast_group m;

	strncpy(m.name, name, sizeof(m.name));
	m.portid = getpid();

        if (DeviceIoControl(s->s_handle, IOCTL_WINDRBD_ROOT_JOIN_MC_GROUP, (void*) &m, sizeof(m), NULL, 0, &unused, NULL) == 0) {
	        err = GetLastError();
		printf("DeviceIoControl() failed, error is %d\n", err);
		return -1;
	}
	return 0;
}

static struct genl_sock *genl_connect(__u32 nl_groups)
{
	struct genl_sock *s = calloc(1, sizeof(*s));

	if (!s)
		return NULL;

	/* start with some sane sequence number */
	s->s_seq_expect = s->s_seq_next = time(0);

	s->s_handle = do_open_root_device(0);
	if (s->s_handle == INVALID_HANDLE_VALUE)
		goto fail;

	return s;

fail:
	free(s);
	return NULL;
}

static int do_send(struct genl_sock *s, const void *buf, int len)
{
	int err;
	unsigned int unused;

        if (DeviceIoControl(s->s_handle, IOCTL_WINDRBD_ROOT_SEND_NL_PACKET, (void*) buf, len, NULL, 0, &unused, NULL) == 0) {
	        err = GetLastError();
		printf("DeviceIoControl() failed, error is %d\n", err);
		return -1;
	}
	return 0;
}

int genl_send(struct genl_sock *s, struct msg_buff *msg)
{
	struct nlmsghdr *n = (struct nlmsghdr *)msg->data;

	fill_in_header(s, msg);

#define LOCAL_DEBUG_LEVEL 3
#if LOCAL_DEBUG_LEVEL <= DEBUG_LEVEL
	struct genlmsghdr *g = nlmsg_data(n);

	dbg(LOCAL_DEBUG_LEVEL, "sending %smessage, pid:%u seq:%u, g.cmd/version:%u/%u",
			n->nlmsg_type == GENL_ID_CTRL ? "ctrl " : "",
			n->nlmsg_pid, n->nlmsg_seq, g->cmd, g->version);
#endif

	return do_send(s, msg->data, n->nlmsg_len);
}

/* Unfortunately, implementing a POLL semantics that honors CygWin signals
 * (like SIGINT when user presses Ctrl-C) isn't that easy to do in kernel,
 * since CygWin implements signals in user space.
 *
 * We therefore poll from user space here. It costs a little CPU, but
 * we can interrupt the process any time we like.
 */

#define BUSY_POLLING_INTERVAL_MS 100

int genl_recv_msgs(struct genl_sock *s, struct iovec *iov, char **err_desc, int timeout_ms)
{
	struct windrbd_ioctl_genl_portid p;
	unsigned int size;
	int err;
	int forever;

	p.portid = getpid();
	forever = timeout_ms < 0;
	while (forever || timeout_ms > 0) {
	        if (DeviceIoControl(s->s_handle, IOCTL_WINDRBD_ROOT_RECEIVE_NL_PACKET, &p, sizeof(p), iov->iov_base, iov->iov_len, &size, NULL) == 0) {
		        err = GetLastError();
			printf("DeviceIoControl() failed, error is %d\n", err);
			if (err_desc)
				*err_desc = "ioctl error";
			return -1;
		}
		if (size > 0)
			break;

		usleep(BUSY_POLLING_INTERVAL_MS * 1000);
		if (!forever)
			timeout_ms -= BUSY_POLLING_INTERVAL_MS;
	}
	if (size == 0) {
		if (err_desc)
			*err_desc = "timed out waiting for reply";
		return -E_RCV_TIMEDOUT;
	}

	return verify_header(s, iov, size, err_desc);
}

struct genl_sock *genl_connect_to_family(struct genl_family *family)
{
	struct genl_sock *s = NULL;

	family->id = WINDRBD_NETLINK_FAMILY_ID;

	s = genl_connect(family->nl_groups);
	if (!s)
		dbg(1, "error creating netlink socket");
	else
		s->s_family = family;

	return s;
}

