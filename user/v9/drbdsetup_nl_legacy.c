/*
 * The legacy "drbd" generic netlink family as a drbdsetup dialect.
 *
 * Byte for byte what drbdsetup has always sent and understood: a fixed
 * drbd_genlmsghdr (minor, flags, ret_code), DRBD_NLA_* top level nests,
 * raw sockaddr blobs for addresses, and the generated parsers of
 * linux/drbd_genl_userspace.h.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "drbdsetup_nl.h"
#include "drbdsetup.h"

static struct genl_family legacy_family = {
	.name = "drbd",
	.version = DRBD_FAMILY_VERSION,
	.hdrsize = sizeof(struct drbd_genlmsghdr),
};

static const int legacy_cmd[__DRBD_NL_CMD_MAX] = {
	[DRBD_NL_CMD_NEW_RESOURCE]	= DRBD_ADM_NEW_RESOURCE,
	[DRBD_NL_CMD_DEL_RESOURCE]	= DRBD_ADM_DEL_RESOURCE,
	[DRBD_NL_CMD_RESOURCE_OPTS]	= DRBD_ADM_RESOURCE_OPTS,
	[DRBD_NL_CMD_RENAME_RESOURCE]	= DRBD_ADM_RENAME_RESOURCE,
	[DRBD_NL_CMD_DOWN]		= DRBD_ADM_DOWN,
	[DRBD_NL_CMD_PRIMARY]		= DRBD_ADM_PRIMARY,
	[DRBD_NL_CMD_SECONDARY]		= DRBD_ADM_SECONDARY,
	[DRBD_NL_CMD_SUSPEND_IO]	= DRBD_ADM_SUSPEND_IO,
	[DRBD_NL_CMD_RESUME_IO]		= DRBD_ADM_RESUME_IO,
	[DRBD_NL_CMD_NEW_MINOR]		= DRBD_ADM_NEW_MINOR,
	[DRBD_NL_CMD_DEL_MINOR]		= DRBD_ADM_DEL_MINOR,
	[DRBD_NL_CMD_ATTACH]		= DRBD_ADM_ATTACH,
	[DRBD_NL_CMD_DETACH]		= DRBD_ADM_DETACH,
	[DRBD_NL_CMD_DISK_OPTS]		= DRBD_ADM_DISK_OPTS,
	[DRBD_NL_CMD_RESIZE]		= DRBD_ADM_RESIZE,
	[DRBD_NL_CMD_OUTDATE]		= DRBD_ADM_OUTDATE,
	[DRBD_NL_CMD_INVALIDATE]	= DRBD_ADM_INVALIDATE,
	[DRBD_NL_CMD_NEW_C_UUID]	= DRBD_ADM_NEW_C_UUID,
	[DRBD_NL_CMD_NEW_PEER]		= DRBD_ADM_NEW_PEER,
	[DRBD_NL_CMD_DEL_PEER]		= DRBD_ADM_DEL_PEER,
	[DRBD_NL_CMD_CONNECT]		= DRBD_ADM_CONNECT,
	[DRBD_NL_CMD_DISCONNECT]	= DRBD_ADM_DISCONNECT,
	[DRBD_NL_CMD_NET_OPTS]		= DRBD_ADM_NET_OPTS,
	[DRBD_NL_CMD_FORGET_PEER]	= DRBD_ADM_FORGET_PEER,
	[DRBD_NL_CMD_NEW_PATH]		= DRBD_ADM_NEW_PATH,
	[DRBD_NL_CMD_DEL_PATH]		= DRBD_ADM_DEL_PATH,
	[DRBD_NL_CMD_PEER_DEVICE_OPTS]	= DRBD_ADM_PEER_DEVICE_OPTS,
	[DRBD_NL_CMD_INVALIDATE_PEER]	= DRBD_ADM_INVALIDATE_PEER,
	[DRBD_NL_CMD_PAUSE_SYNC]	= DRBD_ADM_PAUSE_SYNC,
	[DRBD_NL_CMD_RESUME_SYNC]	= DRBD_ADM_RESUME_SYNC,
	[DRBD_NL_CMD_START_OV]		= DRBD_ADM_START_OV,
	[DRBD_NL_CMD_GET_TIMEOUT_TYPE]	= DRBD_ADM_GET_TIMEOUT_TYPE,
	[DRBD_NL_CMD_GET_RESOURCES]	= DRBD_ADM_GET_RESOURCES,
	[DRBD_NL_CMD_GET_DEVICES]	= DRBD_ADM_GET_DEVICES,
	[DRBD_NL_CMD_GET_CONNECTIONS]	= DRBD_ADM_GET_CONNECTIONS,
	[DRBD_NL_CMD_GET_PEER_DEVICES]	= DRBD_ADM_GET_PEER_DEVICES,
	[DRBD_NL_CMD_GET_PATHS]		= DRBD_ADM_GET_PATHS,
	[DRBD_NL_CMD_GET_INITIAL_STATE]	= DRBD_ADM_GET_INITIAL_STATE,
};

static const int legacy_set[__NL_SET_MAX] = {
	[NL_SET_DISK_CONF]		= DRBD_NLA_DISK_CONF,
	[NL_SET_NET_CONF]		= DRBD_NLA_NET_CONF,
	[NL_SET_RES_OPTS]		= DRBD_NLA_RESOURCE_OPTS,
	[NL_SET_PEER_DEVICE_CONF]	= DRBD_NLA_PEER_DEVICE_OPTS,
	[NL_SET_DEVICE_CONF]		= DRBD_NLA_DEVICE_CONF,
	[NL_SET_SET_ROLE_PARMS]		= DRBD_NLA_SET_ROLE_PARMS,
	[NL_SET_RESIZE_PARMS]		= DRBD_NLA_RESIZE_PARMS,
	[NL_SET_START_OV_PARMS]		= DRBD_NLA_START_OV_PARMS,
	[NL_SET_NEW_C_UUID_PARMS]	= DRBD_NLA_NEW_C_UUID_PARMS,
	[NL_SET_DISCONNECT_PARMS]	= DRBD_NLA_DISCONNECT_PARMS,
	[NL_SET_DETACH_PARMS]		= DRBD_NLA_DETACH_PARMS,
	[NL_SET_INVALIDATE_PARMS]	= DRBD_NLA_INVALIDATE_PARMS,
	[NL_SET_INVALIDATE_PEER_PARMS]	= DRBD_NLA_INVAL_PEER_PARAMS,
	[NL_SET_FORGET_PEER_PARMS]	= DRBD_NLA_FORGET_PEER_PARMS,
	[NL_SET_CONNECT_PARMS]		= DRBD_NLA_CONNECT_PARMS,
	[NL_SET_PATH_PARMS]		= DRBD_NLA_PATH_PARMS,
	[NL_SET_RENAME_RESOURCE_PARMS]	= DRBD_NLA_RENAME_RESOURCE_PARMS,
	[NL_SET_SUSPEND_IO_PARMS]	= DRBD_NLA_SUSPEND_IO_PARAMS,
};

struct legacy_policy {
	const struct nla_policy *policy;
	int maxtype;
};

#define LEGACY_POLICY(p) { drbd_ ## p ## _nl_policy, ARRAY_SIZE(drbd_ ## p ## _nl_policy) - 1 }

static const struct legacy_policy legacy_policy[__NL_SET_MAX] = {
	[NL_SET_DISK_CONF]		= LEGACY_POLICY(disk_conf),
	[NL_SET_NET_CONF]		= LEGACY_POLICY(net_conf),
	[NL_SET_RES_OPTS]		= LEGACY_POLICY(res_opts),
	[NL_SET_PEER_DEVICE_CONF]	= LEGACY_POLICY(peer_device_conf),
	[NL_SET_DEVICE_CONF]		= LEGACY_POLICY(device_conf),
	[NL_SET_SET_ROLE_PARMS]		= LEGACY_POLICY(set_role_parms),
	[NL_SET_RESIZE_PARMS]		= LEGACY_POLICY(resize_parms),
	[NL_SET_START_OV_PARMS]		= LEGACY_POLICY(start_ov_parms),
	[NL_SET_NEW_C_UUID_PARMS]	= LEGACY_POLICY(new_c_uuid_parms),
	[NL_SET_DISCONNECT_PARMS]	= LEGACY_POLICY(disconnect_parms),
	[NL_SET_DETACH_PARMS]		= LEGACY_POLICY(detach_parms),
	[NL_SET_INVALIDATE_PARMS]	= LEGACY_POLICY(invalidate_parms),
	[NL_SET_INVALIDATE_PEER_PARMS]	= LEGACY_POLICY(invalidate_peer_parms),
	[NL_SET_FORGET_PEER_PARMS]	= LEGACY_POLICY(forget_peer_parms),
	[NL_SET_CONNECT_PARMS]		= LEGACY_POLICY(connect_parms),
	[NL_SET_PATH_PARMS]		= LEGACY_POLICY(path_parms),
	[NL_SET_RENAME_RESOURCE_PARMS]	= LEGACY_POLICY(rename_resource_parms),
	[NL_SET_SUSPEND_IO_PARMS]	= LEGACY_POLICY(suspend_io_parms),
};

static struct drbd_genlmsghdr *legacy_hdr(struct msg_buff *msg)
{
	return genlmsg_data(nlmsg_data((struct nlmsghdr *)msg->data));
}

static bool legacy_cmd_known(enum drbd_nl_cmd cmd)
{
	if (cmd <= DRBD_NL_CMD_NONE || cmd >= __DRBD_NL_CMD_MAX || !legacy_family.id)
		return false;
	return genl_op_known(&legacy_family, legacy_cmd[cmd]);
}

static int legacy_put_request(struct msg_buff *msg, enum drbd_nl_cmd cmd, int nlflags)
{
	struct drbd_genlmsghdr *dhdr;

	if (cmd <= DRBD_NL_CMD_NONE || cmd >= __DRBD_NL_CMD_MAX || !legacy_cmd[cmd])
		return -EINVAL;
	dhdr = genlmsg_put(msg, &legacy_family, nlflags, legacy_cmd[cmd]);
	if (!dhdr)
		return -EMSGSIZE;
	dhdr->minor = -1;
	dhdr->flags = 0;
	return 0;
}

/*
 * The identity of the object: minor in the fixed header, the rest in the
 * CFG_CONTEXT nest. Path addresses and the peer to forget are not part
 * of the context in this family; they are the parameters of their
 * commands, so they go into the PATH_PARMS / FORGET_PEER_PARMS nest.
 */
