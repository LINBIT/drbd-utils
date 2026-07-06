/*
 * DRBD setup via genetlink
 *
 * Copyright (C) 2008-2024, LINBIT HA-Solutions GmbH.
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

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>

#include "drbdtool_common.h"
#include "drbdsetup.h"
#include "path.h"

enum c84_ctx_key {
	CCTX_MINOR,
	CCTX_RES_VIA_MINOR,
	CCTX_RESOURCE,
	CCTX_PEER_DEVICE,
};

static bool load_opts(const struct drbd_cmd_ctx *ctx, int *argc, char **argv,
		      const struct drbd_cmd *cmd, enum c84_ctx_key key);

/*
 * This heuristic needs to be in sync with generate_implicit_node_id()
 * in drbdadm_postparse.c.
 * The side with the greater hash value gets node id 0.
 */
static int compare_addr(const char *my_addr, const char *peer_addr)
{
	int my_hash, peer_hash;

	my_hash = crc32c(0x1a656f21, (void *)my_addr, strlen(my_addr));
	peer_hash = crc32c(0x1a656f21, (void *)peer_addr, strlen(peer_addr));

	if (my_hash == peer_hash) {
		fprintf(stderr, "my and peer addr produce the same hash!\n");
		exit(20);
	}

	return my_hash > peer_hash;
}

static void drbd8_compat_relevant_opts(const struct drbd_cmd *cm, char **argv_in,
				       int argc_in, char **args_out, int *argc)
{
	struct option *options;
	char *arg;
	int c;

	options = make_longoptions(cm, true);
	optind = 0;
	for (;;) {
		int idx;

		c = getopt_long(argc_in, argv_in, "(", options, &idx);
		if (c == -1)
			break;
		if ((c >= OPT_ALT_BASE && c != OPT_COMPAT84) || c == 0) {
			if (optarg == argv_in[optind - 1]) {
				/* If it is "--opt val", copy the "--opt" first */
				arg = argv_in[optind - 2];
				args_out[(*argc)++] = arg;
			}
			/* This is the "val" or "--opt=val" */
			arg = argv_in[optind - 1];
			args_out[(*argc)++] = arg;
		}
		/* if (c == '?') continue; */
	}
}


static void drbd8_compat_set_peer_device_options(const struct drbd_cmd_ctx *base)
{
	struct drbd_cmd_ctx c = *base;
	const char *resname = c.objname;
	char path[200], *pd_args[40];
	struct dirent *dirent;
	int i, vol, argc;
	bool loaded;
	DIR *dir;

	c.context = CTX_PEER_DEVICE;
	pd_args[0] = (char *)peer_device_options_cmd.cmd;

	snprintf(path, 200, "%s/compat-84/res-%s", drbd_run_dir(), resname);
	dir = opendir(path);
	if (!dir) {
		/* ENOENT: nothing stored for this resource, nothing to apply */
		if (errno != ENOENT)
			fprintf(stderr, "Failed to open '%s' with %s (%d)\n",
				path, strerror(errno), errno);
		return;
	}

	while ((dirent = readdir(dir))) {
		if (sscanf(dirent->d_name, "vol-%d", &vol) == 1) {
			argc = 1;
			c.nl.ctx_volume = vol;
			loaded = load_opts(&c, &argc, pd_args, &peer_device_options_cmd,
					   CCTX_PEER_DEVICE);
			if (loaded) {
				_generic_config_cmd(&peer_device_options_cmd, &c, argc, pd_args);
				for (i = 1; i < argc; i++)
					free(pd_args[i]); /* load_opts() malloc()ed */
			}
		}
	}
	closedir(dir);
}

static int drbd8_compat_fake_new_peer(const struct drbd_cmd_ctx *base, int peer_node_id,
				      int argc_in, char **argv_in)
{
	struct drbd_cmd_ctx c = *base;
	char *new_peer_args[40];
	int rv, i, argc_dyn_start, argc = 0;

	c.context = CTX_PEER_NODE;
	c.nl.ctx_peer_node_id = peer_node_id;

	new_peer_args[argc++] = (char *)new_peer_cmd.cmd;
	new_peer_args[argc++] = "--_name=remote";

	drbd8_compat_relevant_opts(&new_peer_cmd, argv_in, argc_in, new_peer_args, &argc);
	argc_dyn_start = argc;
	load_opts(&c, &argc, new_peer_args, &new_peer_cmd, CCTX_RESOURCE);

	rv = _generic_config_cmd(&new_peer_cmd, &c, argc, new_peer_args);
	for (i = argc_dyn_start; i < argc; i++)
		free(new_peer_args[i]); /* load_opts() malloc()ed them via getline() */

	drbd8_compat_set_peer_device_options(&c);
	return rv;
}

