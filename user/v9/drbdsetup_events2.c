#include <stdint.h>
#include <search.h>
#include <sys/time.h>
#include <time.h>
#include "drbdsetup.h"
#include "drbd_nla.h"
#include "drbdtool_common.h"
#include <linux/genl_magic_func.h>
#include "drbd_strings.h"
#include "drbdsetup_colors.h"

void *all_resources;

static int resource_obj_cmp(const void *a, const void *b) {
	return strcmp(((const struct entry *)a)->key, ((const struct entry *)b)->key);
}

static void store_resource(struct resources_list *resource)
{
	struct entry **found;
	struct entry entry = { .key = resource->name };

	found = tsearch(&entry, &all_resources, resource_obj_cmp);
	(*found)->data = resource;
}

static struct resources_list *find_resource(char *name)
{
	struct entry **found;
	struct entry entry = { .key = name };

	found = tfind(&entry, &all_resources, resource_obj_cmp);
	if (found)
		return (*found)->data;
	return NULL;
}

static void store_device(struct resources_list *resource, struct devices_list *new_device)
{
	struct devices_list *device, **tail = &resource->devices->next;

	for (device = resource->devices; device; device = device->next)
		tail = &device->next;

	(*tail)->next = new_device;
	new_device->next = NULL;
}

static struct devices_list *find_device(struct resources_list *resource, int minor)
{
	struct devices_list *device;
	for (device = resource->devices; device; device = device->next) {
		if (device->minor == minor)
			return device;
	}
	return NULL;
}

static void store_connection(struct resources_list *resource, struct connections_list *new_connection)
{
	struct connections_list *connection, **tail = &resource->connections->next;

	for (connection = resource->connections; connection; connection = connection->next)
		tail = &connection->next;

	(*tail)->next = new_connection;
	new_connection->next = NULL;
}

static struct connections_list *find_connection(struct resources_list *resource, const char *name)
{
	struct connections_list *connection;
	for (connection = resource->connections; connection; connection = connection->next) {
		if (!strcmp(connection->ctx.ctx_conn_name, name))
			return connection;
	}
	return NULL;
}

static void store_peer_device(struct resources_list *resource, struct peer_devices_list *new_peer_device)
{
	struct peer_devices_list *peer_device, **tail;
	struct connections_list *connection;

	connection = find_connection(resource, new_peer_device->ctx.ctx_conn_name);
	if (!connection) {
		fprintf(stderr, "Connection %s not found in resource %s", new_peer_device->ctx.ctx_conn_name, resource->name);
		exit(20);
	}
	tail = &connection->peer_devices->next;
	for (peer_device = connection->peer_devices; peer_device; peer_device = peer_device->next)
		tail = &peer_device->next;
	(*tail)->next = new_peer_device;
	new_peer_device->next = NULL;
}

static struct peer_devices_list *find_peer_device(struct resources_list *resource, struct drbd_cfg_context *ctx)
{
	struct connections_list *connection;
	struct peer_devices_list *peer_device;

	connection = find_connection(resource, ctx->ctx_conn_name);
	if (!connection) {
		fprintf(stderr, "Connection %s not found in resource %s", ctx->ctx_conn_name, resource->name);
		exit(20);
	}
	for (peer_device = connection->peer_devices; peer_device; peer_device = peer_device->next) {
		if (peer_device->ctx.ctx_peer_node_id == ctx->ctx_peer_node_id &&
		    peer_device->ctx.ctx_volume == ctx->ctx_volume)
			return peer_device;
	}
	return NULL;
}