static int legacy_put_context(struct msg_buff *msg, const struct drbd_cfg_context *ctx,
			      unsigned int minor, unsigned int what,
			      enum drbd_nl_attr_set set)
{
	struct nlattr *nla;

	if (what & CTX_MINOR)
		legacy_hdr(msg)->minor = minor;

	if (what & ~(CTX_MINOR | CTX_MY_ADDR | CTX_PEER_ADDR)) {
		nla = nla_nest_start(msg, DRBD_NLA_CFG_CONTEXT);
		if (!nla)
			return -EMSGSIZE;
		if (what & CTX_RESOURCE)
			nla_put_string(msg, DRBD_A_DRBD_CFG_CONTEXT_CTX_RESOURCE_NAME,
				       ctx->ctx_resource_name);
		if ((what & CTX_PEER_NODE_ID) && set != NL_SET_FORGET_PEER_PARMS)
			nla_put_u32(msg, DRBD_A_DRBD_CFG_CONTEXT_CTX_PEER_NODE_ID,
				    ctx->ctx_peer_node_id);
		if (what & CTX_VOLUME)
			nla_put_u32(msg, DRBD_A_DRBD_CFG_CONTEXT_CTX_VOLUME, ctx->ctx_volume);
		nla_nest_end(msg, nla);
	}