static int drbd8_compat_fake_new_path(const struct drbd_cmd_ctx *base, int peer_node_id,
				      const char *my_addr, const char *peer_addr)
{
	struct drbd_cmd_ctx c = *base;
	char *new_path_args[3];
	int argc = 0;

	new_path_args[argc++] = (char *)new_path_cmd.cmd;
	new_path_args[argc++] = (char *)my_addr;
	new_path_args[argc++] = (char *)peer_addr;
	c.context = CTX_PEER_NODE;
	c.nl.ctx_peer_node_id = peer_node_id;
	return _generic_config_cmd(&new_path_cmd, &c, argc, new_path_args);
}

static int drbd8_compat_fake_connect(const struct drbd_cmd_ctx *base, int peer_node_id)
{
	struct drbd_cmd_ctx c = *base;
	char *connect_args[1];
	int argc = 0;

	connect_args[argc++] = (char *)connect_cmd.cmd;
	c.context = CTX_PEER_NODE;
	c.nl.ctx_peer_node_id = peer_node_id;
	return _generic_config_cmd(&connect_cmd, &c, argc, connect_args);
}

static int drbd8_compat_fake_del_peer(const struct drbd_cmd_ctx *base, int peer_node_id)
{
	struct drbd_cmd_ctx c = *base;
	char *del_peer_args[1];
	int argc = 0;

	del_peer_args[argc++] = "del-peer";
	c.context = CTX_PEER_NODE;
	c.nl.ctx_peer_node_id = peer_node_id;
	return _generic_config_cmd(&del_peer_cmd, &c, argc, del_peer_args);
}

static int drbd8_compat_fake_del_path(const struct drbd_cmd_ctx *base, int peer_node_id,
				      const char *my_addr, const char *peer_addr)
{
	struct drbd_cmd_ctx c = *base;
	char *del_path_args[3];
	int argc = 0;

	del_path_args[argc++] = (char *)del_path_cmd.cmd;
	del_path_args[argc++] = (char *)my_addr;
	del_path_args[argc++] = (char *)peer_addr;
	c.context = CTX_PEER_NODE;
	c.nl.ctx_peer_node_id = peer_node_id;
	return _generic_config_cmd(&del_path_cmd, &c, argc, del_path_args);
}

static int drbd8_compat_fake_disconnect(const struct drbd_cmd_ctx *base, int peer_node_id)
{
	struct drbd_cmd_ctx c = *base;
	char *disconnect_args[1];
	int argc = 0;

	disconnect_args[argc++] = (char *)disconnect_cmd.cmd;
	c.context = CTX_PEER_NODE;
	c.nl.ctx_peer_node_id = peer_node_id;
	return _generic_config_cmd(&disconnect_cmd, &c, argc, disconnect_args);
}

/*
 * Parse the addresses of the local and remote node from the command line.
 */
static int drbd8_compat_get_my_node_id(int argc, char **argv,
				       const char **my_addr, const char **peer_addr)
{
	/* parse my_addr */
	*my_addr = argv[optind];
	if (strncmp(*my_addr, "local:", 6) == 0)
		*my_addr += 6;

	optind++;
	if (optind >= argc) {
		return -1;
	}

	/* parse peer_addr */
	*peer_addr = argv[optind];
	if (strncmp(*peer_addr, "peer:", 5) == 0)
		peer_addr += 5;

	return compare_addr(*my_addr, *peer_addr);
}

/*
 * This is a special case for compatibility with the drbd-8-style command line syntax.
 * In drbd 8, the connect command directly took the addresses of the local and remote node. This has changed in drbd 9,
 * where we now need to issue a new-peer and new-path command first.
 *
 * So this function:
 * 1. Arbitrates the node IDs of the local and remote node based on the given IP addresses.
 * 2. Creates a new peer with the remote node ID.
 * 3. Creates a new path to that node with the given addresses.
 * 4. Finally, issues a connect command to that peer.
 */
