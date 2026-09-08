/*
 * Unit checks for the netlink dialect layer of drbdsetup. Linked without
 * drbdsetup.o and config_flags.o, so it defines the "nl" global itself.
 * Exit status is the number of failed checks.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

#include "../drbdsetup_nl.h"
#include "../drbdsetup.h"
#ifndef WINDRBD
#include <uapi/linux/drbd2.h>
#include "linux/drbd2_genl_userspace.h"
#endif

const struct drbd_nl_dialect *nl;
extern const struct drbd_nl_dialect legacy_dialect;

static int failures;

#define CHECK(cond) do {							\
	if (!(cond)) {								\
		fprintf(stderr, "%s:%d: CHECK failed: %s\n",			\
			__FILE__, __LINE__, #cond);				\
		failures++;							\
	}									\
} while (0)

static void test_event_init_defaults(void)
{
	struct drbd_nl_event ev;

	drbd_nl_event_init(&ev, NL_OBJ_DEVICE);
	CHECK(ev.kind == NL_OBJ_DEVICE);
	CHECK(ev.action == NOTIFY_EXISTS);
	CHECK(ev.minor == -1U);
	CHECK(ev.ret_code == NO_ERROR);
	CHECK(ev.ctx.ctx_volume == -1U);
	CHECK(ev.ctx.ctx_peer_node_id == -1U);
	CHECK(ev.info.device.dev_disk_state == D_DISKLESS);
	CHECK(ev.info.device.is_intentional_diskless == IS_INTENTIONAL_DEF);
	CHECK(ev.info.device.dev_is_open == DEV_IS_OPEN_UNKNOWN);
	CHECK(ev.stats.device.dev_size == -1ULL);
	CHECK(!ev.have_info);
	CHECK(ev.opts == NULL && ev.opts2 == NULL && ev.paths == NULL);

	drbd_nl_event_init(&ev, NL_OBJ_PEER_DEVICE);
	CHECK(ev.info.peer_device.peer_is_intentional_diskless == IS_INTENTIONAL_DEF);
	CHECK(ev.stats.peer_device.peer_dev_out_of_sync == -1ULL);
}

static void test_event_copy_deep_copies_nests(void)
{
	struct msg_buff *m = msg_new(DEFAULT_MSG_SIZE);
	struct nlattr *nest = nla_nest_start(m, DRBD_NLA_RESOURCE_OPTS);
	struct drbd_nl_event ev, *copy;
	struct nlattr *found;

	nla_put_u32(m, DRBD_A_RES_OPTS_NODE_ID, 7);
	nla_nest_end(m, nest);

	drbd_nl_event_init(&ev, NL_OBJ_RESOURCE);
	ev.opts = nest;
	copy = drbd_nl_event_copy(&ev);
	CHECK(copy != NULL);
	CHECK(copy->opts != NULL && copy->opts != nest);
	CHECK(nla_len(copy->opts) == nla_len(nest));
	found = nla_find_nested(copy->opts, DRBD_A_RES_OPTS_NODE_ID);
	CHECK(found && nla_get_u32(found) == 7);
	CHECK(copy->opts2 == NULL && copy->paths == NULL);
	drbd_nl_event_free(copy);
	msg_free(m);
}

static void test_disk_conf_from_nest(void)
{
	struct msg_buff *m = msg_new(DEFAULT_MSG_SIZE);
	struct nlattr *nest = nla_nest_start(m, DRBD_NLA_DISK_CONF);
	struct disk_conf dc;

	nla_put_string(m, DRBD_A_DISK_CONF_BACKING_DEV, "/dev/sda");
	nla_put_string(m, DRBD_A_DISK_CONF_META_DEV, "/dev/sda1");
	nla_put_u32(m, DRBD_A_DISK_CONF_META_DEV_IDX, DRBD_MD_INDEX_FLEX_INT);
	nla_nest_end(m, nest);

	memset(&dc, 0, sizeof(dc));
	CHECK(drbd_nl_disk_conf_from_nest(&dc, nest) == 0);
	CHECK(!strcmp(dc.backing_dev, "/dev/sda"));
	CHECK(dc.meta_dev_idx == DRBD_MD_INDEX_FLEX_INT);
	CHECK(drbd_nl_disk_conf_from_nest(&dc, NULL) == -ENOMSG);
	msg_free(m);
}

static struct nlmsghdr *finish_msg(struct msg_buff *m)
{
	struct nlmsghdr *nlh = (struct nlmsghdr *)m->data;

	nlh->nlmsg_len = m->tail - m->data;
	return nlh;
}

static void test_legacy_request_layout(void)
{
	struct msg_buff *m = msg_new(DEFAULT_MSG_SIZE);
	struct drbd_cfg_context ctx = { .ctx_volume = -1U, .ctx_peer_node_id = 3 };
	struct nlattr *nest, *tla[DRBD_TLA_NL_POLICY_LEN], *a;
	struct nlmsghdr *nlh;
	struct genlmsghdr *gh;
	struct drbd_genlmsghdr *dh;
	int maxtype;

	strcpy(ctx.ctx_resource_name, "r0");
	CHECK(legacy_dialect.put_request(m, DRBD_NL_CMD_NET_OPTS, 0) == 0);
	nest = legacy_dialect.nest_start(m, NL_SET_NET_CONF);
	CHECK(nest != NULL);
	CHECK(legacy_dialect.attr_id(NL_SET_NET_CONF, DRBD_A_NET_CONF_PING_INT) == DRBD_A_NET_CONF_PING_INT);
	nla_put_u32(m, DRBD_A_NET_CONF_PING_INT, 5);
	nla_nest_end(m, nest);
	legacy_dialect.put_set_defaults(m);
	CHECK(legacy_dialect.put_context(m, &ctx, 9, CTX_RESOURCE | CTX_PEER_NODE_ID | CTX_MINOR,
					 NL_SET_NET_CONF) == 0);
	nlh = finish_msg(m);

	gh = nlmsg_data(nlh);
	dh = genlmsg_data(gh);
	CHECK(gh->cmd == DRBD_ADM_NET_OPTS);
	CHECK(dh->minor == 9);
	CHECK(dh->flags == DRBD_GENL_F_SET_DEFAULTS);
	CHECK(nla_parse(tla, DRBD_TLA_NL_POLICY_LEN - 1,
			nlmsg_attrdata(nlh, GENL_HDRLEN + sizeof(*dh)),
			nlmsg_attrlen(nlh, GENL_HDRLEN + sizeof(*dh)), drbd_tla_nl_policy) == 0);
	CHECK(tla[DRBD_NLA_NET_CONF] != NULL);
	CHECK(tla[DRBD_NLA_CFG_CONTEXT] != NULL);
	a = nla_find_nested(tla[DRBD_NLA_CFG_CONTEXT], DRBD_A_DRBD_CFG_CONTEXT_CTX_PEER_NODE_ID);
	CHECK(a && nla_get_u32(a) == 3);
	CHECK(legacy_dialect.policy(NL_SET_NET_CONF, &maxtype) == drbd_net_conf_nl_policy);
	CHECK(maxtype == DRBD_A_NET_CONF_RDMA_CTRL_SNDBUF_SIZE);
	msg_free(m);
}

static void test_legacy_path_and_forget_peer_context(void)
{
	struct msg_buff *m = msg_new(DEFAULT_MSG_SIZE);
	struct drbd_cfg_context ctx = { .ctx_volume = -1U, .ctx_peer_node_id = 2 };
	struct sockaddr_in sin = { .sin_family = AF_INET, .sin_port = htons(7789) };
	struct nlattr *tla[DRBD_TLA_NL_POLICY_LEN], *a;
	struct nlmsghdr *nlh;
	int hdrlen = GENL_HDRLEN + sizeof(struct drbd_genlmsghdr);

	strcpy(ctx.ctx_resource_name, "r0");
	memcpy(ctx.ctx_my_addr, &sin, sizeof(sin));
	ctx.ctx_my_addr_len = sizeof(sin);
	sin.sin_port = htons(7790);
	memcpy(ctx.ctx_peer_addr, &sin, sizeof(sin));
	ctx.ctx_peer_addr_len = sizeof(sin);

	legacy_dialect.put_request(m, DRBD_NL_CMD_NEW_PATH, 0);
	legacy_dialect.put_context(m, &ctx, -1U,
				   CTX_RESOURCE | CTX_PEER_NODE_ID | CTX_MY_ADDR | CTX_PEER_ADDR,
				   NL_SET_PATH_PARMS);
	nlh = finish_msg(m);
	CHECK(nla_parse(tla, DRBD_TLA_NL_POLICY_LEN - 1, nlmsg_attrdata(nlh, hdrlen),
			nlmsg_attrlen(nlh, hdrlen), drbd_tla_nl_policy) == 0);
	CHECK(tla[DRBD_NLA_PATH_PARMS] != NULL);
	a = nla_find_nested(tla[DRBD_NLA_PATH_PARMS], DRBD_A_PATH_PARMS_PEER_ADDR);
	CHECK(a && nla_len(a) == sizeof(sin) &&
	      ((struct sockaddr_in *)nla_data(a))->sin_port == htons(7790));
	msg_free(m);

	m = msg_new(DEFAULT_MSG_SIZE);
	legacy_dialect.put_request(m, DRBD_NL_CMD_FORGET_PEER, 0);
	legacy_dialect.put_context(m, &ctx, -1U, CTX_RESOURCE | CTX_PEER_NODE_ID,
				   NL_SET_FORGET_PEER_PARMS);
	nlh = finish_msg(m);
	CHECK(nla_parse(tla, DRBD_TLA_NL_POLICY_LEN - 1, nlmsg_attrdata(nlh, hdrlen),
			nlmsg_attrlen(nlh, hdrlen), drbd_tla_nl_policy) == 0);
	CHECK(tla[DRBD_NLA_FORGET_PEER_PARMS] != NULL);
	a = nla_find_nested(tla[DRBD_NLA_FORGET_PEER_PARMS], DRBD_A_FORGET_PEER_PARMS_FORGET_PEER_NODE_ID);
	CHECK(a && nla_get_u32(a) == 2);
	/* the peer node id is carried by the parms nest only, as before */
	CHECK(nla_find_nested(tla[DRBD_NLA_CFG_CONTEXT], DRBD_A_DRBD_CFG_CONTEXT_CTX_PEER_NODE_ID) == NULL);
	msg_free(m);
}

