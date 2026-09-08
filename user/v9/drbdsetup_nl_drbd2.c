/*
 * The "drbd2" generic netlink family as a drbdsetup dialect.
 *
 * Everything that knows about the bytes of this family: the context nest
 * that identifies the object, address nests instead of sockaddr blobs,
 * the per-set attribute numbering, set-defaults as a flag attribute, and
 * state-result plus extended ACK instead of a ret_code header field. The
 * command logic keeps using the legacy attribute ids as field ids; the
 * tables below translate.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

#include "drbdsetup_nl.h"
#include "drbdsetup.h"
#include "linux/drbd2_genl_userspace.h"

static struct genl_family drbd2_family = {
	.name = DRBD2_FAMILY_NAME,
	.version = DRBD2_FAMILY_VERSION,
	.hdrsize = 0,
};

static const int drbd2_cmd[__DRBD_NL_CMD_MAX] = {
	[DRBD_NL_CMD_NEW_RESOURCE]	= DRBD2_CMD_RESOURCE_NEW,
	[DRBD_NL_CMD_DEL_RESOURCE]	= DRBD2_CMD_RESOURCE_DEL,
	[DRBD_NL_CMD_RESOURCE_OPTS]	= DRBD2_CMD_RESOURCE_SET,
	[DRBD_NL_CMD_RENAME_RESOURCE]	= DRBD2_CMD_RESOURCE_RENAME,
	[DRBD_NL_CMD_DOWN]		= DRBD2_CMD_RESOURCE_DOWN,
	[DRBD_NL_CMD_PRIMARY]		= DRBD2_CMD_RESOURCE_PRIMARY,
	[DRBD_NL_CMD_SECONDARY]		= DRBD2_CMD_RESOURCE_SECONDARY,
	[DRBD_NL_CMD_SUSPEND_IO]	= DRBD2_CMD_RESOURCE_SUSPEND_IO,
	[DRBD_NL_CMD_RESUME_IO]		= DRBD2_CMD_RESOURCE_RESUME_IO,
	[DRBD_NL_CMD_NEW_MINOR]		= DRBD2_CMD_DEVICE_NEW,
	[DRBD_NL_CMD_DEL_MINOR]		= DRBD2_CMD_DEVICE_DEL,
	[DRBD_NL_CMD_ATTACH]		= DRBD2_CMD_DEVICE_ATTACH,
	[DRBD_NL_CMD_DETACH]		= DRBD2_CMD_DEVICE_DETACH,
	[DRBD_NL_CMD_DISK_OPTS]		= DRBD2_CMD_DISK_SET,
	[DRBD_NL_CMD_RESIZE]		= DRBD2_CMD_DEVICE_RESIZE,
	[DRBD_NL_CMD_OUTDATE]		= DRBD2_CMD_DEVICE_OUTDATE,
	[DRBD_NL_CMD_INVALIDATE]	= DRBD2_CMD_DEVICE_INVALIDATE,
	[DRBD_NL_CMD_NEW_C_UUID]	= DRBD2_CMD_DEVICE_NEW_CURRENT_UUID,
	[DRBD_NL_CMD_NEW_PEER]		= DRBD2_CMD_CONNECTION_NEW,
	[DRBD_NL_CMD_DEL_PEER]		= DRBD2_CMD_CONNECTION_DEL,
	[DRBD_NL_CMD_CONNECT]		= DRBD2_CMD_CONNECTION_CONNECT,
	[DRBD_NL_CMD_DISCONNECT]	= DRBD2_CMD_CONNECTION_DISCONNECT,
	[DRBD_NL_CMD_NET_OPTS]		= DRBD2_CMD_CONNECTION_SET,
	[DRBD_NL_CMD_FORGET_PEER]	= DRBD2_CMD_CONNECTION_FORGET,
	[DRBD_NL_CMD_NEW_PATH]		= DRBD2_CMD_PATH_NEW,
	[DRBD_NL_CMD_DEL_PATH]		= DRBD2_CMD_PATH_DEL,
	[DRBD_NL_CMD_PEER_DEVICE_OPTS]	= DRBD2_CMD_PEER_DEVICE_SET,
	[DRBD_NL_CMD_INVALIDATE_PEER]	= DRBD2_CMD_PEER_DEVICE_INVALIDATE,
	[DRBD_NL_CMD_PAUSE_SYNC]	= DRBD2_CMD_PEER_DEVICE_PAUSE_SYNC,
	[DRBD_NL_CMD_RESUME_SYNC]	= DRBD2_CMD_PEER_DEVICE_RESUME_SYNC,
	[DRBD_NL_CMD_START_OV]		= DRBD2_CMD_PEER_DEVICE_START_OV,
	[DRBD_NL_CMD_GET_TIMEOUT_TYPE]	= DRBD2_CMD_TIMEOUT_TYPE_GET,
	[DRBD_NL_CMD_GET_RESOURCES]	= DRBD2_CMD_RESOURCE_GET,
	[DRBD_NL_CMD_GET_DEVICES]	= DRBD2_CMD_DEVICE_GET,
	[DRBD_NL_CMD_GET_CONNECTIONS]	= DRBD2_CMD_CONNECTION_GET,
	[DRBD_NL_CMD_GET_PEER_DEVICES]	= DRBD2_CMD_PEER_DEVICE_GET,
	[DRBD_NL_CMD_GET_PATHS]		= DRBD2_CMD_PATH_GET,
	[DRBD_NL_CMD_GET_INITIAL_STATE]	= DRBD2_CMD_STATE_GET,
};

/* The top level attribute of each set; 0 for the two sets drbd2 carries in the context. */
static const int drbd2_set[__NL_SET_MAX] = {
	[NL_SET_DISK_CONF]		= DRBD2_A_DISK_CONF,
	[NL_SET_NET_CONF]		= DRBD2_A_NET_CONF,
	[NL_SET_RES_OPTS]		= DRBD2_A_RESOURCE_OPTS,
	[NL_SET_PEER_DEVICE_CONF]	= DRBD2_A_PEER_DEVICE_CONF,
	[NL_SET_DEVICE_CONF]		= DRBD2_A_DEVICE_CONF,
	[NL_SET_SET_ROLE_PARMS]		= DRBD2_A_SET_ROLE_PARMS,
	[NL_SET_RESIZE_PARMS]		= DRBD2_A_RESIZE_PARMS,
	[NL_SET_START_OV_PARMS]		= DRBD2_A_START_OV_PARMS,
	[NL_SET_NEW_C_UUID_PARMS]	= DRBD2_A_NEW_CURRENT_UUID_PARMS,
	[NL_SET_DISCONNECT_PARMS]	= DRBD2_A_DISCONNECT_PARMS,
	[NL_SET_DETACH_PARMS]		= DRBD2_A_DETACH_PARMS,
	[NL_SET_INVALIDATE_PARMS]	= DRBD2_A_INVALIDATE_PARMS,
	[NL_SET_INVALIDATE_PEER_PARMS]	= DRBD2_A_INVALIDATE_PEER_PARMS,
	[NL_SET_CONNECT_PARMS]		= DRBD2_A_CONNECT_PARMS,
	[NL_SET_RENAME_RESOURCE_PARMS]	= DRBD2_A_RENAME_PARMS,
	[NL_SET_SUSPEND_IO_PARMS]	= DRBD2_A_SUSPEND_IO_PARMS,
};

