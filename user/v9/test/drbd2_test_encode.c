/*
 * Test-only "kernel": encode a neutral event as the drbd2 message the
 * module's drbd_nl_drbd2.c would send, so the instrumented drbdsetup can
 * run every legacy fixture through the drbd2 dialect as well.
 */
#define _GNU_SOURCE
#include <string.h>
#include <netinet/in.h>

#include "../drbdsetup_nl.h"
#include "../drbdsetup.h"
#include "linux/drbd2_genl_userspace.h"

struct nlmsghdr *drbd2_test_encode(const struct drbd_nl_event *ev, enum drbd_nl_cmd cmd,
				   struct msg_buff *out);

static void put_str(struct msg_buff *m, int type, const char *s, unsigned len, unsigned size)
{
	/* as the kernel: the string including its NUL, unless the buffer is full */
	nla_put(m, type, len < size ? len + 1 : size, s);
}

static void put_context(struct msg_buff *m, int type, const struct drbd_nl_event *ev)
{
	struct nlattr *nla = nla_nest_start(m, type);

	if (ev->ctx.ctx_resource_name_len)
		nla_put_string(m, DRBD2_A_CONTEXT_RESOURCE_NAME, ev->ctx.ctx_resource_name);
	if (ev->ctx.ctx_volume != -1U)
		nla_put_u32(m, DRBD2_A_CONTEXT_VOLUME, ev->ctx.ctx_volume);
	if (ev->minor != -1U)
		nla_put_u32(m, DRBD2_A_CONTEXT_MINOR, ev->minor);
	if (ev->ctx.ctx_peer_node_id != -1U)
		nla_put_u32(m, DRBD2_A_CONTEXT_PEER_NODE_ID, ev->ctx.ctx_peer_node_id);
	if (ev->ctx.ctx_conn_name_len)
		nla_put_string(m, DRBD2_A_CONTEXT_CONNECTION_NAME, ev->ctx.ctx_conn_name);
	if (ev->ctx.ctx_my_addr_len)
		drbd2_put_address(m, DRBD2_A_CONTEXT_MY_ADDRESS, ev->ctx.ctx_my_addr,
				  ev->ctx.ctx_my_addr_len);
	if (ev->ctx.ctx_peer_addr_len)
		drbd2_put_address(m, DRBD2_A_CONTEXT_PEER_ADDRESS, ev->ctx.ctx_peer_addr,
				  ev->ctx.ctx_peer_addr_len);
	nla_nest_end(m, nla);
}

static void put_resource(struct msg_buff *m, int type, const struct drbd_nl_event *ev)
{
	struct nlattr *obj = nla_nest_start(m, type), *nla;

	put_context(m, DRBD2_A_RESOURCE_CONTEXT, ev);
	if (ev->have_info) {
		const struct resource_info *i = &ev->info.resource;

		nla = nla_nest_start(m, DRBD2_A_RESOURCE_INFO);
		nla_put_u32(m, DRBD2_A_RESOURCE_INFO_ROLE, i->res_role);
		nla_put_u8(m, DRBD2_A_RESOURCE_INFO_SUSP, i->res_susp);
		nla_put_u8(m, DRBD2_A_RESOURCE_INFO_SUSP_NOD, i->res_susp_nod);
		nla_put_u8(m, DRBD2_A_RESOURCE_INFO_SUSP_FEN, i->res_susp_fen);
		nla_put_u8(m, DRBD2_A_RESOURCE_INFO_SUSP_QUORUM, i->res_susp_quorum);
		nla_put_u8(m, DRBD2_A_RESOURCE_INFO_FAIL_IO, i->res_fail_io);
		nla_nest_end(m, nla);
	}
	if (ev->stats.resource.res_stat_write_ordering != -1U) {
		nla = nla_nest_start(m, DRBD2_A_RESOURCE_STATISTICS);
		nla_put_u32(m, DRBD2_A_RESOURCE_STATISTICS_WRITE_ORDERING,
			    ev->stats.resource.res_stat_write_ordering);
		nla_nest_end(m, nla);
	}
	if (ev->rename.res_new_name_len)
		put_str(m, DRBD2_A_RESOURCE_NEW_NAME, ev->rename.res_new_name,
			ev->rename.res_new_name_len, sizeof(ev->rename.res_new_name));
	if (ev->opts)
		drbd2_renumber_to_wire(m, DRBD2_A_RESOURCE_RESOURCE_OPTS, ev->opts, NL_SET_RES_OPTS);
	nla_nest_end(m, obj);
}

