/*
 * DRBD setup via genetlink
 *
 * This file is part of DRBD by Philipp Reisner and Lars Ellenberg.
 *
 * Copyright (C) 2001-2008, LINBIT Information Technologies GmbH.
 * Copyright (C) 1999-2008, Philipp Reisner <philipp.reisner@linbit.com>.
 * Copyright (C) 2002-2008, Lars Ellenberg <lars.ellenberg@linbit.com>.
 *
 * drbd is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * drbd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with drbd; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#define _GNU_SOURCE
#define _XOPEN_SOURCE 600
#define _FILE_OFFSET_BITS 64

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

#ifdef min
#undef min
#endif
#define min(a,b) \
	({ __typeof__ (a) _a = (a); \
	 __typeof__ (b) _b = (b); \
	 _a < _b ? _a : _b; })

#define TIMESTAMP_LEN sizeof("....-..-..T..:..:.........+..:.. ")

static const char *action_exists = "exists";
static const char *action_create = "create";
static const char *action_change = "change";
static const char *action_destroy = "destroy";
static const char *action_call = "call";
static const char *action_response = "response";

static const char *object_resource = "resource";
static const char *object_device = "device";
static const char *object_connection = "connection";
static const char *object_peer_device = "peer-device";
static const char *object_helper = "helper";
static const char *object_path = "path";

void *all_resources;
struct resources_list *update_resources;

static int apply_event(const char *prefix, struct genl_info *info, bool initial_state);

static int resource_obj_cmp(const void *a, const void *b)
{
	return strcmp(((const struct resources_list *)a)->name, ((const struct resources_list *)b)->name);
}

static void store_resource(struct resources_list *resource)
{
	tsearch(resource, &all_resources, resource_obj_cmp);
}

static struct resources_list *find_resource(char *name)
{
	struct resources_list **found;
	struct resources_list key = { .name = name };

	found = tfind(&key, &all_resources, resource_obj_cmp);
	if (found)
	{
		return *found;
	}
	return NULL;
}

static void delete_resource(struct resources_list *resource)
{
	tdelete(resource, &all_resources, resource_obj_cmp);
}

static void store_update_resource(struct resources_list *new_resource)
{
	new_resource->next = update_resources;
	update_resources = new_resource;
}

static struct devices_list *find_device(struct resources_list *resource, unsigned volume)
{
	struct devices_list *device;
	for (device = resource->devices; device; device = device->next) {
		if (device->ctx.ctx_volume == volume)
			return device;
	}
	return NULL;
}

static void store_device(struct resources_list *resource, struct devices_list *new_device)
{
	new_device->next = resource->devices;
	resource->devices = new_device;
}

static void store_device_check(struct resources_list *resource, struct devices_list *new_device)
{
	struct devices_list *old_device;

	old_device = find_device(resource, new_device->ctx.ctx_volume);
	if (old_device) {
		free_device(new_device);
		return;
	}

	store_device(resource, new_device);
}

static void delete_device(struct resources_list *resource, unsigned volume)
{
	struct devices_list *device, **previous_next = &resource->devices;
	for (device = resource->devices; device; device = device->next) {
		if (device->ctx.ctx_volume == volume) {
			*previous_next = device->next;
			free_device(device);
			return;
		}
		previous_next = &device->next;
	}
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

static void store_connection(struct resources_list *resource, struct connections_list *new_connection)
{
	new_connection->next = resource->connections;
	resource->connections = new_connection;
}

static void store_connection_check(struct resources_list *resource, struct connections_list *new_connection)
{
	struct connections_list *old_connection;

	old_connection = find_connection(resource, new_connection->ctx.ctx_conn_name);
	if (old_connection) {
		free_connection(new_connection);
		return;
	}

	store_connection(resource, new_connection);
}

static void delete_connection(struct resources_list *resource, const char *name)
{
	struct connections_list *connection, **previous_next = &resource->connections;
	for (connection = resource->connections; connection; connection = connection->next) {
		if (!strcmp(connection->ctx.ctx_conn_name, name)) {
			*previous_next = connection->next;
			free_connection(connection);
			return;
		}
		previous_next = &connection->next;
	}
}

static struct peer_devices_list *connection_find_peer_device(struct connections_list *connection, unsigned volume)
{
	struct peer_devices_list *peer_device;
	for (peer_device = connection->peer_devices; peer_device; peer_device = peer_device->next) {
		if (peer_device->ctx.ctx_volume == volume)
			return peer_device;
	}
	return NULL;
}

static struct peer_devices_list *find_peer_device(struct resources_list *resource, struct drbd_cfg_context *ctx)
{
	struct connections_list *connection;

	connection = find_connection(resource, ctx->ctx_conn_name);
	if (!connection)
		return NULL;

	return connection_find_peer_device(connection, ctx->ctx_volume);
}

static void connection_store_peer_device(struct connections_list *connection, struct peer_devices_list *new_peer_device)
{
	new_peer_device->next = connection->peer_devices;
	connection->peer_devices = new_peer_device;
}

static void store_peer_device_check(struct resources_list *resource, struct peer_devices_list *new_peer_device)
{
	struct connections_list *connection;
	struct peer_devices_list *old_peer_device;

	connection = find_connection(resource, new_peer_device->ctx.ctx_conn_name);
	if (!connection) {
		free_peer_device(new_peer_device);
		return;
	}

	old_peer_device = connection_find_peer_device(connection, new_peer_device->ctx.ctx_volume);
	if (old_peer_device) {
		free_peer_device(new_peer_device);
		return;
	}

	connection_store_peer_device(connection, new_peer_device);
}

static void delete_peer_device(struct resources_list *resource, struct drbd_cfg_context *ctx)
{
	struct connections_list *connection;
	struct peer_devices_list *peer_device, **previous_next;

	connection = find_connection(resource, ctx->ctx_conn_name);
	if (!connection)
		return;

	previous_next = &connection->peer_devices;
	for (peer_device = connection->peer_devices; peer_device; peer_device = peer_device->next) {
		if (peer_device->ctx.ctx_volume == ctx->ctx_volume) {
			*previous_next = peer_device->next;
			free_peer_device(peer_device);
			return;
		}
		previous_next = &peer_device->next;
	}
}

static bool path_address_strs(struct drbd_cfg_context *ctx, char *my_addr, char *peer_addr)
{
	if (!address_str(my_addr, ctx->ctx_my_addr, ctx->ctx_my_addr_len)) {
		fprintf(stderr, "Invalid local address for path for name:%s peer-node-id:%u conn-name:%s\n",
				ctx->ctx_resource_name, ctx->ctx_peer_node_id, ctx->ctx_conn_name);
		return false;
	}

	if (!address_str(peer_addr, ctx->ctx_peer_addr, ctx->ctx_peer_addr_len)) {
		fprintf(stderr, "Invalid peer address for path for name:%s peer-node-id:%u conn-name:%s\n",
				ctx->ctx_resource_name, ctx->ctx_peer_node_id, ctx->ctx_conn_name);
		return false;
	}

	return true;
}

static bool paths_equal(struct drbd_cfg_context *ctx_a, struct drbd_cfg_context *ctx_b)
{
	/* Just compare bytes. Strictly speaking, there can be situations where
	 * addresses with different binary reprepresentations correspond to the
	 * same address. However, we can rely on DRBD always using the same
	 * representation for a given path. */

	if (ctx_a->ctx_my_addr_len != ctx_b->ctx_my_addr_len)
		return false;

	if (ctx_a->ctx_peer_addr_len != ctx_b->ctx_peer_addr_len)
		return false;

	if (memcmp(ctx_a->ctx_my_addr, ctx_b->ctx_my_addr, ctx_a->ctx_my_addr_len))
		return false;

	if (memcmp(ctx_a->ctx_peer_addr, ctx_b->ctx_peer_addr, ctx_a->ctx_peer_addr_len))
		return false;

	return true;
}

