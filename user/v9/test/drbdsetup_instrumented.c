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
#define _XOPEN_SOURCE 700
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <getopt.h>

#include <netinet/in.h>

#include "../drbdsetup.h"
#include "../drbdsetup_colors.h"

#include "drbd_protocol.h"

int print_event(struct drbd_cmd *cm, struct genl_info *info, void *u_ptr);

extern struct genl_family drbd_genl_family;

static char *test_resource_name = "some-resource";
static __u32 test_node_id = 4;
static __u32 test_minor = 1000;
static __u32 test_minor_b = 1001;
static __u32 test_volume_number = 0;
static __u32 test_volume_number_b = 1;
static __u32 test_peer_node_id = 1;
static char *test_peer_name = "some-peer";
static char *test_backing_dev_path = "/dev/sda";

struct test_vars {
	int msg_seq;
	int minor;
	int volume_number;
};

/*
 * ################# Helper functions #################
 */

static char *backing_dev(__u32 disk_state)
{
	if (disk_state == D_UP_TO_DATE || disk_state == D_INCONSISTENT)
		return test_backing_dev_path;
	return "none";
}

void test_msg_put(struct msg_buff *smsg, __u8 cmd, __u32 minor)
{
	struct drbd_genlmsghdr *dhdr;

	dhdr = genlmsg_put(smsg, &drbd_genl_family, 0, cmd);
	dhdr->minor = minor;
	/* set ret_code because this will be directly mapped to a kernel->user message */
	dhdr->ret_code = NO_ERROR;
}

void test_notification_header(struct msg_buff *smsg, __u32 nh_type)
{
	struct nlattr *nla = nla_nest_start(smsg, DRBD_NLA_NOTIFICATION_HEADER);
	nla_put_u32(smsg, T_nh_type, nh_type);
	nla_nest_end(smsg, nla);
}

void test_resource_context(struct msg_buff *smsg, struct test_vars *vars)
{
	struct nlattr *nla = nla_nest_start(smsg, DRBD_NLA_CFG_CONTEXT);
	nla_put_string(smsg, T_ctx_resource_name, test_resource_name);
	nla_nest_end(smsg, nla);
}

void test_resource_opts(struct msg_buff *smsg, struct test_vars *vars)
{
	struct nlattr *nla = nla_nest_start(smsg, DRBD_NLA_RESOURCE_OPTS);
	nla_put_u32(smsg,  __nla_type(T_node_id), test_node_id);
	nla_nest_end(smsg, nla);
}

void test_resource_info(struct msg_buff *smsg, struct test_vars *vars, __u32 res_role)
{
	struct nlattr *nla = nla_nest_start(smsg, DRBD_NLA_RESOURCE_INFO);
	nla_put_u32(smsg, T_res_role, res_role);
	nla_put_u8(smsg, T_res_susp, false);
	nla_put_u8(smsg, T_res_susp_nod, false);
	nla_put_u8(smsg, T_res_susp_fen, false);
	nla_put_u8(smsg, T_res_susp_quorum, false);
	nla_nest_end(smsg, nla);
}

void test_resource_statistics(struct msg_buff *smsg, struct test_vars *vars)
{
	struct nlattr *nla = nla_nest_start(smsg, DRBD_NLA_RESOURCE_STATISTICS);
	nla_put_u32(smsg, T_res_stat_write_ordering, WO_NONE);
	nla_nest_end(smsg, nla);
}

void test_device_context(struct msg_buff *smsg, struct test_vars *vars, __u32 volume_number)
{
	struct nlattr *nla = nla_nest_start(smsg, DRBD_NLA_CFG_CONTEXT);
	nla_put_string(smsg, T_ctx_resource_name, test_resource_name);
	nla_put_u32(smsg, T_ctx_volume, volume_number);
	nla_nest_end(smsg, nla);
}

void test_disk_conf(struct msg_buff *smsg, struct test_vars *vars, __u32 dev_disk_state)
{
	struct nlattr *nla = nla_nest_start(smsg, DRBD_NLA_DISK_CONF);
	nla_put_string(smsg, T_backing_dev, backing_dev(dev_disk_state));
	nla_put_string(smsg, T_meta_dev, backing_dev(dev_disk_state));
	nla_put_u32(smsg, T_meta_dev_idx, DRBD_MD_INDEX_FLEX_INT);
	nla_nest_end(smsg, nla);
}