static void put_device(struct msg_buff *m, int type, const struct drbd_nl_event *ev)
{
	struct nlattr *obj = nla_nest_start(m, type), *nla;
	const struct device_info *i = &ev->info.device;
	const struct device_statistics *s = &ev->stats.device;

	put_context(m, DRBD2_A_DEVICE_CONTEXT, ev);
	if (ev->have_info) {
		nla = nla_nest_start(m, DRBD2_A_DEVICE_INFO);
		nla_put_u32(m, DRBD2_A_DEVICE_INFO_DISK_STATE, i->dev_disk_state);
		nla_put_u8(m, DRBD2_A_DEVICE_INFO_IS_INTENTIONAL_DISKLESS, i->is_intentional_diskless);
		nla_put_u8(m, DRBD2_A_DEVICE_INFO_HAS_QUORUM, i->dev_has_quorum);
		nla_put_u8(m, DRBD2_A_DEVICE_INFO_IS_OPEN, i->dev_is_open);
		put_str(m, DRBD2_A_DEVICE_INFO_BACKING_DEV_PATH, i->backing_dev_path,
			i->backing_dev_path_len, sizeof(i->backing_dev_path));
		nla_nest_end(m, nla);
	}
	if (s->dev_size != -1ULL) {
		nla = nla_nest_start(m, DRBD2_A_DEVICE_STATISTICS);
		nla_put_u64(m, DRBD2_A_DEVICE_STATISTICS_SIZE, s->dev_size);
		nla_put_u64(m, DRBD2_A_DEVICE_STATISTICS_READ, s->dev_read);
		nla_put_u64(m, DRBD2_A_DEVICE_STATISTICS_WRITE, s->dev_write);
		nla_put_u64(m, DRBD2_A_DEVICE_STATISTICS_AL_WRITES, s->dev_al_writes);
		nla_put_u64(m, DRBD2_A_DEVICE_STATISTICS_BM_WRITES, s->dev_bm_writes);
		nla_put_u32(m, DRBD2_A_DEVICE_STATISTICS_UPPER_PENDING, s->dev_upper_pending);
		nla_put_u32(m, DRBD2_A_DEVICE_STATISTICS_LOWER_PENDING, s->dev_lower_pending);
		nla_put_u8(m, DRBD2_A_DEVICE_STATISTICS_UPPER_BLOCKED, s->dev_upper_blocked);
		nla_put_u8(m, DRBD2_A_DEVICE_STATISTICS_LOWER_BLOCKED, s->dev_lower_blocked);
		nla_put_u8(m, DRBD2_A_DEVICE_STATISTICS_AL_SUSPENDED, s->dev_al_suspended);
		nla_put_u64(m, DRBD2_A_DEVICE_STATISTICS_EXPOSED_DATA_UUID, s->dev_exposed_data_uuid);
		nla_put_u64(m, DRBD2_A_DEVICE_STATISTICS_CURRENT_UUID, s->dev_current_uuid);
		nla_put_u32(m, DRBD2_A_DEVICE_STATISTICS_DISK_FLAGS, s->dev_disk_flags);
		if (s->history_uuids_len != -1U)
			nla_put(m, DRBD2_A_DEVICE_STATISTICS_HISTORY_UUIDS,
				s->history_uuids_len < HISTORY_UUIDS_SIZE ? s->history_uuids_len : HISTORY_UUIDS_SIZE,
				s->history_uuids);
		nla_nest_end(m, nla);
	}
	if (ev->opts)
		drbd2_renumber_to_wire(m, DRBD2_A_DEVICE_DISK_CONF, ev->opts, NL_SET_DISK_CONF);
	if (ev->opts2)
		drbd2_renumber_to_wire(m, DRBD2_A_DEVICE_DEVICE_CONF, ev->opts2, NL_SET_DEVICE_CONF);
	nla_nest_end(m, obj);
}