/* A device notification as the legacy kernel sends it. */
static struct nlmsghdr *build_legacy_device_change(struct msg_buff *m)
{
	struct drbd_genlmsghdr *dh = genlmsg_put(m, legacy_dialect.family, 0, DRBD_DEVICE_STATE);
	struct nlattr *nla;

	dh->minor = 1000;
	dh->ret_code = NO_ERROR;
	nla = nla_nest_start(m, DRBD_NLA_CFG_CONTEXT);
	nla_put_string(m, DRBD_A_DRBD_CFG_CONTEXT_CTX_RESOURCE_NAME, "r0");
	nla_put_u32(m, DRBD_A_DRBD_CFG_CONTEXT_CTX_VOLUME, 0);
	nla_nest_end(m, nla);
	nla = nla_nest_start(m, DRBD_NLA_NOTIFICATION_HEADER);
	nla_put_u32(m, DRBD_A_DRBD_NOTIFICATION_HEADER_NH_TYPE, NOTIFY_CHANGE | NOTIFY_CONTINUES);
	nla_nest_end(m, nla);
	nla = nla_nest_start(m, DRBD_NLA_DEVICE_INFO);
	nla_put_u32(m, DRBD_A_DEVICE_INFO_DEV_DISK_STATE, D_UP_TO_DATE);
	nla_put_string(m, DRBD_A_DEVICE_INFO_BACKING_DEV_PATH, "/dev/sda");
	nla_nest_end(m, nla);
	nla = nla_nest_start(m, DRBD_NLA_DEVICE_STATISTICS);
	nla_put_u64(m, DRBD_A_DEVICE_STATISTICS_DEV_SIZE, 4096);
	nla_nest_end(m, nla);
	return finish_msg(m);
}

static void test_legacy_parse_device_change(void)
{
	struct msg_buff *m = msg_new(DEFAULT_MSG_SIZE);
	struct nlmsghdr *nlh = build_legacy_device_change(m);
	struct drbd_nl_event ev;

	nlh->nlmsg_seq = 42;
	CHECK(legacy_dialect.parse_msg(nlh, DRBD_NL_CMD_GET_INITIAL_STATE, &ev) == NL_MSG_EVENT);
	CHECK(ev.kind == NL_OBJ_DEVICE);
	CHECK(ev.action == (NOTIFY_CHANGE | NOTIFY_CONTINUES));
	CHECK(ev.seq == 42);
	CHECK(ev.minor == 1000);
	CHECK(ev.ret_code == NO_ERROR);
	CHECK(!strcmp(ev.ctx.ctx_resource_name, "r0"));
	CHECK(ev.ctx.ctx_volume == 0);
	CHECK(ev.have_info);
	CHECK(ev.info.device.dev_disk_state == D_UP_TO_DATE);
	CHECK(ev.info.device.dev_is_open == DEV_IS_OPEN_UNKNOWN);	/* default kept */
	CHECK(!strcmp(ev.info.device.backing_dev_path, "/dev/sda"));
	CHECK(ev.stats.device.dev_size == 4096);
	CHECK(ev.stats.device.dev_read == -1ULL);			/* absent */
	CHECK(ev.opts == NULL);
	msg_free(m);

	m = msg_new(DEFAULT_MSG_SIZE);
	nlh = (struct nlmsghdr *)m->data;
	memset(nlh, 0, sizeof(*nlh));
	nlh->nlmsg_len = NLMSG_HDRLEN;
	nlh->nlmsg_type = NLMSG_DONE;
	CHECK(legacy_dialect.parse_msg(nlh, DRBD_NL_CMD_GET_INITIAL_STATE, &ev) == NL_MSG_DONE);
	msg_free(m);
}