static struct paths_list *connection_find_path(struct connections_list *connection, struct drbd_cfg_context *ctx)
{
	struct paths_list *path;
	for (path = connection->paths; path; path = path->next) {
		if (paths_equal(&path->ctx, ctx))
			return path;
	}
	return NULL;
}

static struct paths_list *find_path(struct resources_list *resource, struct drbd_cfg_context *ctx)
{
	struct connections_list *connection;

	connection = find_connection(resource, ctx->ctx_conn_name);
	if (!connection)
		return NULL;

	return connection_find_path(connection, ctx);
}

static void connection_store_path(struct connections_list *connection, struct paths_list *new_path)
{
	new_path->next = connection->paths;
	connection->paths = new_path;
}

static void store_path_check(struct resources_list *resource, struct paths_list *new_path)
{
	struct connections_list *connection;
	struct paths_list *old_path;

	connection = find_connection(resource, new_path->ctx.ctx_conn_name);
	if (!connection) {
		free(new_path);
		return;
	}

	old_path = connection_find_path(connection, &new_path->ctx);
	if (old_path) {
		free(new_path);
		return;
	}

	connection_store_path(connection, new_path);
}

static void delete_path(struct resources_list *resource, struct drbd_cfg_context *ctx)
{
	struct connections_list *connection;
	struct paths_list *path, **previous_next;

	connection = find_connection(resource, ctx->ctx_conn_name);
	if (!connection)
		return;

	previous_next = &connection->paths;
	for (path = connection->paths; path; path = path->next) {
		if (paths_equal(&path->ctx, ctx)) {
			*previous_next = path->next;
			free(path);
			return;
		}
		previous_next = &path->next;
	}
}

static struct nlattr *nla_copy(const struct nlattr *old_nla)
{
	int size;
	struct nlattr *new_nla;

	if (!old_nla)
		return NULL;

	size = nla_total_size(nla_len(old_nla));
	new_nla = malloc(size);
	memcpy(new_nla, old_nla, size);
	return new_nla;
}

static struct resources_list *deep_copy_resource(struct resources_list *old_resource)
{
	struct resources_list *new_resource;
	struct devices_list *old_device;
	struct connections_list *old_connection;

	new_resource = calloc(1, sizeof(*new_resource));

