/*
   drbdadm_adjust.c

   This file is part of DRBD by Philipp Reisner and Lars Ellenberg.

   Copyright (C) 2003-2008, LINBIT Information Technologies GmbH.
   Copyright (C) 2003-2008, Philipp Reisner <philipp.reisner@linbit.com>.
   Copyright (C) 2003-2008, Lars Ellenberg <lars.ellenberg@linbit.com>.

   drbd is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   drbd is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with drbd; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#define _GNU_SOURCE
#define _XOPEN_SOURCE 600
#define _FILE_OFFSET_BITS 64

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>

#include "drbdadm.h"
#include "drbdtool_common.h"
#include "drbdadm_parser.h"
#include "config_flags.h"

/* drbdsetup show might complain that the device minor does
   not exist at all. Redirect stderr to /dev/null therefore.
 */
static FILE *m_popen(int *pid, char * const* argv)
{
	int mpid;
	int pipes[2];
	int dev_null;

	if(pipe(pipes)) {
		err("Creation of pipes failed: %m\n");
		exit(E_EXEC_ERROR);
	}

	dev_null = open("/dev/null", O_WRONLY);
	if (dev_null == -1) {
		err("Opening /dev/null failed: %m\n");
		exit(E_EXEC_ERROR);
	}

	mpid = fork();
	if(mpid == -1) {
		err("Can not fork");
		exit(E_EXEC_ERROR);
	}
	if(mpid == 0) {
		close(pipes[0]); // close reading end
		dup2(pipes[1], fileno(stdout));
		close(pipes[1]);
		dup2(dev_null, fileno(stderr));
		close(dev_null);
		execvp(argv[0],argv);
		err("Can not exec");
		exit(E_EXEC_ERROR);
	}

	close(pipes[1]); // close writing end
	close(dev_null);
	*pid=mpid;
	return fdopen(pipes[0],"r");
}

