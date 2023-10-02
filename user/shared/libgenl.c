#include "libgenl.h"

#include <sys/types.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <poll.h>

#include "config.h"

int genl_join_mc_group(struct genl_sock *s, const char *name) {
	int g_id;
	int i;

	BUG_ON(!s || !s->s_family);
	for (i = 0; i < 32; i++) {
		if (!s->s_family->mc_groups[i].id)
			continue;
		if (strcmp(s->s_family->mc_groups[i].name, name))
			continue;

		g_id = s->s_family->mc_groups[i].id;
		return setsockopt(s->s_fd, SOL_NETLINK, NETLINK_ADD_MEMBERSHIP,
				&g_id, sizeof(g_id));
	}
	return -2;
}

#define DO_OR_LOG_AND_FAIL(x) \
	do {							\
		int err = x;					\
		if (err) {					\
			dbg(1, "%s failed: %d %s\n",		\
				#x, err, strerror(errno));	\
			goto fail;				\
		}						\
	} while(0)

static struct genl_sock *genl_connect(__u32 nl_groups)
{
	struct genl_sock *s = calloc(1, sizeof(*s));
	socklen_t sock_len;
	int bsz = 1 << 20;

	if (!s)
		return NULL;

	/* autobind; kernel is responsible to give us something unique
	 * in bind() below. */
	s->s_local.nl_pid = 0;
	s->s_local.nl_family = AF_NETLINK;
	/*
	 * If we want to receive multicast traffic on this socket, kernels
	 * before v2.6.23-rc1 require us to indicate which multicast groups we
	 * are interested in in nl_groups.
	 */
	s->s_local.nl_groups = nl_groups;
	s->s_peer.nl_family = AF_NETLINK;
	/* start with some sane sequence number */
	s->s_seq_expect = s->s_seq_next = time(0);

	s->s_fd = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_GENERIC);
	if (s->s_fd == -1)
		goto fail;

	sock_len = sizeof(s->s_local);
	DO_OR_LOG_AND_FAIL(setsockopt(s->s_fd, SOL_SOCKET, SO_SNDBUF, &bsz, sizeof(bsz)));
	DO_OR_LOG_AND_FAIL(setsockopt(s->s_fd, SOL_SOCKET, SO_RCVBUF, &bsz, sizeof(bsz)));
	DO_OR_LOG_AND_FAIL(bind(s->s_fd, (struct sockaddr*) &s->s_local, sizeof(s->s_local)));
	DO_OR_LOG_AND_FAIL(getsockname(s->s_fd, (struct sockaddr*) &s->s_local, &sock_len));

	dbg(3, "bound socket to nl_pid:%u, my pid:%u, len:%u, sizeof:%u\n",
		s->s_local.nl_pid, getpid(),
		(unsigned)sock_len, (unsigned)sizeof(s->s_local));

	return s;

fail:
	free(s);
	return NULL;
}
#undef DO_OR_LOG_AND_FAIL

static int do_send(int fd, const void *buf, int len)
{
	int c;
	while ((c = write(fd, buf, len)) < len) {
		if (c == -1) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		buf += c;
		len -= c;
	}
	return 0;
}

int genl_send(struct genl_sock *s, struct msg_buff *msg)
{
	struct nlmsghdr *n = (struct nlmsghdr *)msg->data;

	n->nlmsg_len = msg->tail - msg->data;
	n->nlmsg_flags |= NLM_F_REQUEST;
	n->nlmsg_seq = s->s_seq_expect = s->s_seq_next++;
	n->nlmsg_pid = s->s_local.nl_pid;

#define LOCAL_DEBUG_LEVEL 3
#if LOCAL_DEBUG_LEVEL <= DEBUG_LEVEL
	struct genlmsghdr *g = nlmsg_data(n);

	dbg(LOCAL_DEBUG_LEVEL, "sending %smessage, pid:%u seq:%u, g.cmd/version:%u/%u",
			n->nlmsg_type == GENL_ID_CTRL ? "ctrl " : "",
			n->nlmsg_pid, n->nlmsg_seq, g->cmd, g->version);
#endif

	return do_send(s->s_fd, msg->data, n->nlmsg_len);
}

