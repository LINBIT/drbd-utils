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

int main(int argc, char **argv)
{
	test_event_init_defaults();
	test_event_copy_deep_copies_nests();
	test_disk_conf_from_nest();
	test_legacy_request_layout();
	test_legacy_path_and_forget_peer_context();
	test_legacy_parse_device_change();

	if (failures)
		fprintf(stderr, "%d check(s) failed\n", failures);
	else
		printf("all checks passed\n");
	return failures;
}