/* A message without the object's identity is skipped, unless it is an
 * error reply, which carries none and is reported by its ret_code. */
static void test_legacy_parse_skips_without_identity(void)
{
	struct msg_buff *m = msg_new(DEFAULT_MSG_SIZE);
	struct drbd_genlmsghdr *dh;
	struct nlmsghdr *nlh;
	struct nlattr *nla;
	struct drbd_nl_event ev;

	/* notification header, no context */
	dh = genlmsg_put(m, legacy_dialect.family, 0, DRBD_RESOURCE_STATE);
	dh->minor = -1U;
	dh->ret_code = NO_ERROR;
	nla = nla_nest_start(m, DRBD_NLA_NOTIFICATION_HEADER);
	nla_put_u32(m, DRBD_A_DRBD_NOTIFICATION_HEADER_NH_TYPE, NOTIFY_CREATE);
	nla_nest_end(m, nla);
	nlh = finish_msg(m);
	CHECK(legacy_dialect.parse_msg(nlh, DRBD_NL_CMD_GET_INITIAL_STATE, &ev) == NL_MSG_SKIP);
	msg_free(m);

	/* context, no notification header: fine for a dump, not for events */
	m = msg_new(DEFAULT_MSG_SIZE);
	dh = genlmsg_put(m, legacy_dialect.family, 0, DRBD_ADM_GET_RESOURCES);
	dh->minor = -1U;
	dh->ret_code = NO_ERROR;
	nla = nla_nest_start(m, DRBD_NLA_CFG_CONTEXT);
	nla_put_string(m, DRBD_A_DRBD_CFG_CONTEXT_CTX_RESOURCE_NAME, "r0");
	nla_nest_end(m, nla);
	nlh = finish_msg(m);
	CHECK(legacy_dialect.parse_msg(nlh, DRBD_NL_CMD_GET_RESOURCES, &ev) == NL_MSG_EVENT);
	CHECK(ev.action == NOTIFY_EXISTS && !strcmp(ev.ctx.ctx_resource_name, "r0"));
	CHECK(legacy_dialect.parse_msg(nlh, DRBD_NL_CMD_GET_INITIAL_STATE, &ev) == NL_MSG_SKIP);
	msg_free(m);

	/* an error reply has neither, and still gets through */
	m = msg_new(DEFAULT_MSG_SIZE);
	dh = genlmsg_put(m, legacy_dialect.family, 0, DRBD_ADM_GET_DEVICES);
	dh->minor = 7;
	dh->ret_code = ERR_MINOR_INVALID;
	nlh = finish_msg(m);
	CHECK(legacy_dialect.parse_msg(nlh, DRBD_NL_CMD_GET_DEVICES, &ev) == NL_MSG_EVENT);
	CHECK(ev.kind == NL_OBJ_DEVICE && ev.ret_code == ERR_MINOR_INVALID && ev.minor == 7);
	msg_free(m);
}

/* An NLMSG_ERROR with extended ACK TLVs, as the kernel builds it. */
static struct nlmsghdr *build_error_reply(struct msg_buff *m, int error, bool capped,
					  const char *text)
{
	struct nlmsghdr *nlh = msg_put(m, NLMSG_HDRLEN);
	struct nlmsgerr *e;
	struct nlmsghdr req = { .nlmsg_len = NLMSG_HDRLEN + 8, .nlmsg_type = 0x1f };
	int payload = sizeof(*e) + (capped ? 0 : 8);

	memset(nlh, 0, NLMSG_HDRLEN);
	nlh->nlmsg_type = NLMSG_ERROR;
	nlh->nlmsg_flags = NLM_F_ACK_TLVS | (capped ? NLM_F_CAPPED : 0);
	e = msg_put(m, NLMSG_ALIGN(payload));
	memset(e, 0, NLMSG_ALIGN(payload));
	e->error = error;
	e->msg = req;
	if (text)
		nla_put_string(m, NLMSGERR_ATTR_MSG, text);
	nlh->nlmsg_len = m->tail - (unsigned char *)nlh;
	return nlh;
}

static void test_extack_message(void)
{
	struct msg_buff *m;
	struct nlmsghdr *nlh;
	char buf[64];

	m = msg_new(DEFAULT_MSG_SIZE);
	nlh = build_error_reply(m, -ENOENT, true, "unknown resource");
	CHECK(nlmsg_extack_msg(nlh, buf, sizeof(buf)) == 1);
	CHECK(!strcmp(buf, "unknown resource"));
	msg_free(m);

	m = msg_new(DEFAULT_MSG_SIZE);
	nlh = build_error_reply(m, -EINVAL, false, "echoed request follows the header");
	CHECK(nlmsg_extack_msg(nlh, buf, sizeof(buf)) == 1);
	CHECK(!strcmp(buf, "echoed request follows the header"));
	msg_free(m);

	m = msg_new(DEFAULT_MSG_SIZE);
	nlh = build_error_reply(m, -EINVAL, true, NULL);
	buf[0] = 'x';
	CHECK(nlmsg_extack_msg(nlh, buf, sizeof(buf)) == 0);
	CHECK(buf[0] == '\0');
	msg_free(m);
}

/* The legacy family also gets extended ACKs from a modern kernel; the
 * text beats strerror(). A positive ACK is not an error. */
static void test_legacy_outcome_keeps_extack_text(void)
{
	struct msg_buff *m = msg_new(DEFAULT_MSG_SIZE);
	struct drbd_nl_outcome out;
	struct nlmsghdr *nlh;

	nlh = build_error_reply(m, -EINVAL, true, "Attribute failed policy validation");
	CHECK(legacy_dialect.recv_outcome(nlh, &out) == 0);
	CHECK(out.ret_code == OTHER_ERROR && out.errnum == EINVAL);
	CHECK(out.desc && !strcmp(out.desc, "Attribute failed policy validation"));
	msg_free(m);

	m = msg_new(DEFAULT_MSG_SIZE);
	nlh = build_error_reply(m, -EPERM, true, NULL);
	CHECK(legacy_dialect.recv_outcome(nlh, &out) == 0);
	CHECK(out.ret_code == OTHER_ERROR && out.errnum == EPERM);
	CHECK(out.desc && !strcmp(out.desc, strerror(EPERM)));
	msg_free(m);

	m = msg_new(DEFAULT_MSG_SIZE);
	nlh = build_error_reply(m, 0, true, NULL);
	CHECK(legacy_dialect.recv_outcome(nlh, &out) == 0);
	CHECK(out.ret_code == NO_ERROR && out.errnum == 0);
	msg_free(m);
}

#ifndef WINDRBD
/* The legacy attributes that are plain NLA_U8 booleans but are represented
 * as NLA_FLAG on the drbd2 wire. Keyed by (set, legacy attribute id) so
 * that check_set_mapping()'s NLA_U8/NLA_FLAG exception below only accepts
 * these specific, deliberately-remapped fields (all FLAG() entries in
 * config_flags.c) rather than any NLA_U8-vs-NLA_FLAG mismatch. */