	new_resource->name = strdup(old_resource->name);
	new_resource->res_opts = nla_copy(old_resource->res_opts);
	new_resource->info = old_resource->info;
	new_resource->statistics = old_resource->statistics;

	for (old_device = old_resource->devices; old_device; old_device = old_device->next) {
		struct devices_list *new_device;

		new_device = calloc(1, sizeof(*new_device));

		new_device->minor = old_device->minor;
		new_device->ctx = old_device->ctx;
		new_device->disk_conf_nl = nla_copy(old_device->disk_conf_nl);
		new_device->disk_conf = old_device->disk_conf;
		new_device->info = old_device->info;
		new_device->statistics = old_device->statistics;

		store_device(new_resource, new_device);
	}

	for (old_connection = old_resource->connections; old_connection; old_connection = old_connection->next) {
		struct connections_list *new_connection;
		struct peer_devices_list *old_peer_device;
		struct paths_list *old_path;

		new_connection = calloc(1, sizeof(*new_connection));

		new_connection->ctx = old_connection->ctx;
		new_connection->path_list = nla_copy(old_connection->path_list);
		new_connection->net_conf = nla_copy(old_connection->net_conf);
		new_connection->info = old_connection->info;
		new_connection->statistics = old_connection->statistics;

		for (old_peer_device = old_connection->peer_devices; old_peer_device; old_peer_device = old_peer_device->next) {
			struct peer_devices_list *new_peer_device;

			new_peer_device = calloc(1, sizeof(*new_peer_device));

			new_peer_device->ctx = old_peer_device->ctx;
			new_peer_device->peer_device_conf = nla_copy(old_peer_device->peer_device_conf);
			new_peer_device->info = old_peer_device->info;
			new_peer_device->statistics = old_peer_device->statistics;

			connection_store_peer_device(new_connection, new_peer_device);
		}

		for (old_path = old_connection->paths; old_path; old_path = old_path->next) {
			struct paths_list *new_path;

			new_path = calloc(1, sizeof(*new_path));

			new_path->ctx = old_path->ctx;
			new_path->info = old_path->info;

			connection_store_path(new_connection, new_path);
		}

		store_connection(new_resource, new_connection);
	}

	return new_resource;
}

static void free_resource(struct resources_list *resource)
{
	free(resource->name);
	free(resource->res_opts);
	free_devices(resource->devices);
	free_connections(resource->connections);
	free(resource);
}

static int count_up_to_date_replicas(struct resources_list *resource, struct devices_list *device)
{
	struct connections_list *connection;
	struct peer_devices_list *peer_device;

	int up_to_date_replicas = 0;

	if (device->info.dev_disk_state == D_UP_TO_DATE) {
		up_to_date_replicas++;
	}

	for (connection = resource->connections; connection; connection = connection->next) {
		for (peer_device = connection->peer_devices; peer_device; peer_device = peer_device->next) {
			if (peer_device->ctx.ctx_volume == device->ctx.ctx_volume && peer_device->info.peer_disk_state == D_UP_TO_DATE)
				up_to_date_replicas++;
		}
	}

	return up_to_date_replicas;
}

static bool have_primary(struct resources_list *resource)
{
	struct connections_list *connection;

	if (resource->info.res_role == R_PRIMARY)
		return true;

	for (connection = resource->connections; connection; connection = connection->next) {
		if (connection->info.conn_role == R_PRIMARY)
			return true;
	}

	return false;
}

struct promotion_info {
	bool may_promote;
	int promotion_score;
};

static struct promotion_info compute_promotion_info(struct resources_list *resource)
{
	struct promotion_info info = {false, 0};
	struct devices_list *device;
	int device_count = 0;
	int up_to_date_devices = 0;
	bool all_devices_have_disk = true;
	int min_up_to_date_replicas = -1;
	int all_local_devices_score;
	int local_devices_score;
	int replicas_score;

	/* Strictly speaking, one may promote resources with no devices.
	 * However, we return false here as a convenience so that resources
	 * that are being configured do not briefly report that they can be
	 * promoted. */
	if (!resource->devices)
		return info;

	for (device = resource->devices; device; device = device->next) {
		int up_to_date_replicas = count_up_to_date_replicas(resource, device);
		enum drbd_disk_state disk_state = device->info.dev_disk_state;

		device_count++;

		if (!device->info.dev_has_quorum)
			return info;

		if (disk_state == D_UP_TO_DATE)
			up_to_date_devices++;
		else if (disk_state != D_CONSISTENT && disk_state != D_INCONSISTENT)
			all_devices_have_disk = false;

		if (min_up_to_date_replicas == -1 || up_to_date_replicas < min_up_to_date_replicas)
			min_up_to_date_replicas = up_to_date_replicas;
	}

	if (min_up_to_date_replicas == -1 || min_up_to_date_replicas == 0)
		return info;

	info.may_promote = !have_primary(resource) && (up_to_date_devices == device_count);

	all_local_devices_score = all_devices_have_disk ? 1 : 0;
	local_devices_score = min(up_to_date_devices, 99);
	replicas_score = min_up_to_date_replicas != -1 ? min(min_up_to_date_replicas, 99) : 0;