void test_device_conf(struct msg_buff *smsg, struct test_vars *vars)
{
	struct nlattr *nla = nla_nest_start(smsg, DRBD_NLA_DEVICE_CONF);
	nla_put_u32(smsg, T_max_bio_size, DRBD_MAX_BIO_SIZE);
	nla_nest_end(smsg, nla);
}

void test_device_info(struct msg_buff *smsg, struct test_vars *vars, __u32 dev_disk_state, __u8 dev_has_quorum)
{
	struct nlattr *nla = nla_nest_start(smsg, DRBD_NLA_DEVICE_INFO);
	nla_put_u32(smsg, T_dev_disk_state, dev_disk_state);
	nla_put_u8(smsg, T_is_intentional_diskless, false);
	nla_put_u8(smsg, T_dev_is_open, false);
	nla_put_u8(smsg, T_dev_has_quorum, dev_has_quorum);
	nla_put_string(smsg, T_backing_dev_path, backing_dev(dev_disk_state));
	nla_nest_end(smsg, nla);
}

void test_device_statistics(struct msg_buff *smsg, struct test_vars *vars)
{
	struct nlattr *nla = nla_nest_start(smsg, DRBD_NLA_DEVICE_STATISTICS);
	nla_put_u64(smsg, T_dev_size, 10);
	nla_put_u64(smsg, T_dev_read, 20);
	nla_put_u64(smsg, T_dev_write, 30);
	nla_put_u64(smsg, T_dev_al_writes, 40);
	nla_put_u64(smsg, T_dev_bm_writes, 50);
	nla_put_u32(smsg, T_dev_upper_pending, 60);
	nla_put_u32(smsg, T_dev_lower_pending, 70);
	nla_put_u8(smsg, T_dev_upper_blocked, false);
	nla_put_u8(smsg, T_dev_lower_blocked, false);
	nla_put_u8(smsg, T_dev_al_suspended, false);
	nla_put_u32(smsg, T_dev_disk_flags, 0);
	nla_nest_end(smsg, nla);
}

void test_connection_context(struct msg_buff *smsg, struct test_vars *vars)
{
	struct nlattr *nla = nla_nest_start(smsg, DRBD_NLA_CFG_CONTEXT);
	nla_put_string(smsg, T_ctx_resource_name, test_resource_name);
	nla_put_string(smsg, T_ctx_conn_name, test_peer_name);
	nla_put_u32(smsg, T_ctx_peer_node_id, test_peer_node_id);
	nla_nest_end(smsg, nla);
}

void test_net_conf(struct msg_buff *smsg, struct test_vars *vars)
{
	struct nlattr *nla = nla_nest_start(smsg, DRBD_NLA_NET_CONF);
	nla_nest_end(smsg, nla);
}

void test_connection_info(struct msg_buff *smsg, struct test_vars *vars, __u32 conn_connection_state, __u32 conn_role)
{
	struct nlattr *nla = nla_nest_start(smsg, DRBD_NLA_CONNECTION_INFO);
	nla_put_u32(smsg, T_conn_connection_state, conn_connection_state);
	nla_put_u32(smsg, T_conn_role, conn_role);
	nla_nest_end(smsg, nla);
}

void test_connection_statistics(struct msg_buff *smsg, struct test_vars *vars)
{
	struct nlattr *nla = nla_nest_start(smsg, DRBD_NLA_CONNECTION_STATISTICS);
	nla_put_u8(smsg, T_conn_congested, false);
	nla_put_u64(smsg, T_ap_in_flight, 10);
	nla_put_u64(smsg, T_rs_in_flight, 20);
	nla_nest_end(smsg, nla);
}

void test_peer_device_context(struct msg_buff *smsg, struct test_vars *vars, __u32 volume_number)
{
	struct nlattr *nla = nla_nest_start(smsg, DRBD_NLA_CFG_CONTEXT);
	nla_put_string(smsg, T_ctx_resource_name, test_resource_name);
	nla_put_string(smsg, T_ctx_conn_name, test_peer_name);
	nla_put_u32(smsg, T_ctx_peer_node_id, test_peer_node_id);
	nla_put_u32(smsg, T_ctx_volume, volume_number);
	nla_nest_end(smsg, nla);
}

void test_peer_device_opts(struct msg_buff *smsg, struct test_vars *vars)
{
	struct nlattr *nla = nla_nest_start(smsg, DRBD_NLA_PEER_DEVICE_OPTS);
	nla_nest_end(smsg, nla);
}