static const struct { enum drbd_nl_attr_set set; int legacy_id; } drbd2_flag_fields[] = {
	{ NL_SET_SET_ROLE_PARMS, DRBD_A_SET_ROLE_PARMS_FORCE },
	{ NL_SET_NEW_C_UUID_PARMS, DRBD_A_NEW_C_UUID_PARMS_CLEAR_BM },
	{ NL_SET_NEW_C_UUID_PARMS, DRBD_A_NEW_C_UUID_PARMS_FORCE_RESYNC },
	{ NL_SET_CONNECT_PARMS, DRBD_A_CONNECT_PARMS_TENTATIVE },
	{ NL_SET_CONNECT_PARMS, DRBD_A_CONNECT_PARMS_DISCARD_MY_DATA },
	{ NL_SET_DETACH_PARMS, DRBD_A_DETACH_PARMS_INTENTIONAL_DISKLESS_DETACH },
	{ NL_SET_DETACH_PARMS, DRBD_A_DETACH_PARMS_FORCE_DETACH },
	{ NL_SET_RESIZE_PARMS, DRBD_A_RESIZE_PARMS_RESIZE_FORCE },
	{ NL_SET_RESIZE_PARMS, DRBD_A_RESIZE_PARMS_NO_RESYNC },
	{ NL_SET_DISCONNECT_PARMS, DRBD_A_DISCONNECT_PARMS_FORCE_DISCONNECT },
};

static bool is_drbd2_flag_field(enum drbd_nl_attr_set set, int legacy_id)
{
	size_t k;

	for (k = 0; k < ARRAY_SIZE(drbd2_flag_fields); k++)
		if (drbd2_flag_fields[k].set == set && drbd2_flag_fields[k].legacy_id == legacy_id)
			return true;
	return false;
}

/* Every legacy attribute of a mapped set has a drbd2 counterpart with a
 * compatible wire type, and no two legacy attributes share one. */
static void check_set_mapping(enum drbd_nl_attr_set set, const struct nla_policy *legacy,
			      int legacy_max, const char *name)
{
	const struct nla_policy *d2;
	int d2_max = 0, i, j;

	d2 = drbd2_dialect.policy(set, &d2_max);
	CHECK(d2 != NULL);
	for (i = 1; i <= legacy_max; i++) {
		int id, lt = legacy[i].type, dt;

		if (lt == NLA_UNSPEC)
			continue;	/* an unused legacy number */
		id = drbd2_dialect.attr_id(set, i);
		if (id <= 0 || id > d2_max) {
			fprintf(stderr, "%s: legacy attr %d unmapped\n", name, i);
			failures++;
			continue;
		}
		dt = d2[id].type;
		/* identical, or one of the documented exceptions */
		if (!(lt == dt ||
		      (lt == NLA_U8 && dt == NLA_FLAG && is_drbd2_flag_field(set, i)) ||
		      (lt == NLA_S32 && dt == NLA_U32) ||
		      (lt == NLA_U32 && dt == NLA_S32) ||
		      (lt == NLA_STRING && dt == NLA_NUL_STRING) ||
		      (lt == NLA_NUL_STRING && dt == NLA_STRING))) {
			fprintf(stderr, "%s: legacy attr %d type %d vs drbd2 attr %d type %d\n",
				name, i, lt, id, dt);
			failures++;
		}
		for (j = 1; j < i; j++)
			if (legacy[j].type != NLA_UNSPEC && drbd2_dialect.attr_id(set, j) == id) {
				fprintf(stderr, "%s: legacy attrs %d and %d both map to %d\n",
					name, j, i, id);
				failures++;
			}
	}
}