	info.promotion_score = all_local_devices_score * 10000 +
		local_devices_score * 100 +
		replicas_score;

	return info;
}

static void print_resource_changes(const char *prefix, const char *action_new, struct resources_list *old_resource, struct resources_list *new_resource)
{
	struct promotion_info new_promotion_info;
	struct promotion_info old_promotion_info;
	bool role_changed;
	bool info_changed;
	bool statistics_changed;
	bool promotion_info_changed;

	new_promotion_info = compute_promotion_info(new_resource);

	role_changed = !old_resource || new_resource->info.res_role != old_resource->info.res_role;
	info_changed = !old_resource ||
			new_resource->info.res_susp != old_resource->info.res_susp ||
			new_resource->info.res_susp_nod != old_resource->info.res_susp_nod ||
			new_resource->info.res_susp_fen != old_resource->info.res_susp_fen ||
			new_resource->info.res_susp_quorum != old_resource->info.res_susp_quorum;
	statistics_changed = opt_statistics &&
		(!old_resource ||
		 memcmp(&new_resource->statistics, &old_resource->statistics, sizeof(struct resource_statistics)));

	if (old_resource) {
		old_promotion_info = compute_promotion_info(old_resource);
		promotion_info_changed = new_promotion_info.may_promote != old_promotion_info.may_promote ||
			new_promotion_info.promotion_score != old_promotion_info.promotion_score;
	} else {
		promotion_info_changed = true;
	}

	if (!role_changed && !info_changed && !statistics_changed && !promotion_info_changed)
		return;

	printf("%s%s ", prefix, old_resource ? action_change : action_new);
	printf("%s name:%s", object_resource, new_resource->name);
	if (role_changed)
		printf(" role:%s%s%s",
				ROLE_COLOR_STRING(new_resource->info.res_role, 1));
	if (info_changed)
		printf(" suspended:%s",
				susp_str(&new_resource->info));
	if (statistics_changed)
		print_resource_statistics(0, old_resource ? &old_resource->statistics : NULL,
				&new_resource->statistics, nowrap_printf);
	if (promotion_info_changed) {
		printf(" may_promote:%s", new_promotion_info.may_promote ? "yes" : "no");
		printf(" promotion_score:%d", new_promotion_info.promotion_score);
	}
	printf("\n");
}

static void print_device_changes(const char *prefix, const char *action_new, const char *resource_name, struct devices_list *new_device, struct devices_list *old_device)
{
	bool info_changed;
	bool statistics_changed;

	info_changed = !old_device || new_device->info.dev_disk_state != old_device->info.dev_disk_state ||
		new_device->info.dev_has_quorum != old_device->info.dev_has_quorum;
	statistics_changed = opt_statistics &&
		(!old_device ||
		 memcmp(&new_device->statistics, &old_device->statistics, sizeof(struct device_statistics)));

	if (!info_changed && !statistics_changed)
		return;

	printf("%s%s ", prefix, old_device ? action_change : action_new);
	printf("%s name:%s volume:%u minor:%u", object_device, resource_name, new_device->ctx.ctx_volume, new_device->minor);
	if (info_changed) {
		bool intentional = new_device->info.is_intentional_diskless == 1;
		printf(" disk:%s%s%s",
				DISK_COLOR_STRING(new_device->info.dev_disk_state, intentional, true));
		printf(" client:%s", intentional_diskless_str(&new_device->info));
		printf(" quorum:%s", new_device->info.dev_has_quorum ? "yes" : "no");
	}
	if (statistics_changed) {
		print_device_statistics(0, old_device ? &old_device->statistics : NULL,
				&new_device->statistics, nowrap_printf);
	}
	printf("\n");
}

static void print_peer_device_changes(const char *prefix, const char *action_new, const char *resource_name, struct peer_devices_list *new_peer_device, struct peer_devices_list *old_peer_device)
{
	bool repl_state_changed;
	bool disk_changed;
	bool resync_suspended_changed;
	bool statistics_changed;

	repl_state_changed = !old_peer_device || new_peer_device->info.peer_repl_state != old_peer_device->info.peer_repl_state;
	disk_changed = !old_peer_device || new_peer_device->info.peer_disk_state != old_peer_device->info.peer_disk_state;
	resync_suspended_changed = !old_peer_device ||
		new_peer_device->info.peer_resync_susp_user != old_peer_device->info.peer_resync_susp_user ||
		new_peer_device->info.peer_resync_susp_peer != old_peer_device->info.peer_resync_susp_peer ||
		new_peer_device->info.peer_resync_susp_dependency != old_peer_device->info.peer_resync_susp_dependency;
	statistics_changed = !old_peer_device ||
		 memcmp(&new_peer_device->statistics, &old_peer_device->statistics, sizeof(struct peer_device_statistics));

	if (!repl_state_changed && !disk_changed && !resync_suspended_changed && !statistics_changed)
		return;

	printf("%s%s ", prefix, old_peer_device ? action_change : action_new);
	printf("%s name:%s peer-node-id:%u conn-name:%s volume:%u", object_peer_device, resource_name, new_peer_device->ctx.ctx_peer_node_id, new_peer_device->ctx.ctx_conn_name, new_peer_device->ctx.ctx_volume);
	if (repl_state_changed) {
		printf(" replication:%s%s%s",
				REPL_COLOR_STRING(new_peer_device->info.peer_repl_state));
	}
	if (disk_changed) {
		bool intentional = new_peer_device->info.peer_is_intentional_diskless == 1;
		printf(" peer-disk:%s%s%s",
				DISK_COLOR_STRING(new_peer_device->info.peer_disk_state, intentional,  false));
		printf(" peer-client:%s", peer_intentional_diskless_str(&new_peer_device->info));
	}
	if (resync_suspended_changed) {
		printf(" resync-suspended:%s",
				resync_susp_str(&new_peer_device->info));
	}

	if (statistics_changed) {
		print_peer_device_statistics(0, old_peer_device ? &old_peer_device->statistics : NULL,
				&new_peer_device->statistics, nowrap_printf);
	}
	printf("\n");
}