static void put_connection(struct msg_buff *m, int type, const struct drbd_nl_event *ev)
{
	struct nlattr *obj = nla_nest_start(m, type), *nla;
	const struct connection_statistics *s = &ev->stats.connection;

	put_context(m, DRBD2_A_CONNECTION_CONTEXT, ev);
	if (ev->have_info) {
		nla = nla_nest_start(m, DRBD2_A_CONNECTION_INFO);
		nla_put_u32(m, DRBD2_A_CONNECTION_INFO_CONNECTION_STATE,
			    ev->info.connection.conn_connection_state);
		nla_put_u32(m, DRBD2_A_CONNECTION_INFO_ROLE, ev->info.connection.conn_role);
		nla_nest_end(m, nla);
	}
	if (s->ap_in_flight != -1ULL) {
		nla = nla_nest_start(m, DRBD2_A_CONNECTION_STATISTICS);
		nla_put_u8(m, DRBD2_A_CONNECTION_STATISTICS_CONGESTED, s->conn_congested);
		nla_put_u64(m, DRBD2_A_CONNECTION_STATISTICS_AP_IN_FLIGHT, s->ap_in_flight);
		nla_put_u64(m, DRBD2_A_CONNECTION_STATISTICS_RS_IN_FLIGHT, s->rs_in_flight);
		nla_nest_end(m, nla);
	}
	if (ev->opts)
		drbd2_renumber_to_wire(m, DRBD2_A_CONNECTION_NET_CONF, ev->opts, NL_SET_NET_CONF);
	if (ev->paths) {
		/* legacy blob list: my, peer, my, peer, ... */
		struct nlattr *a, *path = NULL, *pctx = NULL;
		int rem;

		nla_for_each_nested(a, ev->paths, rem) {
			if (nla_type(a) == DRBD_A_PATH_PARMS_MY_ADDR) {
				path = nla_nest_start(m, DRBD2_A_CONNECTION_PATH);
				pctx = nla_nest_start(m, DRBD2_A_PATH_CONTEXT);
				drbd2_put_address(m, DRBD2_A_CONTEXT_MY_ADDRESS, nla_data(a), nla_len(a));
			} else if (nla_type(a) == DRBD_A_PATH_PARMS_PEER_ADDR && path) {
				drbd2_put_address(m, DRBD2_A_CONTEXT_PEER_ADDRESS, nla_data(a), nla_len(a));
				nla_nest_end(m, pctx);
				nla_nest_end(m, path);
				path = NULL;
			}
		}
	}
	nla_nest_end(m, obj);
}

