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

#define _EVPRINT(checksize, fstr, ...) do { \
	ret = snprintf(key + pos, size, fstr, __VA_ARGS__); \
	if (ret < 0) \
		return ret; \
	pos += ret; \
	if (size && checksize) \
		size -= ret; \
} while(0)
#define EVPRINT(...) _EVPRINT(1, __VA_ARGS__)
/* for llvm static analyzer */
#define EVPRINT_NOSIZE(...) _EVPRINT(0, __VA_ARGS__)
static int event_key(char *key, int size, const char *name, unsigned minor,
		     struct drbd_cfg_context *ctx)
{
	char addr[ADDRESS_STR_MAX];
	int ret, pos = 0;

	if (!ctx)
		return -1;

	EVPRINT("%s", name);

	if (ctx->ctx_resource_name)
		EVPRINT(" name:%s", ctx->ctx_resource_name);

	if (ctx->ctx_peer_node_id != -1U)
		EVPRINT(" peer-node-id:%d", ctx->ctx_peer_node_id);

	if (ctx->ctx_conn_name_len)
		EVPRINT(" conn-name:%s", ctx->ctx_conn_name);

	if (ctx->ctx_my_addr_len &&
	    address_str(addr, ctx->ctx_my_addr, ctx->ctx_my_addr_len))
		EVPRINT(" local:%s", addr);

	if (ctx->ctx_peer_addr_len &&
	    address_str(addr, ctx->ctx_peer_addr, ctx->ctx_peer_addr_len))
		EVPRINT(" peer:%s", addr);

	if (ctx->ctx_volume != -1U)
		EVPRINT(" volume:%u", ctx->ctx_volume);

	if (minor != -1U)
		EVPRINT_NOSIZE(" minor:%u", minor);

	return pos;
}

static int known_objects_cmp(const void *a, const void *b) {
	return strcmp(((const struct entry *)a)->key, ((const struct entry *)b)->key);
}