static void print_path_changes(const char *prefix, const char *action_new, const char *resource_name, struct paths_list *new_path, struct paths_list *old_path)
{
	char my_addr[ADDRESS_STR_MAX];
	char peer_addr[ADDRESS_STR_MAX];
	bool established_changed;

	established_changed = !old_path || new_path->info.path_established != old_path->info.path_established;

	if (!established_changed)
		return;

	if (!path_address_strs(&new_path->ctx, my_addr, peer_addr))
		return;

	printf("%s%s ", prefix, old_path ? action_change : action_new);
	printf("%s name:%s peer-node-id:%u conn-name:%s local:%s peer:%s",
			object_path, resource_name,
			new_path->ctx.ctx_peer_node_id, new_path->ctx.ctx_conn_name,
			my_addr, peer_addr);
	if (established_changed) {
		printf(" established:%s",
				new_path->info.path_established ? "yes" : "no");
	}
	printf("\n");
}

static void print_connection_changes(const char *prefix, const char *action_new, const char *resource_name, struct connections_list *new_connection, struct connections_list *old_connection)
{
	bool connection_state_changed;
	bool role_changed;
	bool statistics_changed;

	connection_state_changed = !old_connection || new_connection->info.conn_connection_state != old_connection->info.conn_connection_state;
	role_changed = !old_connection || new_connection->info.conn_role != old_connection->info.conn_role;
	statistics_changed = opt_statistics &&
		(!old_connection ||
		 memcmp(&new_connection->statistics, &old_connection->statistics, sizeof(struct connection_statistics)));

	if (!connection_state_changed && !role_changed && !statistics_changed)
		return;

	printf("%s%s ", prefix, old_connection ? action_change : action_new);
	printf("%s name:%s peer-node-id:%u conn-name:%s", object_connection, resource_name, new_connection->ctx.ctx_peer_node_id, new_connection->ctx.ctx_conn_name);
	if (connection_state_changed) {
		printf(" connection:%s%s%s",
				CONN_COLOR_STRING(new_connection->info.conn_connection_state));
	}
	if (role_changed) {
		printf(" role:%s%s%s",
				ROLE_COLOR_STRING(new_connection->info.conn_role, 0));
	}
	if (statistics_changed) {
		print_connection_statistics(0, old_connection ? &old_connection->statistics : NULL,
				&new_connection->statistics, nowrap_printf);
	}
	printf("\n");
}