/* "inspired" by libnl nl_recv()
 * You pass in one iovec, which may contain pre-allocated buffer space,
 * obtained by malloc(). It will be realloc()ed on demand.
 * Caller is responsible for free()ing it up on return,
 * regardless of return code.
 */
int genl_recv_timeout(struct genl_sock *s, struct iovec *iov, int timeout_ms)
{
	struct sockaddr_nl addr;
	struct pollfd pfd;
	int flags;
	struct msghdr msg = {
		.msg_name = &addr,
		.msg_namelen = sizeof(struct sockaddr_nl),
		.msg_iov = iov,
		.msg_iovlen = 1,
		.msg_control = NULL,
		.msg_controllen = 0,
		.msg_flags = 0,
	};
	int n;

	if (!iov->iov_len) {
		iov->iov_len = 8192;
		iov->iov_base = malloc(iov->iov_len);
	}

	flags = MSG_PEEK;
retry:
	pfd.fd = s->s_fd;
	pfd.events = POLLIN;
	if ((poll(&pfd, 1, timeout_ms) != 1) || !(pfd.revents & POLLIN))
		return 0; /* which is E_RCV_TIMEDOUT */

	/* for most cases this method will memcopy twice, as the default buffer
	 * is large enough.  But for those few other cases, we now have a
	 * chance to realloc before the rest of the datagram is discarded.
	 */
	n = recvmsg(s->s_fd, &msg, flags);
	if (!n)
		return 0;
	else if (n < 0) {
		if (errno == EINTR) {
			dbg(3, "recvmsg() returned EINTR, retrying\n");
			goto retry;
		} else if (errno == EAGAIN) {
			dbg(3, "recvmsg() returned EAGAIN, aborting\n");
			return 0;
		} else if (errno == ENOBUFS) {
			dbg(3, "recvmsg() returned ENOBUFS\n");
			return -E_RCV_ENOBUFS;
		} else {
			dbg(3, "recvmsg() returned %d, errno = %d\n", n, errno);
			return -E_RCV_FAILED;
		}
	}

	if (iov->iov_len < (unsigned)n ||
	    msg.msg_flags & MSG_TRUNC) {
		/* Provided buffer is not long enough, enlarge it
		 * and try again. */
		iov->iov_len *= 2;
		iov->iov_base = realloc(iov->iov_base, iov->iov_len);
		goto retry;
	} else if (flags != 0) {
		/* Buffer is big enough, do the actual reading */
		flags = 0;
		goto retry;
	}

	if (msg.msg_namelen != sizeof(struct sockaddr_nl))
		return -E_RCV_NO_SOURCE_ADDR;

	if (addr.nl_pid != 0) {
		dbg(3, "ignoring message from sender pid %u != 0\n",
				addr.nl_pid);
		goto retry;
	}
	return n;
}


/* Note that one datagram may contain multiple netlink messages
 * (e.g. for a dump response). This only checks the _first_ message,
 * caller has to iterate over multiple messages with nlmsg_for_each_msg()
 * when necessary. */
int genl_recv_msgs(struct genl_sock *s, struct iovec *iov, char **err_desc, int timeout_ms)
{
	struct nlmsghdr *nlh;
	int c = genl_recv_timeout(s, iov, timeout_ms);
	if (c <= 0) {
		if (err_desc)
			*err_desc = (c == -E_RCV_TIMEDOUT)
				? "timed out waiting for reply"
				: (c == -E_RCV_NO_SOURCE_ADDR)
				? "no source address!"
				: ( c == -E_RCV_ENOBUFS)
			        ? "packets droped, socket receive buffer overrun"
				: "failed to receive netlink reply";
		return c;
	}

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

	/* good reply message(s) */
	dbg(3, "received a good message for seq:%u", s->s_seq_expect);
	return c;
}

static struct genl_family genl_ctrl = {
        .id = GENL_ID_CTRL,
        .name = "nlctrl",
        .version = 0x2,
        .maxattr = CTRL_ATTR_MAX,
};

struct genl_sock *genl_connect_to_family(struct genl_family *family)
{
	struct genl_sock *s = NULL;
	struct msg_buff *msg;
	struct nlmsghdr *nlh;
	struct nlattr *nla;
	struct iovec iov = { .iov_len = 0 };
	int rem;

	BUG_ON(!family);
	BUG_ON(!strlen(family->name));

