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

const struct drbd_nl_dialect *nl;

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

int main(int argc, char **argv)
{
	test_event_init_defaults();
	test_event_copy_deep_copies_nests();
	test_disk_conf_from_nest();

	if (failures)
		fprintf(stderr, "%d check(s) failed\n", failures);
	else
		printf("all checks passed\n");
	return failures;
}