static void test_drbd2_attr_maps_complete(void)
{
#define CHECK_SET(set, p) check_set_mapping(set, drbd_ ## p ## _nl_policy, \
					     ARRAY_SIZE(drbd_ ## p ## _nl_policy) - 1, #p)
	CHECK_SET(NL_SET_DISK_CONF, disk_conf);
	CHECK_SET(NL_SET_NET_CONF, net_conf);
	CHECK_SET(NL_SET_RES_OPTS, res_opts);
	CHECK_SET(NL_SET_PEER_DEVICE_CONF, peer_device_conf);
	CHECK_SET(NL_SET_DEVICE_CONF, device_conf);
	CHECK_SET(NL_SET_SET_ROLE_PARMS, set_role_parms);
	CHECK_SET(NL_SET_RESIZE_PARMS, resize_parms);
	CHECK_SET(NL_SET_START_OV_PARMS, start_ov_parms);
	CHECK_SET(NL_SET_NEW_C_UUID_PARMS, new_c_uuid_parms);
	CHECK_SET(NL_SET_DISCONNECT_PARMS, disconnect_parms);
	CHECK_SET(NL_SET_DETACH_PARMS, detach_parms);
	CHECK_SET(NL_SET_INVALIDATE_PARMS, invalidate_parms);
	CHECK_SET(NL_SET_INVALIDATE_PEER_PARMS, invalidate_peer_parms);
	CHECK_SET(NL_SET_CONNECT_PARMS, connect_parms);
	CHECK_SET(NL_SET_RENAME_RESOURCE_PARMS, rename_resource_parms);
	CHECK_SET(NL_SET_SUSPEND_IO_PARMS, suspend_io_parms);
#undef CHECK_SET
	/* carried by the context nest in drbd2 */
	CHECK(drbd2_dialect.nest_start(NULL, NL_SET_PATH_PARMS) == NULL);
	CHECK(drbd2_dialect.nest_start(NULL, NL_SET_FORGET_PEER_PARMS) == NULL);
}

static void test_drbd2_request_layout(void)
{
	struct msg_buff *m = msg_new(DEFAULT_MSG_SIZE);
	struct drbd_cfg_context ctx = { .ctx_volume = -1U, .ctx_peer_node_id = 3 };
	struct sockaddr_in sin = { .sin_family = AF_INET, .sin_port = htons(7789),
				   .sin_addr = { .s_addr = htonl(0x0a000001) } };
	struct nlattr *nest, *tb[DRBD2_A_MAX + 1], *ctb[DRBD2_A_CONTEXT_MAX + 1],
		      *atb[DRBD2_A_ADDRESS_MAX + 1], *a;
	struct nlmsghdr *nlh;
	struct genlmsghdr *gh;

	strcpy(ctx.ctx_resource_name, "r0");
	memcpy(ctx.ctx_my_addr, &sin, sizeof(sin));
	ctx.ctx_my_addr_len = sizeof(sin);

	CHECK(drbd2_dialect.put_request(m, DRBD_NL_CMD_NET_OPTS, 0) == 0);
	nest = drbd2_dialect.nest_start(m, NL_SET_NET_CONF);
	CHECK(nest != NULL);
	nla_put_u32(m, drbd2_dialect.attr_id(NL_SET_NET_CONF, DRBD_A_NET_CONF_PING_INT), 5);
	nla_nest_end(m, nest);
	drbd2_dialect.put_set_defaults(m);
	CHECK(drbd2_dialect.put_context(m, &ctx, 9,
					CTX_RESOURCE | CTX_PEER_NODE_ID | CTX_MINOR | CTX_MY_ADDR,
					NL_SET_NET_CONF) == 0);
	nlh = finish_msg(m);

	gh = nlmsg_data(nlh);
	CHECK(gh->cmd == DRBD2_CMD_CONNECTION_SET);
	CHECK(nla_parse(tb, DRBD2_A_MAX, nlmsg_attrdata(nlh, GENL_HDRLEN),
			nlmsg_attrlen(nlh, GENL_HDRLEN), drbd2_tla_nl_policy) == 0);
	CHECK(tb[DRBD2_A_SET_DEFAULTS] != NULL);
	CHECK(tb[DRBD2_A_NET_CONF] != NULL);
	a = nla_find_nested(tb[DRBD2_A_NET_CONF], DRBD2_A_NET_CONF_PING_INT);
	CHECK(a && nla_get_u32(a) == 5);
	CHECK(tb[DRBD2_A_CONTEXT] != NULL);
	CHECK(nla_parse_nested(ctb, DRBD2_A_CONTEXT_MAX, tb[DRBD2_A_CONTEXT],
			       drbd2_context_nl_policy) == 0);
	CHECK(ctb[DRBD2_A_CONTEXT_RESOURCE_NAME] &&
	      !strcmp(nla_data(ctb[DRBD2_A_CONTEXT_RESOURCE_NAME]), "r0"));
	CHECK(ctb[DRBD2_A_CONTEXT_MINOR] && nla_get_u32(ctb[DRBD2_A_CONTEXT_MINOR]) == 9);
	CHECK(ctb[DRBD2_A_CONTEXT_PEER_NODE_ID] && nla_get_u32(ctb[DRBD2_A_CONTEXT_PEER_NODE_ID]) == 3);
	CHECK(ctb[DRBD2_A_CONTEXT_VOLUME] == NULL);
	CHECK(ctb[DRBD2_A_CONTEXT_MY_ADDRESS] != NULL && ctb[DRBD2_A_CONTEXT_PEER_ADDRESS] == NULL);
	CHECK(nla_parse_nested(atb, DRBD2_A_ADDRESS_MAX, ctb[DRBD2_A_CONTEXT_MY_ADDRESS],
			       drbd2_address_nl_policy) == 0);
	CHECK(atb[DRBD2_A_ADDRESS_FAMILY] && nla_get_u16(atb[DRBD2_A_ADDRESS_FAMILY]) == AF_INET);
	CHECK(atb[DRBD2_A_ADDRESS_PORT] && nla_get_be16(atb[DRBD2_A_ADDRESS_PORT]) == htons(7789));
	CHECK(atb[DRBD2_A_ADDRESS_IPV4] && nla_get_be32(atb[DRBD2_A_ADDRESS_IPV4]) == htonl(0x0a000001));
	CHECK(atb[DRBD2_A_ADDRESS_IPV6] == NULL);
	msg_free(m);
}

/* A drbd2 reply and refusal, as the kernel sends them. */
static void test_drbd2_outcome(void)
{
	struct msg_buff *m = msg_new(DEFAULT_MSG_SIZE);
	struct drbd_nl_outcome out;
	struct nlmsghdr *nlh;

	genlmsg_put(m, drbd2_dialect.family, 0, DRBD2_CMD_RESOURCE_PRIMARY);
	nla_put_u32(m, DRBD2_A_STATE_RESULT, DRBD2_STATE_RESULT_NO_UP_TO_DATE_DISK);
	nla_put_string(m, DRBD2_A_MESSAGE, "first line\nsecond line");
	nlh = finish_msg(m);
	CHECK(drbd2_dialect.recv_outcome(nlh, &out) == 0);
	CHECK(out.ret_code == SS_NO_UP_TO_DATE_DISK);
	CHECK(out.info && out.info_len == (int)sizeof("first line\0second line"));
	CHECK(!strcmp(out.info, "first line") && !strcmp(out.info + 11, "second line"));
	CHECK(out.timeout_type == -1);
	msg_free(m);

	m = msg_new(DEFAULT_MSG_SIZE);
	genlmsg_put(m, drbd2_dialect.family, 0, DRBD2_CMD_RESOURCE_PRIMARY);
	nla_put_u32(m, DRBD2_A_STATE_RESULT, DRBD2_STATE_RESULT_SUCCESS);
	nlh = finish_msg(m);
	CHECK(drbd2_dialect.recv_outcome(nlh, &out) == 0);
	CHECK(out.ret_code == NO_ERROR && out.info == NULL);
	msg_free(m);

	m = msg_new(DEFAULT_MSG_SIZE);
	genlmsg_put(m, drbd2_dialect.family, 0, DRBD2_CMD_RESOURCE_PRIMARY);
	nla_put_u32(m, DRBD2_A_STATE_RESULT, DRBD2_STATE_RESULT_NOTHING_TO_DO);
	nlh = finish_msg(m);
	CHECK(drbd2_dialect.recv_outcome(nlh, &out) == 0);
	CHECK(out.ret_code == SS_NOTHING_TO_DO);
	msg_free(m);

	m = msg_new(DEFAULT_MSG_SIZE);
	genlmsg_put(m, drbd2_dialect.family, 0, DRBD2_CMD_TIMEOUT_TYPE_GET);
	nla_put_u32(m, DRBD2_A_TIMEOUT_TYPE, DRBD2_TIMEOUT_TYPE_PEER_OUTDATED);
	nlh = finish_msg(m);
	CHECK(drbd2_dialect.recv_outcome(nlh, &out) == 0);
	CHECK(out.ret_code == NO_ERROR && out.timeout_type == UT_PEER_OUTDATED);
	msg_free(m);

	m = msg_new(DEFAULT_MSG_SIZE);
	nlh = build_error_reply(m, -ENOENT, true, "unknown resource");
	CHECK(drbd2_dialect.recv_outcome(nlh, &out) == 0);
	CHECK(out.ret_code == ERR_EXTACK && out.errnum == ENOENT);
	CHECK(out.desc && !strcmp(out.desc, "unknown resource"));
	msg_free(m);
}

/* A device state change as the drbd2 kernel sends it. */
static struct nlmsghdr *build_drbd2_device_change(struct msg_buff *m, bool more)
{
	struct nlattr *obj, *nest;

	genlmsg_put(m, drbd2_dialect.family, 0, DRBD2_CMD_STATE_CHANGE_NTF);
	nla_put_u32(m, DRBD2_A_STATE_CHANGE_ACTION, DRBD2_STATE_CHANGE_ACTION_CHANGE);
	if (more)
		nla_put_flag(m, DRBD2_A_STATE_CHANGE_MORE);
	obj = nla_nest_start(m, DRBD2_A_STATE_CHANGE_DEVICE);
	nest = nla_nest_start(m, DRBD2_A_DEVICE_CONTEXT);
	nla_put_string(m, DRBD2_A_CONTEXT_RESOURCE_NAME, "r0");
	nla_put_u32(m, DRBD2_A_CONTEXT_VOLUME, 0);
	nla_put_u32(m, DRBD2_A_CONTEXT_MINOR, 1000);
	nla_nest_end(m, nest);
	nest = nla_nest_start(m, DRBD2_A_DEVICE_INFO);
	nla_put_u32(m, DRBD2_A_DEVICE_INFO_DISK_STATE, D_UP_TO_DATE);
	nla_put_u8(m, DRBD2_A_DEVICE_INFO_IS_OPEN, 1);
	nla_put_string(m, DRBD2_A_DEVICE_INFO_BACKING_DEV_PATH, "/dev/sda");
	nla_nest_end(m, nest);
	nest = nla_nest_start(m, DRBD2_A_DEVICE_STATISTICS);
	nla_put_u64(m, DRBD2_A_DEVICE_STATISTICS_SIZE, 4096);
	nla_put_u32(m, DRBD2_A_DEVICE_STATISTICS_UPPER_PENDING, 3);
	nla_nest_end(m, nest);
	nla_nest_end(m, obj);
	return finish_msg(m);
}

static void test_drbd2_parse_device_change(void)
{
	struct msg_buff *m = msg_new(DEFAULT_MSG_SIZE);
	struct nlmsghdr *nlh = build_drbd2_device_change(m, true);
	struct drbd_nl_event ev;

	nlh->nlmsg_seq = 7;
	CHECK(drbd2_dialect.parse_msg(nlh, DRBD_NL_CMD_GET_INITIAL_STATE, &ev) == NL_MSG_EVENT);
	CHECK(ev.kind == NL_OBJ_DEVICE);
	CHECK(ev.action == (NOTIFY_CHANGE | NOTIFY_CONTINUES));
	CHECK(ev.seq == 7 && ev.minor == 1000 && ev.ret_code == NO_ERROR);
	CHECK(!strcmp(ev.ctx.ctx_resource_name, "r0") && ev.ctx.ctx_volume == 0);
	CHECK(ev.ctx.ctx_peer_node_id == -1U);
	CHECK(ev.have_info);
	CHECK(ev.info.device.dev_disk_state == D_UP_TO_DATE);
	CHECK(ev.info.device.dev_is_open == 1);
	CHECK(ev.info.device.is_intentional_diskless == IS_INTENTIONAL_DEF);	/* absent */
	CHECK(!strcmp(ev.info.device.backing_dev_path, "/dev/sda"));
	CHECK(ev.info.device.backing_dev_path_len == strlen("/dev/sda"));
	CHECK(ev.stats.device.dev_size == 4096);
	CHECK(ev.stats.device.dev_upper_pending == 3);
	CHECK(ev.stats.device.dev_read == -1ULL);
	CHECK(ev.opts == NULL && ev.opts2 == NULL);
	msg_free(m);
}

/* A connection dump reply with net options and two paths. */
static struct nlmsghdr *build_drbd2_connection_get(struct msg_buff *m)
{
	struct sockaddr_in6 sin6 = { .sin6_family = AF_INET6, .sin6_port = htons(7789),
				     .sin6_addr = IN6ADDR_LOOPBACK_INIT };
	struct sockaddr_in sin = { .sin_family = AF_INET, .sin_port = htons(7790),
				   .sin_addr = { .s_addr = htonl(0x0a000002) } };
	struct nlattr *obj, *nest, *path, *pctx;

	genlmsg_put(m, drbd2_dialect.family, NLM_F_MULTI, DRBD2_CMD_CONNECTION_GET);
	obj = nla_nest_start(m, DRBD2_A_CONNECTION);
	nest = nla_nest_start(m, DRBD2_A_CONNECTION_CONTEXT);
	nla_put_string(m, DRBD2_A_CONTEXT_RESOURCE_NAME, "r0");
	nla_put_u32(m, DRBD2_A_CONTEXT_PEER_NODE_ID, 1);
	nla_put_string(m, DRBD2_A_CONTEXT_CONNECTION_NAME, "peer");
	nla_nest_end(m, nest);
	nest = nla_nest_start(m, DRBD2_A_CONNECTION_INFO);
	nla_put_u32(m, DRBD2_A_CONNECTION_INFO_CONNECTION_STATE, C_CONNECTED);
	nla_put_u32(m, DRBD2_A_CONNECTION_INFO_ROLE, R_SECONDARY);
	nla_nest_end(m, nest);
	nest = nla_nest_start(m, DRBD2_A_CONNECTION_NET_CONF);
	nla_put_u32(m, DRBD2_A_NET_CONF_PROTOCOL, 3);
	nla_put_u32(m, DRBD2_A_NET_CONF_PING_INT, 10);
	nla_put_u8(m, DRBD2_A_NET_CONF_TWO_PRIMARIES, 0);
	nla_nest_end(m, nest);
	path = nla_nest_start(m, DRBD2_A_CONNECTION_PATH);
	pctx = nla_nest_start(m, DRBD2_A_PATH_CONTEXT);
	drbd2_put_address(m, DRBD2_A_CONTEXT_MY_ADDRESS, &sin6, sizeof(sin6));
	drbd2_put_address(m, DRBD2_A_CONTEXT_PEER_ADDRESS, &sin, sizeof(sin));
	nla_nest_end(m, pctx);
	nest = nla_nest_start(m, DRBD2_A_PATH_INFO);
	nla_put_u8(m, DRBD2_A_PATH_INFO_ESTABLISHED, 1);
	nla_nest_end(m, nest);
	nla_nest_end(m, path);
	path = nla_nest_start(m, DRBD2_A_CONNECTION_PATH);
	pctx = nla_nest_start(m, DRBD2_A_PATH_CONTEXT);
	sin.sin_port = htons(7791);
	drbd2_put_address(m, DRBD2_A_CONTEXT_MY_ADDRESS, &sin, sizeof(sin));
	drbd2_put_address(m, DRBD2_A_CONTEXT_PEER_ADDRESS, &sin, sizeof(sin));
	nla_nest_end(m, pctx);
	nla_nest_end(m, path);
	nla_nest_end(m, obj);
	return finish_msg(m);
}

static void test_drbd2_parse_connection_dump(void)
{
	struct msg_buff *m = msg_new(DEFAULT_MSG_SIZE);
	struct nlmsghdr *nlh = build_drbd2_connection_get(m);
	struct drbd_nl_event ev;
	struct nlattr *a;
	int rem, n = 0;

	CHECK(drbd2_dialect.parse_msg(nlh, DRBD_NL_CMD_GET_CONNECTIONS, &ev) == NL_MSG_EVENT);
	CHECK(ev.kind == NL_OBJ_CONNECTION && ev.action == NOTIFY_EXISTS);
	CHECK(ev.ctx.ctx_peer_node_id == 1);
	CHECK(!strcmp(ev.ctx.ctx_conn_name, "peer") && ev.ctx.ctx_conn_name_len == 4);
	CHECK(ev.have_info && ev.info.connection.conn_connection_state == C_CONNECTED);
	CHECK(ev.stats.connection.ap_in_flight == -1ULL);

	/* net options renumbered into the legacy ids */
	CHECK(ev.opts != NULL);
	a = nla_find_nested(ev.opts, DRBD_A_NET_CONF_WIRE_PROTOCOL);
	CHECK(a && nla_get_u32(a) == 3);
	a = nla_find_nested(ev.opts, DRBD_A_NET_CONF_PING_INT);
	CHECK(a && nla_get_u32(a) == 10);
	a = nla_find_nested(ev.opts, DRBD_A_NET_CONF_TWO_PRIMARIES);
	CHECK(a && nla_len(a) == 1 && nla_get_u8(a) == 0);

	/* paths as the legacy blob list: my, peer, my, peer */
	CHECK(ev.paths != NULL);
	nla_for_each_nested(a, ev.paths, rem) {
		if (n == 0) {
			struct sockaddr_in6 *s6 = nla_data(a);

			CHECK(nla_type(a) == DRBD_A_PATH_PARMS_MY_ADDR);
			CHECK(nla_len(a) == sizeof(*s6) && s6->sin6_family == AF_INET6 &&
			      s6->sin6_port == htons(7789) &&
			      IN6_IS_ADDR_LOOPBACK(&s6->sin6_addr));
		} else if (n == 1) {
			struct sockaddr_in *s4 = nla_data(a);

			CHECK(nla_type(a) == DRBD_A_PATH_PARMS_PEER_ADDR);
			CHECK(nla_len(a) == sizeof(*s4) && s4->sin_port == htons(7790) &&
			      s4->sin_addr.s_addr == htonl(0x0a000002));
		} else {
			CHECK(nla_type(a) == (n == 2 ? DRBD_A_PATH_PARMS_MY_ADDR : DRBD_A_PATH_PARMS_PEER_ADDR));
		}
		n++;
	}
	CHECK(n == 4);
	msg_free(m);
}

static void test_drbd2_parse_helper_and_done(void)
{
	struct msg_buff *m = msg_new(DEFAULT_MSG_SIZE);
	struct nlmsghdr *nlh;
	struct nlattr *nest;
	struct drbd_nl_event ev;

	genlmsg_put(m, drbd2_dialect.family, 0, DRBD2_CMD_HELPER_NTF);
	nest = nla_nest_start(m, DRBD2_A_CONTEXT);
	nla_put_string(m, DRBD2_A_CONTEXT_RESOURCE_NAME, "r0");
	nla_put_u32(m, DRBD2_A_CONTEXT_MINOR, 5);
	nla_put_u32(m, DRBD2_A_CONTEXT_VOLUME, 0);
	nla_nest_end(m, nest);
	nest = nla_nest_start(m, DRBD2_A_HELPER);
	nla_put_string(m, DRBD2_A_HELPER_INFO_NAME, "before-resync-target");
	nla_put_u32(m, DRBD2_A_HELPER_INFO_STATUS, 0);
	nla_put_u32(m, DRBD2_A_HELPER_INFO_PHASE, DRBD2_HELPER_PHASE_RESPONSE);
	nla_nest_end(m, nest);
	nlh = finish_msg(m);
	CHECK(drbd2_dialect.parse_msg(nlh, DRBD_NL_CMD_GET_INITIAL_STATE, &ev) == NL_MSG_EVENT);
	CHECK(ev.kind == NL_OBJ_HELPER && ev.action == NOTIFY_RESPONSE);
	CHECK(ev.minor == 5 && ev.ctx.ctx_volume == 0);
	CHECK(ev.have_info && !strcmp(ev.helper.helper_name, "before-resync-target"));
	msg_free(m);

	m = msg_new(DEFAULT_MSG_SIZE);
	nlh = (struct nlmsghdr *)m->data;
	memset(nlh, 0, sizeof(*nlh));
	nlh->nlmsg_len = NLMSG_HDRLEN;
	nlh->nlmsg_type = NLMSG_DONE;
	nlh->nlmsg_seq = 11;
	CHECK(drbd2_dialect.parse_msg(nlh, DRBD_NL_CMD_GET_INITIAL_STATE, &ev) == NL_MSG_EVENT);
	CHECK(ev.kind == NL_OBJ_INITIAL_STATE_DONE && ev.seq == 11);
	CHECK(drbd2_dialect.parse_msg(nlh, DRBD_NL_CMD_GET_DEVICES, &ev) == NL_MSG_DONE);
	msg_free(m);
}

static void test_drbd2_parse_resource_rename(void)
{
	struct msg_buff *m = msg_new(DEFAULT_MSG_SIZE);
	struct nlmsghdr *nlh;
	struct nlattr *obj, *nest;
	struct drbd_nl_event ev;

	genlmsg_put(m, drbd2_dialect.family, 0, DRBD2_CMD_STATE_CHANGE_NTF);
	nla_put_u32(m, DRBD2_A_STATE_CHANGE_ACTION, DRBD2_STATE_CHANGE_ACTION_RENAME);
	obj = nla_nest_start(m, DRBD2_A_STATE_CHANGE_RESOURCE);
	nest = nla_nest_start(m, DRBD2_A_RESOURCE_CONTEXT);
	nla_put_string(m, DRBD2_A_CONTEXT_RESOURCE_NAME, "r0");
	nla_nest_end(m, nest);
	nla_put_string(m, DRBD2_A_RESOURCE_NEW_NAME, "r1");
	nest = nla_nest_start(m, DRBD2_A_RESOURCE_RESOURCE_OPTS);
	nla_put_u32(m, DRBD2_A_RESOURCE_OPTS_NODE_ID, 4);
	nla_put_s32(m, DRBD2_A_RESOURCE_OPTS_QUORUM, -1);
	nla_nest_end(m, nest);
	nla_nest_end(m, obj);
	nlh = finish_msg(m);
	CHECK(drbd2_dialect.parse_msg(nlh, DRBD_NL_CMD_GET_INITIAL_STATE, &ev) == NL_MSG_EVENT);
	CHECK(ev.kind == NL_OBJ_RESOURCE && ev.action == NOTIFY_RENAME);
	CHECK(!ev.have_info);
	CHECK(!strcmp(ev.rename.res_new_name, "r1") && ev.rename.res_new_name_len == 2);
	CHECK(ev.opts && nla_find_nested(ev.opts, DRBD_A_RES_OPTS_NODE_ID));
	CHECK(nla_get_u32(nla_find_nested(ev.opts, DRBD_A_RES_OPTS_NODE_ID)) == 4);
	CHECK((int)nla_get_u32(nla_find_nested(ev.opts, DRBD_A_RES_OPTS_QUORUM)) == -1);
	msg_free(m);
}

/* A message for a different genl family is skipped, not misparsed. */
static void test_drbd2_parse_msg_skip_other_family(void)
{
	struct msg_buff *m = msg_new(DEFAULT_MSG_SIZE);
	struct nlmsghdr *nlh;
	struct drbd_nl_event ev;

	genlmsg_put(m, drbd2_dialect.family, 0, DRBD2_CMD_HELPER_NTF);
	nlh = finish_msg(m);
	nlh->nlmsg_type = drbd2_dialect.family->id + 1;
	CHECK(drbd2_dialect.parse_msg(nlh, DRBD_NL_CMD_GET_INITIAL_STATE, &ev) == NL_MSG_SKIP);
	msg_free(m);
}

/* A DRBD2_CMD_PATH_GET dump reply: a path's context and info. */
static void test_drbd2_parse_path_get(void)
{
	struct sockaddr_in sin = { .sin_family = AF_INET, .sin_port = htons(7789),
				   .sin_addr = { .s_addr = htonl(0x0a000001) } };
	struct msg_buff *m = msg_new(DEFAULT_MSG_SIZE);
	struct nlmsghdr *nlh;
	struct nlattr *obj, *pctx, *nest;
	struct drbd_nl_event ev;

	genlmsg_put(m, drbd2_dialect.family, NLM_F_MULTI, DRBD2_CMD_PATH_GET);
	obj = nla_nest_start(m, DRBD2_A_PATH);
	pctx = nla_nest_start(m, DRBD2_A_PATH_CONTEXT);
	nla_put_string(m, DRBD2_A_CONTEXT_RESOURCE_NAME, "r0");
	nla_put_u32(m, DRBD2_A_CONTEXT_PEER_NODE_ID, 1);
	drbd2_put_address(m, DRBD2_A_CONTEXT_MY_ADDRESS, &sin, sizeof(sin));
	nla_nest_end(m, pctx);
	nest = nla_nest_start(m, DRBD2_A_PATH_INFO);
	nla_put_u8(m, DRBD2_A_PATH_INFO_ESTABLISHED, 1);
	nla_nest_end(m, nest);
	nla_nest_end(m, obj);
	nlh = finish_msg(m);

	CHECK(drbd2_dialect.parse_msg(nlh, DRBD_NL_CMD_GET_PATHS, &ev) == NL_MSG_EVENT);
	CHECK(ev.kind == NL_OBJ_PATH);
	CHECK(!strcmp(ev.ctx.ctx_resource_name, "r0") && ev.ctx.ctx_peer_node_id == 1);
	CHECK(ev.have_info);
	CHECK(ev.info.path.path_established == 1);
	CHECK(ev.ctx.ctx_my_addr_len == sizeof(struct sockaddr_in));
	msg_free(m);
}
/* An object nest without its context is skipped, not applied to "". */
static void test_drbd2_parse_skips_without_identity(void)
{
	struct msg_buff *m = msg_new(DEFAULT_MSG_SIZE);
	struct nlmsghdr *nlh;
	struct nlattr *obj, *nest;
	struct drbd_nl_event ev;

	genlmsg_put(m, drbd2_dialect.family, NLM_F_MULTI, DRBD2_CMD_RESOURCE_GET);
	obj = nla_nest_start(m, DRBD2_A_RESOURCE);
	nest = nla_nest_start(m, DRBD2_A_RESOURCE_INFO);
	nla_put_u32(m, DRBD2_A_RESOURCE_INFO_ROLE, R_PRIMARY);
	nla_nest_end(m, nest);
	nla_nest_end(m, obj);
	nlh = finish_msg(m);
	CHECK(drbd2_dialect.parse_msg(nlh, DRBD_NL_CMD_GET_RESOURCES, &ev) == NL_MSG_SKIP);
	msg_free(m);
}

/* A path whose address cannot be decoded is left out of the legacy
 * blob list rather than emitted as a zero length sockaddr. */
static void test_drbd2_parse_connection_drops_undecodable_path(void)
{
	struct sockaddr_in sin = { .sin_family = AF_INET, .sin_port = htons(7789),
				   .sin_addr = { .s_addr = htonl(0x0a000001) } };
	struct msg_buff *m = msg_new(DEFAULT_MSG_SIZE);
	struct nlmsghdr *nlh;
	struct nlattr *obj, *nest, *path, *pctx, *a;
	struct drbd_nl_event ev;
	int rem, n = 0;

	genlmsg_put(m, drbd2_dialect.family, NLM_F_MULTI, DRBD2_CMD_CONNECTION_GET);
	obj = nla_nest_start(m, DRBD2_A_CONNECTION);
	nest = nla_nest_start(m, DRBD2_A_CONNECTION_CONTEXT);
	nla_put_string(m, DRBD2_A_CONTEXT_RESOURCE_NAME, "r0");
	nla_put_u32(m, DRBD2_A_CONTEXT_PEER_NODE_ID, 1);
	nla_nest_end(m, nest);
	/* peer address of an unknown family */
	path = nla_nest_start(m, DRBD2_A_CONNECTION_PATH);
	pctx = nla_nest_start(m, DRBD2_A_PATH_CONTEXT);
	drbd2_put_address(m, DRBD2_A_CONTEXT_MY_ADDRESS, &sin, sizeof(sin));
	nest = nla_nest_start(m, DRBD2_A_CONTEXT_PEER_ADDRESS);
	nla_put_u16(m, DRBD2_A_ADDRESS_FAMILY, AF_UNIX);
	nla_nest_end(m, nest);
	nla_nest_end(m, pctx);
	nla_nest_end(m, path);
	/* a good one */
	path = nla_nest_start(m, DRBD2_A_CONNECTION_PATH);
	pctx = nla_nest_start(m, DRBD2_A_PATH_CONTEXT);
	drbd2_put_address(m, DRBD2_A_CONTEXT_MY_ADDRESS, &sin, sizeof(sin));
	drbd2_put_address(m, DRBD2_A_CONTEXT_PEER_ADDRESS, &sin, sizeof(sin));
	nla_nest_end(m, pctx);
	nla_nest_end(m, path);
	nla_nest_end(m, obj);
	nlh = finish_msg(m);

	CHECK(drbd2_dialect.parse_msg(nlh, DRBD_NL_CMD_GET_CONNECTIONS, &ev) == NL_MSG_EVENT);
	CHECK(ev.paths != NULL);
	nla_for_each_nested(a, ev.paths, rem) {
		CHECK(nla_len(a) == sizeof(struct sockaddr_in));
		n++;
	}
	CHECK(n == 2);
	msg_free(m);
}
#endif /* !WINDRBD */

int main(int argc, char **argv)
{
	test_event_init_defaults();
	test_event_copy_deep_copies_nests();
	test_disk_conf_from_nest();
	test_legacy_request_layout();
	test_legacy_path_and_forget_peer_context();
	test_legacy_parse_device_change();
	test_legacy_parse_skips_without_identity();
	test_extack_message();
	test_legacy_outcome_keeps_extack_text();
#ifndef WINDRBD
	test_drbd2_attr_maps_complete();
	test_drbd2_request_layout();
	test_drbd2_outcome();
	test_drbd2_parse_device_change();
	test_drbd2_parse_connection_dump();
	test_drbd2_parse_helper_and_done();
	test_drbd2_parse_resource_rename();
	test_drbd2_parse_msg_skip_other_family();
	test_drbd2_parse_path_get();
	test_drbd2_parse_skips_without_identity();
	test_drbd2_parse_connection_drops_undecodable_path();
#endif

	if (failures)
		fprintf(stderr, "%d check(s) failed\n", failures);
	else
		printf("all checks passed\n");
	return failures;
}