static void put_peer_device(struct msg_buff *m, int type, const struct drbd_nl_event *ev)
{
	struct nlattr *obj = nla_nest_start(m, type), *nla;
	const struct peer_device_info *i = &ev->info.peer_device;
	const struct peer_device_statistics *s = &ev->stats.peer_device;

	put_context(m, DRBD2_A_PEER_DEVICE_CONTEXT, ev);
	if (ev->have_info) {
		nla = nla_nest_start(m, DRBD2_A_PEER_DEVICE_INFO);
		nla_put_u32(m, DRBD2_A_PEER_DEVICE_INFO_REPL_STATE, i->peer_repl_state);
		nla_put_u32(m, DRBD2_A_PEER_DEVICE_INFO_DISK_STATE, i->peer_disk_state);
		nla_put_u8(m, DRBD2_A_PEER_DEVICE_INFO_RESYNC_SUSP_USER, !!i->peer_resync_susp_user);
		nla_put_u8(m, DRBD2_A_PEER_DEVICE_INFO_RESYNC_SUSP_PEER, !!i->peer_resync_susp_peer);
		nla_put_u8(m, DRBD2_A_PEER_DEVICE_INFO_RESYNC_SUSP_DEPENDENCY, !!i->peer_resync_susp_dependency);
		nla_put_u8(m, DRBD2_A_PEER_DEVICE_INFO_IS_INTENTIONAL_DISKLESS, i->peer_is_intentional_diskless);
		nla_put_u8(m, DRBD2_A_PEER_DEVICE_INFO_RESYNC_SUSP_MAX_PARALLEL, !!i->peer_resync_susp_max_parallel);
		nla_nest_end(m, nla);
	}
	if (s->peer_dev_received != -1ULL) {
		nla = nla_nest_start(m, DRBD2_A_PEER_DEVICE_STATISTICS);
		nla_put_u64(m, DRBD2_A_PEER_DEVICE_STATISTICS_RECEIVED, s->peer_dev_received);
		nla_put_u64(m, DRBD2_A_PEER_DEVICE_STATISTICS_SENT, s->peer_dev_sent);
		nla_put_u32(m, DRBD2_A_PEER_DEVICE_STATISTICS_PENDING, s->peer_dev_pending);
		nla_put_u32(m, DRBD2_A_PEER_DEVICE_STATISTICS_UNACKED, s->peer_dev_unacked);
		nla_put_u64(m, DRBD2_A_PEER_DEVICE_STATISTICS_OUT_OF_SYNC, s->peer_dev_out_of_sync);
		nla_put_u64(m, DRBD2_A_PEER_DEVICE_STATISTICS_RESYNC_FAILED, s->peer_dev_resync_failed);
		nla_put_u64(m, DRBD2_A_PEER_DEVICE_STATISTICS_BITMAP_UUID, s->peer_dev_bitmap_uuid);
		nla_put_u32(m, DRBD2_A_PEER_DEVICE_STATISTICS_FLAGS, s->peer_dev_flags);
		nla_put_u64(m, DRBD2_A_PEER_DEVICE_STATISTICS_RS_TOTAL, s->peer_dev_rs_total);
		nla_put_u64(m, DRBD2_A_PEER_DEVICE_STATISTICS_OV_START_SECTOR, s->peer_dev_ov_start_sector);
		nla_put_u64(m, DRBD2_A_PEER_DEVICE_STATISTICS_OV_STOP_SECTOR, s->peer_dev_ov_stop_sector);
		nla_put_u64(m, DRBD2_A_PEER_DEVICE_STATISTICS_OV_POSITION, s->peer_dev_ov_position);
		nla_put_u64(m, DRBD2_A_PEER_DEVICE_STATISTICS_OV_LEFT, s->peer_dev_ov_left);
		nla_put_u64(m, DRBD2_A_PEER_DEVICE_STATISTICS_OV_SKIPPED, s->peer_dev_ov_skipped);
		nla_put_u64(m, DRBD2_A_PEER_DEVICE_STATISTICS_RS_SAME_CSUM, s->peer_dev_rs_same_csum);
		nla_put_u64(m, DRBD2_A_PEER_DEVICE_STATISTICS_RS_DT_START_MS, s->peer_dev_rs_dt_start_ms);
		nla_put_u64(m, DRBD2_A_PEER_DEVICE_STATISTICS_RS_PAUSED_MS, s->peer_dev_rs_paused_ms);
		nla_put_u64(m, DRBD2_A_PEER_DEVICE_STATISTICS_RS_DT0_MS, s->peer_dev_rs_dt0_ms);
		nla_put_u64(m, DRBD2_A_PEER_DEVICE_STATISTICS_RS_DB0_SECTORS, s->peer_dev_rs_db0_sectors);
		nla_put_u64(m, DRBD2_A_PEER_DEVICE_STATISTICS_RS_DT1_MS, s->peer_dev_rs_dt1_ms);
		nla_put_u64(m, DRBD2_A_PEER_DEVICE_STATISTICS_RS_DB1_SECTORS, s->peer_dev_rs_db1_sectors);
		nla_put_u32(m, DRBD2_A_PEER_DEVICE_STATISTICS_RS_C_SYNC_RATE, s->peer_dev_rs_c_sync_rate);
		nla_put_u64(m, DRBD2_A_PEER_DEVICE_STATISTICS_UUID_FLAGS, s->peer_dev_uuid_flags);
		nla_nest_end(m, nla);
	}
	if (ev->opts)
		drbd2_renumber_to_wire(m, DRBD2_A_PEER_DEVICE_PEER_DEVICE_CONF, ev->opts,
				       NL_SET_PEER_DEVICE_CONF);
	nla_nest_end(m, obj);
}

static void put_path(struct msg_buff *m, int type, const struct drbd_nl_event *ev)
{
	struct nlattr *obj = nla_nest_start(m, type), *nla;

	put_context(m, DRBD2_A_PATH_CONTEXT, ev);
	if (ev->have_info) {
		nla = nla_nest_start(m, DRBD2_A_PATH_INFO);
		nla_put_u8(m, DRBD2_A_PATH_INFO_ESTABLISHED, ev->info.path.path_established);
		nla_nest_end(m, nla);
	}
	nla_nest_end(m, obj);
}

static void put_object(struct msg_buff *m, int type, const struct drbd_nl_event *ev)
{
	switch (ev->kind) {
	case NL_OBJ_RESOURCE:	put_resource(m, type, ev); break;
	case NL_OBJ_DEVICE:	put_device(m, type, ev); break;
	case NL_OBJ_CONNECTION:	put_connection(m, type, ev); break;
	case NL_OBJ_PEER_DEVICE: put_peer_device(m, type, ev); break;
	case NL_OBJ_PATH:	put_path(m, type, ev); break;
	default: break;
	}
}

static __u32 action_of(enum drbd_notification_type action)
{
	switch (action & ~NOTIFY_FLAGS) {
	case NOTIFY_CREATE:	return DRBD2_STATE_CHANGE_ACTION_CREATE;
	case NOTIFY_CHANGE:	return DRBD2_STATE_CHANGE_ACTION_CHANGE;
	case NOTIFY_DESTROY:	return DRBD2_STATE_CHANGE_ACTION_DESTROY;
	case NOTIFY_RENAME:	return DRBD2_STATE_CHANGE_ACTION_RENAME;
	default:		return DRBD2_STATE_CHANGE_ACTION_EXISTS;
	}
}