int print_event(struct drbd_cmd *cm, struct genl_info *info, void *u_ptr)
{
	static const char *action_name[] = {
		[NOTIFY_EXISTS] = "exists",
		[NOTIFY_CREATE] = "create",
		[NOTIFY_CHANGE] = "change",
		[NOTIFY_DESTROY] = "destroy",
		[NOTIFY_CALL] = "call",
		[NOTIFY_RESPONSE] = "response",
	};
	static const char *object_name[] = {
		[DRBD_RESOURCE_STATE] = "resource",
		[DRBD_DEVICE_STATE] = "device",
		[DRBD_CONNECTION_STATE] = "connection",
		[DRBD_PEER_DEVICE_STATE] = "peer-device",
		[DRBD_HELPER] = "helper",
		[DRBD_PATH_STATE] = "path",
	};
	static uint32_t last_seq;
	static bool last_seq_known;
	static struct timeval tv;
	static bool keep_tv;

	struct drbd_cfg_context ctx = { .ctx_volume = -1U, .ctx_peer_node_id = -1U, };
	struct drbd_notification_header nh = { .nh_type = -1U };
	enum drbd_notification_type action;
	struct resources_list *resource;
	struct devices_list *device;
	struct connections_list *connection;
	struct peer_devices_list *peer_device;
	struct drbd_genlmsghdr *dh;
	char *key = NULL;
	int err;

	if (!info) {
		keep_tv = false;
		return 0;
	}

	dh = info->userhdr;
	if (dh->ret_code == ERR_MINOR_INVALID && cm->missing_ok)
		return 0;
	if (dh->ret_code != NO_ERROR)
		return dh->ret_code;

	err = drbd_notification_header_from_attrs(&nh, info);
	if (err)
		return 0;
	action = nh.nh_type & ~NOTIFY_FLAGS;
	if (action >= ARRAY_SIZE(action_name) ||
	    !action_name[action]) {
		dbg(1, "unknown notification type\n");
		goto out;
	}

	if (opt_now && action != NOTIFY_EXISTS)
		return 0;

	if (info->genlhdr->cmd == DRBD_INITIAL_STATE_DONE) {
		printf("%s -\n", action_name[NOTIFY_EXISTS]);
		return opt_now ? -1 : 0;
	}

	err = drbd_cfg_context_from_attrs(&ctx, info);
	if (err)
		return 0;
	if (info->genlhdr->cmd >= ARRAY_SIZE(object_name) ||
	    !object_name[info->genlhdr->cmd]) {
		dbg(1, "unknown notification\n");
		goto out;
	}

	if (action != NOTIFY_EXISTS) {
		if (last_seq_known) {
			int skipped = info->nlhdr->nlmsg_seq - (last_seq + 1);

			if (skipped)
				printf("- skipped %d\n", skipped);
		}
		last_seq = info->nlhdr->nlmsg_seq;
		last_seq_known = true;
	}

	if (opt_timestamps) {
		struct tm *tm;

		if (!keep_tv)
			gettimeofday(&tv, NULL);
		keep_tv = !!(nh.nh_type & NOTIFY_CONTINUES);

		tm = localtime(&tv.tv_sec);
		printf("%04u-%02u-%02uT%02u:%02u:%02u.%06u%+03d:%02u ",
		       tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
		       tm->tm_hour, tm->tm_min, tm->tm_sec,
		       (int)tv.tv_usec,
		       (int)(tm->tm_gmtoff / 3600),
		       (int)((abs(tm->tm_gmtoff) / 60) % 60));
	}



	resource = find_resource(ctx.ctx_resource_name);
	if (action != NOTIFY_CREATE && action != NOTIFY_EXISTS && !resource) {
		fprintf(stderr, "Resource %s not in data structure\n", ctx.ctx_resource_name);
		goto fail;

	switch (action) {
	case NOTIFY_EXISTS:
	case NOTIFY_CREATE:
		switch(info->genlhdr->cmd) {
		case DRBD_RESOURCE_STATE:
			resource = new_resource_from_info(info);
			store_resource(resource);
			break;
		case DRBD_DEVICE_STATE:
			device = new_device_from_info(info);
			store_device(resource, device);
			break;
		case DRBD_CONNECTION_STATE:
			connection = new_connection_from_info(info);
			store_connection(resource, connection);
			break;
		case DRBD_PEER_DEVICE_STATE:
			peer_device = new_peer_device_from_info(info);
			store_peer_device(resource, peer_device);
			break;
		case DRBD_PATH_STATE:
			break;
		}
		break;
	case NOTIFY_CHANGE:
		switch(info->genlhdr->cmd) {
		case DRBD_RESOURCE_STATE:
			resource_info_from_attrs(&resource->info, info);
			memset(&resource->statistics, -1, sizeof(resource->statistics));
			resource_statistics_from_attrs(&resource->statistics, info);
			break;
		case DRBD_DEVICE_STATE:
			device = find_device(resource, ((struct drbd_genlmsghdr*)(info->userhdr))->minor);
			disk_conf_from_attrs(&device->disk_conf, info);
			device->info.dev_disk_state = D_DISKLESS;
			device->info.is_intentional_diskless = IS_INTENTIONAL_DEF;
			device_info_from_attrs(&device->info, info);
			memset(&device->statistics, -1, sizeof(device->statistics));
			device_statistics_from_attrs(&device->statistics, info);
			break;
		case DRBD_CONNECTION_STATE:
			connection = find_connection(resource, ctx.ctx_conn_name);
			connection_info_from_attrs(&connection->info, info);
			memset(&connection->statistics, -1, sizeof(connection->statistics));
			connection_statistics_from_attrs(&connection->statistics, info);
			break;
		case DRBD_PEER_DEVICE_STATE:
			peer_device = find_peer_device(resource, &ctx);
			peer_device_info_from_attrs(&peer_device->info, info);
			memset(&peer_device->statistics, -1, sizeof(peer_device->statistics));
			peer_device_statistics_from_attrs(&peer_device->statistics, info);
			break;
		case DRBD_PATH_STATE:
			break;
		}

		break;
	case NOTIFY_DESTROY:
		break;
	case NOTIFY_CALL:
	case NOTIFY_RESPONSE:
		break;
	}

	if (!(nh.nh_type & NOTIFY_CONTINUES)) {
		printf("%s %s", action_name[action], "something");
        }

nl_out:
	printf("\n");
out:
	free(key);
	fflush(stdout);
	return 0;

fail:
	perror(progname);
	exit(20);
}