	msg = msg_new(DEFAULT_MSG_SIZE);
	if (!msg) {
		dbg(1, "could not allocate genl message");
		goto out;
	}

	s = genl_connect(family->nl_groups);
	if (!s) {
		dbg(1, "error creating netlink socket");
		goto out;
	}
	genlmsg_put(msg, &genl_ctrl, 0, CTRL_CMD_GETFAMILY);

	nla_put_string(msg, CTRL_ATTR_FAMILY_NAME, family->name);
	if (genl_send(s, msg)) {
		dbg(1, "failed to send netlink message");
		free(s);
		s = NULL;
		goto out;
	}

	if (genl_recv_msgs(s, &iov, NULL, 3000) <= 0) {
		close(s->s_fd);
		free(s);
		s = NULL;
		goto out;
	}


	nlh = (struct nlmsghdr*)iov.iov_base;
	nla_for_each_attr(nla, nlmsg_attrdata(nlh, GENL_HDRLEN),
			nlmsg_attrlen(nlh, GENL_HDRLEN), rem) {
		switch (nla_type(nla)) {
		case CTRL_ATTR_FAMILY_ID:
			family->id = nla_get_u16(nla);
			dbg(2, "'%s' genl family id: %d", family->name, family->id);
			break;
		case CTRL_ATTR_FAMILY_NAME:
			break;
#ifdef HAVE_CTRL_ATTR_VERSION
		case CTRL_ATTR_VERSION:
			family->version = nla_get_u32(nla);
			dbg(2, "'%s' genl family version: %d", family->name, family->version);
			break;
#endif
#ifdef HAVE_CTRL_ATTR_HDRSIZE
		case CTRL_ATTR_HDRSIZE:
			family->hdrsize = nla_get_u32(nla);
			dbg(2, "'%s' genl family hdrsize: %d", family->name, family->hdrsize);
			break;
#endif
#ifdef HAVE_CTRL_ATTR_MCAST_GROUPS
		case CTRL_ATTR_MCAST_GROUPS:
			{
			static struct nla_policy policy[] = {
				[CTRL_ATTR_MCAST_GRP_NAME] = { .type = NLA_NUL_STRING, .len = GENL_NAMSIZ },
				[CTRL_ATTR_MCAST_GRP_ID] = { .type = NLA_U32 },
			};
			struct nlattr *ntb[__CTRL_ATTR_MCAST_GRP_MAX];
			struct nlattr *idx;
			int tmp;
			int i = 0;
			nla_for_each_nested(idx, nla, tmp) {
				BUG_ON(i >= 32);
				nla_parse_nested(ntb, CTRL_ATTR_MCAST_GRP_MAX, idx, policy);
				if (ntb[CTRL_ATTR_MCAST_GRP_NAME] &&
				    ntb[CTRL_ATTR_MCAST_GRP_ID]) {
					struct genl_multicast_group *grp = &family->mc_groups[i++];
					grp->id = nla_get_u32(ntb[CTRL_ATTR_MCAST_GRP_ID]);
					nla_strlcpy(grp->name, ntb[CTRL_ATTR_MCAST_GRP_NAME],
							sizeof(grp->name));
					dbg(2, "'%s'-'%s' multicast group found (id: %u)\n",
						family->name, grp->name, grp->id);
				}
			}
			break;
			};
#endif
		case CTRL_ATTR_OPS:
			{
			struct nlattr *ntb[__CTRL_ATTR_OP_MAX];
			struct nlattr *idx;
			int tmp;
			nla_for_each_nested(idx, nla, tmp) {
				nla_parse_nested(ntb, CTRL_ATTR_OP_MAX, idx, NULL);
				if (ntb[CTRL_ATTR_OP_ID]) {
					int id = nla_get_u32(ntb[CTRL_ATTR_OP_ID]);

					if (id < GENL_MAX_OPS)
						family->op_known[id] = true;
				}
			}
			break;
			}
		default: ;
		}
	}

	if (!family->id)
		dbg(1, "genl family '%s' not found", family->name);
	else
		s->s_family = family;

out:
	free(iov.iov_base);
	msg_free(msg);

	return s;
}

bool genl_op_known(struct genl_family *family, int id)
{
	if (id >= GENL_MAX_OPS)
		return false;

	return family->op_known[id];
}

/* Shared nla code from Lars has been moved to libnla.c */
