/*
 * Dialect-neutral helpers of the drbdsetup netlink layer.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drbdsetup_nl.h"

void drbd_nl_event_init(struct drbd_nl_event *ev, enum drbd_nl_obj kind)
{
	memset(ev, 0, sizeof(*ev));
	ev->kind = kind;
	ev->action = NOTIFY_EXISTS;
	ev->minor = -1U;
	ev->ret_code = NO_ERROR;
	ev->ctx.ctx_volume = -1U;
	ev->ctx.ctx_peer_node_id = -1U;
	/* statistics are "unknown" unless the message carries them */
	memset(&ev->stats, -1, sizeof(ev->stats));

	/* The defaults an old kernel's message leaves in place. */
	switch (kind) {
	case NL_OBJ_DEVICE:
		ev->info.device.dev_disk_state = D_DISKLESS;
		ev->info.device.is_intentional_diskless = IS_INTENTIONAL_DEF;
		ev->info.device.dev_is_open = DEV_IS_OPEN_UNKNOWN;
		break;
	case NL_OBJ_PEER_DEVICE:
		ev->info.peer_device.peer_is_intentional_diskless = IS_INTENTIONAL_DEF;
		break;
	default:
		break;
	}
}

static struct nlattr *nla_dup(const struct nlattr *nla)
{
	struct nlattr *copy;
	int size;

	if (!nla)
		return NULL;
	size = nla_total_size(nla_len(nla));
	copy = malloc(size);
	if (!copy)
		return NULL;
	memcpy(copy, nla, size);
	return copy;
}

struct drbd_nl_event *drbd_nl_event_copy(const struct drbd_nl_event *ev)
{
	struct drbd_nl_event *copy = malloc(sizeof(*copy));

	if (!copy)
		return NULL;
	*copy = *ev;
	copy->opts = nla_dup(ev->opts);
	if (ev->opts && !copy->opts) {
		fprintf(stderr, "out of memory\n");
		exit(20);
	}
	copy->opts2 = nla_dup(ev->opts2);
	if (ev->opts2 && !copy->opts2) {
		fprintf(stderr, "out of memory\n");
		exit(20);
	}
	copy->paths = nla_dup(ev->paths);
	if (ev->paths && !copy->paths) {
		fprintf(stderr, "out of memory\n");
		exit(20);
	}
	return copy;
}

void drbd_nl_event_free(struct drbd_nl_event *ev)
{
	if (!ev)
		return;
	free(ev->opts);
	free(ev->opts2);
	free(ev->paths);
	free(ev);
}

/*
 * The generated parsers want a genl_info with the top level attribute
 * table; a stored option nest is exactly one such attribute.
 */
int drbd_nl_disk_conf_from_nest(struct disk_conf *dc, struct nlattr *nest)
{
	struct nlattr *attrs[DRBD_TLA_NL_POLICY_LEN] = {
		[DRBD_NLA_DISK_CONF] = nest,
	};
	struct genl_info info = { .attrs = attrs };

	if (!nest)
		return -ENOMSG;
	return disk_conf_from_attrs(dc, &info);
}