/*
 * Legacy attribute id -> drbd2 attribute id, one table per set. The
 * reverse direction (for received option nests) is derived at first use.
 */
#define MAP(legacy, d2) [DRBD_A_ ## legacy] = DRBD2_A_ ## d2

static const __u16 disk_conf_map[DRBD_A_DISK_CONF_D_BITMAP + 1] = {
	MAP(DISK_CONF_BACKING_DEV, DISK_CONF_BACKING_DEV),
	MAP(DISK_CONF_META_DEV, DISK_CONF_META_DEV),
	MAP(DISK_CONF_META_DEV_IDX, DISK_CONF_META_DEV_IDX),
	MAP(DISK_CONF_DISK_SIZE, DISK_CONF_SIZE),
	MAP(DISK_CONF_ON_IO_ERROR, DISK_CONF_ON_IO_ERROR),
	MAP(DISK_CONF_RESYNC_AFTER, DISK_CONF_RESYNC_AFTER),
	MAP(DISK_CONF_AL_EXTENTS, DISK_CONF_AL_EXTENTS),
	MAP(DISK_CONF_DISK_BARRIER, DISK_CONF_DISK_BARRIER),
	MAP(DISK_CONF_DISK_FLUSHES, DISK_CONF_DISK_FLUSHES),
	MAP(DISK_CONF_DISK_DRAIN, DISK_CONF_DISK_DRAIN),
	MAP(DISK_CONF_MD_FLUSHES, DISK_CONF_MD_FLUSHES),
	MAP(DISK_CONF_DISK_TIMEOUT, DISK_CONF_DISK_TIMEOUT),
	MAP(DISK_CONF_READ_BALANCING, DISK_CONF_READ_BALANCING),
	MAP(DISK_CONF_UNPLUG_WATERMARK, DISK_CONF_UNPLUG_WATERMARK),
	MAP(DISK_CONF_RS_DISCARD_GRANULARITY, DISK_CONF_RS_DISCARD_GRANULARITY),
	MAP(DISK_CONF_AL_UPDATES, DISK_CONF_AL_UPDATES),
	MAP(DISK_CONF_DISCARD_ZEROES_IF_ALIGNED, DISK_CONF_DISCARD_ZEROES_IF_ALIGNED),
	MAP(DISK_CONF_DISABLE_WRITE_SAME, DISK_CONF_DISABLE_WRITE_SAME),
	MAP(DISK_CONF_D_BITMAP, DISK_CONF_BITMAP),
};