static void print_changes(const char *prefix, const char *action_new, struct resources_list *old_resource, struct resources_list *new_resource)
{
	struct devices_list *new_device, *old_device;
	struct connections_list *new_connection, *old_connection;

	print_resource_changes(prefix, action_new, old_resource, new_resource);

	for (new_connection = new_resource->connections; new_connection; new_connection = new_connection->next) {
		old_connection = old_resource ? find_connection(old_resource, new_connection->ctx.ctx_conn_name) : NULL;
		print_connection_changes(prefix, action_new, new_resource->name, new_connection, old_connection);
	}

	for (new_device = new_resource->devices; new_device; new_device = new_device->next) {
		old_device = old_resource ? find_device(old_resource, new_device->ctx.ctx_volume) : NULL;
		print_device_changes(prefix, action_new, new_resource->name, new_device, old_device);
	}

	for (new_connection = new_resource->connections; new_connection; new_connection = new_connection->next) {
		struct peer_devices_list *new_peer_device, *old_peer_device;
		struct paths_list *new_path, *old_path;

		old_connection = old_resource ? find_connection(old_resource, new_connection->ctx.ctx_conn_name) : NULL;

		for (new_peer_device = new_connection->peer_devices; new_peer_device; new_peer_device = new_peer_device->next) {
			old_peer_device = old_connection ? connection_find_peer_device(old_connection, new_peer_device->ctx.ctx_volume) : NULL;
			print_peer_device_changes(prefix, action_new, new_resource->name, new_peer_device, old_peer_device);
		}

		for (new_path = new_connection->paths; new_path; new_path = new_path->next) {
			old_path = old_connection ? connection_find_path(old_connection, &new_path->ctx) : NULL;
			print_path_changes(prefix, action_new, new_resource->name, new_path, old_path);
		}

		for (old_path = old_connection ? old_connection->paths : NULL; old_path; old_path = old_path->next) {
			struct paths_list *new_path = connection_find_path(new_connection, &old_path->ctx);
			if (!new_path) {
				char my_addr[ADDRESS_STR_MAX];
				char peer_addr[ADDRESS_STR_MAX];

				if (!path_address_strs(&old_path->ctx, my_addr, peer_addr))
					continue;

				printf("%s%s %s name:%s peer-node-id:%u conn-name:%s local:%s peer:%s\n",
						prefix, action_destroy, object_path, new_resource->name,
						old_path->ctx.ctx_peer_node_id, old_path->ctx.ctx_conn_name,
						my_addr, peer_addr);
			}
		}

		for (old_peer_device = old_connection ? old_connection->peer_devices : NULL; old_peer_device; old_peer_device = old_peer_device->next) {
			struct peer_devices_list *new_peer_device = connection_find_peer_device(new_connection, old_peer_device->ctx.ctx_volume);
			if (!new_peer_device) {
				printf("%s%s %s name:%s peer-node-id:%u conn-name:%s volume:%u\n",
						prefix, action_destroy, object_peer_device, new_resource->name,
						old_peer_device->ctx.ctx_peer_node_id, old_peer_device->ctx.ctx_conn_name,
						old_peer_device->ctx.ctx_volume);
			}
		}
	}

	for (old_connection = old_resource ? old_resource->connections : NULL; old_connection; old_connection = old_connection->next) {
		struct connections_list *new_connection = find_connection(new_resource, old_connection->ctx.ctx_conn_name);
		if (!new_connection) {
			printf("%s%s %s name:%s peer-node-id:%u conn-name:%s\n",
					prefix, action_destroy, object_connection, new_resource->name,
					old_connection->ctx.ctx_peer_node_id, old_connection->ctx.ctx_conn_name);
		}
	}

	for (old_device = old_resource ? old_resource->devices : NULL; old_device; old_device = old_device->next) {
		struct devices_list *new_device = find_device(new_resource, old_device->ctx.ctx_volume);
		if (!new_device) {
			printf("%s%s %s name:%s volume:%u\n",
					prefix, action_destroy, object_device, new_resource->name,
					old_device->ctx.ctx_volume);
		}
	}

	if (new_resource->destroyed) {
		printf("%s%s %s name:%s\n", prefix, action_destroy, object_resource, new_resource->name);
	}
}

static int format_timestamp(char *timestamp_prefix)
{
	struct timeval tv;
	struct tm *tm;
	int ret;

	if (!opt_timestamps) {
		timestamp_prefix[0] = '\0';
		return 0;
	}

	gettimeofday(&tv, NULL);

	tm = localtime(&tv.tv_sec);
	ret = snprintf(timestamp_prefix, TIMESTAMP_LEN, "%04u-%02u-%02uT%02u:%02u:%02u.%06u%+03d:%02u ",
			tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
			tm->tm_hour, tm->tm_min, tm->tm_sec,
			(int)tv.tv_usec,
			(int)(tm->tm_gmtoff / 3600),
			(int)((abs(tm->tm_gmtoff) / 60) % 60));
	if (ret < 0) {
		fprintf(stderr, "Unable to format timestamp\n");
		return ret;
	}

	return 0;
}

static void print_helper(const char *timestamp_prefix, struct drbd_cfg_context *ctx, unsigned minor, bool response, struct drbd_helper_info *helper_info)
{
	char my_addr[ADDRESS_STR_MAX] = "";
	char peer_addr[ADDRESS_STR_MAX] = "";

	if (ctx->ctx_my_addr_len && ctx->ctx_peer_addr_len) {
		if (!path_address_strs(ctx, my_addr, peer_addr))
			return;
	}

	printf("%s%s %s", timestamp_prefix, response ? action_response : action_call, object_helper);

	if (ctx->ctx_resource_name)
		printf(" name:%s", ctx->ctx_resource_name);

	if (ctx->ctx_peer_node_id != -1U)
		printf(" peer-node-id:%d", ctx->ctx_peer_node_id);

	if (ctx->ctx_conn_name_len)
		printf(" conn-name:%s", ctx->ctx_conn_name);

	if (my_addr[0])
		printf(" local:%s", my_addr);

	if (peer_addr[0])
		printf(" peer:%s", peer_addr);

	if (ctx->ctx_volume != -1U)
		printf(" volume:%u", ctx->ctx_volume);

	if (minor != -1U)
		printf(" minor:%u", minor);

	printf(" helper:%s", helper_info->helper_name);
	if (response)
		printf(" status:%u", helper_info->helper_status);

	printf("\n");
}

/* singly-linked list entry with a netlink message */
struct nlmsg_entry {
	struct nlmsg_entry *next;
	struct nlmsghdr *nlh;
};

