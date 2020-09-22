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

int print_event(struct drbd_cmd *cm, struct genl_info *info, void *u_ptr);

extern struct genl_family drbd_genl_family;

static char *test_resource_name = "some-resource";
static __u32 test_minor = 1000;
static __u32 test_minor_b = 1001;
static __u32 test_volume_number = 0;
static __u32 test_volume_number_b = 1;
static __u32 test_peer_node_id = 1;
static char *test_peer_name = "some-peer";

typedef void (*test_msg)(struct msg_buff *smsg);

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

void test_resource_context(struct msg_buff *smsg)
{
	struct nlattr *nla = nla_nest_start(smsg, DRBD_NLA_CFG_CONTEXT);
	nla_put_string(smsg, T_ctx_resource_name, test_resource_name);
	nla_nest_end(smsg, nla);
}

void test_resource_info(struct msg_buff *smsg, __u32 res_role)
{
	struct nlattr *nla = nla_nest_start(smsg, DRBD_NLA_RESOURCE_INFO);
	nla_put_u32(smsg, T_res_role, res_role);
	nla_put_u8(smsg, T_res_susp, false);
	nla_put_u8(smsg, T_res_susp_nod, false);
	nla_put_u8(smsg, T_res_susp_fen, false);
	nla_put_u8(smsg, T_res_susp_quorum, false);
	nla_nest_end(smsg, nla);
}

void test_resource_statistics(struct msg_buff *smsg)
{
	struct nlattr *nla = nla_nest_start(smsg, DRBD_NLA_RESOURCE_STATISTICS);
	nla_put_u32(smsg, T_res_stat_write_ordering, WO_NONE);
	nla_nest_end(smsg, nla);
}

void test_device_context(struct msg_buff *smsg, __u32 volume_number)
{
	struct nlattr *nla = nla_nest_start(smsg, DRBD_NLA_CFG_CONTEXT);
	nla_put_string(smsg, T_ctx_resource_name, test_resource_name);
	nla_put_u32(smsg, T_ctx_volume, volume_number);
	nla_nest_end(smsg, nla);
}

void test_device_info(struct msg_buff *smsg, __u32 dev_disk_state, __u8 dev_has_quorum)
{
	struct nlattr *nla = nla_nest_start(smsg, DRBD_NLA_DEVICE_INFO);
	nla_put_u32(smsg, T_dev_disk_state, dev_disk_state);
	nla_put_u8(smsg, T_is_intentional_diskless, false);
	nla_put_u8(smsg, T_dev_has_quorum, dev_has_quorum);
	nla_nest_end(smsg, nla);
}

void test_device_statistics(struct msg_buff *smsg)
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

void test_connection_context(struct msg_buff *smsg)
{
	struct nlattr *nla = nla_nest_start(smsg, DRBD_NLA_CFG_CONTEXT);
	nla_put_string(smsg, T_ctx_resource_name, test_resource_name);
	nla_put_string(smsg, T_ctx_conn_name, test_peer_name);
	nla_put_u32(smsg, T_ctx_peer_node_id, test_peer_node_id);
	nla_nest_end(smsg, nla);
}

void test_connection_info(struct msg_buff *smsg, __u32 conn_connection_state, __u32 conn_role)
{
	struct nlattr *nla = nla_nest_start(smsg, DRBD_NLA_CONNECTION_INFO);
	nla_put_u32(smsg, T_conn_connection_state, conn_connection_state);
	nla_put_u32(smsg, T_conn_role, conn_role);
	nla_nest_end(smsg, nla);
}

void test_connection_statistics(struct msg_buff *smsg)
{
	struct nlattr *nla = nla_nest_start(smsg, DRBD_NLA_CONNECTION_STATISTICS);
	nla_put_u8(smsg, T_conn_congested, false);
	nla_put_u64(smsg, T_ap_in_flight, 10);
	nla_put_u64(smsg, T_rs_in_flight, 20);
	nla_nest_end(smsg, nla);
}

void test_peer_device_context(struct msg_buff *smsg)
{
	struct nlattr *nla = nla_nest_start(smsg, DRBD_NLA_CFG_CONTEXT);
	nla_put_string(smsg, T_ctx_resource_name, test_resource_name);
	nla_put_string(smsg, T_ctx_conn_name, test_peer_name);
	nla_put_u32(smsg, T_ctx_peer_node_id, test_peer_node_id);
	nla_put_u32(smsg, T_ctx_volume, test_volume_number);
	nla_nest_end(smsg, nla);
}