static const __u16 res_opts_map[DRBD_A_RES_OPTS_EXPLICIT_DRBD8_COMPAT + 1] = {
	MAP(RES_OPTS_CPU_MASK, RESOURCE_OPTS_CPU_MASK),
	MAP(RES_OPTS_ON_NO_DATA, RESOURCE_OPTS_ON_NO_DATA_ACCESSIBLE),
	MAP(RES_OPTS_AUTO_PROMOTE, RESOURCE_OPTS_AUTO_PROMOTE),
	MAP(RES_OPTS_NODE_ID, RESOURCE_OPTS_NODE_ID),
	MAP(RES_OPTS_PEER_ACK_WINDOW, RESOURCE_OPTS_PEER_ACK_WINDOW),
	MAP(RES_OPTS_TWOPC_TIMEOUT, RESOURCE_OPTS_TWOPC_TIMEOUT),
	MAP(RES_OPTS_TWOPC_RETRY_TIMEOUT, RESOURCE_OPTS_TWOPC_RETRY_TIMEOUT),
	MAP(RES_OPTS_PEER_ACK_DELAY, RESOURCE_OPTS_PEER_ACK_DELAY),
	MAP(RES_OPTS_AUTO_PROMOTE_TIMEOUT, RESOURCE_OPTS_AUTO_PROMOTE_TIMEOUT),
	MAP(RES_OPTS_NR_REQUESTS, RESOURCE_OPTS_MAX_IO_DEPTH),
	MAP(RES_OPTS_QUORUM, RESOURCE_OPTS_QUORUM),
	MAP(RES_OPTS_ON_NO_QUORUM, RESOURCE_OPTS_ON_NO_QUORUM),
	MAP(RES_OPTS_QUORUM_MIN_REDUNDANCY, RESOURCE_OPTS_QUORUM_MIN_REDUNDANCY),
	MAP(RES_OPTS_ON_SUSP_PRIMARY_OUTDATED, RESOURCE_OPTS_ON_SUSPENDED_PRIMARY_OUTDATED),
	MAP(RES_OPTS_DRBD8_COMPAT_MODE, RESOURCE_OPTS_DRBD8_COMPAT_MODE),
	MAP(RES_OPTS_EXPLICIT_DRBD8_COMPAT, RESOURCE_OPTS_EXPLICIT_DRBD8_COMPAT),
};