/* copy an entire netlink message */
static struct nlmsg_entry *nlmsg_copy(struct genl_info *info)
{
	struct nlmsg_entry *entry = calloc(1, sizeof(struct nlmsg_entry));
	entry->nlh = malloc(info->nlhdr->nlmsg_len);
	memcpy(entry->nlh, info->nlhdr, info->nlhdr->nlmsg_len);
	return entry;
}

int print_event(struct drbd_cmd *cm, struct genl_info *info, void *u_ptr)
{
	static uint32_t last_seq;
	static bool last_seq_known;
	static bool initial_state = true;
	static struct nlmsg_entry *stored_messages = NULL;

	struct drbd_notification_header nh = { .nh_type = -1U };
	enum drbd_notification_type action;
	struct drbd_genlmsghdr *dh;
	int err;
	char timestamp_prefix[TIMESTAMP_LEN];

	if (!info)
		return 0;

	dh = info->userhdr;
	if (dh->ret_code == ERR_MINOR_INVALID && cm->missing_ok)
		return 0;
	if (dh->ret_code != NO_ERROR)
		return dh->ret_code;

	err = drbd_notification_header_from_attrs(&nh, info);
	if (err)
		return 0;
	action = nh.nh_type & ~NOTIFY_FLAGS;

	if (opt_now && action != NOTIFY_EXISTS)
		return 0;

	err = format_timestamp(timestamp_prefix);
	if (err)
		exit(20);

	if (info->genlhdr->cmd == DRBD_INITIAL_STATE_DONE) {
		struct nlmsg_entry *entry, *next_entry;
		initial_state = false;
		printf("%s%s -\n", timestamp_prefix, action_exists);
		fflush(stdout);
		if (opt_now)
			return -1;

		/* now apply stored messages */
		for (entry = stored_messages; entry; entry = next_entry) {
			struct genl_info stored_info = {
				.seq = entry->nlh->nlmsg_seq,
				.nlhdr = entry->nlh,
				.genlhdr = nlmsg_data(entry->nlh),
				.userhdr = genlmsg_data(nlmsg_data(entry->nlh)),
				.attrs = global_attrs,
			};

			err = drbd_tla_parse(entry->nlh);
			if (err) {
				fprintf(stderr, "drbd_tla_parse() failed");
				return 1;
			}

			err = apply_event(timestamp_prefix, &stored_info, false);
			if (err)
				return err;

			next_entry = entry->next;
			free(entry->nlh);
			free(entry);
		}

		return 0;
	}

	if (action != NOTIFY_EXISTS) {
		if (last_seq_known) {
			int skipped = info->nlhdr->nlmsg_seq - (last_seq + 1);

			if (skipped) {
				printf("%s- skipped %d\n", timestamp_prefix, skipped);
				fflush(stdout);
			}
		}
		last_seq = info->nlhdr->nlmsg_seq;
		last_seq_known = true;

		if (initial_state) {
			/* store message until initial state is finished */
			struct nlmsg_entry *entry, **previous_next = &stored_messages;
			for (entry = stored_messages; entry; entry = entry->next)
				previous_next = &entry->next;
			*previous_next = nlmsg_copy(info);
			return 0;
		}
	}

	return apply_event(timestamp_prefix, info, initial_state);
}