void test_peer_device_info(struct msg_buff *smsg, struct test_vars *vars, __u32 peer_repl_state, __u32 peer_disk_state)
{
	struct nlattr *nla = nla_nest_start(smsg, DRBD_NLA_PEER_DEVICE_INFO);
	nla_put_u32(smsg, T_peer_repl_state, peer_repl_state);
	nla_put_u32(smsg, T_peer_disk_state, peer_disk_state);
	nla_put_u32(smsg, T_peer_resync_susp_user, false);
	nla_put_u32(smsg, T_peer_resync_susp_peer, false);
	nla_put_u32(smsg, T_peer_resync_susp_dependency, false);
	nla_put_u8(smsg, T_peer_is_intentional_diskless, false);
	nla_nest_end(smsg, nla);
}

void test_peer_device_statistics(struct msg_buff *smsg, struct test_vars *vars, bool resync)
{
	struct nlattr *nla = nla_nest_start(smsg, DRBD_NLA_PEER_DEVICE_STATISTICS);
	nla_put_u64(smsg, T_peer_dev_received, 10);
	nla_put_u64(smsg, T_peer_dev_sent, 20);
	nla_put_u32(smsg, T_peer_dev_pending, 30);
	nla_put_u32(smsg, T_peer_dev_unacked, 40);
	nla_put_u64(smsg, T_peer_dev_out_of_sync, resync ? 5000 : 0);
	nla_put_u64(smsg, T_peer_dev_resync_failed, 0);
	nla_put_u32(smsg, T_peer_dev_flags, 0);
	nla_put_u64(smsg, T_peer_dev_rs_total, resync ? 8000 : 0);
	nla_put_u64(smsg, T_peer_dev_ov_start_sector, 0);
	nla_put_u64(smsg, T_peer_dev_ov_stop_sector, 0);
	nla_put_u64(smsg, T_peer_dev_ov_position, 0);
	nla_put_u64(smsg, T_peer_dev_ov_left, 0);
	nla_put_u64(smsg, T_peer_dev_ov_skipped, 0);
	nla_put_u64(smsg, T_peer_dev_rs_same_csum, 0);
	nla_put_u64(smsg, T_peer_dev_rs_dt_start_ms, 0);
	nla_put_u64(smsg, T_peer_dev_rs_paused_ms, 0);
	nla_put_u64(smsg, T_peer_dev_rs_dt0_ms, 0);
	nla_put_u64(smsg, T_peer_dev_rs_db0_sectors, 0);
	nla_put_u64(smsg, T_peer_dev_rs_dt1_ms, resync ? 100 : 0);
	nla_put_u64(smsg, T_peer_dev_rs_db1_sectors, resync ? 50 : 0);
	nla_put_u32(smsg, T_peer_dev_rs_c_sync_rate, 0);
	nla_put_u64(smsg, T_peer_dev_uuid_flags, UUID_FLAG_STABLE);
	nla_nest_end(smsg, nla);
}

void test_path_context(struct msg_buff *smsg, struct test_vars *vars)
{
	struct sockaddr_in test_my_addr = {
		.sin_family = AF_INET,
		.sin_port = htons(7789),
		.sin_addr = { .s_addr = 0x04030201 },
	};
	struct sockaddr_in test_peer_addr = {
		.sin_family = AF_INET,
		.sin_port = htons(7790),
		.sin_addr = { .s_addr = 0x08070605 },
	};
	struct nlattr *nla = nla_nest_start(smsg, DRBD_NLA_CFG_CONTEXT);
	nla_put_string(smsg, T_ctx_resource_name, test_resource_name);
	nla_put_string(smsg, T_ctx_conn_name, test_peer_name);
	nla_put_u32(smsg, T_ctx_peer_node_id, test_peer_node_id);
	nla_put(smsg, T_ctx_my_addr, sizeof(test_my_addr), &test_my_addr);
	nla_put(smsg, T_ctx_peer_addr, sizeof(test_peer_addr), &test_peer_addr);
	nla_nest_end(smsg, nla);
}

void test_path_info(struct msg_buff *smsg, struct test_vars *vars, __u8 path_established)
{
	struct nlattr *nla = nla_nest_start(smsg, DRBD_NLA_PATH_INFO);
	nla_put_u8(smsg, T_path_established, path_established);
	nla_nest_end(smsg, nla);
}

void test_helper(struct msg_buff *smsg, struct test_vars *vars, __u32 status)
{
	struct nlattr *nla = nla_nest_start(smsg, DRBD_NLA_HELPER);
	nla_put_string(smsg, T_helper_name, "before-resync-target");
	nla_put_u32(smsg, T_helper_status, status);
	nla_nest_end(smsg, nla);
}

/*
 * ################# events2 messages #################
 */