	if (set == NL_SET_PATH_PARMS && (what & (CTX_MY_ADDR | CTX_PEER_ADDR))) {
		nla = nla_nest_start(msg, DRBD_NLA_PATH_PARMS);
		if (!nla)
			return -EMSGSIZE;
		if (what & CTX_MY_ADDR)
			nla_put(msg, DRBD_A_PATH_PARMS_MY_ADDR, ctx->ctx_my_addr_len,
				ctx->ctx_my_addr);
		if (what & CTX_PEER_ADDR)
			nla_put(msg, DRBD_A_PATH_PARMS_PEER_ADDR, ctx->ctx_peer_addr_len,
				ctx->ctx_peer_addr);
		nla_nest_end(msg, nla);
	}

	if (set == NL_SET_FORGET_PEER_PARMS && (what & CTX_PEER_NODE_ID)) {
		nla = nla_nest_start(msg, DRBD_NLA_FORGET_PEER_PARMS);
		if (!nla)
			return -EMSGSIZE;
		nla_put_u32(msg, DRBD_A_FORGET_PEER_PARMS_FORGET_PEER_NODE_ID,
			    ctx->ctx_peer_node_id);
		nla_nest_end(msg, nla);
	}
	return 0;
}

static struct nlattr *legacy_nest_start(struct msg_buff *msg, enum drbd_nl_attr_set set)
{
	if (set <= NL_SET_NONE || set >= __NL_SET_MAX)
		return NULL;
	return nla_nest_start(msg, legacy_set[set]);
}

/* The legacy attribute ids are the neutral field ids. */
static int legacy_attr_id(enum drbd_nl_attr_set set, int field_id)
{
	return field_id;
}