static int drbd8_compat_connect_special_case(const struct drbd_cmd_ctx *base,
					     const struct drbd_cmd *cmd, int argc, char **argv)
{
	struct drbd_cmd_ctx c = *base;
	const char *my_addr, *peer_addr;
	int my_node_id, peer_node_id;
	int err;

	my_node_id = drbd8_compat_get_my_node_id(argc, argv, &my_addr, &peer_addr);
	if (my_node_id < 0) {
		print_command_usage(cmd, FULL);
		exit(20);
	}
	peer_node_id = my_node_id == 0 ? 1 : 0;

	err = drbd8_compat_fake_new_peer(&c, peer_node_id, argc, argv);
	if (err) {
		fprintf(stderr, "Failed to create peer with id %d: %s\n", peer_node_id, strerror(-err));
		exit(20);
	}

	err = drbd8_compat_fake_new_path(&c, peer_node_id, my_addr, peer_addr);
	if (err) {
		fprintf(stderr, "Failed to create path from '%s' to '%s': %s\n", my_addr, peer_addr, strerror(-err));
		exit(20);
	}

	err = drbd8_compat_fake_connect(&c, peer_node_id);
	if (err) {
		fprintf(stderr, "Failed to connect to peer node %d: %s\n", peer_node_id, strerror(-err));
		exit(20);
	}

	return 0;
}

/*
 * Same as drbd8_compat_connect_special_case, but symmetric for the disconnect command.
 * Delete the peer and path we created in the "connect" special case, then do the disconnect.
 */
static int drbd8_compat_disconnect_special_case(const struct drbd_cmd_ctx *base,
						const struct drbd_cmd *cmd, int argc, char **argv)
{
	struct drbd_cmd_ctx c = *base;
	const char *my_addr, *peer_addr;
	int my_node_id, peer_node_id;
	int err;

	my_node_id = drbd8_compat_get_my_node_id(argc, argv, &my_addr, &peer_addr);
	if (my_node_id < 0) {
		print_command_usage(cmd, FULL);
		exit(20);
	}
	peer_node_id = my_node_id == 0 ? 1 : 0;

	err = drbd8_compat_fake_del_peer(&c, peer_node_id);
	if (err) {
		fprintf(stderr, "Failed to delete peer with id %d: %s\n", peer_node_id, strerror(-err));
		exit(20);
	}

	err = drbd8_compat_fake_del_path(&c, peer_node_id, my_addr, peer_addr);
	if (err) {
		fprintf(stderr, "Failed to delete path from '%s' to '%s': %s\n", my_addr, peer_addr, strerror(-err));
		exit(20);
	}

	err = drbd8_compat_fake_disconnect(&c, peer_node_id);
	if (err) {
		fprintf(stderr, "Failed to disconnect from peer node %d: %s\n", peer_node_id, strerror(-err));
		exit(20);
	}

	return 0;
}

int drbd8_compat_connect_or_disconnect(const struct drbd_cmd_ctx *ctx,
				       const struct drbd_cmd *cmd, int argc, char **argv)
{
	if (strcmp(cmd->cmd, "connect") == 0) {
		/* This is the "connect" command, and parsing the peer node ID failed.
		 * This might be the user trying to use the old drbd-8-style command line syntax,
		 * where the "connect" command took the addresses of the local and remote node directly.
		 * Make another attempt to parse the command line arguments, but this time
		 * interpret them as the drbd-8 style "connect" syntax to try and emulate the old behavior.
		 */
		return drbd8_compat_connect_special_case(ctx, cmd, argc, argv);
	} else if (strcmp(cmd->cmd, "disconnect") == 0) {
		/* Same as above, but for the "disconnect" command */
		return drbd8_compat_disconnect_special_case(ctx, cmd, argc, argv);
	} else
		return -1;
}

static void cctx_key_to_path(const struct drbd_cmd_ctx *ctx, enum c84_ctx_key key,
			     char *path, int p_len, const char *cmd_str)
{
	const char *resname = ctx->objname;
	const char *dd = drbd_run_dir();
	int vol = ctx->nl.ctx_volume;


	if (key == CCTX_MINOR) {
		snprintf(path, p_len, "%s/compat-84/minor-%d/%s-opts", dd, ctx->minor, cmd_str);
	} else if (key == CCTX_RES_VIA_MINOR) {
		snprintf(path, p_len, "%s/compat-84/minor-%d/../%s-opts", dd, ctx->minor, cmd_str);
	} else if (key == CCTX_RESOURCE) {
		snprintf(path, p_len, "%s/compat-84/res-%s/%s-opts", dd, resname, cmd_str);
	} else if (key == CCTX_PEER_DEVICE) {
		snprintf(path, p_len,
			 "%s/compat-84/res-%s/vol-%d/%s-opts", dd, resname, vol, cmd_str);
	} else {
		fprintf(stderr, "invalid value for cctx_key: %d\n", key);
		exit(30);
	}
}

