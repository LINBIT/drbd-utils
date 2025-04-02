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

#include "drbdsetup.h"

/*
 * Compare two sockaddr_storage structures.
 * Returns -1 if a_sockaddr < b_sockaddr, 0 if equal, 1 if a_sockaddr > b_sockaddr.
 * Compares the address family first, then the address, and finally the port.
 */
static int compare_sockaddr(struct sockaddr_storage *a_sockaddr,
							struct sockaddr_storage *b_sockaddr)
{
	if (a_sockaddr->ss_family < b_sockaddr->ss_family)
		return -1;
	if (a_sockaddr->ss_family > b_sockaddr->ss_family)
		return 1;
	if (a_sockaddr->ss_family == AF_INET) {
		struct sockaddr_in *a4_sockaddr = (struct sockaddr_in *)a_sockaddr;
		struct sockaddr_in *b4_sockaddr = (struct sockaddr_in *)b_sockaddr;
		int cmp = memcmp(&a4_sockaddr->sin_addr, &b4_sockaddr->sin_addr, sizeof(struct in_addr));
		if (cmp)
			return cmp;
		if (a4_sockaddr->sin_port < b4_sockaddr->sin_port)
			return -1;
		if (a4_sockaddr->sin_port > b4_sockaddr->sin_port)
			return 1;
		return 0;
	} else if (a_sockaddr->ss_family == AF_INET6) {
		struct sockaddr_in6 *a6_sockaddr = (struct sockaddr_in6 *)a_sockaddr;
		struct sockaddr_in6 *b6_sockaddr = (struct sockaddr_in6 *)b_sockaddr;
		int cmp = memcmp(&a6_sockaddr->sin6_addr, &b6_sockaddr->sin6_addr, sizeof(struct in6_addr));
		if (!cmp)
			return cmp;
		if (a6_sockaddr->sin6_port < b6_sockaddr->sin6_port)
			return -1;
		if (a6_sockaddr->sin6_port > b6_sockaddr->sin6_port)
			return 1;
		return 0;
	}

	exit(20);
}

static void drbd8_compat_relevant_opts(const struct drbd_cmd *cm, char **argv_in,
				       int argc_in, char **args_out, int *argc)
{
	struct option *options;
	char *arg;
	int c;

	options = make_longoptions(cm);
	optind = 0;
	for (;;) {
		int idx;

		c = getopt_long(argc_in, argv_in, "(", options, &idx);
		if (c == -1)
			break;
		if (c >= 1000 || c == 0) {
			arg = argv_in[optind];
			args_out[(*argc)++] = arg;
			if (optarg)
				args_out[(*argc)++] = optarg;
		}
	}
}

static int drbd8_compat_fake_new_peer(int peer_node_id, int argc_in, char **argv_in)
{
	char *new_peer_args[40];
	int argc = 0;

	new_peer_args[argc++] = (char *)new_peer_cmd.cmd;
	new_peer_args[argc++] = "--_name=remote";

	drbd8_compat_relevant_opts(&new_peer_cmd, argv_in, argc_in,
				   new_peer_args, &argc);
	context = CTX_PEER_NODE;
	global_ctx.ctx_peer_node_id = peer_node_id;
	return _generic_config_cmd(&new_peer_cmd, argc, new_peer_args);
}

static int drbd8_compat_fake_new_path(int peer_node_id, const char *my_addr,
				      const char *peer_addr)
{
	char *new_path_args[3];
	int argc = 0;

	new_path_args[argc++] = (char *)new_path_cmd.cmd;
	new_path_args[argc++] = (char *)my_addr;
	new_path_args[argc++] = (char *)peer_addr;
	context = CTX_PEER_NODE;
	global_ctx.ctx_peer_node_id = peer_node_id;
	return _generic_config_cmd(&new_path_cmd, argc, new_path_args);
}

static int drbd8_compat_fake_connect(int peer_node_id)
{
	char *connect_args[1];
	int argc = 0;

	connect_args[argc++] = (char *)connect_cmd.cmd;
	context = CTX_PEER_NODE;
	global_ctx.ctx_peer_node_id = peer_node_id;
	return _generic_config_cmd(&connect_cmd, argc, connect_args);
}

static int drbd8_compat_fake_del_peer(int peer_node_id)
{
	char *del_peer_args[1];
	int argc = 0;

	del_peer_args[argc++] = "del-peer";
	context = CTX_PEER_NODE;
	global_ctx.ctx_peer_node_id = peer_node_id;
	return _generic_config_cmd(&del_peer_cmd, argc, del_peer_args);
}

static int drbd8_compat_fake_del_path(int peer_node_id, const char *my_addr,
				      const char *peer_addr)
{
	char *del_path_args[3];
	int argc = 0;

	del_path_args[argc++] = (char *)del_path_cmd.cmd;
	del_path_args[argc++] = (char *)my_addr;
	del_path_args[argc++] = (char *)peer_addr;
	context = CTX_PEER_NODE;
	global_ctx.ctx_peer_node_id = peer_node_id;
	return _generic_config_cmd(&del_path_cmd, argc, del_path_args);
}