static const struct nla_policy *legacy_get_policy(enum drbd_nl_attr_set set, int *maxtype)
{
	if (set <= NL_SET_NONE || set >= __NL_SET_MAX)
		return NULL;
	*maxtype = legacy_policy[set].maxtype;
	return legacy_policy[set].policy;
}

static void legacy_put_set_defaults(struct msg_buff *msg)
{
	legacy_hdr(msg)->flags |= DRBD_GENL_F_SET_DEFAULTS;
}

static int legacy_tla_parse(struct nlattr *tla[], struct nlmsghdr *nlh)
{
	const int hdrlen = GENL_HDRLEN + legacy_family.hdrsize;

	return nla_parse(tla, DRBD_TLA_NL_POLICY_LEN - 1,
			 nlmsg_attrdata(nlh, hdrlen), nlmsg_attrlen(nlh, hdrlen),
			 drbd_tla_nl_policy);
}

/* The info texts of all CFG_REPLY nests, NUL separated. */
static char legacy_info_buf[DEFAULT_MSG_SIZE];

static void legacy_collect_info(struct nlmsghdr *nlh, struct drbd_nl_outcome *out)
{
	const int hdrlen = GENL_HDRLEN + legacy_family.hdrsize;
	struct nlattr *o_nla, *nla;
	int o_rem, rem, len = 0;

	nla_for_each_attr(o_nla, nlmsg_attrdata(nlh, hdrlen), nlmsg_attrlen(nlh, hdrlen), o_rem) {
		if (nla_type(o_nla) != DRBD_NLA_CFG_REPLY || o_nla->nla_len == 0)
			continue;
		nla_for_each_nested(nla, o_nla, rem) {
			int l;

			if (nla_type(nla) != DRBD_A_DRBD_CFG_REPLY_INFO_TEXT)
				continue;
			l = nla_strlcpy(legacy_info_buf + len, nla,
					sizeof(legacy_info_buf) - len);
			if (l >= (int)sizeof(legacy_info_buf) - len)
				return;
			len += l + 1;
		}
	}
	if (len) {
		out->info = legacy_info_buf;
		out->info_len = len;
	}
}

static int legacy_recv_outcome(struct nlmsghdr *nlh, struct drbd_nl_outcome *out)
{
	struct nlattr *tla[DRBD_TLA_NL_POLICY_LEN];
	struct drbd_genlmsghdr *dh;

	memset(out, 0, sizeof(*out));
	out->timeout_type = -1;

	if (nlh->nlmsg_type == NLMSG_ERROR) {
		struct nlmsgerr *e = nlmsg_data(nlh);

		if (e->error == 0) {
			out->ret_code = NO_ERROR;
			return 0;
		}
		out->ret_code = OTHER_ERROR;
		out->errnum = -e->error;
		out->desc = strerror(out->errnum);
		return 0;
	}
	if (nlh->nlmsg_type != legacy_family.id)
		return -EINVAL;

	dh = genlmsg_data(nlmsg_data(nlh));
	out->ret_code = dh->ret_code;
	legacy_collect_info(nlh, out);
	if (legacy_tla_parse(tla, nlh) == 0 && tla[DRBD_NLA_TIMEOUT_PARMS]) {
		struct genl_info info = { .attrs = tla };
		struct timeout_parms parms;

		if (timeout_parms_from_attrs(&parms, &info) == 0)
			out->timeout_type = parms.timeout_type;
	}
	return 0;
}

static enum drbd_nl_msg legacy_parse_msg(struct nlmsghdr *nlh, enum drbd_nl_cmd cmd,
					 struct drbd_nl_event *ev)
{
	struct nlattr *tla[DRBD_TLA_NL_POLICY_LEN];
	struct drbd_notification_header nh = { .nh_type = NOTIFY_EXISTS };
	struct genlmsghdr *gh;
	struct drbd_genlmsghdr *dh;
	struct genl_info info;
	enum drbd_nl_obj kind;

	if (nlh->nlmsg_type == NLMSG_DONE)
		return NL_MSG_DONE;
	if (nlh->nlmsg_type != legacy_family.id)
		return NL_MSG_SKIP;