void test_initial_state_done(struct msg_buff *smsg, struct test_vars *vars)
{
	test_msg_put(smsg, DRBD_INITIAL_STATE_DONE, -1U);
	test_notification_header(smsg, NOTIFY_EXISTS);
}

void test_resource_exists(struct msg_buff *smsg, struct test_vars *vars)
{
	test_msg_put(smsg, DRBD_RESOURCE_STATE, -1U);
	test_resource_context(smsg, vars);
	test_notification_header(smsg, NOTIFY_EXISTS);
	test_resource_info(smsg, vars, R_SECONDARY);
	test_resource_statistics(smsg, vars);
}

void test_resource_create(struct msg_buff *smsg, struct test_vars *vars)
{
	test_msg_put(smsg, DRBD_RESOURCE_STATE, -1U);
	test_resource_context(smsg, vars);
	test_notification_header(smsg, NOTIFY_CREATE);
	test_resource_info(smsg, vars, R_SECONDARY);
	test_resource_statistics(smsg, vars);
}

void test_resource_change_role(struct msg_buff *smsg, struct test_vars *vars)
{
	test_msg_put(smsg, DRBD_RESOURCE_STATE, -1U);
	test_resource_context(smsg, vars);
	test_notification_header(smsg, NOTIFY_CHANGE);
	test_resource_info(smsg, vars, R_PRIMARY);
	test_resource_statistics(smsg, vars);
}

void test_resource_destroy(struct msg_buff *smsg, struct test_vars *vars)
{
	test_msg_put(smsg, DRBD_RESOURCE_STATE, -1U);
	test_resource_context(smsg, vars);
	test_notification_header(smsg, NOTIFY_DESTROY);
}

void test_device_exists(struct msg_buff *smsg, struct test_vars *vars)
{
	test_msg_put(smsg, DRBD_DEVICE_STATE, test_minor);
	test_device_context(smsg, vars, test_volume_number);
	test_notification_header(smsg, NOTIFY_EXISTS);
	test_device_info(smsg, vars, D_DISKLESS, true);
	test_device_statistics(smsg, vars);
}

void test_device_create(struct msg_buff *smsg, struct test_vars *vars)
{
	test_msg_put(smsg, DRBD_DEVICE_STATE, test_minor);
	test_device_context(smsg, vars, test_volume_number);
	test_notification_header(smsg, NOTIFY_CREATE);
	test_device_info(smsg, vars, D_DISKLESS, true);
	test_device_statistics(smsg, vars);
}

void test_device_create_b(struct msg_buff *smsg, struct test_vars *vars)
{
	test_msg_put(smsg, DRBD_DEVICE_STATE, test_minor_b);
	test_device_context(smsg, vars, test_volume_number_b);
	test_notification_header(smsg, NOTIFY_CREATE);
	test_device_info(smsg, vars, D_DISKLESS, true);
	test_device_statistics(smsg, vars);
}

void test_device_change_disk(struct msg_buff *smsg, struct test_vars *vars)
{
	test_msg_put(smsg, DRBD_DEVICE_STATE, test_minor);
	test_device_context(smsg, vars, test_volume_number);
	test_notification_header(smsg, NOTIFY_CHANGE);
	test_device_info(smsg, vars, D_UP_TO_DATE, true);
	test_device_statistics(smsg, vars);
}

void test_device_change_disk_b(struct msg_buff *smsg, struct test_vars *vars)
{
	test_msg_put(smsg, DRBD_DEVICE_STATE, test_minor_b);
	test_device_context(smsg, vars, test_volume_number_b);
	test_notification_header(smsg, NOTIFY_CHANGE);
	test_device_info(smsg, vars, D_UP_TO_DATE, true);
	test_device_statistics(smsg, vars);
}

void test_device_change_disk_inconsistent(struct msg_buff *smsg, struct test_vars *vars)
{
	test_msg_put(smsg, DRBD_DEVICE_STATE, test_minor);
	test_device_context(smsg, vars, test_volume_number);
	test_notification_header(smsg, NOTIFY_CHANGE);
	test_device_info(smsg, vars, D_INCONSISTENT, true);
	test_device_statistics(smsg, vars);
}

void test_device_change_quorum(struct msg_buff *smsg, struct test_vars *vars)
{
	test_msg_put(smsg, DRBD_DEVICE_STATE, test_minor);
	test_device_context(smsg, vars, test_volume_number);
	test_notification_header(smsg, NOTIFY_CHANGE);
	test_device_info(smsg, vars, D_UP_TO_DATE, false);
	test_device_statistics(smsg, vars);
}