static int apply_event(const char *prefix, struct genl_info *info, bool initial_state)
{
	int err;
	struct drbd_notification_header nh = { .nh_type = -1U };
	enum drbd_notification_type action;
	struct drbd_cfg_context ctx = { .ctx_volume = -1U, .ctx_peer_node_id = -1U, };
	bool is_resource_create;
	struct resources_list *new_resource;
	struct resources_list *old_resource;
	struct devices_list *device;
	struct connections_list *connection;
	struct peer_devices_list *peer_device;
	struct paths_list *path;

	err = drbd_notification_header_from_attrs(&nh, info);
	if (err)
		return 0;
	action = nh.nh_type & ~NOTIFY_FLAGS;

	err = drbd_cfg_context_from_attrs(&ctx, info);
	if (err)
		return 0;

	is_resource_create = info->genlhdr->cmd == DRBD_RESOURCE_STATE &&
		(action == NOTIFY_CREATE || action == NOTIFY_EXISTS);

	/* look for the resource in the current update */
	for (new_resource = update_resources; new_resource; new_resource = new_resource->next) {
		if (!strcmp(new_resource->name, ctx.ctx_resource_name))
			break;
	}
	/* look for the resource in the master copy */
	old_resource = find_resource(ctx.ctx_resource_name);

	if (is_resource_create) {
		if (new_resource || old_resource)
			return 0;

		new_resource = new_resource_from_info(info);
		store_update_resource(new_resource);
	} else if (!new_resource) {
		if (!old_resource)
			return 0;

		new_resource = deep_copy_resource(old_resource);
		store_update_resource(new_resource);
	}

	switch (action) {
	case NOTIFY_EXISTS:
	case NOTIFY_CREATE:
		switch(info->genlhdr->cmd) {
		case DRBD_RESOURCE_STATE:
			// resource already created
			break;
		case DRBD_DEVICE_STATE:
			device = new_device_from_info(info);
			store_device_check(new_resource, device);
			break;
		case DRBD_CONNECTION_STATE:
			connection = new_connection_from_info(info);
			store_connection_check(new_resource, connection);
			break;
		case DRBD_PEER_DEVICE_STATE:
			peer_device = new_peer_device_from_info(info);
			store_peer_device_check(new_resource, peer_device);
			break;
		case DRBD_PATH_STATE:
			path = new_path_from_info(info);
			store_path_check(new_resource, path);
			break;
		default:
			dbg(1, "unknown exists/create notification %d\n", info->genlhdr->cmd);
			goto out;
		}
		break;
	case NOTIFY_CHANGE:
		switch(info->genlhdr->cmd) {
		case DRBD_RESOURCE_STATE:
			resource_info_from_attrs(&new_resource->info, info);
			memset(&new_resource->statistics, -1, sizeof(new_resource->statistics));
			resource_statistics_from_attrs(&new_resource->statistics, info);
			break;
		case DRBD_DEVICE_STATE:
			device = find_device(new_resource, ctx.ctx_volume);
			if (!device)
				break;
			disk_conf_from_attrs(&device->disk_conf, info);
			device->info.dev_disk_state = D_DISKLESS;
			device->info.is_intentional_diskless = IS_INTENTIONAL_DEF;
			device_info_from_attrs(&device->info, info);
			memset(&device->statistics, -1, sizeof(device->statistics));
			device_statistics_from_attrs(&device->statistics, info);
			break;
		case DRBD_CONNECTION_STATE:
			connection = find_connection(new_resource, ctx.ctx_conn_name);
			if (!connection)
				break;
			connection_info_from_attrs(&connection->info, info);
			memset(&connection->statistics, -1, sizeof(connection->statistics));
			connection_statistics_from_attrs(&connection->statistics, info);
			break;
		case DRBD_PEER_DEVICE_STATE:
			peer_device = find_peer_device(new_resource, &ctx);
			if (!peer_device)
				break;
			peer_device->info.peer_is_intentional_diskless = IS_INTENTIONAL_DEF;
			peer_device_info_from_attrs(&peer_device->info, info);
			memset(&peer_device->statistics, -1, sizeof(peer_device->statistics));
			peer_device_statistics_from_attrs(&peer_device->statistics, info);
			break;
		case DRBD_PATH_STATE:
			/* DRBD does not send initial exists messages for paths
			 * so we have to be prepared for changes to unknown
			 * paths */
			path = find_path(new_resource, &ctx);
			if (path) {
				drbd_path_info_from_attrs(&path->info, info);
			} else {
				path = new_path_from_info(info);
				store_path_check(new_resource, path);
			}
			break;
		default:
			dbg(1, "unknown change notification %d\n", info->genlhdr->cmd);
			goto out;
		}

		break;
	case NOTIFY_DESTROY:
		switch(info->genlhdr->cmd) {
		case DRBD_RESOURCE_STATE:
			new_resource->destroyed = true;
			break;
		case DRBD_DEVICE_STATE:
			delete_device(new_resource, ctx.ctx_volume);
			break;
		case DRBD_CONNECTION_STATE:
			delete_connection(new_resource, ctx.ctx_conn_name);
			break;
		case DRBD_PEER_DEVICE_STATE:
			delete_peer_device(new_resource, &ctx);
			break;
		case DRBD_PATH_STATE:
			/* DRBD does not send initial exists messages for paths
			 * so we have to be prepared for destroy messages for
			 * unknown paths */
			delete_path(new_resource, &ctx);
			break;
		default:
			dbg(1, "unknown destroy notification %d\n", info->genlhdr->cmd);
			goto out;
		}
		break;
	case NOTIFY_CALL:
	case NOTIFY_RESPONSE: {
		struct drbd_helper_info helper_info;

		if (info->genlhdr->cmd != DRBD_HELPER)
		{
			dbg(1, "unknown call/response notification %d\n", info->genlhdr->cmd);
			goto out;
		}

		err = drbd_helper_info_from_attrs(&helper_info, info);
		if (err) {
			dbg(1, "helper info missing\n");
			goto out;
		}

		print_helper(prefix, &ctx,
				((struct drbd_genlmsghdr*)(info->userhdr))->minor,
				action == NOTIFY_RESPONSE, &helper_info);
		break;
	}
	default:
		dbg(1, "unknown notification type %d\n", action);
		goto out;
	}

	if (!(nh.nh_type & NOTIFY_CONTINUES)) {
		struct resources_list *next_resource;

		for (new_resource = update_resources; new_resource; new_resource = next_resource) {
			struct resources_list *old_resource = find_resource(new_resource->name);

			print_changes(prefix, initial_state ? action_exists : action_create, old_resource, new_resource);

			if (old_resource) {
				delete_resource(old_resource);
				free_resource(old_resource);
			}

			next_resource = new_resource->next;
			if (new_resource->destroyed) {
				free_resource(new_resource);
			} else {
				store_resource(new_resource);
			}
		}

		update_resources = NULL;
	}

out:
	fflush(stdout);
	return 0;
}