static void *update_info(char **key, void *value, size_t size)
{
	static void *known_objects;

	struct entry entry = { .key = *key }, **found;

	if (value) {
		void *old_value = NULL;

		found = tsearch(&entry, &known_objects, known_objects_cmp);
		if (*found != &entry)
			old_value = (*found)->data;
		else {
			*found = malloc(sizeof(**found));
			if (!*found)
				goto fail;
			(*found)->key = *key;
			*key = NULL;
		}

		(*found)->data = malloc(size);
		if (!(*found)->data)
			goto fail;
		memcpy((*found)->data, value, size);

		return old_value;
	} else {
		found = tfind(&entry, &known_objects, known_objects_cmp);
		if (found) {
			struct entry *entry = *found;

			tdelete(entry, &known_objects, known_objects_cmp);
			free(entry->data);
			free(entry->key);
			free(entry);
		}
		return NULL;
	}

fail:
	perror(progname);
	exit(20);
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
	struct drbd_genlmsghdr *dh;
	char *key = NULL;
	const char *name;
	int err, size;

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

	name = object_name[info->genlhdr->cmd];
	size = event_key(NULL, 0, name, dh->minor, &ctx);
	if (size < 0)
		goto fail;
	key = malloc(size + 1);
	if (!key)
		goto fail;
	event_key(key, size + 1, name, dh->minor, &ctx);

	printf("%s %s", action_name[action], key);

	switch(info->genlhdr->cmd) {
	case DRBD_RESOURCE_STATE:
		if (action != NOTIFY_DESTROY) {
			bool have_new_stats = true;
			struct {
				struct resource_info i;
				struct resource_statistics s;
			} *old, new;

			err = resource_info_from_attrs(&new.i, info);
			if (err) {
				dbg(1, "resource info missing\n");
				goto nl_out;
			}
			memset(&new.s, -1, sizeof(new.s));
			err = resource_statistics_from_attrs(&new.s, info);
			if (err) {
				dbg(1, "resource statistics missing\n");
				have_new_stats = false;
			}
			old = update_info(&key, &new, sizeof(new));
			if (old && !have_new_stats)
				new.s = old->s;

			if (!old || new.i.res_role != old->i.res_role)
				printf(" role:%s%s%s",
						ROLE_COLOR_STRING(new.i.res_role, 1));
			if (!old ||
			    new.i.res_susp != old->i.res_susp ||
			    new.i.res_susp_nod != old->i.res_susp_nod ||
			    new.i.res_susp_fen != old->i.res_susp_fen ||
			    new.i.res_susp_quorum != old->i.res_susp_quorum)
				printf(" suspended:%s",
				       susp_str(&new.i));
			if (opt_statistics && have_new_stats)
				print_resource_statistics(0, old ? &old->s : NULL,
							  &new.s, nowrap_printf);
			free(old);
		} else
			update_info(&key, NULL, 0);
		break;
	case DRBD_DEVICE_STATE:
		if (action != NOTIFY_DESTROY) {
			bool have_new_stats = true;
			struct {
				struct device_info i;
				struct device_statistics s;
			} *old, new;

			new.i.is_intentional_diskless = IS_INTENTIONAL_DEF;
			err = device_info_from_attrs(&new.i, info);
			if (err) {
				dbg(1, "device info missing\n");
				goto nl_out;
			}
			memset(&new.s, -1, sizeof(new.s));
			err = device_statistics_from_attrs(&new.s, info);
			if (err) {
				dbg(1, "device statistics missing\n");
				have_new_stats = false;
			}
			old = update_info(&key, &new, sizeof(new));
			if (old && !have_new_stats)
				new.s = old->s;
			if (!old || new.i.dev_disk_state != old->i.dev_disk_state ||
			    new.i.dev_has_quorum != old->i.dev_has_quorum) {
				bool intentional = new.i.is_intentional_diskless == 1;
				printf(" disk:%s%s%s",
						DISK_COLOR_STRING(new.i.dev_disk_state, intentional, true));
				printf(" client:%s", intentional_diskless_str(&new.i));
				printf(" quorum:%s", new.i.dev_has_quorum ? "yes" : "no");
			}
			if (opt_statistics && have_new_stats)
				print_device_statistics(0, old ? &old->s : NULL,
							&new.s, nowrap_printf);
			free(old);
		} else
			update_info(&key, NULL, 0);
		break;
	case DRBD_CONNECTION_STATE:
		if (action != NOTIFY_DESTROY) {
			bool have_new_stats = true;
			struct {
				struct connection_info i;
				struct connection_statistics s;
			} *old, new;

			err = connection_info_from_attrs(&new.i, info);
			if (err) {
				dbg(1, "connection info missing\n");
				goto nl_out;
			}
			memset(&new.s, -1, sizeof(new.s));
			err = connection_statistics_from_attrs(&new.s, info);
			if (err) {
				dbg(1, "connection statistics missing\n");
				have_new_stats = false;
			}
			old = update_info(&key, &new, sizeof(new));
			if (old && !have_new_stats)
				new.s = old->s;
			if (!old ||
			    new.i.conn_connection_state != old->i.conn_connection_state)
				printf(" connection:%s%s%s",
						CONN_COLOR_STRING(new.i.conn_connection_state));
			if (!old ||
			    new.i.conn_role != old->i.conn_role)
				printf(" role:%s%s%s",
						ROLE_COLOR_STRING(new.i.conn_role, 0));
			if (opt_statistics && have_new_stats)
				print_connection_statistics(0, old ? &old->s : NULL,
							    &new.s, nowrap_printf);
			free(old);
		} else
			update_info(&key, NULL, 0);
		break;
	case DRBD_PEER_DEVICE_STATE:
		if (action != NOTIFY_DESTROY) {
			bool have_new_stats = true;
			struct {
				struct peer_device_info i;
				struct peer_device_statistics s;
			} *old, new;

			new.i.peer_is_intentional_diskless = IS_INTENTIONAL_DEF;
			err = peer_device_info_from_attrs(&new.i, info);
			if (err) {
				dbg(1, "peer device info missing\n");
				goto nl_out;
			}

			memset(&new.s, -1, sizeof(new.s));
			err = peer_device_statistics_from_attrs(&new.s, info);
			if (err) {
				dbg(1, "peer device statistics missing\n");
				have_new_stats = false;
			}

			old = update_info(&key, &new, sizeof(new));
			if (old && !have_new_stats)
				new.s = old->s;

			if (!old || new.i.peer_repl_state != old->i.peer_repl_state)
				printf(" replication:%s%s%s",
						REPL_COLOR_STRING(new.i.peer_repl_state));
			if (!old || new.i.peer_disk_state != old->i.peer_disk_state) {
				bool intentional = new.i.peer_is_intentional_diskless == 1;
				printf(" peer-disk:%s%s%s",
						DISK_COLOR_STRING(new.i.peer_disk_state, intentional,  false));
				printf(" peer-client:%s", peer_intentional_diskless_str(&new.i));
			}
			if (!old ||
			    new.i.peer_resync_susp_user != old->i.peer_resync_susp_user ||
			    new.i.peer_resync_susp_peer != old->i.peer_resync_susp_peer ||
			    new.i.peer_resync_susp_dependency != old->i.peer_resync_susp_dependency)
				printf(" resync-suspended:%s",
				       resync_susp_str(&new.i));

			if (have_new_stats)
				print_peer_device_statistics(0, old ? &old->s : NULL,
							     &new.s, nowrap_printf);

			free(old);
		} else
			update_info(&key, NULL, 0);
		break;
	case DRBD_PATH_STATE:
		if (action != NOTIFY_DESTROY) {
			struct drbd_path_info new = {}, *old;

			err = drbd_path_info_from_attrs(&new, info);
			if (err) {
				dbg(1, "path info missing\n");
				goto nl_out;
			}
			old = update_info(&key, &new, sizeof(new));
			if (!old || old->path_established != new.path_established)
				printf(" established:%s",
				       new.path_established ? "yes" : "no");
			free(old);
		} else
			update_info(&key, NULL, 0);
		break;
	case DRBD_HELPER: {
		struct drbd_helper_info helper_info;

		err = drbd_helper_info_from_attrs(&helper_info, info);
		if (err) {
			dbg(1, "helper info missing\n");
			goto nl_out;
		}
		printf(" helper:%s", helper_info.helper_name);
		if (action == NOTIFY_RESPONSE)
			printf(" status:%u", helper_info.helper_status);
		}
		break;
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