struct nlmsghdr *drbd2_test_encode(const struct drbd_nl_event *ev, enum drbd_nl_cmd cmd,
				   struct msg_buff *out)
{
	struct nlmsghdr *nlh = (struct nlmsghdr *)out->data;
	struct nlattr *nla;

	if (ev->kind == NL_OBJ_INITIAL_STATE_DONE) {
		/* the end of the state-get dump: a plain NLMSG_DONE */
		nlh = msg_put(out, NLMSG_HDRLEN);
		memset(nlh, 0, NLMSG_HDRLEN);
		nlh->nlmsg_type = NLMSG_DONE;
		*(int *)msg_put(out, NLMSG_ALIGN(sizeof(int))) = 0;
		nlh->nlmsg_len = out->tail - out->data;
		return nlh;
	}
	if (ev->kind == NL_OBJ_HELPER) {
		genlmsg_put(out, drbd2_dialect.family, 0, DRBD2_CMD_HELPER_NTF);
		put_context(out, DRBD2_A_CONTEXT, ev);
		nla = nla_nest_start(out, DRBD2_A_HELPER);
		nla_put_string(out, DRBD2_A_HELPER_INFO_NAME, ev->helper.helper_name);
		nla_put_u32(out, DRBD2_A_HELPER_INFO_STATUS, ev->helper.helper_status);
		nla_put_u32(out, DRBD2_A_HELPER_INFO_PHASE,
			    (ev->action & ~NOTIFY_FLAGS) == NOTIFY_RESPONSE ?
			    DRBD2_HELPER_PHASE_RESPONSE : DRBD2_HELPER_PHASE_CALL);
		nla_nest_end(out, nla);
		nlh->nlmsg_len = out->tail - out->data;
		return nlh;
	}

	switch (cmd) {
	case DRBD_NL_CMD_GET_RESOURCES:
		genlmsg_put(out, drbd2_dialect.family, NLM_F_MULTI, DRBD2_CMD_RESOURCE_GET);
		put_object(out, DRBD2_A_RESOURCE, ev);
		break;
	case DRBD_NL_CMD_GET_DEVICES:
		genlmsg_put(out, drbd2_dialect.family, NLM_F_MULTI, DRBD2_CMD_DEVICE_GET);
		put_object(out, DRBD2_A_DEVICE, ev);
		break;
	case DRBD_NL_CMD_GET_CONNECTIONS:
		genlmsg_put(out, drbd2_dialect.family, NLM_F_MULTI, DRBD2_CMD_CONNECTION_GET);
		put_object(out, DRBD2_A_CONNECTION, ev);
		break;
	case DRBD_NL_CMD_GET_PEER_DEVICES:
		genlmsg_put(out, drbd2_dialect.family, NLM_F_MULTI, DRBD2_CMD_PEER_DEVICE_GET);
		put_object(out, DRBD2_A_PEER_DEVICE, ev);
		break;
	case DRBD_NL_CMD_GET_PATHS:
		genlmsg_put(out, drbd2_dialect.family, NLM_F_MULTI, DRBD2_CMD_PATH_GET);
		put_object(out, DRBD2_A_PATH, ev);
		break;
	default: {
		/* the initial state replay and multicast events */
		static const int obj_attr[] = {
			[NL_OBJ_RESOURCE] = DRBD2_A_STATE_CHANGE_RESOURCE,
			[NL_OBJ_DEVICE] = DRBD2_A_STATE_CHANGE_DEVICE,
			[NL_OBJ_CONNECTION] = DRBD2_A_STATE_CHANGE_CONNECTION,
			[NL_OBJ_PEER_DEVICE] = DRBD2_A_STATE_CHANGE_PEER_DEVICE,
			[NL_OBJ_PATH] = DRBD2_A_STATE_CHANGE_PATH,
		};
		bool replay = (ev->action & ~NOTIFY_FLAGS) == NOTIFY_EXISTS;

		genlmsg_put(out, drbd2_dialect.family, replay ? NLM_F_MULTI : 0,
			    replay ? DRBD2_CMD_STATE_GET : DRBD2_CMD_STATE_CHANGE_NTF);
		nla_put_u32(out, DRBD2_A_STATE_CHANGE_ACTION, action_of(ev->action));
		if (ev->action & NOTIFY_CONTINUES)
			nla_put_flag(out, DRBD2_A_STATE_CHANGE_MORE);
		put_object(out, obj_attr[ev->kind], ev);
		break;
	}
	}
	nlh->nlmsg_len = out->tail - out->data;
	return nlh;
}