static int drbd8_compat_fake_disconnect(int peer_node_id)
{
	char *disconnect_args[1];
	int argc = 0;

	disconnect_args[argc++] = (char *)disconnect_cmd.cmd;
	context = CTX_PEER_NODE;
	global_ctx.ctx_peer_node_id = peer_node_id;
	return _generic_config_cmd(&disconnect_cmd, argc, disconnect_args);
}

/*
 * Parse the addresses of the local and remote node from the command line.
 */
static int drbd8_compat_get_my_node_id(int argc, char **argv, const char **my_addr, const char **peer_addr)
{
	struct sockaddr_storage *x;

	/* parse my_addr */
	*my_addr = argv[optind];
	if (strncmp(*my_addr, "local:", 6) == 0)
		*my_addr += 6;
	assert(sizeof(global_ctx.ctx_my_addr) >= sizeof(*x));
	x = (struct sockaddr_storage *)&global_ctx.ctx_my_addr;
	global_ctx.ctx_my_addr_len = sockaddr_from_str(x, *my_addr);

	optind++;
	if (optind >= argc) {
		return -1;
	}

	/* parse peer_addr */
	*peer_addr = argv[optind];
	if (strncmp(*peer_addr, "peer:", 5) == 0)
		peer_addr += 5;
	assert(sizeof(global_ctx.ctx_peer_addr) >= sizeof(*x));
	x = (struct sockaddr_storage *) &global_ctx.ctx_peer_addr;
	global_ctx.ctx_peer_addr_len = sockaddr_from_str(x, *peer_addr);

	/* compare */
	int cmp = compare_sockaddr((struct sockaddr_storage *) &global_ctx.ctx_my_addr,
							   (struct sockaddr_storage *) &global_ctx.ctx_peer_addr);
	assert(cmp != 0); /* addresses cannot be equal */

	/* the lower address gets ID 0 */
	return cmp == -1 ? 0 : 1;
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
static int drbd8_compat_connect_special_case(int argc, char **argv, const struct drbd_cmd *cmd)
{
	const char *my_addr, *peer_addr;
	int my_node_id, peer_node_id;
	int err;

	my_node_id = drbd8_compat_get_my_node_id(argc, argv, &my_addr, &peer_addr);
	if (my_node_id < 0) {
		print_command_usage(cmd, FULL);
		exit(20);
	}
	peer_node_id = my_node_id == 0 ? 1 : 0;

	err = drbd8_compat_fake_new_peer(peer_node_id, argc, argv);
	if (err) {
		fprintf(stderr, "Failed to create peer with id %d: %s\n", peer_node_id, strerror(-err));
		exit(20);
	}

	err = drbd8_compat_fake_new_path(peer_node_id, my_addr, peer_addr);
	if (err) {
		fprintf(stderr, "Failed to create path from '%s' to '%s': %s\n", my_addr, peer_addr, strerror(-err));
		exit(20);
	}

	err = drbd8_compat_fake_connect(peer_node_id);
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
static int drbd8_compat_disconnect_special_case(int argc, char **argv, const struct drbd_cmd *cmd)
{
	const char *my_addr, *peer_addr;
	int my_node_id, peer_node_id;
	int err;

	my_node_id = drbd8_compat_get_my_node_id(argc, argv, &my_addr, &peer_addr);
	if (my_node_id < 0) {
		print_command_usage(cmd, FULL);
		exit(20);
	}
	peer_node_id = my_node_id == 0 ? 1 : 0;

	err = drbd8_compat_fake_del_peer(peer_node_id);
	if (err) {
		fprintf(stderr, "Failed to delete peer with id %d: %s\n", peer_node_id, strerror(-err));
		exit(20);
	}

	err = drbd8_compat_fake_del_path(peer_node_id, my_addr, peer_addr);
	if (err) {
		fprintf(stderr, "Failed to delete path from '%s' to '%s': %s\n", my_addr, peer_addr, strerror(-err));
		exit(20);
	}

	err = drbd8_compat_fake_disconnect(peer_node_id);
	if (err) {
		fprintf(stderr, "Failed to disconnect from peer node %d: %s\n", peer_node_id, strerror(-err));
		exit(20);
	}

	return 0;
}

int drbd8_compat_connect_or_disconnect(int argc, char **argv, const struct drbd_cmd *cmd)
{
	if (strcmp(cmd->cmd, "connect") == 0) {
		/* This is the "connect" command, and parsing the peer node ID failed.
		 * This might be the user trying to use the old drbd-8-style command line syntax,
		 * where the "connect" command took the addresses of the local and remote node directly.
		 * Make another attempt to parse the command line arguments, but this time
		 * interpret them as the drbd-8 style "connect" syntax to try and emulate the old behavior.
		 */
		return drbd8_compat_connect_special_case(argc, argv, cmd);
	} else if (strcmp(cmd->cmd, "disconnect") == 0) {
		/* Same as above, but for the "disconnect" command */
		return drbd8_compat_disconnect_special_case(argc, argv, cmd);
	} else
		return -1;
}