void test_peer_device_info(struct msg_buff *smsg, __u32 peer_repl_state, __u32 peer_disk_state)
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

void test_peer_device_statistics(struct msg_buff *smsg, bool resync)
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
	nla_nest_end(smsg, nla);
}

void test_path_context(struct msg_buff *smsg)
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

void test_path_info(struct msg_buff *smsg, __u8 path_established)
{
	struct nlattr *nla = nla_nest_start(smsg, DRBD_NLA_PATH_INFO);
	nla_put_u8(smsg, T_path_established, path_established);
	nla_nest_end(smsg, nla);
}

void test_helper(struct msg_buff *smsg, __u32 status)
{
	struct nlattr *nla = nla_nest_start(smsg, DRBD_NLA_HELPER);
	nla_put_string(smsg, T_helper_name, "before-resync-target");
	nla_put_u32(smsg, T_helper_status, status);
	nla_nest_end(smsg, nla);
}

void test_initial_state_done(struct msg_buff *smsg)
{
	test_msg_put(smsg, DRBD_INITIAL_STATE_DONE, -1U);
	test_notification_header(smsg, NOTIFY_EXISTS);
}

void test_resource_exists(struct msg_buff *smsg)
{
	test_msg_put(smsg, DRBD_RESOURCE_STATE, -1U);
	test_resource_context(smsg);
	test_notification_header(smsg, NOTIFY_EXISTS);
	test_resource_info(smsg, R_SECONDARY);
	test_resource_statistics(smsg);
}

void test_resource_create(struct msg_buff *smsg)
{
	test_msg_put(smsg, DRBD_RESOURCE_STATE, -1U);
	test_resource_context(smsg);
	test_notification_header(smsg, NOTIFY_CREATE);
	test_resource_info(smsg, R_SECONDARY);
	test_resource_statistics(smsg);
}

void test_resource_change_role(struct msg_buff *smsg)
{
	test_msg_put(smsg, DRBD_RESOURCE_STATE, -1U);
	test_resource_context(smsg);
	test_notification_header(smsg, NOTIFY_CHANGE);
	test_resource_info(smsg, R_PRIMARY);
	test_resource_statistics(smsg);
}

void test_resource_destroy(struct msg_buff *smsg)
{
	test_msg_put(smsg, DRBD_RESOURCE_STATE, -1U);
	test_resource_context(smsg);
	test_notification_header(smsg, NOTIFY_DESTROY);
}

void test_device_exists(struct msg_buff *smsg)
{
	test_msg_put(smsg, DRBD_DEVICE_STATE, test_minor);
	test_device_context(smsg, test_volume_number);
	test_notification_header(smsg, NOTIFY_EXISTS);
	test_device_info(smsg, D_DISKLESS, true);
	test_device_statistics(smsg);
}

void test_device_create(struct msg_buff *smsg)
{
	test_msg_put(smsg, DRBD_DEVICE_STATE, test_minor);
	test_device_context(smsg, test_volume_number);
	test_notification_header(smsg, NOTIFY_CREATE);
	test_device_info(smsg, D_DISKLESS, true);
	test_device_statistics(smsg);
}

void test_device_create_b(struct msg_buff *smsg)
{
	test_msg_put(smsg, DRBD_DEVICE_STATE, test_minor_b);
	test_device_context(smsg, test_volume_number_b);
	test_notification_header(smsg, NOTIFY_CREATE);
	test_device_info(smsg, D_DISKLESS, true);
	test_device_statistics(smsg);
}

void test_device_change_disk(struct msg_buff *smsg)
{
	test_msg_put(smsg, DRBD_DEVICE_STATE, test_minor);
	test_device_context(smsg, test_volume_number);
	test_notification_header(smsg, NOTIFY_CHANGE);
	test_device_info(smsg, D_UP_TO_DATE, true);
	test_device_statistics(smsg);
}

void test_device_change_disk_b(struct msg_buff *smsg)
{
	test_msg_put(smsg, DRBD_DEVICE_STATE, test_minor_b);
	test_device_context(smsg, test_volume_number_b);
	test_notification_header(smsg, NOTIFY_CHANGE);
	test_device_info(smsg, D_UP_TO_DATE, true);
	test_device_statistics(smsg);
}