	gh = nlmsg_data(nlh);
	dh = genlmsg_data(gh);
	switch (gh->cmd) {
	case DRBD_ADM_GET_RESOURCES:
	case DRBD_RESOURCE_STATE:
		kind = NL_OBJ_RESOURCE;
		break;
	case DRBD_ADM_GET_DEVICES:
	case DRBD_DEVICE_STATE:
		kind = NL_OBJ_DEVICE;
		break;
	case DRBD_ADM_GET_CONNECTIONS:
	case DRBD_CONNECTION_STATE:
		kind = NL_OBJ_CONNECTION;
		break;
	case DRBD_ADM_GET_PEER_DEVICES:
	case DRBD_PEER_DEVICE_STATE:
		kind = NL_OBJ_PEER_DEVICE;
		break;
	case DRBD_ADM_GET_PATHS:
	case DRBD_PATH_STATE:
		kind = NL_OBJ_PATH;
		break;
	case DRBD_HELPER:
		kind = NL_OBJ_HELPER;
		break;
	case DRBD_INITIAL_STATE_DONE:
		kind = NL_OBJ_INITIAL_STATE_DONE;
		break;
	default:
		dbg(1, "unknown legacy command %d\n", gh->cmd);
		return NL_MSG_SKIP;
	}

	if (legacy_tla_parse(tla, nlh))
		return NL_MSG_INVALID;

	drbd_nl_event_init(ev, kind);
	ev->seq = nlh->nlmsg_seq;
	ev->minor = dh->minor;
	ev->ret_code = dh->ret_code;

	info = (struct genl_info){
		.seq = nlh->nlmsg_seq,
		.nlhdr = nlh,
		.genlhdr = gh,
		.userhdr = dh,
		.attrs = tla,
	};
	/* An object without its identity cannot be applied to anything. An
	 * error reply carries none; its ret_code is what matters then. */
	if (drbd_cfg_context_from_attrs(&ev->ctx, &info) &&
	    kind != NL_OBJ_INITIAL_STATE_DONE && dh->ret_code == NO_ERROR)
		return NL_MSG_SKIP;
	if (drbd_notification_header_from_attrs(&nh, &info) == 0)
		ev->action = nh.nh_type;
	else if (cmd == DRBD_NL_CMD_GET_INITIAL_STATE && dh->ret_code == NO_ERROR)
		return NL_MSG_SKIP;	/* every event carries one */

	switch (kind) {
	case NL_OBJ_RESOURCE:
		ev->have_info = resource_info_from_attrs(&ev->info.resource, &info) == 0;
		resource_statistics_from_attrs(&ev->stats.resource, &info);
		rename_resource_info_from_attrs(&ev->rename, &info);
		ev->opts = tla[DRBD_NLA_RESOURCE_OPTS];
		break;
	case NL_OBJ_DEVICE:
		ev->have_info = device_info_from_attrs(&ev->info.device, &info) == 0;
		device_statistics_from_attrs(&ev->stats.device, &info);
		ev->opts = tla[DRBD_NLA_DISK_CONF];
		ev->opts2 = tla[DRBD_NLA_DEVICE_CONF];
		break;
	case NL_OBJ_CONNECTION:
		ev->have_info = connection_info_from_attrs(&ev->info.connection, &info) == 0;
		connection_statistics_from_attrs(&ev->stats.connection, &info);
		ev->opts = tla[DRBD_NLA_NET_CONF];
		ev->paths = tla[DRBD_NLA_PATH_PARMS];
		break;
	case NL_OBJ_PEER_DEVICE:
		ev->have_info = peer_device_info_from_attrs(&ev->info.peer_device, &info) == 0;
		peer_device_statistics_from_attrs(&ev->stats.peer_device, &info);
		ev->opts = tla[DRBD_NLA_PEER_DEVICE_OPTS];
		break;
	case NL_OBJ_PATH:
		ev->have_info = drbd_path_info_from_attrs(&ev->info.path, &info) == 0;
		break;
	case NL_OBJ_HELPER:
		ev->have_info = drbd_helper_info_from_attrs(&ev->helper, &info) == 0;
		break;
	case NL_OBJ_INITIAL_STATE_DONE:
		break;
	}
	return NL_MSG_EVENT;
}

const struct drbd_nl_dialect legacy_dialect = {
	.name = "drbd",
	.family = &legacy_family,
	.api_version = DRBD_FAMILY_VERSION,
	.api_hdrsize = sizeof(struct drbd_genlmsghdr),
	.cmd_known = legacy_cmd_known,
	.put_request = legacy_put_request,
	.put_context = legacy_put_context,
	.nest_start = legacy_nest_start,
	.attr_id = legacy_attr_id,
	.policy = legacy_get_policy,
	.put_set_defaults = legacy_put_set_defaults,
	.recv_outcome = legacy_recv_outcome,
	.parse_msg = legacy_parse_msg,
};