void test_device_destroy(struct msg_buff *smsg, struct test_vars *vars)
{
	test_msg_put(smsg, DRBD_DEVICE_STATE, -1U);
	test_device_context(smsg, vars, test_volume_number);
	test_notification_header(smsg, NOTIFY_DESTROY);
}

void test_device_destroy_b(struct msg_buff *smsg, struct test_vars *vars)
{
	test_msg_put(smsg, DRBD_DEVICE_STATE, -1U);
	test_device_context(smsg, vars, test_volume_number_b);
	test_notification_header(smsg, NOTIFY_DESTROY);
}

void test_connection_create(struct msg_buff *smsg, struct test_vars *vars)
{
	test_msg_put(smsg, DRBD_CONNECTION_STATE, -1U);
	test_connection_context(smsg, vars);
	test_notification_header(smsg, NOTIFY_CREATE);
	test_connection_info(smsg, vars, C_STANDALONE, R_UNKNOWN);
	test_connection_statistics(smsg, vars);
}

void test_connection_change_connection(struct msg_buff *smsg, struct test_vars *vars)
{
	test_msg_put(smsg, DRBD_CONNECTION_STATE, -1U);
	test_connection_context(smsg, vars);
	test_notification_header(smsg, NOTIFY_CHANGE);
	test_connection_info(smsg, vars, C_CONNECTED, R_SECONDARY);
	test_connection_statistics(smsg, vars);
}

void test_connection_change_role(struct msg_buff *smsg, struct test_vars *vars)
{
	test_msg_put(smsg, DRBD_CONNECTION_STATE, -1U);
	test_connection_context(smsg, vars);
	test_notification_header(smsg, NOTIFY_CHANGE);
	test_connection_info(smsg, vars, C_CONNECTED, R_PRIMARY);
	test_connection_statistics(smsg, vars);
}

void test_connection_destroy(struct msg_buff *smsg, struct test_vars *vars)
{
	test_msg_put(smsg, DRBD_CONNECTION_STATE, -1U);
	test_connection_context(smsg, vars);
	test_notification_header(smsg, NOTIFY_DESTROY);
}

void test_peer_device_create(struct msg_buff *smsg, struct test_vars *vars)
{
	test_msg_put(smsg, DRBD_PEER_DEVICE_STATE, -1U);
	test_peer_device_context(smsg, vars, test_volume_number);
	test_notification_header(smsg, NOTIFY_CREATE);
	test_peer_device_info(smsg, vars, L_OFF, D_UNKNOWN);
	test_peer_device_statistics(smsg, vars, false);
}

void test_peer_device_change_replication(struct msg_buff *smsg, struct test_vars *vars)
{
	test_msg_put(smsg, DRBD_PEER_DEVICE_STATE, -1U);
	test_peer_device_context(smsg, vars, test_volume_number);
	test_notification_header(smsg, NOTIFY_CHANGE);
	test_peer_device_info(smsg, vars, L_ESTABLISHED, D_UP_TO_DATE);
	test_peer_device_statistics(smsg, vars, false);
}

void test_peer_device_change_sync(struct msg_buff *smsg, struct test_vars *vars)
{
	test_msg_put(smsg, DRBD_PEER_DEVICE_STATE, -1U);
	test_peer_device_context(smsg, vars, test_volume_number);
	test_notification_header(smsg, NOTIFY_CHANGE);
	test_peer_device_info(smsg, vars, L_SYNC_SOURCE, D_INCONSISTENT);
	test_peer_device_statistics(smsg, vars, true);
}

void test_peer_device_destroy(struct msg_buff *smsg, struct test_vars *vars)
{
	test_msg_put(smsg, DRBD_PEER_DEVICE_STATE, -1U);
	test_peer_device_context(smsg, vars, test_volume_number);
	test_notification_header(smsg, NOTIFY_DESTROY);
}

void test_path_create(struct msg_buff *smsg, struct test_vars *vars)
{
	test_msg_put(smsg, DRBD_PATH_STATE, -1U);
	test_path_context(smsg, vars);
	test_notification_header(smsg, NOTIFY_CREATE);
	test_path_info(smsg, vars, false);
}

void test_path_change_established(struct msg_buff *smsg, struct test_vars *vars)
{
	test_msg_put(smsg, DRBD_PATH_STATE, -1U);
	test_path_context(smsg, vars);
	test_notification_header(smsg, NOTIFY_CHANGE);
	test_path_info(smsg, vars, true);
}