static const __u16 net_conf_map[DRBD_A_NET_CONF_RDMA_CTRL_SNDBUF_SIZE + 1] = {
	MAP(NET_CONF_SHARED_SECRET, NET_CONF_SHARED_SECRET),
	MAP(NET_CONF_CRAM_HMAC_ALG, NET_CONF_CRAM_HMAC_ALG),
	MAP(NET_CONF_INTEGRITY_ALG, NET_CONF_INTEGRITY_ALG),
	MAP(NET_CONF_VERIFY_ALG, NET_CONF_VERIFY_ALG),
	MAP(NET_CONF_CSUMS_ALG, NET_CONF_CSUMS_ALG),
	MAP(NET_CONF_WIRE_PROTOCOL, NET_CONF_PROTOCOL),
	MAP(NET_CONF_CONNECT_INT, NET_CONF_CONNECT_INT),
	MAP(NET_CONF_TIMEOUT, NET_CONF_TIMEOUT),
	MAP(NET_CONF_PING_INT, NET_CONF_PING_INT),
	MAP(NET_CONF_PING_TIMEO, NET_CONF_PING_TIMEO),
	MAP(NET_CONF_SNDBUF_SIZE, NET_CONF_SNDBUF_SIZE),
	MAP(NET_CONF_RCVBUF_SIZE, NET_CONF_RCVBUF_SIZE),
	MAP(NET_CONF_KO_COUNT, NET_CONF_KO_COUNT),
	MAP(NET_CONF_MAX_EPOCH_SIZE, NET_CONF_MAX_EPOCH_SIZE),
	MAP(NET_CONF_AFTER_SB_0P, NET_CONF_AFTER_SB_0PRI),
	MAP(NET_CONF_AFTER_SB_1P, NET_CONF_AFTER_SB_1PRI),
	MAP(NET_CONF_AFTER_SB_2P, NET_CONF_AFTER_SB_2PRI),
	MAP(NET_CONF_RR_CONFLICT, NET_CONF_RR_CONFLICT),
	MAP(NET_CONF_ON_CONGESTION, NET_CONF_ON_CONGESTION),
	MAP(NET_CONF_CONG_FILL, NET_CONF_CONG_FILL),
	MAP(NET_CONF_CONG_EXTENTS, NET_CONF_CONG_EXTENTS),
	MAP(NET_CONF_TWO_PRIMARIES, NET_CONF_TWO_PRIMARIES),
	MAP(NET_CONF_TCP_CORK, NET_CONF_TCP_CORK),
	MAP(NET_CONF_ALWAYS_ASBP, NET_CONF_ALWAYS_ASBP),
	MAP(NET_CONF_USE_RLE, NET_CONF_USE_RLE),
	MAP(NET_CONF_FENCING_POLICY, NET_CONF_FENCING),
	MAP(NET_CONF_NAME, NET_CONF_CONNECTION_NAME),
	MAP(NET_CONF_CSUMS_AFTER_CRASH_ONLY, NET_CONF_CSUMS_AFTER_CRASH_ONLY),
	MAP(NET_CONF_SOCK_CHECK_TIMEO, NET_CONF_SOCK_CHECK_TIMEO),
	MAP(NET_CONF_TRANSPORT_NAME, NET_CONF_TRANSPORT_NAME),
	MAP(NET_CONF_MAX_BUFFERS, NET_CONF_MAX_BUFFERS),
	MAP(NET_CONF_ALLOW_REMOTE_READ, NET_CONF_ALLOW_REMOTE_READ),
	MAP(NET_CONF_TLS, NET_CONF_TLS),
	MAP(NET_CONF_TLS_PRIVKEY, NET_CONF_TLS_PRIVKEY),
	MAP(NET_CONF_TLS_CERTIFICATE, NET_CONF_TLS_CERTIFICATE),
	MAP(NET_CONF_TLS_KEYRING, NET_CONF_TLS_KEYRING),
	MAP(NET_CONF_LOAD_BALANCE_PATHS, NET_CONF_LOAD_BALANCE_PATHS),
	MAP(NET_CONF_RDMA_CTRL_RCVBUF_SIZE, NET_CONF_RDMA_CTRL_RCVBUF_SIZE),
	MAP(NET_CONF_RDMA_CTRL_SNDBUF_SIZE, NET_CONF_RDMA_CTRL_SNDBUF_SIZE),
};

static const __u16 peer_device_conf_map[DRBD_A_PEER_DEVICE_CONF_PEER_TIEBREAKER + 1] = {
	MAP(PEER_DEVICE_CONF_RESYNC_RATE, PEER_DEVICE_CONF_RESYNC_RATE),
	MAP(PEER_DEVICE_CONF_C_PLAN_AHEAD, PEER_DEVICE_CONF_C_PLAN_AHEAD),
	MAP(PEER_DEVICE_CONF_C_DELAY_TARGET, PEER_DEVICE_CONF_C_DELAY_TARGET),
	MAP(PEER_DEVICE_CONF_C_FILL_TARGET, PEER_DEVICE_CONF_C_FILL_TARGET),
	MAP(PEER_DEVICE_CONF_C_MAX_RATE, PEER_DEVICE_CONF_C_MAX_RATE),
	MAP(PEER_DEVICE_CONF_C_MIN_RATE, PEER_DEVICE_CONF_C_MIN_RATE),
	MAP(PEER_DEVICE_CONF_BITMAP, PEER_DEVICE_CONF_BITMAP),
	MAP(PEER_DEVICE_CONF_RESYNC_WITHOUT_REPLICATION, PEER_DEVICE_CONF_RESYNC_WITHOUT_REPLICATION),
	MAP(PEER_DEVICE_CONF_PEER_TIEBREAKER, PEER_DEVICE_CONF_PEER_TIEBREAKER),
};

static const __u16 device_conf_map[DRBD_A_DEVICE_CONF_DISCARD_GRANULARITY + 1] = {
	MAP(DEVICE_CONF_MAX_BIO_SIZE, DEVICE_CONF_MAX_BIO_SIZE),
	MAP(DEVICE_CONF_INTENTIONAL_DISKLESS, DEVICE_CONF_INTENTIONAL_DISKLESS),
	MAP(DEVICE_CONF_BLOCK_SIZE, DEVICE_CONF_BLOCK_SIZE),
	MAP(DEVICE_CONF_DISCARD_GRANULARITY, DEVICE_CONF_DISCARD_GRANULARITY),
};

