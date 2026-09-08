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
	CHECK(genl_extack_msg(nlh, buf, sizeof(buf)) == 1);
	CHECK(!strcmp(buf, "unknown resource"));
	msg_free(m);

	m = msg_new(DEFAULT_MSG_SIZE);
	nlh = build_error_reply(m, -EINVAL, false, "echoed request follows the header");
	CHECK(genl_extack_msg(nlh, buf, sizeof(buf)) == 1);
	CHECK(!strcmp(buf, "echoed request follows the header"));
	msg_free(m);

	m = msg_new(DEFAULT_MSG_SIZE);
	nlh = build_error_reply(m, -EINVAL, true, NULL);
	buf[0] = 'x';
	CHECK(genl_extack_msg(nlh, buf, sizeof(buf)) == 0);
	CHECK(buf[0] == '\0');
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
#endif /* !WINDRBD */

int main(int argc, char **argv)
{
	test_event_init_defaults();
	test_event_copy_deep_copies_nests();
	test_disk_conf_from_nest();
	test_legacy_request_layout();
	test_legacy_path_and_forget_peer_context();
	test_legacy_parse_device_change();
	test_extack_message();
#ifndef WINDRBD
	test_drbd2_attr_maps_complete();
	test_drbd2_request_layout();
	test_drbd2_outcome();
#endif

	if (failures)
		fprintf(stderr, "%d check(s) failed\n", failures);
	else
		printf("all checks passed\n");
	return failures;
}