void test_path_destroy(struct msg_buff *smsg, struct test_vars *vars)
{
	test_msg_put(smsg, DRBD_PATH_STATE, -1U);
	test_path_context(smsg, vars);
	test_notification_header(smsg, NOTIFY_DESTROY);
}

void test_helper_call(struct msg_buff *smsg, struct test_vars *vars)
{
	test_msg_put(smsg, DRBD_HELPER, -1U);
	test_peer_device_context(smsg, vars, test_volume_number);
	test_notification_header(smsg, NOTIFY_CALL);
	test_helper(smsg, vars, 0);
}

void test_helper_response(struct msg_buff *smsg, struct test_vars *vars)
{
	test_msg_put(smsg, DRBD_HELPER, -1U);
	test_peer_device_context(smsg, vars, test_volume_number);
	test_notification_header(smsg, NOTIFY_RESPONSE);
	test_helper(smsg, vars, 1);
}

/*
 * ################# "get" messages #################
 */

static void test_get_resource(struct msg_buff *smsg, struct test_vars *vars)
{
	test_msg_put(smsg, DRBD_ADM_GET_RESOURCES, -1U);
	test_resource_context(smsg, vars);
	test_resource_opts(smsg, vars);
	test_resource_info(smsg, vars, R_SECONDARY);
	test_resource_statistics(smsg, vars);
}

static void test_get_device(struct msg_buff *smsg, struct test_vars *vars)
{
	test_msg_put(smsg, DRBD_ADM_GET_DEVICES, test_minor);
	test_device_context(smsg, vars, test_volume_number);
	test_disk_conf(smsg, vars, D_UP_TO_DATE);
	test_device_conf(smsg, vars);
	test_device_info(smsg, vars, D_UP_TO_DATE, true);
	test_device_statistics(smsg, vars);
}

static void test_get_device_b(struct msg_buff *smsg, struct test_vars *vars)
{
	test_msg_put(smsg, DRBD_ADM_GET_DEVICES, test_minor_b);
	test_device_context(smsg, vars, test_volume_number_b);
	test_disk_conf(smsg, vars, D_UP_TO_DATE);
	test_device_conf(smsg, vars);
	test_device_info(smsg, vars, D_UP_TO_DATE, true);
	test_device_statistics(smsg, vars);
}

static void test_get_connection(struct msg_buff *smsg, struct test_vars *vars)
{
	test_msg_put(smsg, DRBD_ADM_GET_CONNECTIONS, -1U);
	test_connection_context(smsg, vars);
	test_net_conf(smsg, vars);
	test_connection_info(smsg, vars, C_CONNECTED, R_PRIMARY);
	test_connection_statistics(smsg, vars);
}

static void test_get_peer_device(struct msg_buff *smsg, struct test_vars *vars)
{
	test_msg_put(smsg, DRBD_ADM_GET_PEER_DEVICES, test_minor);
	test_peer_device_context(smsg, vars, test_volume_number);
	test_peer_device_info(smsg, vars, L_ESTABLISHED, D_UP_TO_DATE);
	test_peer_device_statistics(smsg, vars, false);
	test_peer_device_opts(smsg, vars);
}

static void test_get_peer_device_b(struct msg_buff *smsg, struct test_vars *vars)
{
	test_msg_put(smsg, DRBD_ADM_GET_PEER_DEVICES, test_minor_b);
	test_peer_device_context(smsg, vars, test_volume_number_b);
	test_peer_device_info(smsg, vars, L_ESTABLISHED, D_UP_TO_DATE);
	test_peer_device_statistics(smsg, vars, false);
	test_peer_device_opts(smsg, vars);
}

/*
 * ################# main() #################
 */

static struct genl_info nlmsghdr_to_genl_info(struct nlmsghdr *nlh, struct nlattr **tla)
{
	int err;
	struct genl_info info = {
		.seq = nlh->nlmsg_seq,
		.nlhdr = nlh,
		.genlhdr = nlmsg_data(nlh),
		.userhdr = genlmsg_data(nlmsg_data(nlh)),
		.attrs = tla,
	};
	err = drbd_tla_parse(tla, nlh);
	if (err) {
		fprintf(stderr, "drbd_tla_parse() failed");
		exit(1);
	}

	return info;
}

#define TEST_MSG(name) do { \
	if (!strcmp(msg_name, #name)) { \
		test_ ## name(smsg, vars); \
		return 0; \
	} \
} while (0)