__attribute__((format(printf, 2, 3)))
void report_compare(bool differs, const char *fmt, ...)
{
	va_list ap;
	if (verbose <= (3 - differs))
		return;
	if (differs)
		fprintf(stderr, " [ne] ");
	else
		fprintf(stderr, " [eq] ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

static struct field_def *field_def_of(const char *opt_name, struct context_def *ctx)
{
	struct field_def *field;

	for (field = ctx->fields; field->name; field++) {
		if (!strcmp(field->name, opt_name))
			return field;
	}

	err("Internal error: option '%s' not known in this context\n", opt_name);
	abort();
}

int is_equal(struct context_def *ctx, struct d_option *a, struct d_option *b)
{
	struct field_def *field = field_def_of(a->name, ctx);

	return field->ops->is_equal(field, a->value, b->value);
}

static bool is_default(struct context_def *ctx, struct d_option *opt)
{
	struct field_def *field = field_def_of(opt->name, ctx);

	return field->ops->is_default(field, opt->value);
}

static int opts_equal(struct context_def *ctx, struct options *conf, struct options *run_base)
{
	struct d_option *opt, *run_opt;

	STAILQ_FOREACH(run_opt, run_base, link) {
		if (run_opt->adj_skip)
			continue;

		opt = find_opt(conf, run_opt->name);
		if (opt) {
			if (!is_equal(ctx, run_opt, opt)) {
				if (verbose > 2)
					err("Value of '%s' differs: r=%s c=%s\n",
					    opt->name, run_opt->value, opt->value);
				return 0;
			}
			if (verbose > 3)
				err("Value of '%s' equal: r=%s c=%s\n",
				    opt->name, run_opt->value, opt->value);
			opt->mentioned = 1;
		} else {
			if (!is_default(ctx, run_opt)) {
				if (verbose > 2)
					err("Only in running config %s: %s\n",
					    run_opt->name, run_opt->value);
				return 0;
			}
			if (verbose > 3)
				err("Is default: '%s' equal: r=%s\n",
				    run_opt->name, run_opt->value);
		}
	}

	STAILQ_FOREACH(opt, conf, link) {
		if (opt->adj_skip)
			continue;

		if (opt->mentioned==0 && !is_default(ctx, opt)) {
			if (verbose > 2)
				err("Only in config file %s: %s\n", opt->name, opt->value);
			return 0;
		}
	}
	return 1;
}

static int
opt_equal(struct context_def *ctx, const char *opt_name, struct options *conf, struct options *run_base)
{
	struct field_def *field = field_def_of(opt_name, ctx);
	struct d_option *opt = find_opt(conf, opt_name);
	struct d_option *run_opt = find_opt(run_base, opt_name);

	if (opt && run_opt)
		return field->ops->is_equal(field, opt->value, run_opt->value);
	else if (opt)
		return field->ops->is_default(field, opt->value);
	else if (run_opt)
		return field->ops->is_default(field, run_opt->value);

	return 1; /* Both not set */
}

static struct path *find_path_by_addrs(struct connection *conn, struct path *pattern)
{
	struct path *path;

	for_each_path(path, &conn->paths) {
		if (addresses_equal(path->my_address, pattern->my_address) &&
		    addresses_equal(path->connect_to, pattern->connect_to))
			return path;
	}

	return NULL;
}

static bool adjust_paths(const struct cfg_ctx *ctx, struct connection *running_conn)
{
	struct connection *configured_conn = ctx->conn;
	struct path *configured_path, *running_path;
	struct cfg_ctx tmp_ctx = *ctx;
	int nr_running = 0;
	bool del_path = false;

	for_each_path(configured_path, &configured_conn->paths) {
		if (configured_path->ignore)
			continue;
		running_path = find_path_by_addrs(running_conn, configured_path);
		if (!running_path) {
			tmp_ctx.path = configured_path;
			schedule_deferred_cmd(&new_path_cmd, &tmp_ctx, CFG_NET_PATH);
		} else {
			running_path->adj_seen = 1;
		}
	}

	for_each_path(running_path, &running_conn->paths) {
		nr_running++;
		if (!running_path->adj_seen) {
			tmp_ctx.path = running_path;
			schedule_deferred_cmd(&del_path_cmd, &tmp_ctx, CFG_NET_PREP_DOWN);
			del_path = true;
		}
	}

	if (nr_running == 1 && del_path) {
		/* Deleting the last path fails is the connection is C_CONNECTING */
		if (!running_conn->is_standalone)
			schedule_deferred_cmd(&disconnect_cmd, &tmp_ctx, CFG_NET_DISCONNECT);
		return true;
	}

	return false;
}

static struct connection *matching_conn(struct connection *pattern, struct connections *pool, bool ret_me)
{
	struct connection *conn;

	for_each_connection(conn, pool) {
		if (ret_me && conn->me)
			return conn;
		if (conn->ignore)
			continue;
		if (!strcmp(pattern->peer->node_id, conn->peer->node_id))
			return conn;
	}

	return NULL;
}

/* Are both internal, or are both not internal. */
static int int_eq(char* m_conf, char* m_running)
{
	return !strcmp(m_conf,"internal") == !strcmp(m_running,"internal");
}

static int disk_equal(struct d_volume *conf, struct d_volume *running)
{
	int eq = 1;

	if (conf->disk == NULL && running->disk == NULL)
		return 1;
	if (conf->disk == NULL) {
		if (running->disk && !strcmp(running->disk, "none"))
			return 1;
		report_compare(1, "minor %u (vol:%u) %s missing from config\n",
			running->device_minor, running->vnr, running->disk);
		return 0;
	} else if (running->disk == NULL) {
		report_compare(1, "minor %u (vol:%u) %s missing from kernel\n",
			conf->device_minor, conf->vnr, conf->disk);
		return 0;
	}

	eq = !strcmp(conf->disk, running->disk);
	report_compare(!eq, "minor %u (vol:%u) disk: r=%s c=%s\n",
		running->device_minor, running->vnr, running->disk, conf->disk);
	if (eq) {
		eq = int_eq(conf->meta_disk, running->meta_disk);
		if (eq && strcmp(conf->meta_disk, "internal") != 0)
			eq = !strcmp(conf->meta_disk, running->meta_disk);
	}
	report_compare(!eq, "minor %u (vol:%u) meta-disk: r=%s c=%s\n",
		running->device_minor, running->vnr, running->meta_disk, conf->meta_disk);
	return eq;
}

/* The following is a cruel misuse of the cmd->name field. The whole proxy_reconf
   function should be rewritten in a sane way!
   It should schedule itself to get invoked later, and at the late point in time
   iterate the config and find out what to do...

   Obviously the schedule_deferred_proxy_reconf() function should go away */

static int do_proxy_reconf(const struct cfg_ctx *ctx)
{
	int rv;
	char *argv[4] = { drbd_proxy_ctl, "-c", (char*)ctx->cmd->name, NULL };

	rv = m_system_ex(argv, SLEEPS_SHORT, ctx->res->name);
	return rv;
}

static void schedule_deferred_proxy_reconf(const struct cfg_ctx *ctx, char *text)
{
	struct adm_cmd *cmd;

	cmd = calloc(1, sizeof(struct adm_cmd));
	if (cmd == NULL) {
		err("calloc: %m\n");
		exit(E_EXEC_ERROR);
	}

	cmd->name = text;
	cmd->function = &do_proxy_reconf;
	schedule_deferred_cmd(cmd, ctx, CFG_NET);
}

#define MAX_PLUGINS (10)
#define MAX_PLUGIN_NAME (16)

/* The new name is appended to the alist. */
bool _is_plugin_in_list(char *string,
		char slist[MAX_PLUGINS][MAX_PLUGIN_NAME],
		char alist[MAX_PLUGINS][MAX_PLUGIN_NAME],
		int list_len)
{
	int word_len, i;
	char *copy;

	for(word_len=0; string[word_len]; word_len++)
		if (isspace(string[word_len]))
			break;

	if (word_len+1 >= MAX_PLUGIN_NAME) {
		err("Wrong proxy plugin name %*.*s", word_len, word_len, string);
		exit(E_CONFIG_INVALID);
	}

	copy = alist[list_len];
	strncpy(copy, string, word_len);
	copy[word_len] = 0;


	for(i=0; i<list_len && *slist; i++) {
		if (strcmp(slist[i], copy) == 0)
			return true;
	}

	/* Not found, insert into list. */
	if (list_len >= MAX_PLUGINS) {
		err("Too many proxy plugins.");
		exit(E_CONFIG_INVALID);
	}

	return false;
}


static int proxy_reconf(const struct cfg_ctx *ctx, struct connection *running_conn)
{
	int reconn = 0;
	struct connection *conn = ctx->conn;
	struct path *path = STAILQ_FIRST(&conn->paths); /* multiple paths via proxy, later! */
	struct path *running_path; /* multiple paths via proxy, later! */
	struct d_option* res_o, *run_o;
	unsigned long long v1, v2, minimum;
	char *plugin_changes[MAX_PLUGINS], *cp, *conn_name;
	/* It's less memory usage when we're storing char[]. malloc overhead for
	 * the few bytes + pointers is much more. */
	char p_res[MAX_PLUGINS][MAX_PLUGIN_NAME],
		 p_run[MAX_PLUGINS][MAX_PLUGIN_NAME];
	int used, i;

	reconn = 0;

	if (!running_conn)
		goto redo_whole_conn;

	running_path = STAILQ_FIRST(&running_conn->paths); /* multiple paths via proxy, later! */
	if (!running_path->my_proxy)
		goto redo_whole_conn;

	if (running_path->proxy_conn_is_down)
		goto up_whole_conn;

	res_o = find_opt(&path->my_proxy->options, "memlimit");
	run_o = find_opt(&running_path->my_proxy->options, "memlimit");
	v1 = res_o ? m_strtoll(res_o->value, 1) : 0;
	v2 = run_o ? m_strtoll(run_o->value, 1) : 0;
	minimum = v1 < v2 ? v1 : v2;
	/* We allow an є [epsilon] of 2%, so that small (rounding) deviations do
	 * not cause the connection to be re-established. */
	if (res_o &&
			(!run_o || abs(v1-v2)/(float)minimum > 0.02))
	{
	redo_whole_conn:
		/* As the memory is in use while the connection is allocated we have to
		 * completely destroy and rebuild the connection. */

		schedule_deferred_cmd(&proxy_conn_down_cmd, ctx, CFG_NET_PREP_DOWN);
	up_whole_conn:
		schedule_deferred_cmd(&proxy_conn_up_cmd, ctx, CFG_NET_PREP_UP);
		schedule_deferred_cmd(&proxy_conn_plugins_cmd, ctx, CFG_NET_PREP_UP);

		/* With connection cleanup and reopen everything is rebuild anyway, and
		 * DRBD will get a reconnect too.  */
		return 0;
	}


	res_o = STAILQ_FIRST(&path->my_proxy->plugins);
	run_o = STAILQ_FIRST(&running_path->my_proxy->plugins);
	used = 0;
	conn_name = proxy_connection_name(ctx->res, conn); /* this is not possible on running_conn */
	for(i=0; i<MAX_PLUGINS; i++)
	{
		if (used >= sizeof(plugin_changes)-1) {
			err("Too many proxy plugin changes");
			exit(E_CONFIG_INVALID);
		}
		/* Now we can be sure that we can store another pointer. */

		if (!res_o) {
			if (run_o) {
				/* More plugins running than configured - just stop here. */
				m_asprintf(&cp, "set plugin %s %d end", conn_name, i);
				plugin_changes[used++] = cp;
			}
			else {
				/* Both at the end? ok, quit loop */
			}
			break;
		}

		/* res_o != NULL. */

		if (!run_o) {
			p_run[i][0] = 0;
			if (_is_plugin_in_list(res_o->name, p_run, p_res, i)) {
				/* Current plugin was already active, just at another position.
				 * Redo the whole connection. */
				goto redo_whole_conn;
			}

			/* More configured than running - just add it, if it's not already
			 * somewhere else. */
			m_asprintf(&cp, "set plugin %s %d %s", conn_name, i, res_o->name);
			plugin_changes[used++] = cp;
		} else {
			/* If we get here, both lists have been filled in parallel, so we
			 * can simply use the common counter. */
			bool re_do = _is_plugin_in_list(res_o->name, p_run, p_res, i) ||
				_is_plugin_in_list(run_o->name, p_res, p_run, i);
			if (re_do) {
				/* Plugin(s) were moved, not simple reconfigured.
				 * Re-do the whole connection. */
				goto redo_whole_conn;
			}

			/* TODO: We don't (yet) account for possible different ordering of
			 * the parameters to the plugin.
			 *    plugin A 1 B 2
			 * should be treated as equal to
			 *    plugin B 2 A 1. */
			if (strcmp(run_o->name, res_o->name) != 0) {
				/* Either a different plugin, or just different settings
				 * - plugin can be overwritten.  */
				m_asprintf(&cp, "set plugin %s %d %s", conn_name, i, res_o->name);
				plugin_changes[used++] = cp;
			}
		}


		if (res_o)
			res_o = STAILQ_NEXT(res_o, link);
		if (run_o)
			run_o = STAILQ_NEXT(run_o, link);
	}

	/* change only a few plugin settings. */
	for(i=0; i<used; i++)
		schedule_deferred_proxy_reconf(ctx, plugin_changes[i]);

	return reconn;
}

int need_trigger_kobj_change(struct d_resource *res)
{
	struct stat sbuf;
	char *link_name;
	int err;

	m_asprintf(&link_name, "/dev/drbd/by-res/%s", res->name);

	err = stat("/dev/drbd/by-res", &sbuf);
	if (err)	/* probably no udev rules in use */
		return 0;

	err = stat(link_name, &sbuf);
	if (err)
		/* resource link cannot be stat()ed. */
		return 1;

	/* double check device information */
	if (!S_ISBLK(sbuf.st_mode))
		return 1;
	if (major(sbuf.st_rdev) != DRBD_MAJOR)
		return 1;
	if (minor(sbuf.st_rdev) != STAILQ_FIRST(&res->me->volumes)->device_minor)
		return 1;

	/* Link exists, and is expected block major:minor.
	 * Do nothing. */
	return 0;
}

void compare_size(struct d_volume *conf, struct d_volume *kern)
{
	struct d_option *c = find_opt(&conf->disk_options, "size");
	struct d_option *k = find_opt(&kern->disk_options, "size");

	if (c)
		c->adj_skip = 1;
	if (k)
		k->adj_skip = 1;

	/* simplify logic below, would otherwise have to
	 * (!x || is_default(x) all the time. */
	if (k && is_default(&attach_cmd_ctx, k))
		k = NULL;

	/* size was set, but it is no longer in config, or vice versa */
	if (!k != !c)
		conf->adj_resize = 1;

	/* size options differ */
	if (k && c && !is_equal(&attach_cmd_ctx, c, k))
		conf->adj_resize = 1;

	report_compare(conf->adj_resize, "minor %u (vol:%u) size: r=%s c=%s\n",
		conf->device_minor, conf->vnr,
		k ? k->value : "-", c ? c->value : "-");
}

void compare_volume(struct d_volume *conf, struct d_volume *kern)
{
	conf->adj_new_minor = conf->device_minor != kern->device_minor;
	conf->adj_del_minor = conf->adj_new_minor;

	if (conf->adj_new_minor)
		report_compare(1, "vol:%u minor differs: r=%u c=%u\n",
			conf->vnr, kern->device_minor, conf->device_minor);

	if (!disk_equal(conf, kern) || conf->adj_new_minor) {
		conf->adj_attach = conf->disk != NULL && strcmp(conf->disk, "none");
		conf->adj_detach = kern->disk != NULL && strcmp(kern->disk, "none");
	}

	/* Do we need to resize?
	 * If we are going to attach, or we don't even have a local disk,
	 * don't even compare the size. */
	if (!conf->adj_attach && !conf->adj_detach && conf->disk != NULL)
		compare_size(conf, kern);

	/* is it sufficient to only adjust the disk options? */
	if (!(conf->adj_detach || conf->adj_attach) && conf->disk)
		conf->adj_disk_opts = !opts_equal(&disk_options_ctx, &conf->disk_options, &kern->disk_options);
}

struct d_volume *new_to_be_deleted_minor_from_template(struct d_volume *kern)
{
	/* need to delete it from kernel.
	 * Create a minimal volume,
	 * and flag it as "del_minor". */
	struct d_volume *conf = calloc(1, sizeof(*conf));
	conf->vnr = kern->vnr;
	/* conf->device: no need */
	conf->device_minor = kern->device_minor;
	if (kern->disk && strcmp(kern->disk, "none")) {
		conf->disk = strdup(kern->disk);
		conf->meta_disk = strdup(kern->meta_disk);
		conf->meta_index = strdup(kern->meta_index);
		conf->adj_detach = 1;
	}

	conf->adj_del_minor = 1;
	return conf;
}

#define ASSERT(x) do { if (!(x)) {				\
	err("%s:%u:%s: ASSERT(%s) failed.\n", __FILE__,		\
	     __LINE__, __func__, #x);				\
	abort(); }						\
	} while (0)

/* Both conf and kern are single linked lists
 * supposed to be ordered by ->vnr;
 * We may need to conjure dummy volumes to issue "del-minor" on,
 * and insert these into the conf list.
 */
void compare_volumes(struct volumes *conf_head, struct volumes *kern_head)
{
	struct volumes to_be_deleted = STAILQ_HEAD_INITIALIZER(to_be_deleted);
	struct d_volume *conf = STAILQ_FIRST(conf_head);
	struct d_volume *kern = STAILQ_FIRST(kern_head);
	struct d_volume *next;

	while (conf || kern) {
		if (kern && (conf == NULL || kern->vnr < conf->vnr)) {
			insert_volume(&to_be_deleted, new_to_be_deleted_minor_from_template(kern));
			if (verbose > 2)
				err("Deleting minor %u (vol:%u) from kernel, not in config\n",
					kern->device_minor, kern->vnr);
			kern = STAILQ_NEXT(kern, link);
		} else if (conf && (kern == NULL || kern->vnr > conf->vnr)) {
			conf->adj_new_minor = 1;
			if (conf->disk)
				conf->adj_attach = 1;
			if (verbose > 2)
				err("New minor %u (vol:%u)\n", conf->device_minor, conf->vnr);
			conf = STAILQ_NEXT(conf, link);
		} else {
			ASSERT(conf);
			ASSERT(kern);
			ASSERT(conf->vnr == kern->vnr);

			compare_volume(conf, kern);
			conf = STAILQ_NEXT(conf, link);
			kern = STAILQ_NEXT(kern, link);
		}
	}
	for_each_volume_safe(conf, next, &to_be_deleted)
		insert_volume(conf_head, conf);
}

static struct peer_device *matching_peer_device(struct peer_device *pattern, struct peer_devices *pool)
{
	struct peer_device *peer_device;

	STAILQ_FOREACH(peer_device, pool, connection_link) {
		if (pattern->vnr == peer_device->vnr)
			return peer_device;
	}
	return NULL;
}

static void
adjust_peer_devices(const struct cfg_ctx *ctx, struct connection *running_conn)
{
	struct adm_cmd *cmd = &peer_device_options_defaults_cmd;
	struct context_def *oc = &peer_device_options_ctx;
	struct peer_device *peer_device, *running_pd;
	struct cfg_ctx tmp_ctx = *ctx;

	STAILQ_FOREACH(peer_device, &ctx->conn->peer_devices, connection_link) {
		running_pd = matching_peer_device(peer_device, &running_conn->peer_devices);
		tmp_ctx.vol = volume_by_vnr(&ctx->conn->peer->volumes, peer_device->vnr);
		if (!running_pd) {
			schedule_deferred_cmd(cmd, &tmp_ctx, CFG_PEER_DEVICE | SCHEDULE_ONCE);
			continue;
		}
		if (!opts_equal(oc, &peer_device->pd_options, &running_pd->pd_options))
			schedule_deferred_cmd(cmd, &tmp_ctx, CFG_PEER_DEVICE);
	}
}

void schedule_peer_device_options(const struct cfg_ctx *ctx)
{
	struct adm_cmd *cmd = &peer_device_options_defaults_cmd;
	struct cfg_ctx tmp_ctx = *ctx;
	struct peer_device *peer_device;

	if (!tmp_ctx.vol && !tmp_ctx.conn) {
		err("Call schedule_peer_devices_options() with vol or conn set!");
		exit(E_THINKO);
	} else if (!tmp_ctx.vol) {
		struct d_host_info *me = ctx->res->me;
		struct d_volume *vol;
		for_each_volume(vol, &me->volumes) {
			peer_device = find_peer_device(tmp_ctx.conn, vol->vnr);
			if (!peer_device || STAILQ_EMPTY(&peer_device->pd_options))
				continue;

			tmp_ctx.vol = vol;
			schedule_deferred_cmd(cmd, &tmp_ctx, CFG_PEER_DEVICE | SCHEDULE_ONCE);
		}
	} else if (!tmp_ctx.conn) {
		struct connection *conn;
		for_each_connection(conn, &ctx->res->connections) {
			peer_device = find_peer_device(conn, tmp_ctx.vol->vnr);
			if (!peer_device || STAILQ_EMPTY(&peer_device->pd_options) || !conn->peer)
				continue;

			tmp_ctx.conn = conn;
			schedule_deferred_cmd(cmd, &tmp_ctx, CFG_PEER_DEVICE | SCHEDULE_ONCE);
		}
	} else {
		err("vol and conn set in schedule_peer_devices_options()!");
		exit(E_THINKO);
	}
}

static struct d_volume *matching_volume(struct d_volume *conf_vol, struct volumes *kern_head)
{
	struct d_volume *vol;

	for_each_volume(vol, kern_head) {
		if (vol->vnr == conf_vol->vnr)
			return vol;
	}
	return NULL;
}


static void
adjust_net(const struct cfg_ctx *ctx, struct d_resource* running)
{
	struct connection *conn;

	if (running) {
		for_each_connection(conn, &running->connections) {
			struct connection *configured_conn;

			configured_conn = matching_conn(conn, &ctx->res->connections, true);
			if (!configured_conn) {
				struct cfg_ctx tmp_ctx = { .res = running, .conn = conn };
				schedule_deferred_cmd(&del_peer_cmd, &tmp_ctx, CFG_NET_PREP_DOWN);
			}
		}
	}

	for_each_connection(conn, &ctx->res->connections) {
		struct connection *running_conn = NULL;
		struct path *path;
		const struct cfg_ctx tmp_ctx = { .res = ctx->res, .conn = conn };

		if (conn->ignore)
			continue;

		if (running)
			running_conn = matching_conn(conn, &running->connections, false);
		if (!running_conn) {
			schedule_deferred_cmd(&new_peer_cmd, &tmp_ctx, CFG_NET_PREP_UP);
			schedule_deferred_cmd(&new_path_cmd, &tmp_ctx, CFG_NET_PATH);
			schedule_deferred_cmd(&connect_cmd, &tmp_ctx, CFG_NET_CONNECT);
			schedule_peer_device_options(&tmp_ctx);
		} else {
			struct context_def *oc = &show_net_options_ctx;
			struct options *conf_o = &conn->net_options;
			struct options *runn_o = &running_conn->net_options;
			bool connect = false, new_path = false;

			if (running_conn->is_standalone)
				connect = true;

			if (!opts_equal(oc, conf_o, runn_o)) {
				if (!opt_equal(oc, "transport", conf_o, runn_o)) {
					/* disconnect implicit by del-peer */
					schedule_deferred_cmd(&del_peer_cmd, &tmp_ctx, CFG_NET_PREP_DOWN);
					schedule_deferred_cmd(&new_peer_cmd, &tmp_ctx, CFG_NET_PREP_UP);
					new_path = true;
					connect = true;
					schedule_peer_device_options(&tmp_ctx);
				} else {
					del_opt(&tmp_ctx.conn->net_options, "transport");
					schedule_deferred_cmd(&net_options_defaults_cmd, &tmp_ctx, CFG_NET);
				}
			}

			if (new_path)
				schedule_deferred_cmd(&new_path_cmd, &tmp_ctx, CFG_NET_PATH);
			else
				connect |= adjust_paths(&tmp_ctx, running_conn);

			if (connect)
				schedule_deferred_cmd(&connect_cmd, &tmp_ctx, CFG_NET_CONNECT);

			adjust_peer_devices(&tmp_ctx, running_conn);
		}

		path = STAILQ_FIRST(&conn->paths); /* multiple paths via proxy, later! */
		if (path->my_proxy && hostname_in_list(hostname, &path->my_proxy->on_hosts))
			proxy_reconf(&tmp_ctx, running_conn);
	}
}


static void adjust_disk(const struct cfg_ctx *ctx, struct d_resource* running)
{
	struct d_volume *vol;

	/* do we need to attach,
	 * do we need to detach first,
	 * or is this just some attribute change? */
	for_each_volume(vol, &ctx->res->me->volumes) {
		struct cfg_ctx tmp_ctx = { .res = ctx->res, .vol = vol };

		if (ctx->vol && vol != ctx->vol) /* In case we know the volume ignore all others. */
			continue;

		if (vol->adj_detach || vol->adj_del_minor) {
			struct d_volume *kern_vol = matching_volume(vol, &running->me->volumes);
			struct cfg_ctx k_ctx = tmp_ctx;
			if (kern_vol != NULL)
				k_ctx.vol = kern_vol;
			if (vol->adj_detach)
				schedule_deferred_cmd(&detach_cmd, &k_ctx, CFG_DISK_PREP_DOWN);
			if (vol->adj_del_minor)
				schedule_deferred_cmd(&del_minor_cmd, &k_ctx, CFG_DISK_PREP_DOWN);
		}
		if (vol->adj_attach)
			schedule_deferred_cmd(&attach_cmd, &tmp_ctx, CFG_DISK);
		if (vol->adj_disk_opts)
			schedule_deferred_cmd(&disk_options_defaults_cmd, &tmp_ctx, CFG_DISK);
		if (vol->adj_resize)
			schedule_deferred_cmd(&resize_cmd, &tmp_ctx, CFG_DISK);
	}
}

char config_file_drbdsetup_show[] = "drbdsetup show";
struct resources running_config = STAILQ_HEAD_INITIALIZER(running_config);

struct d_resource *parse_drbdsetup_show(const char *name)
{
	struct d_resource *res = NULL;
	char *fake_drbdsetup_show;
	char* argv[4];
	int pid = -1, argc;
	int token;

	fake_drbdsetup_show = getenv("FAKE_DRBDSETUP_SHOW");
	/* disable check_uniq, so it won't interfere
	 * with parsing of drbdsetup show output */
	config_valid = 2;

	/* setup error reporting context for the parsing routines */
	line = 1;
	config_file = config_file_drbdsetup_show;

	argc = 0;
	argv[argc++] = drbdsetup;
	argv[argc++] = "show";
	if (name)
		argv[argc++] = ssprintf("%s", name);
	argv[argc++] = NULL;

	if (fake_drbdsetup_show) {
		yyin = fopen(fake_drbdsetup_show, "r");
		if (!yyin) {
			err("Failed to open FAKE_DRBDSETUP_SHOW %s\n", fake_drbdsetup_show);
			exit(E_USAGE);
		}
	} else {
		yyin = m_popen(&pid, argv);
	}
	for (;;) {
		token = yylex();
		if (token == 0)
			break;

		if (token != TK_RESOURCE)
			break;

		token = yylex();
		if (token != TK_STRING)
			break;

		token = yylex();
		if (token != '{')
			break;

		res = parse_resource(yylval.txt, PARSE_FOR_ADJUST);
		insert_tail(&running_config, res);
	}
	fclose(yyin);
	if (pid != -1)
		waitpid(pid, 0, 0);

	post_parse(&running_config, DRBDSETUP_SHOW);

	return res;
}

struct d_resource *running_res_by_name(const char *name)
{
	static bool drbdsetup_show_parsed = false;
	struct d_resource *res;

	if (adjust_more_than_one_resource && !drbdsetup_show_parsed) {
		parse_drbdsetup_show(NULL); /* all in one go */
		drbdsetup_show_parsed = true;
	}

	for_each_resource(res, &running_config) {
		if (strcmp(name, res->name) == 0)
			return res;
	}

	if (!adjust_more_than_one_resource)
		return parse_drbdsetup_show(name);

	return NULL;
}


/*
 * CAUTION this modifies global static char * config_file!
 */
int _adm_adjust(const struct cfg_ctx *ctx, int adjust_flags)
{
	char config_file_dummy[250];
	struct d_resource* running;
	struct volumes empty = STAILQ_HEAD_INITIALIZER(empty);
	struct d_volume *vol;

	/* necessary per resource actions */
	int do_res_options = 0;

	/* necessary per volume actions are flagged
	 * in the vol->adj_* members. */

	set_me_in_resource(ctx->res, true);
	set_peer_in_resource(ctx->res, true);

	running = running_res_by_name(ctx->res->name);

	if (running) {
		set_me_in_resource(running, DRBDSETUP_SHOW);
		set_peer_in_resource(running, DRBDSETUP_SHOW);
	}

	/* Parse proxy settings, if this host has a proxy definition.
	 * FIXME what about "zombie" proxy settings, if we remove proxy
	 * settings from the config file without prior proxy-down, this won't
	 * clean them from the proxy. */
	if (running) {
		struct connection *conn;
		for_each_connection(conn, &running->connections) {
			struct connection *configured_conn = NULL;
			struct path *configured_path;
			struct path *path = STAILQ_FIRST(&conn->paths); /* multiple paths via proxy, later! */
			struct cfg_ctx tmp_ctx = { .res = ctx->res };
			char *show_conn;
			int pid, argc, status, w;
			char *argv[20];

			if (!path)
				continue;
			configured_conn = matching_conn(conn, &ctx->res->connections, false);
			if (!configured_conn)
				continue;
			configured_path = STAILQ_FIRST(&configured_conn->paths);
			if (!configured_path->peer_proxy)
				continue;

			tmp_ctx.conn = configured_conn;

			line = 1;
			m_asprintf(&show_conn, "show proxy-settings %s", proxy_connection_name(tmp_ctx.res, configured_conn));
			sprintf(config_file_dummy, "drbd-proxy-ctl -c '%s'", show_conn);
			config_file = config_file_dummy;

			argc=0;
			argv[argc++]=drbd_proxy_ctl;
			argv[argc++]="-c";
			argv[argc++]=show_conn;
			argv[argc++]=0;

			/* actually parse "drbd-proxy-ctl show" output */
			yyin = m_popen(&pid, argv);
			path->proxy_conn_is_down = parse_proxy_options_section(&path->my_proxy);
			fclose(yyin);

			w = waitpid(pid, &status, 0);
			if (w == -1)
				err("waitpid() errno = %d\n", errno);

			if (WIFEXITED(status) && WEXITSTATUS(status))
				path->proxy_conn_is_down = 1;
		}
	}

	if (!running && verbose > 2)
		err("New resource %s\n", ctx->res->name);

	compare_volumes(&ctx->res->me->volumes, running ? &running->me->volumes : &empty);

	if (running) {
		do_res_options = !opts_equal(&resource_options_ctx, &ctx->res->res_options, &running->res_options);
	} else {
		schedule_deferred_cmd(&new_resource_cmd, ctx, CFG_PREREQ);
	}

	if (do_res_options)
		schedule_deferred_cmd(&res_options_defaults_cmd, ctx, CFG_RESOURCE);

	if (adjust_flags & ADJUST_NET)
		adjust_net(ctx, running);

	if (adjust_flags & ADJUST_DISK)
		adjust_disk(ctx, running);

	for_each_volume(vol, &ctx->res->me->volumes) {
		if (ctx->vol && vol != ctx->vol) /* In case we know the volume ignore all others. */
			continue;

		if (vol->adj_new_minor) {
			struct cfg_ctx tmp_ctx = { .res = ctx->res, .vol = vol };
			schedule_deferred_cmd(&new_minor_cmd, &tmp_ctx, CFG_DISK_PREP_UP);
			if (adjust_flags & ADJUST_NET && adjust_flags & ADJUST_DISK)
				schedule_peer_device_options(&tmp_ctx);
		}
	}

	return 0;
}