void test_device_change_disk_inconsistent(struct msg_buff *smsg)
{
	test_msg_put(smsg, DRBD_DEVICE_STATE, test_minor);
	test_device_context(smsg, test_volume_number);
	test_notification_header(smsg, NOTIFY_CHANGE);
	test_device_info(smsg, D_INCONSISTENT, true);
	test_device_statistics(smsg);
}

void test_device_change_quorum(struct msg_buff *smsg)
{
	test_msg_put(smsg, DRBD_DEVICE_STATE, test_minor);
	test_device_context(smsg, test_volume_number);
	test_notification_header(smsg, NOTIFY_CHANGE);
	test_device_info(smsg, D_UP_TO_DATE, false);
	test_device_statistics(smsg);
}

void test_device_destroy(struct msg_buff *smsg)
{
	test_msg_put(smsg, DRBD_DEVICE_STATE, -1U);
	test_device_context(smsg, test_volume_number);
	test_notification_header(smsg, NOTIFY_DESTROY);
}

void test_device_destroy_b(struct msg_buff *smsg)
{
	test_msg_put(smsg, DRBD_DEVICE_STATE, -1U);
	test_device_context(smsg, test_volume_number_b);
	test_notification_header(smsg, NOTIFY_DESTROY);
}

void test_connection_create(struct msg_buff *smsg)
{
	test_msg_put(smsg, DRBD_CONNECTION_STATE, -1U);
	test_connection_context(smsg);
	test_notification_header(smsg, NOTIFY_CREATE);
	test_connection_info(smsg, C_STANDALONE, R_UNKNOWN);
	test_connection_statistics(smsg);
}

void test_connection_change_connection(struct msg_buff *smsg)
{
	test_msg_put(smsg, DRBD_CONNECTION_STATE, -1U);
	test_connection_context(smsg);
	test_notification_header(smsg, NOTIFY_CHANGE);
	test_connection_info(smsg, C_CONNECTED, R_SECONDARY);
	test_connection_statistics(smsg);
}

void test_connection_change_role(struct msg_buff *smsg)
{
	test_msg_put(smsg, DRBD_CONNECTION_STATE, -1U);
	test_connection_context(smsg);
	test_notification_header(smsg, NOTIFY_CHANGE);
	test_connection_info(smsg, C_CONNECTED, R_PRIMARY);
	test_connection_statistics(smsg);
}

void test_connection_destroy(struct msg_buff *smsg)
{
	test_msg_put(smsg, DRBD_CONNECTION_STATE, -1U);
	test_connection_context(smsg);
	test_notification_header(smsg, NOTIFY_DESTROY);
}

void test_peer_device_create(struct msg_buff *smsg)
{
	test_msg_put(smsg, DRBD_PEER_DEVICE_STATE, -1U);
	test_peer_device_context(smsg);
	test_notification_header(smsg, NOTIFY_CREATE);
	test_peer_device_info(smsg, L_OFF, D_UNKNOWN);
	test_peer_device_statistics(smsg, false);
}

void test_peer_device_change_replication(struct msg_buff *smsg)
{
	test_msg_put(smsg, DRBD_PEER_DEVICE_STATE, -1U);
	test_peer_device_context(smsg);
	test_notification_header(smsg, NOTIFY_CHANGE);
	test_peer_device_info(smsg, L_ESTABLISHED, D_UP_TO_DATE);
	test_peer_device_statistics(smsg, false);
}

void test_peer_device_change_sync(struct msg_buff *smsg)
{
	test_msg_put(smsg, DRBD_PEER_DEVICE_STATE, -1U);
	test_peer_device_context(smsg);
	test_notification_header(smsg, NOTIFY_CHANGE);
	test_peer_device_info(smsg, L_SYNC_SOURCE, D_INCONSISTENT);
	test_peer_device_statistics(smsg, true);
}

void test_peer_device_destroy(struct msg_buff *smsg)
{
	test_msg_put(smsg, DRBD_PEER_DEVICE_STATE, -1U);
	test_peer_device_context(smsg);
	test_notification_header(smsg, NOTIFY_DESTROY);
}