int test_build_msg(struct msg_buff *smsg, char *msg_name, struct test_vars *vars)
{
	TEST_MSG(initial_state_done);
	TEST_MSG(resource_exists);
	TEST_MSG(resource_create);
	TEST_MSG(resource_change_role);
	TEST_MSG(resource_destroy);
	TEST_MSG(device_exists);
	TEST_MSG(device_create);
	TEST_MSG(device_create_b);
	TEST_MSG(device_change_disk);
	TEST_MSG(device_change_disk_b);
	TEST_MSG(device_change_disk_inconsistent);
	TEST_MSG(device_change_quorum);
	TEST_MSG(device_destroy);
	TEST_MSG(device_destroy_b);
	TEST_MSG(connection_create);
	TEST_MSG(connection_change_connection);
	TEST_MSG(connection_change_role);
	TEST_MSG(connection_destroy);
	TEST_MSG(peer_device_create);
	TEST_MSG(peer_device_change_replication);
	TEST_MSG(peer_device_change_sync);
	TEST_MSG(peer_device_destroy);
	TEST_MSG(path_create);
	TEST_MSG(path_change_established);
	TEST_MSG(path_destroy);
	TEST_MSG(helper_call);
	TEST_MSG(helper_response);
	TEST_MSG(get_resource);
	TEST_MSG(get_device);
	TEST_MSG(get_device_b);
	TEST_MSG(get_connection);
	TEST_MSG(get_peer_device);
	TEST_MSG(get_peer_device_b);
	fprintf(stderr, "unknown message '%s'\n", msg_name);
	return 1;
}

#define MAX_INPUT_LENGTH 100

struct test_vars test_init_vars()
{
	struct test_vars vars = {
		.msg_seq = -1,
		.minor = test_minor,
		.volume_number = test_volume_number,
	};

	return vars;
}

int test_parse_vars(char *input, char *msg_name, struct test_vars *vars)
{
	char var_name[MAX_INPUT_LENGTH];
	int consumed;

	if (sscanf(input, "%s%n", msg_name, &consumed) < 1) {
		fprintf(stderr, "Failed to read msg_name\n");
		return 1;
	}
	input += consumed;

	while (sscanf(input, "%s%n", var_name, &consumed) == 1) {
		input += consumed;

		if (!strcmp(var_name, "msg_seq")) {
			if (sscanf(input, "%d%n", &vars->msg_seq, &consumed) < 1) {
				fprintf(stderr, "Failed to read value for '%s'\n", var_name);
				return 1;
			}
			input += consumed;
			continue;
		}

		fprintf(stderr, "Unknown var: '%s'\n", var_name);
		return 1;
	}

	return 0;
}

int test_events2()
{
	int next_msg_seq = 0;
	char input[MAX_INPUT_LENGTH];

	while (fgets(input, MAX_INPUT_LENGTH, stdin)) {
		char msg_name[MAX_INPUT_LENGTH];
		struct test_vars vars = test_init_vars();
		struct msg_buff *smsg;
		struct nlmsghdr *nlh;
		int err;
		struct drbd_cmd cm;
		struct nlattr *tla[128];
		struct genl_info info;

		err = test_parse_vars(input, msg_name, &vars);
		if (err)
			return err;

		if (vars.msg_seq != -1)
			next_msg_seq = vars.msg_seq;

		/* build msg as if sending */
		smsg = msg_new(DEFAULT_MSG_SIZE);
		err = test_build_msg(smsg, msg_name, &vars);
		if (err) {
			msg_free(smsg);
			return err;
		}

		/* convert to transfer format */
		nlh = (struct nlmsghdr *)smsg->data;
		nlh->nlmsg_len = smsg->tail - smsg->data;
		nlh->nlmsg_flags |= NLM_F_REQUEST;
		nlh->nlmsg_seq = next_msg_seq;

		/* read message as if receiving */
		info = nlmsghdr_to_genl_info(nlh, tla);

		err = print_event(&cm, &info, NULL);
		if (err) {
			msg_free(smsg);
			return err;
		}

		msg_free(smsg);
		next_msg_seq++;
	}

	return 0;
}