static const __u16 set_role_parms_map[DRBD_A_SET_ROLE_PARMS_FORCE + 1] = {
	MAP(SET_ROLE_PARMS_FORCE, SET_ROLE_PARMS_FORCE),
};

static const __u16 resize_parms_map[DRBD_A_RESIZE_PARMS_AL_STRIPE_SIZE + 1] = {
	MAP(RESIZE_PARMS_RESIZE_SIZE, RESIZE_PARMS_SIZE),
	MAP(RESIZE_PARMS_RESIZE_FORCE, RESIZE_PARMS_ASSUME_PEER_HAS_SPACE),
	MAP(RESIZE_PARMS_NO_RESYNC, RESIZE_PARMS_ASSUME_CLEAN),
	MAP(RESIZE_PARMS_AL_STRIPES, RESIZE_PARMS_AL_STRIPES),
	MAP(RESIZE_PARMS_AL_STRIPE_SIZE, RESIZE_PARMS_AL_STRIPE_SIZE),
};

static const __u16 start_ov_parms_map[DRBD_A_START_OV_PARMS_OV_STOP_SECTOR + 1] = {
	MAP(START_OV_PARMS_OV_START_SECTOR, START_OV_PARMS_START_SECTOR),
	MAP(START_OV_PARMS_OV_STOP_SECTOR, START_OV_PARMS_STOP_SECTOR),
};

static const __u16 new_c_uuid_parms_map[DRBD_A_NEW_C_UUID_PARMS_FORCE_RESYNC + 1] = {
	MAP(NEW_C_UUID_PARMS_CLEAR_BM, NEW_CURRENT_UUID_PARMS_CLEAR_BM),
	MAP(NEW_C_UUID_PARMS_FORCE_RESYNC, NEW_CURRENT_UUID_PARMS_FORCE_RESYNC),
};

static const __u16 disconnect_parms_map[DRBD_A_DISCONNECT_PARMS_FORCE_DISCONNECT + 1] = {
	MAP(DISCONNECT_PARMS_FORCE_DISCONNECT, DISCONNECT_PARMS_FORCE),
};

static const __u16 detach_parms_map[DRBD_A_DETACH_PARMS_INTENTIONAL_DISKLESS_DETACH + 1] = {
	MAP(DETACH_PARMS_FORCE_DETACH, DETACH_PARMS_FORCE),
	MAP(DETACH_PARMS_INTENTIONAL_DISKLESS_DETACH, DETACH_PARMS_INTENTIONAL_DISKLESS_DETACH),
};

static const __u16 invalidate_parms_map[DRBD_A_INVALIDATE_PARMS_RESET_BITMAP + 1] = {
	MAP(INVALIDATE_PARMS_SYNC_FROM_PEER_NODE_ID, INVALIDATE_PARMS_SYNC_FROM_PEER_NODE_ID),
	MAP(INVALIDATE_PARMS_RESET_BITMAP, INVALIDATE_PARMS_RESET_BITMAP),
};

static const __u16 invalidate_peer_parms_map[DRBD_A_INVALIDATE_PEER_PARMS_P_RESET_BITMAP + 1] = {
	MAP(INVALIDATE_PEER_PARMS_P_RESET_BITMAP, INVALIDATE_PEER_PARMS_RESET_BITMAP),
};

static const __u16 connect_parms_map[DRBD_A_CONNECT_PARMS_DISCARD_MY_DATA + 1] = {
	MAP(CONNECT_PARMS_TENTATIVE, CONNECT_PARMS_TENTATIVE),
	MAP(CONNECT_PARMS_DISCARD_MY_DATA, CONNECT_PARMS_DISCARD_MY_DATA),
};

static const __u16 rename_resource_parms_map[DRBD_A_RENAME_RESOURCE_PARMS_NEW_RESOURCE_NAME + 1] = {
	MAP(RENAME_RESOURCE_PARMS_NEW_RESOURCE_NAME, RENAME_PARMS_NEW_NAME),
};

static const __u16 suspend_io_parms_map[DRBD_A_SUSPEND_IO_PARMS_BDEV_FREEZE + 1] = {
	MAP(SUSPEND_IO_PARMS_BDEV_FREEZE, SUSPEND_IO_PARMS_BDEV_FREEZE),
};
#undef MAP

struct drbd2_attr_map {
	const __u16 *fwd;		/* legacy id -> drbd2 id */
	int fwd_max;			/* highest legacy id */
	__u16 *rev;			/* drbd2 id -> legacy id, filled lazily */
	const struct nla_policy *policy;/* the drbd2 nest policy */
	int max;			/* highest drbd2 id */
};