void test_path_create(struct msg_buff *smsg)
{
	test_msg_put(smsg, DRBD_PATH_STATE, -1U);
	test_path_context(smsg);
	test_notification_header(smsg, NOTIFY_CREATE);
	test_path_info(smsg, false);
}

void test_path_change_established(struct msg_buff *smsg)
{
	test_msg_put(smsg, DRBD_PATH_STATE, -1U);
	test_path_context(smsg);
	test_notification_header(smsg, NOTIFY_CHANGE);
	test_path_info(smsg, true);
}

void test_path_destroy(struct msg_buff *smsg)
{
	test_msg_put(smsg, DRBD_PATH_STATE, -1U);
	test_path_context(smsg);
	test_notification_header(smsg, NOTIFY_DESTROY);
}

void test_helper_call(struct msg_buff *smsg)
{
	test_msg_put(smsg, DRBD_HELPER, -1U);
	test_peer_device_context(smsg);
	test_notification_header(smsg, NOTIFY_CALL);
	test_helper(smsg, 0);
}

void test_helper_response(struct msg_buff *smsg)
{
	test_msg_put(smsg, DRBD_HELPER, -1U);
	test_peer_device_context(smsg);
	test_notification_header(smsg, NOTIFY_RESPONSE);
	test_helper(smsg, 1);
}

int test_print_event(struct nlmsghdr *nlh)
{
	struct drbd_cmd cm;
	int err;
	/* read message as if receiving */
	struct genl_info info = {
		.seq = nlh->nlmsg_seq,
		.nlhdr = nlh,
		.genlhdr = nlmsg_data(nlh),
		.userhdr = genlmsg_data(nlmsg_data(nlh)),
		.attrs = global_attrs,
	};
	err = drbd_tla_parse(nlh);
	if (err) {
		fprintf(stderr, "drbd_tla_parse() failed");
		return 1;
	}

	print_event(&cm, &info, NULL);
	return 0;
}

#define TEST_MSG(name) do { \
	if (!strcmp(msg_name, #name)) { \
		test_ ## name(smsg); \
		return 0; \
	} \
} while (0)

int test_build_msg(struct msg_buff *smsg, char *msg_name)
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
	fprintf(stderr, "unknown message '%s'\n", msg_name);
	return 1;
}

#define MAX_INPUT_LENGTH 100

int test_events2()
{
	int msg_index = 0;
	char input[MAX_INPUT_LENGTH];

	while (fgets(input, MAX_INPUT_LENGTH, stdin)) {
		struct msg_buff *smsg;
		struct nlmsghdr *nlh;
		int err;

		input[strcspn(input, "\n")] = 0;

		/* build msg as if sending */
		smsg = msg_new(DEFAULT_MSG_SIZE);
		err = test_build_msg(smsg, input);
		if (err) {
			msg_free(smsg);
			return err;
		}

		/* convert to transfer format */
		nlh = (struct nlmsghdr *)smsg->data;
		nlh->nlmsg_len = smsg->tail - smsg->data;
		nlh->nlmsg_flags |= NLM_F_REQUEST;
		nlh->nlmsg_seq = msg_index;

		err = test_print_event(nlh);
		if (err) {
			msg_free(smsg);
			return err;
		}

		msg_free(smsg);
		msg_index++;
	}

	return 0;
}

/*
 * "main" for the instrumented events2 test program.
 *
 * Messages to fake are read from stdin.
 */
int main(int argc, char **argv)
{
	struct option options[] = {
		{ "help", no_argument, 0, 'h' },
		{ "timestamps", no_argument, 0, 'T' },
		{ "statistics", no_argument, 0, 's' },
		{ "now", no_argument, 0, 'n' },
		{ "color", no_argument, 0, 'c' },
		{ }
	};

	opt_color = NEVER_COLOR;
	for(;;) {
		int c;
		c = getopt_long(argc, argv, "hTsnc::", options, NULL);
		if (c == -1)
			break;
		switch(c) {
		default:
		case 'h':
		case '?':
			fprintf(stderr, "drbdsetup_events2_instrumented - Fake drbdsetup events2 from messages on stdin\n\n");
			fprintf(stderr, "USAGE: %s [options]\n", argv[0]);
			fprintf(stderr, "    [--timestamps] [--statistics] [--now] [--color]\n");
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

		case 'c':
			opt_color = ALWAYS_COLOR;
			break;
		}
	}
	return test_events2();
}