int generic_get_instrumented(const struct drbd_cmd *cm, int timeout_arg, void *u_ptr)
{
	char input[MAX_INPUT_LENGTH];
	char *cmd_id_name;

	if (!fgets(input, MAX_INPUT_LENGTH, stdin)) {
		fprintf(stderr, "Unexpected generic_get() cmd_id=%d\n", cm->cmd_id);
		exit(1);
	}
	input[strcspn(input, "\n")] = 0;

	switch (cm->cmd_id) {
		case DRBD_ADM_GET_RESOURCES:
			cmd_id_name = "DRBD_ADM_GET_RESOURCES";
			break;
		case DRBD_ADM_GET_DEVICES:
			cmd_id_name = "DRBD_ADM_GET_DEVICES";
			break;
		case DRBD_ADM_GET_CONNECTIONS:
			cmd_id_name = "DRBD_ADM_GET_CONNECTIONS";
			break;
		case DRBD_ADM_GET_PEER_DEVICES:
			cmd_id_name = "DRBD_ADM_GET_PEER_DEVICES";
			break;
		default:
			fprintf(stderr, "Unknown cmd_id=%d\n", cm->cmd_id);
			exit(1);
	}

	if (strcmp(cmd_id_name, input)) {
		fprintf(stderr, "Command '%s' does not match expected '%s'\n",
				cmd_id_name, input);
		exit(1);
	}

	while (fgets(input, MAX_INPUT_LENGTH, stdin)) {
		char msg_name[MAX_INPUT_LENGTH];
		struct test_vars vars = test_init_vars();
		struct msg_buff *smsg;
		struct nlmsghdr *nlh;
		struct nlattr *tla[128];
		struct genl_info info;
		int err;

		err = test_parse_vars(input, msg_name, &vars);
		if (err)
			return err;

		if (!strcmp(msg_name, "-"))
			return 0;

		/* build msg as if sending */
		smsg = msg_new(DEFAULT_MSG_SIZE);
		if (test_build_msg(smsg, msg_name, &vars))
			exit(1);

		/* convert to transfer format */
		nlh = (struct nlmsghdr *)smsg->data;
		nlh->nlmsg_len = smsg->tail - smsg->data;
		nlh->nlmsg_flags |= NLM_F_MULTI;

		/* read message as if receiving */
		info = nlmsghdr_to_genl_info(nlh, tla);

		err = cm->handle_reply(cm, &info, u_ptr);
		if (err)
			return err;
	}

	return 0;
}

int main_events2(int argc, char **argv)
{
	struct option options[] = {
		{ "help", no_argument, 0, 'h' },
		{ "timestamps", no_argument, 0, 'T' },
		{ "statistics", no_argument, 0, 's' },
		{ "now", no_argument, 0, 'n' },
		{ "diff", optional_argument, 0, 'i' },
		{ "full", no_argument, 0, 'f' },
		{ "color", no_argument, 0, 'c' },
		{ }
	};

	opt_color = NEVER_COLOR;
	for(;;) {
		int c;
		c = getopt_long(argc, argv, "hTifsnc::", options, NULL);
		if (c == -1)
			break;
		switch(c) {
		default:
		case 'h':
		case '?':
			fprintf(stderr, "drbdsetup_instrumented events2 - Fake drbdsetup events2 from messages on stdin\n\n");
			fprintf(stderr, "Input line format:\n");
			fprintf(stderr, "message_name [sequence_number]\n\n");
			fprintf(stderr, "USAGE: drbdsetup_instrumented %s [options]\n", argv[0]);
			fprintf(stderr, "    [--timestamps] [--statistics] [--now] [--diff] [--full] [--color]\n");
			return 1;

		case 'n':
			opt_now = true;
			break;

		case 's':
			++opt_verbose;
			opt_statistics = true;
			break;

		case 'T':
			opt_timestamps = true;
			break;

		case 'i':
			opt_diff = true;
			break;

		case 'f':
			++opt_verbose;
			opt_statistics = true;
			opt_fullch = true;
			break;

		case 'c':
			opt_color = ALWAYS_COLOR;
			break;
		}
	}
	return test_events2();
}

int main_generic_instrumented(int argc, char **argv)
{
	/* Prevent reading of version from /proc/drbd */
	setenv("DRBD_DRIVER_VERSION_OVERRIDE", "9.2.14", 1);

	/* Redirect calls to generic_get() */
	fake_generic_get = generic_get_instrumented;

	return drbdsetup_main(argc, argv);
}

/*
 * "main" for the instrumented drbdsetup test program.
 *
 * Messages to fake are read from stdin.
 */
int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "USAGE: drbdsetup_instrumented {events2|show} [options]\n");
		return 1;
	}

	if (strcmp(argv[1], "events2") == 0)
		return main_events2(argc - 1, argv + 1);

	if (strcmp(argv[1], "show") == 0)
		return main_generic_instrumented(argc, argv);

	fprintf(stderr, "Unknown command '%s'\n", argv[1]);
	fprintf(stderr, "USAGE: drbdsetup_instrumented {events2|show} [options]\n");
	return 1;
}