#define ATTR_MAP(name, NAME)						\
	{ name ## _map, ARRAY_SIZE(name ## _map) - 1,			\
	  (__u16 [DRBD2_A_ ## NAME ## _MAX + 1]){},			\
	  drbd2_ ## name ## _nl_policy, DRBD2_A_ ## NAME ## _MAX }

static struct drbd2_attr_map attr_maps[__NL_SET_MAX] = {
	[NL_SET_DISK_CONF]		= ATTR_MAP(disk_conf, DISK_CONF),
	[NL_SET_NET_CONF]		= ATTR_MAP(net_conf, NET_CONF),
	[NL_SET_RES_OPTS]		= { res_opts_map, ARRAY_SIZE(res_opts_map) - 1,
					    (__u16 [DRBD2_A_RESOURCE_OPTS_MAX + 1]){},
					    drbd2_resource_opts_nl_policy, DRBD2_A_RESOURCE_OPTS_MAX },
	[NL_SET_PEER_DEVICE_CONF]	= ATTR_MAP(peer_device_conf, PEER_DEVICE_CONF),
	[NL_SET_DEVICE_CONF]		= ATTR_MAP(device_conf, DEVICE_CONF),
	[NL_SET_SET_ROLE_PARMS]		= ATTR_MAP(set_role_parms, SET_ROLE_PARMS),
	[NL_SET_RESIZE_PARMS]		= ATTR_MAP(resize_parms, RESIZE_PARMS),
	[NL_SET_START_OV_PARMS]		= ATTR_MAP(start_ov_parms, START_OV_PARMS),
	[NL_SET_NEW_C_UUID_PARMS]	= { new_c_uuid_parms_map, ARRAY_SIZE(new_c_uuid_parms_map) - 1,
					    (__u16 [DRBD2_A_NEW_CURRENT_UUID_PARMS_MAX + 1]){},
					    drbd2_new_current_uuid_parms_nl_policy,
					    DRBD2_A_NEW_CURRENT_UUID_PARMS_MAX },
	[NL_SET_DISCONNECT_PARMS]	= ATTR_MAP(disconnect_parms, DISCONNECT_PARMS),
	[NL_SET_DETACH_PARMS]		= ATTR_MAP(detach_parms, DETACH_PARMS),
	[NL_SET_INVALIDATE_PARMS]	= ATTR_MAP(invalidate_parms, INVALIDATE_PARMS),
	[NL_SET_INVALIDATE_PEER_PARMS]	= ATTR_MAP(invalidate_peer_parms, INVALIDATE_PEER_PARMS),
	[NL_SET_CONNECT_PARMS]		= ATTR_MAP(connect_parms, CONNECT_PARMS),
	[NL_SET_RENAME_RESOURCE_PARMS]	= { rename_resource_parms_map,
					    ARRAY_SIZE(rename_resource_parms_map) - 1,
					    (__u16 [DRBD2_A_RENAME_PARMS_MAX + 1]){},
					    drbd2_rename_parms_nl_policy, DRBD2_A_RENAME_PARMS_MAX },
	[NL_SET_SUSPEND_IO_PARMS]	= ATTR_MAP(suspend_io_parms, SUSPEND_IO_PARMS),
};
#undef ATTR_MAP

static void drbd2_maps_init(void)
{
	static bool done;
	int set, i;

	if (done)
		return;
	for (set = 1; set < __NL_SET_MAX; set++) {
		struct drbd2_attr_map *m = &attr_maps[set];

		if (!m->fwd)
			continue;
		for (i = 1; i <= m->fwd_max; i++)
			if (m->fwd[i] && m->fwd[i] <= m->max)
				m->rev[m->fwd[i]] = i;
	}
	done = true;
}

static bool drbd2_cmd_known(enum drbd_nl_cmd cmd)
{
	/* one spec, one version: every command is there */
	return cmd > DRBD_NL_CMD_NONE && cmd < __DRBD_NL_CMD_MAX && drbd2_cmd[cmd] != 0;
}

static int drbd2_put_request(struct msg_buff *msg, enum drbd_nl_cmd cmd, int nlflags)
{
	drbd2_maps_init();
	if (!drbd2_cmd_known(cmd))
		return -EINVAL;
	if (!genlmsg_put(msg, &drbd2_family, nlflags, drbd2_cmd[cmd]))
		return -EMSGSIZE;
	return 0;
}

/* A sockaddr blob as an address nest: family, port, and the address bytes.
 * The drbd2 family only knows how to carry AF_INET/AF_INET6 addresses; any
 * other family (SDP, SSOCKS, ...) is refused here rather than silently sent
 * on the wire as a bare family with no address, which the kernel would
 * otherwise happily accept and misinterpret. */
int drbd2_put_address(struct msg_buff *msg, int attrtype, const void *sockaddr, int len)
{
	const struct sockaddr_storage *ss = sockaddr;
	struct nlattr *nla;

	if (ss->ss_family != AF_INET && ss->ss_family != AF_INET6) {
		fprintf(stderr, "address family %d is not supported by the drbd2 netlink family\n",
			ss->ss_family);
		return -EAFNOSUPPORT;
	}
	nla = nla_nest_start(msg, attrtype);
	if (!nla)
		return -EMSGSIZE;
	nla_put_u16(msg, DRBD2_A_ADDRESS_FAMILY, ss->ss_family);
	if (ss->ss_family == AF_INET && len >= (int)sizeof(struct sockaddr_in)) {
		const struct sockaddr_in *sin = sockaddr;

		nla_put(msg, DRBD2_A_ADDRESS_PORT, sizeof(sin->sin_port), &sin->sin_port);
		nla_put(msg, DRBD2_A_ADDRESS_IPV4, sizeof(sin->sin_addr), &sin->sin_addr);
	} else if (ss->ss_family == AF_INET6 && len >= (int)sizeof(struct sockaddr_in6)) {
		const struct sockaddr_in6 *sin6 = sockaddr;

		nla_put(msg, DRBD2_A_ADDRESS_PORT, sizeof(sin6->sin6_port), &sin6->sin6_port);
		nla_put(msg, DRBD2_A_ADDRESS_IPV6, sizeof(sin6->sin6_addr), &sin6->sin6_addr);
	}
	nla_nest_end(msg, nla);
	return 0;
}

/* Everything that identifies the object goes into the one context nest. */
static int drbd2_put_context(struct msg_buff *msg, const struct drbd_cfg_context *ctx,
			     unsigned int minor, unsigned int what,
			     enum drbd_nl_attr_set set)
{
	struct nlattr *nla;
	int err = 0;

	if (!(what & (CTX_RESOURCE | CTX_MINOR | CTX_VOLUME | CTX_PEER_NODE_ID |
		      CTX_MY_ADDR | CTX_PEER_ADDR | CTX_ALL)))
		return 0;

	nla = nla_nest_start(msg, DRBD2_A_CONTEXT);
	if (!nla)
		return -EMSGSIZE;
	if (what & CTX_RESOURCE)
		nla_put_string(msg, DRBD2_A_CONTEXT_RESOURCE_NAME, ctx->ctx_resource_name);
	if (what & CTX_MINOR)
		nla_put_u32(msg, DRBD2_A_CONTEXT_MINOR, minor);
	if (what & CTX_VOLUME)
		nla_put_u32(msg, DRBD2_A_CONTEXT_VOLUME, ctx->ctx_volume);
	if (what & CTX_PEER_NODE_ID)
		nla_put_u32(msg, DRBD2_A_CONTEXT_PEER_NODE_ID, ctx->ctx_peer_node_id);
	if (what & CTX_MY_ADDR) {
		err = drbd2_put_address(msg, DRBD2_A_CONTEXT_MY_ADDRESS, ctx->ctx_my_addr,
					 ctx->ctx_my_addr_len);
		if (err)
			goto out;
	}
	if (what & CTX_PEER_ADDR) {
		err = drbd2_put_address(msg, DRBD2_A_CONTEXT_PEER_ADDRESS, ctx->ctx_peer_addr,
					 ctx->ctx_peer_addr_len);
		if (err)
			goto out;
	}
out:
	/* libgenl has no nla_nest_cancel(); the caller aborts the whole
	 * request on a nonzero return anyway, so just close the nest. */
	nla_nest_end(msg, nla);
	return err;
}

static struct nlattr *drbd2_nest_start(struct msg_buff *msg, enum drbd_nl_attr_set set)
{
	if (set <= NL_SET_NONE || set >= __NL_SET_MAX || !drbd2_set[set])
		return NULL;
	return nla_nest_start(msg, drbd2_set[set]);
}

static int drbd2_attr_id(enum drbd_nl_attr_set set, int field_id)
{
	const struct drbd2_attr_map *m;

	drbd2_maps_init();
	if (set <= NL_SET_NONE || set >= __NL_SET_MAX)
		return -1;
	m = &attr_maps[set];
	if (!m->fwd || field_id <= 0 || field_id > m->fwd_max || !m->fwd[field_id])
		return -1;
	return m->fwd[field_id];
}

static const struct nla_policy *drbd2_get_policy(enum drbd_nl_attr_set set, int *maxtype)
{
	if (set <= NL_SET_NONE || set >= __NL_SET_MAX || !attr_maps[set].policy)
		return NULL;
	*maxtype = attr_maps[set].max;
	return attr_maps[set].policy;
}

static void drbd2_put_set_defaults(struct msg_buff *msg)
{
	nla_put_flag(msg, DRBD2_A_SET_DEFAULTS);
}

/*
 * A legacy-numbered option nest as a drbd2 nest (used by the test encoder;
 * the reverse direction lives in the receive half).
 */
struct nlattr *drbd2_renumber_to_wire(struct msg_buff *msg, int attrtype,
				      const struct nlattr *legacy_nest,
				      enum drbd_nl_attr_set set)
{
	const struct drbd2_attr_map *m = &attr_maps[set];
	struct nlattr *out, *a;
	int rem;

	drbd2_maps_init();
	out = nla_nest_start(msg, attrtype);
	if (!out)
		return NULL;
	nla_for_each_nested(a, legacy_nest, rem) {
		int t = nla_type(a);

		if (t <= 0 || t > m->fwd_max || !m->fwd[t])
			continue;
		nla_put(msg, m->fwd[t], nla_len(a), nla_data(a));
	}
	nla_nest_end(msg, out);
	return out;
}

/* state-result back to the drbd_state_rv the command logic tests. */
static int drbd2_state_result_to_rv(__u32 v)
{
	if (v == DRBD2_STATE_RESULT_SUCCESS)
		return NO_ERROR;
	if (v > DRBD2_STATE_RESULT_SUCCESS)
		return SS_SUCCESS + (v - DRBD2_STATE_RESULT_SUCCESS);
	return -(int)v;
}

static char drbd2_info_buf[DEFAULT_MSG_SIZE];

static int drbd2_recv_outcome(struct nlmsghdr *nlh, struct drbd_nl_outcome *out)
{
	struct nlattr *tb[DRBD2_A_MAX + 1];

	memset(out, 0, sizeof(*out));
	out->timeout_type = -1;

	if (nlh->nlmsg_type == NLMSG_ERROR) {
		/* This family reports a refused request as errno plus text. */
		if (drbd_nl_recv_error(nlh, out))
			out->ret_code = ERR_EXTACK;
		return 0;
	}
	if (nlh->nlmsg_type != drbd2_family.id)
		return -EINVAL;
	if (nla_parse(tb, DRBD2_A_MAX, nlmsg_attrdata(nlh, GENL_HDRLEN),
		      nlmsg_attrlen(nlh, GENL_HDRLEN), drbd2_tla_nl_policy))
		return -EINVAL;

	out->ret_code = tb[DRBD2_A_STATE_RESULT] ?
		drbd2_state_result_to_rv(nla_get_u32(tb[DRBD2_A_STATE_RESULT])) : NO_ERROR;

	if (tb[DRBD2_A_MESSAGE]) {
		/* one string, lines separated by newlines -> NUL separated texts */
		int len = nla_strlcpy(drbd2_info_buf, tb[DRBD2_A_MESSAGE], sizeof(drbd2_info_buf));
		char *p;

		if (len >= (int)sizeof(drbd2_info_buf))
			len = sizeof(drbd2_info_buf) - 1;
		for (p = drbd2_info_buf; (p = strchr(p, '\n')); p++)
			*p = '\0';
		out->info = drbd2_info_buf;
		out->info_len = len + 1;
	}
	if (tb[DRBD2_A_TIMEOUT_TYPE]) {
		switch (nla_get_u32(tb[DRBD2_A_TIMEOUT_TYPE])) {
		case DRBD2_TIMEOUT_TYPE_DEGRADED:
			out->timeout_type = UT_DEGRADED;
			break;
		case DRBD2_TIMEOUT_TYPE_PEER_OUTDATED:
			out->timeout_type = UT_PEER_OUTDATED;
			break;
		default:
			out->timeout_type = UT_DEFAULT;
			break;
		}
	}
	return 0;
}

/* Filled in by the receive half (Task 9). */
static enum drbd_nl_msg drbd2_parse_msg(struct nlmsghdr *nlh, enum drbd_nl_cmd cmd,
					struct drbd_nl_event *ev)
{
	if (nlh->nlmsg_type == NLMSG_DONE)
		return NL_MSG_DONE;
	return NL_MSG_SKIP;
}

const struct drbd_nl_dialect drbd2_dialect = {
	.name = "drbd2",
	.family = &drbd2_family,
	.api_version = DRBD2_FAMILY_VERSION,
	.api_hdrsize = 0,
	.cmd_known = drbd2_cmd_known,
	.put_request = drbd2_put_request,
	.put_context = drbd2_put_context,
	.nest_start = drbd2_nest_start,
	.attr_id = drbd2_attr_id,
	.policy = drbd2_get_policy,
	.put_set_defaults = drbd2_put_set_defaults,
	.recv_outcome = drbd2_recv_outcome,
	.parse_msg = drbd2_parse_msg,
};
