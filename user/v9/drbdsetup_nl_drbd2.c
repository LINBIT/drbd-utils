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
static char drbd2_extack_buf[256];

static int drbd2_recv_outcome(struct nlmsghdr *nlh, struct drbd_nl_outcome *out)
{
	struct nlattr *tb[DRBD2_A_MAX + 1];

	memset(out, 0, sizeof(*out));
	out->timeout_type = -1;

	if (nlh->nlmsg_type == NLMSG_ERROR) {
		struct nlmsgerr *e = nlmsg_data(nlh);

		if (e->error == 0) {
			out->ret_code = NO_ERROR;
			return 0;
		}
		out->ret_code = ERR_EXTACK;
		out->errnum = -e->error;
		if (genl_extack_msg(nlh, drbd2_extack_buf, sizeof(drbd2_extack_buf)))
			out->desc = drbd2_extack_buf;
		else
			out->desc = strerror(out->errnum);
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

/*
 * Receive side. Every object nest is turned into a struct drbd_nl_event.
 * The option nests are copied with their attribute ids rewritten into the
 * legacy numbering so that print_options() and friends work unchanged;
 * the connection's paths become the legacy PATH_PARMS blob list.
 */

static struct msg_buff *scratch;

static void drbd2_scratch_reset(void)
{
	if (!scratch) {
		scratch = msg_new(2 * DEFAULT_MSG_SIZE);
		if (!scratch) {
			fprintf(stderr, "out of memory\n");
			exit(20);
		}
	}
	scratch->tail = scratch->data;
}

/* An address nest into the sockaddr blob the legacy code works with. */
static int drbd2_get_address(const struct nlattr *nest, char *dst, __u32 *dst_len)
{
	struct nlattr *tb[DRBD2_A_ADDRESS_MAX + 1];

	if (nla_parse_nested(tb, DRBD2_A_ADDRESS_MAX, nest, drbd2_address_nl_policy))
		return -EINVAL;
	if (!tb[DRBD2_A_ADDRESS_FAMILY])
		return -ENOMSG;

	switch (nla_get_u16(tb[DRBD2_A_ADDRESS_FAMILY])) {
	case AF_INET: {
		struct sockaddr_in sin = { .sin_family = AF_INET };

		if (tb[DRBD2_A_ADDRESS_PORT])
			memcpy(&sin.sin_port, nla_data(tb[DRBD2_A_ADDRESS_PORT]), sizeof(sin.sin_port));
		if (tb[DRBD2_A_ADDRESS_IPV4])
			memcpy(&sin.sin_addr, nla_data(tb[DRBD2_A_ADDRESS_IPV4]), sizeof(sin.sin_addr));
		memcpy(dst, &sin, sizeof(sin));
		*dst_len = sizeof(sin);
		return 0;
	}
	case AF_INET6: {
		struct sockaddr_in6 sin6 = { .sin6_family = AF_INET6 };

		if (tb[DRBD2_A_ADDRESS_PORT])
			memcpy(&sin6.sin6_port, nla_data(tb[DRBD2_A_ADDRESS_PORT]), sizeof(sin6.sin6_port));
		if (tb[DRBD2_A_ADDRESS_IPV6])
			memcpy(&sin6.sin6_addr, nla_data(tb[DRBD2_A_ADDRESS_IPV6]), sizeof(sin6.sin6_addr));
		memcpy(dst, &sin6, sizeof(sin6));
		*dst_len = sizeof(sin6);
		return 0;
	}
	default:
		return -ENOMSG;
	}
}

static int drbd2_get_context(const struct nlattr *nest, struct drbd_cfg_context *ctx,
			     unsigned int *minor)
{
	struct nlattr *tb[DRBD2_A_CONTEXT_MAX + 1], *a;

	if (nla_parse_nested(tb, DRBD2_A_CONTEXT_MAX, nest, drbd2_context_nl_policy))
		return -EINVAL;
	if ((a = tb[DRBD2_A_CONTEXT_RESOURCE_NAME]))
		ctx->ctx_resource_name_len = nla_strlcpy(ctx->ctx_resource_name, a,
							 sizeof(ctx->ctx_resource_name));
	if ((a = tb[DRBD2_A_CONTEXT_VOLUME]))
		ctx->ctx_volume = nla_get_u32(a);
	if ((a = tb[DRBD2_A_CONTEXT_MINOR]))
		*minor = nla_get_u32(a);
	if ((a = tb[DRBD2_A_CONTEXT_PEER_NODE_ID]))
		ctx->ctx_peer_node_id = nla_get_u32(a);
	if ((a = tb[DRBD2_A_CONTEXT_CONNECTION_NAME]))
		ctx->ctx_conn_name_len = nla_strlcpy(ctx->ctx_conn_name, a, sizeof(ctx->ctx_conn_name));
	if ((a = tb[DRBD2_A_CONTEXT_MY_ADDRESS]))
		drbd2_get_address(a, ctx->ctx_my_addr, &ctx->ctx_my_addr_len);
	if ((a = tb[DRBD2_A_CONTEXT_PEER_ADDRESS]))
		drbd2_get_address(a, ctx->ctx_peer_addr, &ctx->ctx_peer_addr_len);
	return 0;
}

#define GET_U8(field, ATTR)  do { if (tb[ATTR]) s->field = nla_get_u8(tb[ATTR]); } while (0)
#define GET_U32(field, ATTR) do { if (tb[ATTR]) s->field = nla_get_u32(tb[ATTR]); } while (0)
#define GET_U64(field, ATTR) do { if (tb[ATTR]) s->field = nla_get_u64(tb[ATTR]); } while (0)

static int drbd2_get_resource_info(const struct nlattr *nest, struct resource_info *s)
{
	struct nlattr *tb[DRBD2_A_RESOURCE_INFO_MAX + 1];

	if (nla_parse_nested(tb, DRBD2_A_RESOURCE_INFO_MAX, nest, drbd2_resource_info_nl_policy))
		return -EINVAL;
	GET_U32(res_role, DRBD2_A_RESOURCE_INFO_ROLE);
	GET_U8(res_susp, DRBD2_A_RESOURCE_INFO_SUSP);
	GET_U8(res_susp_nod, DRBD2_A_RESOURCE_INFO_SUSP_NOD);
	GET_U8(res_susp_fen, DRBD2_A_RESOURCE_INFO_SUSP_FEN);
	GET_U8(res_susp_quorum, DRBD2_A_RESOURCE_INFO_SUSP_QUORUM);
	GET_U8(res_fail_io, DRBD2_A_RESOURCE_INFO_FAIL_IO);
	return 0;
}

static int drbd2_get_resource_statistics(const struct nlattr *nest, struct resource_statistics *s)
{
	struct nlattr *tb[DRBD2_A_RESOURCE_STATISTICS_MAX + 1];

	if (nla_parse_nested(tb, DRBD2_A_RESOURCE_STATISTICS_MAX, nest,
			     drbd2_resource_statistics_nl_policy))
		return -EINVAL;
	GET_U32(res_stat_write_ordering, DRBD2_A_RESOURCE_STATISTICS_WRITE_ORDERING);
	return 0;
}

static int drbd2_get_device_info(const struct nlattr *nest, struct device_info *s)
{
	struct nlattr *tb[DRBD2_A_DEVICE_INFO_MAX + 1];

	if (nla_parse_nested(tb, DRBD2_A_DEVICE_INFO_MAX, nest, drbd2_device_info_nl_policy))
		return -EINVAL;
	GET_U32(dev_disk_state, DRBD2_A_DEVICE_INFO_DISK_STATE);
	GET_U8(is_intentional_diskless, DRBD2_A_DEVICE_INFO_IS_INTENTIONAL_DISKLESS);
	GET_U8(dev_has_quorum, DRBD2_A_DEVICE_INFO_HAS_QUORUM);
	GET_U8(dev_is_open, DRBD2_A_DEVICE_INFO_IS_OPEN);
	if (tb[DRBD2_A_DEVICE_INFO_BACKING_DEV_PATH])
		s->backing_dev_path_len = nla_strlcpy(s->backing_dev_path,
						      tb[DRBD2_A_DEVICE_INFO_BACKING_DEV_PATH],
						      sizeof(s->backing_dev_path));
	return 0;
}

static int drbd2_get_device_statistics(const struct nlattr *nest, struct device_statistics *s)
{
	struct nlattr *tb[DRBD2_A_DEVICE_STATISTICS_MAX + 1];

	if (nla_parse_nested(tb, DRBD2_A_DEVICE_STATISTICS_MAX, nest,
			     drbd2_device_statistics_nl_policy))
		return -EINVAL;
	GET_U64(dev_size, DRBD2_A_DEVICE_STATISTICS_SIZE);
	GET_U64(dev_read, DRBD2_A_DEVICE_STATISTICS_READ);
	GET_U64(dev_write, DRBD2_A_DEVICE_STATISTICS_WRITE);
	GET_U64(dev_al_writes, DRBD2_A_DEVICE_STATISTICS_AL_WRITES);
	GET_U64(dev_bm_writes, DRBD2_A_DEVICE_STATISTICS_BM_WRITES);
	GET_U32(dev_upper_pending, DRBD2_A_DEVICE_STATISTICS_UPPER_PENDING);
	GET_U32(dev_lower_pending, DRBD2_A_DEVICE_STATISTICS_LOWER_PENDING);
	GET_U8(dev_upper_blocked, DRBD2_A_DEVICE_STATISTICS_UPPER_BLOCKED);
	GET_U8(dev_lower_blocked, DRBD2_A_DEVICE_STATISTICS_LOWER_BLOCKED);
	GET_U8(dev_al_suspended, DRBD2_A_DEVICE_STATISTICS_AL_SUSPENDED);
	GET_U64(dev_exposed_data_uuid, DRBD2_A_DEVICE_STATISTICS_EXPOSED_DATA_UUID);
	GET_U64(dev_current_uuid, DRBD2_A_DEVICE_STATISTICS_CURRENT_UUID);
	GET_U32(dev_disk_flags, DRBD2_A_DEVICE_STATISTICS_DISK_FLAGS);
	if (tb[DRBD2_A_DEVICE_STATISTICS_HISTORY_UUIDS])
		s->history_uuids_len = nla_memcpy(s->history_uuids,
						  tb[DRBD2_A_DEVICE_STATISTICS_HISTORY_UUIDS],
						  HISTORY_UUIDS_SIZE);
	return 0;
}

static int drbd2_get_connection_info(const struct nlattr *nest, struct connection_info *s)
{
	struct nlattr *tb[DRBD2_A_CONNECTION_INFO_MAX + 1];

	if (nla_parse_nested(tb, DRBD2_A_CONNECTION_INFO_MAX, nest, drbd2_connection_info_nl_policy))
		return -EINVAL;
	GET_U32(conn_connection_state, DRBD2_A_CONNECTION_INFO_CONNECTION_STATE);
	GET_U32(conn_role, DRBD2_A_CONNECTION_INFO_ROLE);
	return 0;
}

static int drbd2_get_connection_statistics(const struct nlattr *nest, struct connection_statistics *s)
{
	struct nlattr *tb[DRBD2_A_CONNECTION_STATISTICS_MAX + 1];

	if (nla_parse_nested(tb, DRBD2_A_CONNECTION_STATISTICS_MAX, nest,
			     drbd2_connection_statistics_nl_policy))
		return -EINVAL;
	GET_U8(conn_congested, DRBD2_A_CONNECTION_STATISTICS_CONGESTED);
	GET_U64(ap_in_flight, DRBD2_A_CONNECTION_STATISTICS_AP_IN_FLIGHT);
	GET_U64(rs_in_flight, DRBD2_A_CONNECTION_STATISTICS_RS_IN_FLIGHT);
	return 0;
}

static int drbd2_get_peer_device_info(const struct nlattr *nest, struct peer_device_info *s)
{
	struct nlattr *tb[DRBD2_A_PEER_DEVICE_INFO_MAX + 1];

	if (nla_parse_nested(tb, DRBD2_A_PEER_DEVICE_INFO_MAX, nest, drbd2_peer_device_info_nl_policy))
		return -EINVAL;
	GET_U32(peer_repl_state, DRBD2_A_PEER_DEVICE_INFO_REPL_STATE);
	GET_U32(peer_disk_state, DRBD2_A_PEER_DEVICE_INFO_DISK_STATE);
	/* u8 on the drbd2 wire, u32 in the legacy struct */
	GET_U8(peer_resync_susp_user, DRBD2_A_PEER_DEVICE_INFO_RESYNC_SUSP_USER);
	GET_U8(peer_resync_susp_peer, DRBD2_A_PEER_DEVICE_INFO_RESYNC_SUSP_PEER);
	GET_U8(peer_resync_susp_dependency, DRBD2_A_PEER_DEVICE_INFO_RESYNC_SUSP_DEPENDENCY);
	GET_U8(peer_is_intentional_diskless, DRBD2_A_PEER_DEVICE_INFO_IS_INTENTIONAL_DISKLESS);
	GET_U8(peer_resync_susp_max_parallel, DRBD2_A_PEER_DEVICE_INFO_RESYNC_SUSP_MAX_PARALLEL);
	return 0;
}

static int drbd2_get_peer_device_statistics(const struct nlattr *nest, struct peer_device_statistics *s)
{
	struct nlattr *tb[DRBD2_A_PEER_DEVICE_STATISTICS_MAX + 1];

	if (nla_parse_nested(tb, DRBD2_A_PEER_DEVICE_STATISTICS_MAX, nest,
			     drbd2_peer_device_statistics_nl_policy))
		return -EINVAL;
	GET_U64(peer_dev_received, DRBD2_A_PEER_DEVICE_STATISTICS_RECEIVED);
	GET_U64(peer_dev_sent, DRBD2_A_PEER_DEVICE_STATISTICS_SENT);
	GET_U32(peer_dev_pending, DRBD2_A_PEER_DEVICE_STATISTICS_PENDING);
	GET_U32(peer_dev_unacked, DRBD2_A_PEER_DEVICE_STATISTICS_UNACKED);
	GET_U64(peer_dev_out_of_sync, DRBD2_A_PEER_DEVICE_STATISTICS_OUT_OF_SYNC);
	GET_U64(peer_dev_resync_failed, DRBD2_A_PEER_DEVICE_STATISTICS_RESYNC_FAILED);
	GET_U64(peer_dev_bitmap_uuid, DRBD2_A_PEER_DEVICE_STATISTICS_BITMAP_UUID);
	GET_U32(peer_dev_flags, DRBD2_A_PEER_DEVICE_STATISTICS_FLAGS);
	GET_U64(peer_dev_rs_total, DRBD2_A_PEER_DEVICE_STATISTICS_RS_TOTAL);
	GET_U64(peer_dev_ov_start_sector, DRBD2_A_PEER_DEVICE_STATISTICS_OV_START_SECTOR);
	GET_U64(peer_dev_ov_stop_sector, DRBD2_A_PEER_DEVICE_STATISTICS_OV_STOP_SECTOR);
	GET_U64(peer_dev_ov_position, DRBD2_A_PEER_DEVICE_STATISTICS_OV_POSITION);
	GET_U64(peer_dev_ov_left, DRBD2_A_PEER_DEVICE_STATISTICS_OV_LEFT);
	GET_U64(peer_dev_ov_skipped, DRBD2_A_PEER_DEVICE_STATISTICS_OV_SKIPPED);
	GET_U64(peer_dev_rs_same_csum, DRBD2_A_PEER_DEVICE_STATISTICS_RS_SAME_CSUM);
	GET_U64(peer_dev_rs_dt_start_ms, DRBD2_A_PEER_DEVICE_STATISTICS_RS_DT_START_MS);
	GET_U64(peer_dev_rs_paused_ms, DRBD2_A_PEER_DEVICE_STATISTICS_RS_PAUSED_MS);
	GET_U64(peer_dev_rs_dt0_ms, DRBD2_A_PEER_DEVICE_STATISTICS_RS_DT0_MS);
	GET_U64(peer_dev_rs_db0_sectors, DRBD2_A_PEER_DEVICE_STATISTICS_RS_DB0_SECTORS);
	GET_U64(peer_dev_rs_dt1_ms, DRBD2_A_PEER_DEVICE_STATISTICS_RS_DT1_MS);
	GET_U64(peer_dev_rs_db1_sectors, DRBD2_A_PEER_DEVICE_STATISTICS_RS_DB1_SECTORS);
	GET_U32(peer_dev_rs_c_sync_rate, DRBD2_A_PEER_DEVICE_STATISTICS_RS_C_SYNC_RATE);
	GET_U64(peer_dev_uuid_flags, DRBD2_A_PEER_DEVICE_STATISTICS_UUID_FLAGS);
	return 0;
}

static int drbd2_get_path_info(const struct nlattr *nest, struct drbd_path_info *s)
{
	struct nlattr *tb[DRBD2_A_PATH_INFO_MAX + 1];

	if (nla_parse_nested(tb, DRBD2_A_PATH_INFO_MAX, nest, drbd2_path_info_nl_policy))
		return -EINVAL;
	GET_U8(path_established, DRBD2_A_PATH_INFO_ESTABLISHED);
	return 0;
}

#undef GET_U8
#undef GET_U32
#undef GET_U64

/* A drbd2 option nest as a legacy-numbered nest in the scratch buffer. */
static struct nlattr *drbd2_renumber_to_legacy(const struct nlattr *nest, int legacy_nest_type,
					       enum drbd_nl_attr_set set)
{
	const struct drbd2_attr_map *m = &attr_maps[set];
	struct nlattr *out, *a;
	int rem;

	if (!nest)
		return NULL;
	out = nla_nest_start(scratch, legacy_nest_type);
	if (!out)
		return NULL;
	nla_for_each_nested(a, nest, rem) {
		int t = nla_type(a);

		/* attribute 0 is the 64 bit padding; consumers memcpy anyway */
		if (t <= 0 || t > m->max || !m->rev[t])
			continue;
		if (nla_put(scratch, m->rev[t], nla_len(a), nla_data(a)) < 0)
			return NULL;
	}
	nla_nest_end(scratch, out);
	return out;
}

/* The path nests of a connection as the legacy PATH_PARMS blob list. */
static struct nlattr *drbd2_paths_to_legacy(const struct nlattr *connection_nest)
{
	struct nlattr *out = NULL, *a;
	int rem;

	nla_for_each_nested(a, connection_nest, rem) {
		struct nlattr *tb[DRBD2_A_PATH_MAX + 1];
		struct drbd_cfg_context pctx = { .ctx_volume = -1U, .ctx_peer_node_id = -1U };
		unsigned int unused_minor;

		if (nla_type(a) != DRBD2_A_CONNECTION_PATH)
			continue;
		if (nla_parse_nested(tb, DRBD2_A_PATH_MAX, a, drbd2_path_nl_policy) ||
		    !tb[DRBD2_A_PATH_CONTEXT])
			continue;
		if (drbd2_get_context(tb[DRBD2_A_PATH_CONTEXT], &pctx, &unused_minor))
			continue;
		if (!out) {
			out = nla_nest_start(scratch, DRBD_NLA_PATH_PARMS);
			if (!out)
				return NULL;
		}
		nla_put(scratch, DRBD_A_PATH_PARMS_MY_ADDR, pctx.ctx_my_addr_len, pctx.ctx_my_addr);
		nla_put(scratch, DRBD_A_PATH_PARMS_PEER_ADDR, pctx.ctx_peer_addr_len, pctx.ctx_peer_addr);
	}
	if (out)
		nla_nest_end(scratch, out);
	return out;
}

/* One object nest into the event. */
static int drbd2_object_to_event(const struct nlattr *nest, struct drbd_nl_event *ev)
{
	switch (ev->kind) {
	case NL_OBJ_RESOURCE: {
		struct nlattr *tb[DRBD2_A_RESOURCE_MAX + 1];

		if (nla_parse_nested(tb, DRBD2_A_RESOURCE_MAX, nest, drbd2_resource_nl_policy))
			return -EINVAL;
		if (tb[DRBD2_A_RESOURCE_CONTEXT])
			drbd2_get_context(tb[DRBD2_A_RESOURCE_CONTEXT], &ev->ctx, &ev->minor);
		if (tb[DRBD2_A_RESOURCE_INFO])
			ev->have_info = drbd2_get_resource_info(tb[DRBD2_A_RESOURCE_INFO],
								&ev->info.resource) == 0;
		if (tb[DRBD2_A_RESOURCE_STATISTICS])
			drbd2_get_resource_statistics(tb[DRBD2_A_RESOURCE_STATISTICS],
						      &ev->stats.resource);
		if (tb[DRBD2_A_RESOURCE_NEW_NAME])
			ev->rename.res_new_name_len = nla_strlcpy(ev->rename.res_new_name,
								  tb[DRBD2_A_RESOURCE_NEW_NAME],
								  sizeof(ev->rename.res_new_name));
		ev->opts = drbd2_renumber_to_legacy(tb[DRBD2_A_RESOURCE_RESOURCE_OPTS],
						    DRBD_NLA_RESOURCE_OPTS, NL_SET_RES_OPTS);
		return 0;
	}
	case NL_OBJ_DEVICE: {
		struct nlattr *tb[DRBD2_A_DEVICE_MAX + 1];

		if (nla_parse_nested(tb, DRBD2_A_DEVICE_MAX, nest, drbd2_device_nl_policy))
			return -EINVAL;
		if (tb[DRBD2_A_DEVICE_CONTEXT])
			drbd2_get_context(tb[DRBD2_A_DEVICE_CONTEXT], &ev->ctx, &ev->minor);
		if (tb[DRBD2_A_DEVICE_INFO])
			ev->have_info = drbd2_get_device_info(tb[DRBD2_A_DEVICE_INFO],
							      &ev->info.device) == 0;
		if (tb[DRBD2_A_DEVICE_STATISTICS])
			drbd2_get_device_statistics(tb[DRBD2_A_DEVICE_STATISTICS], &ev->stats.device);
		ev->opts = drbd2_renumber_to_legacy(tb[DRBD2_A_DEVICE_DISK_CONF],
						    DRBD_NLA_DISK_CONF, NL_SET_DISK_CONF);
		ev->opts2 = drbd2_renumber_to_legacy(tb[DRBD2_A_DEVICE_DEVICE_CONF],
						     DRBD_NLA_DEVICE_CONF, NL_SET_DEVICE_CONF);
		return 0;
	}
	case NL_OBJ_CONNECTION: {
		struct nlattr *tb[DRBD2_A_CONNECTION_MAX + 1];

		if (nla_parse_nested(tb, DRBD2_A_CONNECTION_MAX, nest, drbd2_connection_nl_policy))
			return -EINVAL;
		if (tb[DRBD2_A_CONNECTION_CONTEXT])
			drbd2_get_context(tb[DRBD2_A_CONNECTION_CONTEXT], &ev->ctx, &ev->minor);
		if (tb[DRBD2_A_CONNECTION_INFO])
			ev->have_info = drbd2_get_connection_info(tb[DRBD2_A_CONNECTION_INFO],
								  &ev->info.connection) == 0;
		if (tb[DRBD2_A_CONNECTION_STATISTICS])
			drbd2_get_connection_statistics(tb[DRBD2_A_CONNECTION_STATISTICS],
							&ev->stats.connection);
		ev->opts = drbd2_renumber_to_legacy(tb[DRBD2_A_CONNECTION_NET_CONF],
						    DRBD_NLA_NET_CONF, NL_SET_NET_CONF);
		ev->paths = drbd2_paths_to_legacy(nest);
		return 0;
	}
	case NL_OBJ_PEER_DEVICE: {
		struct nlattr *tb[DRBD2_A_PEER_DEVICE_MAX + 1];

		if (nla_parse_nested(tb, DRBD2_A_PEER_DEVICE_MAX, nest, drbd2_peer_device_nl_policy))
			return -EINVAL;
		if (tb[DRBD2_A_PEER_DEVICE_CONTEXT])
			drbd2_get_context(tb[DRBD2_A_PEER_DEVICE_CONTEXT], &ev->ctx, &ev->minor);
		if (tb[DRBD2_A_PEER_DEVICE_INFO])
			ev->have_info = drbd2_get_peer_device_info(tb[DRBD2_A_PEER_DEVICE_INFO],
								   &ev->info.peer_device) == 0;
		if (tb[DRBD2_A_PEER_DEVICE_STATISTICS])
			drbd2_get_peer_device_statistics(tb[DRBD2_A_PEER_DEVICE_STATISTICS],
							 &ev->stats.peer_device);
		ev->opts = drbd2_renumber_to_legacy(tb[DRBD2_A_PEER_DEVICE_PEER_DEVICE_CONF],
						    DRBD_NLA_PEER_DEVICE_OPTS, NL_SET_PEER_DEVICE_CONF);
		return 0;
	}
	case NL_OBJ_PATH: {
		struct nlattr *tb[DRBD2_A_PATH_MAX + 1];

		if (nla_parse_nested(tb, DRBD2_A_PATH_MAX, nest, drbd2_path_nl_policy))
			return -EINVAL;
		if (tb[DRBD2_A_PATH_CONTEXT])
			drbd2_get_context(tb[DRBD2_A_PATH_CONTEXT], &ev->ctx, &ev->minor);
		if (tb[DRBD2_A_PATH_INFO])
			ev->have_info = drbd2_get_path_info(tb[DRBD2_A_PATH_INFO], &ev->info.path) == 0;
		return 0;
	}
	default:
		return -EINVAL;
	}
}

/* The state-change attribute set has no generated policy: it is never nested. */
static const struct nla_policy drbd2_state_change_nl_policy[DRBD2_A_STATE_CHANGE_MAX + 1] = {
	[DRBD2_A_STATE_CHANGE_ACTION]		= { .type = NLA_U32 },
	[DRBD2_A_STATE_CHANGE_MORE]		= { .type = NLA_FLAG },
	[DRBD2_A_STATE_CHANGE_RESOURCE]		= { .type = NLA_NESTED },
	[DRBD2_A_STATE_CHANGE_DEVICE]		= { .type = NLA_NESTED },
	[DRBD2_A_STATE_CHANGE_CONNECTION]	= { .type = NLA_NESTED },
	[DRBD2_A_STATE_CHANGE_PEER_DEVICE]	= { .type = NLA_NESTED },
	[DRBD2_A_STATE_CHANGE_PATH]		= { .type = NLA_NESTED },
};

static enum drbd_notification_type drbd2_action(__u32 action)
{
	switch (action) {
	case DRBD2_STATE_CHANGE_ACTION_CREATE:	return NOTIFY_CREATE;
	case DRBD2_STATE_CHANGE_ACTION_CHANGE:	return NOTIFY_CHANGE;
	case DRBD2_STATE_CHANGE_ACTION_DESTROY:	return NOTIFY_DESTROY;
	case DRBD2_STATE_CHANGE_ACTION_RENAME:	return NOTIFY_RENAME;
	default:				return NOTIFY_EXISTS;
	}
}

static enum drbd_nl_msg drbd2_parse_msg(struct nlmsghdr *nlh, enum drbd_nl_cmd cmd,
					struct drbd_nl_event *ev)
{
	struct genlmsghdr *gh;
	struct nlattr *nest = NULL;
	enum drbd_nl_obj kind;

	if (nlh->nlmsg_type == NLMSG_DONE) {
		/* No explicit end-of-initial-state message in this family:
		 * the end of the state-get dump is that message. */
		if (cmd != DRBD_NL_CMD_GET_INITIAL_STATE)
			return NL_MSG_DONE;
		drbd_nl_event_init(ev, NL_OBJ_INITIAL_STATE_DONE);
		ev->seq = nlh->nlmsg_seq;
		return NL_MSG_EVENT;
	}
	if (nlh->nlmsg_type != drbd2_family.id)
		return NL_MSG_SKIP;

	drbd2_maps_init();
	drbd2_scratch_reset();
	gh = nlmsg_data(nlh);

	switch (gh->cmd) {
	case DRBD2_CMD_RESOURCE_GET:
	case DRBD2_CMD_DEVICE_GET:
	case DRBD2_CMD_CONNECTION_GET:
	case DRBD2_CMD_PEER_DEVICE_GET:
	case DRBD2_CMD_PATH_GET: {
		struct nlattr *tb[DRBD2_A_MAX + 1];

		if (nla_parse(tb, DRBD2_A_MAX, nlmsg_attrdata(nlh, GENL_HDRLEN),
			      nlmsg_attrlen(nlh, GENL_HDRLEN), drbd2_tla_nl_policy))
			return NL_MSG_INVALID;
		switch (gh->cmd) {
		case DRBD2_CMD_RESOURCE_GET:
			kind = NL_OBJ_RESOURCE; nest = tb[DRBD2_A_RESOURCE]; break;
		case DRBD2_CMD_DEVICE_GET:
			kind = NL_OBJ_DEVICE; nest = tb[DRBD2_A_DEVICE]; break;
		case DRBD2_CMD_CONNECTION_GET:
			kind = NL_OBJ_CONNECTION; nest = tb[DRBD2_A_CONNECTION]; break;
		case DRBD2_CMD_PEER_DEVICE_GET:
			kind = NL_OBJ_PEER_DEVICE; nest = tb[DRBD2_A_PEER_DEVICE]; break;
		default:
			kind = NL_OBJ_PATH; nest = tb[DRBD2_A_PATH]; break;
		}
		if (!nest)
			return NL_MSG_INVALID;
		drbd_nl_event_init(ev, kind);
		ev->seq = nlh->nlmsg_seq;
		break;
	}
	case DRBD2_CMD_STATE_GET:
	case DRBD2_CMD_STATE_CHANGE_NTF: {
		struct nlattr *tb[DRBD2_A_STATE_CHANGE_MAX + 1];

		if (nla_parse(tb, DRBD2_A_STATE_CHANGE_MAX, nlmsg_attrdata(nlh, GENL_HDRLEN),
			      nlmsg_attrlen(nlh, GENL_HDRLEN), drbd2_state_change_nl_policy))
			return NL_MSG_INVALID;
		if (tb[DRBD2_A_STATE_CHANGE_RESOURCE]) {
			kind = NL_OBJ_RESOURCE; nest = tb[DRBD2_A_STATE_CHANGE_RESOURCE];
		} else if (tb[DRBD2_A_STATE_CHANGE_DEVICE]) {
			kind = NL_OBJ_DEVICE; nest = tb[DRBD2_A_STATE_CHANGE_DEVICE];
		} else if (tb[DRBD2_A_STATE_CHANGE_CONNECTION]) {
			kind = NL_OBJ_CONNECTION; nest = tb[DRBD2_A_STATE_CHANGE_CONNECTION];
		} else if (tb[DRBD2_A_STATE_CHANGE_PEER_DEVICE]) {
			kind = NL_OBJ_PEER_DEVICE; nest = tb[DRBD2_A_STATE_CHANGE_PEER_DEVICE];
		} else if (tb[DRBD2_A_STATE_CHANGE_PATH]) {
			kind = NL_OBJ_PATH; nest = tb[DRBD2_A_STATE_CHANGE_PATH];
		} else {
			dbg(1, "state change without an object\n");
			return NL_MSG_SKIP;
		}
		drbd_nl_event_init(ev, kind);
		ev->seq = nlh->nlmsg_seq;
		ev->action = tb[DRBD2_A_STATE_CHANGE_ACTION] ?
			drbd2_action(nla_get_u32(tb[DRBD2_A_STATE_CHANGE_ACTION])) : NOTIFY_EXISTS;
		if (tb[DRBD2_A_STATE_CHANGE_MORE])
			ev->action |= NOTIFY_CONTINUES;
		break;
	}
	case DRBD2_CMD_HELPER_NTF: {
		struct nlattr *tb[DRBD2_A_MAX + 1], *htb[DRBD2_A_HELPER_INFO_MAX + 1];

		if (nla_parse(tb, DRBD2_A_MAX, nlmsg_attrdata(nlh, GENL_HDRLEN),
			      nlmsg_attrlen(nlh, GENL_HDRLEN), drbd2_tla_nl_policy))
			return NL_MSG_INVALID;
		drbd_nl_event_init(ev, NL_OBJ_HELPER);
		ev->seq = nlh->nlmsg_seq;
		ev->action = NOTIFY_CALL;
		if (tb[DRBD2_A_CONTEXT])
			drbd2_get_context(tb[DRBD2_A_CONTEXT], &ev->ctx, &ev->minor);
		if (tb[DRBD2_A_HELPER] &&
		    nla_parse_nested(htb, DRBD2_A_HELPER_INFO_MAX, tb[DRBD2_A_HELPER],
				     drbd2_helper_info_nl_policy) == 0) {
			if (htb[DRBD2_A_HELPER_INFO_NAME])
				ev->helper.helper_name_len = nla_strlcpy(ev->helper.helper_name,
									 htb[DRBD2_A_HELPER_INFO_NAME],
									 sizeof(ev->helper.helper_name));
			if (htb[DRBD2_A_HELPER_INFO_STATUS])
				ev->helper.helper_status = nla_get_u32(htb[DRBD2_A_HELPER_INFO_STATUS]);
			if (htb[DRBD2_A_HELPER_INFO_PHASE] &&
			    nla_get_u32(htb[DRBD2_A_HELPER_INFO_PHASE]) == DRBD2_HELPER_PHASE_RESPONSE)
				ev->action = NOTIFY_RESPONSE;
			ev->have_info = htb[DRBD2_A_HELPER_INFO_NAME] != NULL;
		}
		return NL_MSG_EVENT;
	}
	default:
		dbg(1, "unknown drbd2 command %d\n", gh->cmd);
		return NL_MSG_SKIP;
	}

	if (drbd2_object_to_event(nest, ev))
		return NL_MSG_INVALID;
	return NL_MSG_EVENT;
}

const struct drbd_nl_dialect drbd2_dialect = {
	.name = "drbd2",
	.family = &drbd2_family,
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