static bool load_opts(const struct drbd_cmd_ctx *ctx, int *argc, char **argv,
		      const struct drbd_cmd *cmd, enum c84_ctx_key key)
{
	ssize_t nread;
	size_t alloced_size;
	char fname[200], *str;
	FILE *f;

	cctx_key_to_path(ctx, key, fname, 200, cmd->cmd);
	f = fopen(fname, "r");
	if (!f)
		return false;
	while (true) {
		str = NULL;
		alloced_size = 0;
		nread = getline(&str, &alloced_size, f);
		if (nread == -1)
			break;
		if (str[nread - 1] == '\n')
			str[nread - 1] = 0;
		argv[(*argc)++] = str;
	}

	fclose(f);
	unlink(fname);
	return true;
}

static void store_opts(const struct drbd_cmd_ctx *ctx, int argc, char **argv,
		       const struct drbd_cmd *cmd, enum c84_ctx_key key)
{
	char *opts[7], fname[100];
	int i, nr_opts = 0;
	FILE *f;

	drbd8_compat_relevant_opts(cmd, argv, argc, opts, &nr_opts);
	if (nr_opts == 0)
		return;

	cctx_key_to_path(ctx, key, fname, 100, cmd->cmd);
	f = fopen(fname, "w");
	if (!f) {
		fprintf(stderr, "Failed to open '%s' for writing with %s (%d)\n",
			fname, strerror(errno), errno);
		exit(20);
	}
	for (i = 0; i < nr_opts; i++)
		fprintf(f, "%s\n", opts[i]);
	fclose(f);
}

/* drbd8_compat_attach()
 * drbd-8.4 had six additional disk options
 * fencing, c-plan-ahead, c-delay-target, c-fill-target, c-max-rate, c-min-rate
 * fencing is moved to new-peer, the other five are moved to peer-device-options
 */
int drbd8_compat_attach(const struct drbd_cmd_ctx *ctx, int argc_in, char **argv_in)
{
	static struct drbd_cmd attach_cmd_no_recursion;
	static bool initialized = false;
	char *attach_args[20];
	int argc = 4; /* placing the options behind the fixed position args */

	if (!initialized) {
		attach_cmd_no_recursion = attach_cmd;
		initialized = true;
	}

	store_opts(ctx, argc_in, argv_in, &peer_device_options_cmd, CCTX_MINOR);
	store_opts(ctx, argc_in, argv_in, &new_peer_cmd, CCTX_RES_VIA_MINOR);

	drbd8_compat_relevant_opts(&attach_cmd, argv_in, argc_in, attach_args, &argc);

	attach_args[0] = (char *)attach_cmd.cmd;
	attach_args[1] = (char *)argv_in[optind + 0]; /* lower_dev */
	attach_args[2] = (char *)argv_in[optind + 1]; /* meta_data_dev */
	attach_args[3] = (char *)argv_in[optind + 2]; /* meta_data_index */

	return _generic_config_cmd(&attach_cmd_no_recursion, ctx, argc, attach_args);
}


static void mkdir_or_exit(const char *path)
{
	struct stat sb;
	int err;

	err = stat(path, &sb);
	if (!err) {
		if (S_ISDIR(sb.st_mode))
			return;
		fprintf(stderr, "'%s' exists and is not a directory\n", path);
		exit(20);
	}
	err = mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	if (err) {
		fprintf(stderr, "Failed to mkdir '%s' with %s (%d)\n",
			path, strerror(errno), errno);
		exit(20);
	}
}

void drbd8_compat_new_minor(const char *resname, const char *minor_str, const char *vol_str)
{
	char path[200], link[50], existing_link_target[50];
	int minor = atoi(minor_str);
	int vol = atoi(vol_str);
	ssize_t r;
	int err;

	snprintf(path, 200, "%s/compat-84", drbd_run_dir());
	mkdir_or_exit(path);

	snprintf(path, 200, "%s/compat-84/res-%s", drbd_run_dir(), resname);
	mkdir_or_exit(path);

	snprintf(path, 200, "%s/compat-84/res-%s/vol-%d", drbd_run_dir(), resname, vol);
	mkdir_or_exit(path);

	snprintf(link, 50, "%s/compat-84/minor-%d", drbd_run_dir(), minor);
	snprintf(path, 200, "res-%s/vol-%d", resname, vol);
	r = readlink(link, existing_link_target, 50);
	if (r > 0) {
		if (strncmp(path, existing_link_target, r) == 0)
			return;
		unlink(link);
	}
	err = symlink(path, link);
	if (err) {
		fprintf(stderr, "Failed to create symlink '%s' with %s (%d)\n",
			link, strerror(errno), errno);
		exit(20);
	}
}
