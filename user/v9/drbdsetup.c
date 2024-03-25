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

#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <assert.h>
#include <stdarg.h>
#include <libgen.h>
#include <time.h>
#include <syslog.h>
#include <math.h> /* for NAN */

#include <linux/netlink.h>
#include <linux/genetlink.h>

#include "drbdsetup.h"

/*
 * We are not using libnl,
 * using its API for the few things we want to do
 * ends up being almost as much lines of code as
 * coding the necessary bits right here.
 */
#include "libgenl.h"
#include "drbd_nla.h"
#include <linux/drbd_config.h>
#include <linux/drbd_genl_api.h>
#include <linux/drbd_limits.h>
#include "drbdtool_common.h"
#include <linux/genl_magic_func.h>
#include "drbd_strings.h"
#include "registry.h"
#include "config.h"
#include "config_flags.h"
#include "wrap_printf.h"
#include "drbdsetup_colors.h"

#define EXIT_NOMEM 20
#define EXIT_NO_FAMILY 20
#define EXIT_SEND_ERR 20
#define EXIT_RECV_ERR 20
#define EXIT_TIMED_OUT 20
#define EXIT_NOSOCK 30
#define EXIT_THINKO 42

char *progname;

#ifndef AF_INET_SDP
#define AF_INET_SDP 27
#define PF_INET_SDP AF_INET_SDP
#endif

#define MULTIPLE_TIMEOUTS (-2)

/* pretty print helpers */
static int indent = 0;
#define INDENT_WIDTH	4
#define printI(fmt, args... ) printf("%*s" fmt,INDENT_WIDTH * indent,"" , ## args )
#define QUOTED(str) "\"" str "\""
static bool json_output = false;
#define pJ(fmt, args...) do { if (json_output) printI(fmt, ##args); } while(0)
#define pD(fmt, args...) do { if (!json_output) printI(fmt, ##args); } while(0)

#define PTR_NONNULL_OR_EXIT(p) do { \
	if ((p) == NULL) { \
		fprintf(stderr, "%s: pointer NULL\n", __func__); \
		exit(20); \
	} \
} while(0)

#define PTR_NONNULL_OR_EXIT2(p) do { PTR_NONNULL_OR_EXIT(p); PTR_NONNULL_OR_EXIT(*p); } while(0)

enum usage_type {
	BRIEF,
	FULL,
	XML,
};


enum cfg_ctx_key ctx_next_arg(enum cfg_ctx_key *key)
{
	enum cfg_ctx_key next_arg;

	if (*key & CTX_MULTIPLE_ARGUMENTS) {
		next_arg = *key & ~(*key - 1);  /* the lowest set bit */
		next_arg &= ~CTX_MULTIPLE_ARGUMENTS;
	} else
		next_arg = *key;

	*key &= ~next_arg;
	return next_arg;
}

const char *ctx_arg_string(enum cfg_ctx_key key, enum usage_type ut)
{
	bool xml = (ut == XML);

	switch(key) {
	case CTX_RESOURCE:
		return xml ? "resource" : "{resource}";
	case CTX_PEER_NODE_ID:
		return xml ? "peer_node_id" : "{peer_node_id}";
	case CTX_MINOR:
		return xml ? "minor" : "{minor}";
	case CTX_VOLUME:
		return xml ? "volume" : "{volume}";
	case CTX_MY_ADDR:
		return xml ? "local_addr" : "[local:][{af}:]{local_addr}[:{port}]";
	case CTX_PEER_ADDR:
		return xml ? "remote_addr" : "[peer:][{af}:]{remote_addr}[:{port}]";
	case CTX_ALL:
		return "all";
	default:
		assert(0);
	}

	return "unknown argument";
}

// other functions
static int get_af_ssocks(int warn);
static char *af_to_str(int af);
static void print_command_usage(struct drbd_cmd *cm, enum usage_type);
static void print_usage_and_exit(const char *addinfo)
		__attribute__ ((noreturn));

// command functions
static int generic_config_cmd(struct drbd_cmd *cm, int argc, char **argv);
static int down_cmd(struct drbd_cmd *cm, int argc, char **argv);
static int generic_events_cmd(struct drbd_cmd *cm, int argc, char **argv);
static int del_minor_cmd(struct drbd_cmd *cm, int argc, char **argv);
static int del_resource_cmd(struct drbd_cmd *cm, int argc, char **argv);
static int show_cmd(struct drbd_cmd *cm, int argc, char **argv);
static int status_cmd(struct drbd_cmd *cm, int argc, char **argv);
static int role_cmd(struct drbd_cmd *cm, int argc, char **argv);
static int peer_role_cmd(struct drbd_cmd *cm, int argc, char **argv);
static int cstate_cmd(struct drbd_cmd *cm, int argc, char **argv);
static int dstate_cmd(struct drbd_cmd *cm, int argc, char **argv);
static int check_resize_cmd(struct drbd_cmd *cm, int argc, char **argv);
static int show_or_get_gi_cmd(struct drbd_cmd *cm, int argc, char **argv);
static int udev_cmd(struct drbd_cmd *cm, int argc, char **argv);

// sub commands for generic_get_cmd
       int print_event(struct drbd_cmd *, struct genl_info *, void *); /* is in drbdsetup_events2.c */
       void events2_prepare_update(); /* is in drbdsetup_events2.c */
       void events2_reset(); /* is in drbdsetup_events2.c */
static int wait_for_family(struct drbd_cmd *, struct genl_info *, void *);
static int remember_resource(struct drbd_cmd *, struct genl_info *, void *);
static int remember_device(struct drbd_cmd *, struct genl_info *, void *);
static int remember_connection(struct drbd_cmd *, struct genl_info *, void *);
static int remember_peer_device(struct drbd_cmd *, struct genl_info *, void *);


// convert functions for arguments
static int conv_md_idx(struct drbd_argument *ad, struct msg_buff *msg, struct drbd_genlmsghdr *dhdr, char* arg);
static int conv_u32(struct drbd_argument *, struct msg_buff *, struct drbd_genlmsghdr *, char *);
static int conv_addr(struct drbd_argument *ad, struct msg_buff *msg, struct drbd_genlmsghdr *dhdr, char* arg);
static int conv_str(struct drbd_argument *ad, struct msg_buff *msg, struct drbd_genlmsghdr *dhdr, char* arg);

static struct resources_list *list_resources(void);
static struct resources_list *sort_resources(struct resources_list *);
static struct devices_list *list_devices(char *);
static struct connections_list *sort_connections(struct connections_list *);
static struct connections_list *list_connections(char *);
static struct peer_devices_list *list_peer_devices(char *);
static struct paths_list *list_paths(char *);

struct option wait_cmds_options[] = {
	{ "wfc-timeout", required_argument, 0, 't' },
	{ "degr-wfc-timeout", required_argument, 0, 'd'},
	{ "outdated-wfc-timeout", required_argument, 0, 'o'},
	{ "wait-after-sb", optional_argument, 0, 'w'},
	{ }
};

struct option events_cmd_options[] = {
	{ "timestamps", no_argument, 0, 'T' },
	{ "statistics", no_argument, 0, 's' },
	{ "now", no_argument, 0, 'n' },
	{ "poll", no_argument, 0, 'p' },
	{ "full", no_argument, 0, 'f' },
	{ "color", optional_argument, 0, 'c' },
	{ "diff", optional_argument, 0, 'i' },
	{ }
};

struct option show_cmd_options[] = {
	{ "show-defaults", no_argument, 0, 'D' },
	{ "json", no_argument, 0, 'j' },
	{ }
};

static struct option status_cmd_options[] = {
	{ "verbose", no_argument, 0, 'v' },
	{ "statistics", no_argument, 0, 's' },
	{ "color", optional_argument, 0, 'c' },
	{ "json", no_argument, 0, 'j' },
	{ }
};

#define F_CONFIG_CMD	generic_config_cmd
#define NO_PAYLOAD	0
#define F_NEW_EVENTS_CMD(scmd)	DRBD_ADM_GET_INITIAL_STATE, NO_PAYLOAD, generic_events_cmd, \
			.handle_reply = scmd

struct drbd_cmd commands[] = {
	{"primary", CTX_RESOURCE, DRBD_ADM_PRIMARY, DRBD_NLA_SET_ROLE_PARMS,
		F_CONFIG_CMD,
	 .ctx = &primary_cmd_ctx,
	 .summary = "Change the role of a node in a resource to primary." },

	{"secondary", CTX_RESOURCE, DRBD_ADM_SECONDARY, DRBD_NLA_SET_ROLE_PARMS,
		F_CONFIG_CMD,
	 .ctx = &secondary_cmd_ctx,
	 .summary = "Change the role of a node in a resource to secondary." },

	{"attach", CTX_MINOR, DRBD_ADM_ATTACH, DRBD_NLA_DISK_CONF,
		F_CONFIG_CMD,
	 .drbd_args = (struct drbd_argument[]) {
		 { "lower_dev",		T_backing_dev,	conv_block_dev },
		 { "meta_data_dev",	T_meta_dev,	conv_block_dev },
		 { "meta_data_index",	T_meta_dev_idx,	conv_md_idx },
		 { } },
	 .ctx = &attach_cmd_ctx,
	 .summary = "Attach a lower-level device to an existing replicated device." },

	{"disk-options", CTX_MINOR, DRBD_ADM_CHG_DISK_OPTS, DRBD_NLA_DISK_CONF,
		F_CONFIG_CMD,
	 .set_defaults = true,
	 .ctx = &disk_options_ctx,
	 .summary = "Change the disk options of an attached lower-level device." },

	{"detach", CTX_MINOR, DRBD_ADM_DETACH, DRBD_NLA_DETACH_PARMS, F_CONFIG_CMD,
	 .ctx = &detach_cmd_ctx,
	 .summary = "Detach the lower-level device of a replicated device." },

	{"connect", CTX_PEER_NODE,
		DRBD_ADM_CONNECT, DRBD_NLA_CONNECT_PARMS,
		F_CONFIG_CMD,
	 .ctx = &connect_cmd_ctx,
	 .summary = "Attempt to (re)establish a replication link to a peer host." },

	{"new-peer", CTX_PEER_NODE,
		DRBD_ADM_NEW_PEER, DRBD_NLA_NET_CONF,
		F_CONFIG_CMD,
	 .ctx = &new_peer_cmd_ctx,
	 .summary = "Make a peer host known to a resource." },

	{"del-peer", CTX_PEER_NODE,
		DRBD_ADM_DEL_PEER, DRBD_NLA_DISCONNECT_PARMS,
		F_CONFIG_CMD,
	 .ctx = &disconnect_cmd_ctx,
	 .summary = "Remove a connection to a peer host." },

	{"new-path", CTX_PEER_NODE,
		DRBD_ADM_NEW_PATH, DRBD_NLA_PATH_PARMS,
		F_CONFIG_CMD,
	 .drbd_args = (struct drbd_argument[]) {
		{ "local-addr", T_my_addr, conv_addr },
		{ "remote-addr", T_peer_addr, conv_addr },
		{ } },
	 .ctx = &path_cmd_ctx,
	 .summary = "Add a path (endpoint address pair) where a peer host should be reachable." },

	{"del-path", CTX_PEER_NODE,
		DRBD_ADM_DEL_PATH, DRBD_NLA_PATH_PARMS,
		F_CONFIG_CMD,
	 .drbd_args = (struct drbd_argument[]) {
		{ "local-addr", T_my_addr, conv_addr },
		{ "remote-addr", T_peer_addr, conv_addr },
		{ } },
	 .ctx = &path_cmd_ctx,
	 .summary = "Remove a path (endpoint address pair) from a connection to a peer host." },

	{"net-options", CTX_PEER_NODE, DRBD_ADM_CHG_NET_OPTS, DRBD_NLA_NET_CONF,
		F_CONFIG_CMD,
	 .set_defaults = true,
	 .ctx = &net_options_ctx,
	 .summary = "Change the network options of a connection." },

	{"disconnect", CTX_PEER_NODE, DRBD_ADM_DISCONNECT, DRBD_NLA_DISCONNECT_PARMS,
		F_CONFIG_CMD,
	 .ctx = &disconnect_cmd_ctx,
	 .summary = "Unconnect from a peer host." },

	{"resize", CTX_MINOR, DRBD_ADM_RESIZE, DRBD_NLA_RESIZE_PARMS,
		F_CONFIG_CMD,
	 .ctx = &resize_cmd_ctx,
	 .summary = "Reexamine the lower-level device sizes to resize a replicated device." },

	{"resource-options", CTX_RESOURCE, DRBD_ADM_RESOURCE_OPTS, DRBD_NLA_RESOURCE_OPTS,
		F_CONFIG_CMD,
	 .set_defaults = true,
	 .ctx = &resource_options_ctx,
	 .summary = "Change the resource options of an existing resource." },

	{"peer-device-options", CTX_PEER_DEVICE, DRBD_ADM_CHG_PEER_DEVICE_OPTS,
		DRBD_NLA_PEER_DEVICE_OPTS, F_CONFIG_CMD,
	 .set_defaults = true,
	 .ctx = &peer_device_options_ctx,
	 .summary = "Change peer-device options." },

	{"new-current-uuid", CTX_MINOR, DRBD_ADM_NEW_C_UUID, DRBD_NLA_NEW_C_UUID_PARMS,
		F_CONFIG_CMD,
	 .ctx = &new_current_uuid_cmd_ctx,
	 .summary = "Generate a new current UUID." },

	{"invalidate", CTX_MINOR, DRBD_ADM_INVALIDATE, DRBD_NLA_INVALIDATE_PARMS, F_CONFIG_CMD,
	 .ctx = &invalidate_ctx,
	 .summary = "Replace the local data of a volume with that of a peer." },
	{"invalidate-remote", CTX_PEER_DEVICE, DRBD_ADM_INVAL_PEER, DRBD_NLA_INVAL_PEER_PARAMS, F_CONFIG_CMD,
	 .ctx = &invalidate_peer_ctx,
	 .summary = "Replace a peer's data of a volume with the local data." },
	{"pause-sync", CTX_PEER_DEVICE, DRBD_ADM_PAUSE_SYNC, NO_PAYLOAD, F_CONFIG_CMD,
	 .summary = "Stop resynchronizing between a local and a peer device." },
	{"resume-sync", CTX_PEER_DEVICE, DRBD_ADM_RESUME_SYNC, NO_PAYLOAD, F_CONFIG_CMD,
	 .summary = "Allow resynchronization to resume on a replicated device." },
	{"suspend-io", CTX_MINOR, DRBD_ADM_SUSPEND_IO, NO_PAYLOAD, F_CONFIG_CMD,
	 .summary = "Suspend I/O on a replicated device." },
	{"resume-io", CTX_MINOR, DRBD_ADM_RESUME_IO, NO_PAYLOAD, F_CONFIG_CMD,
	 .summary = "Resume I/O on a replicated device." },
	{"outdate", CTX_MINOR, DRBD_ADM_OUTDATE, NO_PAYLOAD, F_CONFIG_CMD,
	 .summary = "Mark the data on a lower-level device as outdated." },
	{"verify", CTX_PEER_DEVICE, DRBD_ADM_START_OV, DRBD_NLA_START_OV_PARMS, F_CONFIG_CMD,
	 .ctx = &verify_cmd_ctx,
	 .summary = "Verify the data on a lower-level device against a peer device." },
	{"down", CTX_RESOURCE | CTX_ALL, DRBD_ADM_DOWN, NO_PAYLOAD, down_cmd,
	 .missing_ok = true,
	 .warn_on_missing = true,
	 .summary = "Take a resource down." },
	{"role", CTX_RESOURCE, 0, NO_PAYLOAD, role_cmd,
	 .lockless = true,
	 .summary = "Show the current role of a resource." },
	{"peer-role", CTX_PEER_NODE, 0, NO_PAYLOAD, peer_role_cmd,
	 .lockless = true,
	 .summary = "Show the current role of a peer." },
	{"cstate", CTX_PEER_NODE, 0, NO_PAYLOAD, cstate_cmd,
	 .lockless = true,
	 .summary = "Show the current state of a connection." },
	{"dstate", CTX_MINOR, 0, NO_PAYLOAD, dstate_cmd,
	 .lockless = true,
	 .summary = "Show the current disk state of a lower-level device." },
	{"show-gi", CTX_PEER_DEVICE, 0, NO_PAYLOAD, show_or_get_gi_cmd,
	 .lockless = true,
	 .summary = "Show the data generation identifiers for a device on a particular connection, with explanations." },
	{"get-gi", CTX_PEER_DEVICE, 0, NO_PAYLOAD, show_or_get_gi_cmd,
	 .lockless = true,
	 .summary = "Show the data generation identifiers for a device on a particular connection." },
	{"show", CTX_RESOURCE | CTX_ALL, 0, 0, show_cmd,
	 .options = show_cmd_options,
	 .lockless = true,
	 .summary = "Show the current configuration of a resource, or of all resources." },
	{"status", CTX_RESOURCE | CTX_ALL, 0, 0, status_cmd,
	 .options = status_cmd_options,
	 .lockless = true,
	 .summary = "Show the state of a resource, or of all resources." },
	{"check-resize", CTX_MINOR, 0, NO_PAYLOAD, check_resize_cmd,
	 .lockless = true,
	 .summary = "Remember the current size of a lower-level device." },
	{"events2", CTX_RESOURCE | CTX_ALL, F_NEW_EVENTS_CMD(print_event),
	 .options = events_cmd_options,
	 .missing_ok = true,
	 .continuous_poll = true,
	 .lockless = true,
	 .summary = "Show the current state and all state changes of a resource, or of all resources." },
	{"wait-sync-volume", CTX_PEER_DEVICE, F_NEW_EVENTS_CMD(wait_for_family),
	 .options = wait_cmds_options,
	 .continuous_poll = true,
	 .lockless = true,
	 .summary = "Wait until resync finished on a volume." },
	{"wait-sync-connection", CTX_PEER_NODE, F_NEW_EVENTS_CMD(wait_for_family),
	 .options = wait_cmds_options,
	 .continuous_poll = true,
	 .lockless = true,
	 .summary = "Wait until resync finished on all volumes of a connection." },
	{"wait-sync-resource", CTX_RESOURCE, F_NEW_EVENTS_CMD(wait_for_family),
	 .options = wait_cmds_options,
	 .continuous_poll = true,
	 .lockless = true,
	 .summary = "Wait until resync finished on all volumes." },
	{"wait-connect-volume", CTX_PEER_DEVICE, F_NEW_EVENTS_CMD(wait_for_family),
	 .options = wait_cmds_options,
	 .continuous_poll = true,
	 .lockless = true,
	 .summary = "Wait until a device on a peer is visible." },
	{"wait-connect-connection", CTX_PEER_NODE, F_NEW_EVENTS_CMD(wait_for_family),
	 .options = wait_cmds_options,
	 .continuous_poll = true,
	 .lockless = true,
	 .summary = "Wait until all peer volumes of connection are visible." },
	{"wait-connect-resource", CTX_RESOURCE, F_NEW_EVENTS_CMD(wait_for_family),
	 .options = wait_cmds_options,
	 .continuous_poll = true,
	 .lockless = true,
	 .summary = "Wait until all connections are establised." },

	{"new-resource", CTX_RESOURCE, DRBD_ADM_NEW_RESOURCE, DRBD_NLA_RESOURCE_OPTS, F_CONFIG_CMD,
	 .drbd_args = (struct drbd_argument[]) {
		 { "node_id",		T_node_id,	conv_u32 },
		 { } },
	 .ctx = &resource_options_ctx,
	 .summary = "Create a new resource." },

	{"new-minor", CTX_RESOURCE | CTX_MINOR | CTX_VOLUME | CTX_MULTIPLE_ARGUMENTS,
		DRBD_ADM_NEW_MINOR, DRBD_NLA_DEVICE_CONF,
		F_CONFIG_CMD,
	 .ctx = &device_options_ctx,
	 .summary = "Create a new replicated device within a resource." },

	{"del-minor", CTX_MINOR, DRBD_ADM_DEL_MINOR, NO_PAYLOAD, del_minor_cmd,
	 .summary = "Remove a replicated device." },
	{"del-resource", CTX_RESOURCE, DRBD_ADM_DEL_RESOURCE, NO_PAYLOAD, del_resource_cmd,
	 .summary = "Remove a resource." },
	{"forget-peer", CTX_RESOURCE, DRBD_ADM_FORGET_PEER, DRBD_NLA_FORGET_PEER_PARMS, F_CONFIG_CMD,
	 .drbd_args = (struct drbd_argument[]) {
		 { "peer_node_id",	T_forget_peer_node_id,	conv_u32 },
		 { } },
	 .summary = "Completely remove any reference to a unconnected peer from meta-data." },
	{"rename-resource", CTX_RESOURCE, DRBD_ADM_RENAME_RESOURCE, DRBD_NLA_RENAME_RESOURCE_PARMS, F_CONFIG_CMD,
	.drbd_args = (struct drbd_argument[]) {
		{ "new_name", T_new_resource_name, conv_str },
		{ } },
	.summary = "Rename a resource." },
	{"udev", CTX_MINOR, 0, NO_PAYLOAD, udev_cmd,
	.lockless = true,
	.summary = "Generate output for udev rules."},
};

bool show_defaults;
bool wait_after_split_brain;

#define EM(C) [ C - ERR_CODE_BASE ]

/* The EM(123) are used for old error messages. */
static const char *error_messages[] = {
	EM(NO_ERROR) = "No further Information available.",
	EM(ERR_LOCAL_ADDR) = "Local address(port) already in use.",
	EM(ERR_PEER_ADDR) = "Remote address(port) already in use.",
	EM(ERR_OPEN_DISK) = "Can not open backing device.",
	EM(ERR_OPEN_MD_DISK) = "Can not open meta device.",
	EM(106) = "Lower device already in use.",
	EM(ERR_DISK_NOT_BDEV) = "Lower device is not a block device.",
	EM(ERR_MD_NOT_BDEV) = "Meta device is not a block device.",
	EM(109) = "Open of lower device failed.",
	EM(110) = "Open of meta device failed.",
	EM(ERR_DISK_TOO_SMALL) = "Low.dev. smaller than requested DRBD-dev. size.",
	EM(ERR_MD_DISK_TOO_SMALL) = "Meta device too small.",
	EM(113) = "You have to use the disk command first.",
	EM(ERR_BDCLAIM_DISK) = "Lower device is already claimed. This usually means it is mounted.",
	EM(ERR_BDCLAIM_MD_DISK) = "Meta device is already claimed. This usually means it is mounted.",
	EM(ERR_MD_IDX_INVALID) = "Lower device / meta device / index combination invalid.",
	EM(117) = "Currently we only support devices up to 3.998TB.\n"
	"(up to 2TB in case you do not have CONFIG_LBD set)\n"
	"Contact office@linbit.com, if you need more.",
	EM(ERR_IO_MD_DISK) = "IO error(s) occurred during initial access to meta-data.\n",
	EM(ERR_MD_UNCLEAN) = "Unclean meta-data found.\nYou need to 'drbdadm apply-al res'\n",
	EM(ERR_MD_INVALID) = "No valid meta-data signature found.\n\n"
	"\t==> Use 'drbdadm create-md res' to initialize meta-data area. <==\n",
	EM(ERR_AUTH_ALG) = "The 'cram-hmac-alg' you specified is not known in "
	"the kernel. (Maybe you need to modprobe it, or modprobe hmac?)",
	EM(ERR_AUTH_ALG_ND) = "The 'cram-hmac-alg' you specified is not a digest.",
	EM(ERR_NOMEM) = "kmalloc() failed. Out of memory?",
	EM(ERR_DISCARD_IMPOSSIBLE) = "--discard-my-data not allowed when primary.",
	EM(ERR_DISK_CONFIGURED) = "Device is attached to a disk (use detach first)",
	EM(ERR_NET_CONFIGURED) = "Device has a net-config (use disconnect first)",
	EM(ERR_MANDATORY_TAG) = "UnknownMandatoryTag",
	EM(ERR_MINOR_INVALID) = "Device minor not allocated",
	EM(128) = "Resulting device state would be invalid",
	EM(ERR_INTR) = "Interrupted by Signal",
	EM(ERR_RESIZE_RESYNC) = "Resize not allowed during resync.",
	EM(ERR_NO_PRIMARY) = "Need one Primary node to resize.",
	EM(ERR_RESYNC_AFTER) = "The resync-after minor number is invalid",
	EM(ERR_RESYNC_AFTER_CYCLE) = "This would cause a resync-after dependency cycle",
	EM(ERR_PAUSE_IS_SET) = "Sync-pause flag is already set",
	EM(ERR_PAUSE_IS_CLEAR) = "Sync-pause flag is already cleared",
	EM(136) = "Disk state is lower than outdated",
	EM(ERR_PACKET_NR) = "Kernel does not know how to handle your request.\n"
	"Maybe API_VERSION mismatch?",
	EM(ERR_NO_DISK) = "Device does not have a disk-config",
	EM(ERR_NOT_PROTO_C) = "Protocol C required",
	EM(ERR_NOMEM_BITMAP) = "vmalloc() failed. Out of memory?",
	EM(ERR_INTEGRITY_ALG) = "The 'data-integrity-alg' you specified is not known in "
	"the kernel. (Maybe you need to modprobe it, or modprobe hmac?)",
	EM(ERR_INTEGRITY_ALG_ND) = "The 'data-integrity-alg' you specified is not a digest.",
	EM(ERR_CPU_MASK_PARSE) = "Invalid cpu-mask.",
	EM(ERR_VERIFY_ALG) = "VERIFYAlgNotAvail",
	EM(ERR_VERIFY_ALG_ND) = "VERIFYAlgNotDigest",
	EM(ERR_VERIFY_RUNNING) = "Can not change verify-alg while online verify runs",
	EM(ERR_DATA_NOT_CURRENT) = "Can only attach to the data we lost last (see kernel log).",
	EM(ERR_CONNECTED) = "Need to be StandAlone",
	EM(ERR_CSUMS_ALG) = "CSUMSAlgNotAvail",
	EM(ERR_CSUMS_ALG_ND) = "CSUMSAlgNotDigest",
	EM(ERR_CSUMS_RESYNC_RUNNING) = "Can not change csums-alg while resync is in progress",
	EM(ERR_PERM) = "Permission denied. CAP_SYS_ADMIN necessary",
	EM(ERR_NEED_APV_93) = "Protocol version 93 required to use --assume-clean",
	EM(ERR_STONITH_AND_PROT_A) = "Fencing policy resource-and-stonith only with prot B or C allowed",
	EM(ERR_CONG_NOT_PROTO_A) = "on-congestion policy pull-ahead only with prot A allowed",
	EM(ERR_PIC_AFTER_DEP) = "Sync-pause flag is already cleared.\n"
	"Note: Resync pause caused by a local resync-after dependency.",
	EM(ERR_PIC_PEER_DEP) = "Sync-pause flag is already cleared.\n"
	"Note: Resync pause caused by the peer node.",
	EM(ERR_RES_NOT_KNOWN) = "Unknown resource",
	EM(ERR_RES_IN_USE) = "Resource still in use (delete all minors first)",
	EM(ERR_MINOR_CONFIGURED) = "Minor still configured (down it first)",
	EM(ERR_MINOR_OR_VOLUME_EXISTS) = "Minor or volume exists already (delete it first)",
	EM(ERR_INVALID_REQUEST) = "Invalid configuration request",
	EM(ERR_NEED_APV_100) = "Prot version 100 required in order to change\n"
	"these network options while connected",
	EM(ERR_NEED_ALLOW_TWO_PRI) = "Can not clear allow_two_primaries as long as\n"
	"there a primaries on both sides",
	EM(ERR_MD_LAYOUT_CONNECTED) = "DRBD need to be connected for online MD layout change\n",
	EM(ERR_MD_LAYOUT_TOO_BIG) = "Resulting AL area too big\n",
	EM(ERR_MD_LAYOUT_TOO_SMALL) = "Resulting AL are too small\n",
	EM(ERR_MD_LAYOUT_NO_FIT) = "Resulting AL does not fit into available meta data space\n",
	EM(ERR_IMPLICIT_SHRINK) = "Implicit device shrinking not allowed. See kernel log.\n",
	EM(ERR_INVALID_PEER_NODE_ID) = "Invalid peer-node-id\n",
	EM(ERR_CREATE_TRANSPORT) = "Failed to create transport (drbd_transport_xxx module missing?)\n",
	EM(ERR_LOCAL_AND_PEER_ADDR) = "Combination of local address(port) and remote address(port) already in use\n",
	EM(ERR_ALREADY_EXISTS) = "Already exists\n",
	EM(ERR_APV_TOO_LOW) = "A higher DRBD protocol level is required for this operation\n",
};
#define MAX_ERROR (sizeof(error_messages)/sizeof(*error_messages))
const char * error_to_string(int err_no)
{
	const unsigned int idx = err_no - ERR_CODE_BASE;
	if (idx >= MAX_ERROR) return "Unknown... maybe API_VERSION mismatch?";
	return error_messages[idx];
}
#undef MAX_ERROR

char *cmdname = NULL; /* "drbdsetup" for reporting in usage etc. */

/*
 * In CTX_MINOR, CTX_RESOURCE, CTX_ALL, objname and minor refer to the object
 * the command operates on.
 */
char *objname;
unsigned minor = -1U;
struct drbd_cfg_context global_ctx;
enum cfg_ctx_key context;

int lock_fd;

struct genl_sock *drbd_sock = NULL;

struct genl_family drbd_genl_family = {
	.name = "drbd",
	.version = GENL_MAGIC_VERSION,
	.hdrsize = GENL_MAGIC_FAMILY_HDRSZ,
};

#if 0
/* currently unused. */
static bool endpoints_equal(struct drbd_cfg_context *a, struct drbd_cfg_context *b)
{
	return a->ctx_my_addr_len == b->ctx_my_addr_len &&
	       a->ctx_peer_addr_len == b->ctx_peer_addr_len &&
	       !memcmp(a->ctx_my_addr, b->ctx_my_addr, a->ctx_my_addr_len) &&
	       !memcmp(a->ctx_peer_addr, b->ctx_peer_addr, a->ctx_peer_addr_len);
}
#endif

static int conv_md_idx(struct drbd_argument *ad, struct msg_buff *msg,
		       struct drbd_genlmsghdr *dhdr, char* arg)
{
	int idx;

	if(!strcmp(arg,"internal")) idx = DRBD_MD_INDEX_FLEX_INT;
	else if(!strcmp(arg,"flexible")) idx = DRBD_MD_INDEX_FLEX_EXT;
	else idx = m_strtoll(arg,1);

	nla_put_u32(msg, ad->nla_type, idx);

	return NO_ERROR;
}

static int conv_u32(struct drbd_argument *ad, struct msg_buff *msg,
		    struct drbd_genlmsghdr *dhdr, char* arg)
{
	unsigned int i = m_strtoll(arg, 1);

	nla_put_u32(msg, ad->nla_type, i);

	return NO_ERROR;
}

static int conv_str(struct drbd_argument *ad, struct msg_buff *msg,
		    struct drbd_genlmsghdr *dhdr, char* arg)
{
	nla_put_string(msg, ad->nla_type, arg);

	return NO_ERROR;
}

static void resolv6(const char *name, struct sockaddr_in6 *addr)
{
	struct addrinfo hints, *res, *tmp;
	int err;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	err = getaddrinfo(name, 0, &hints, &res);
	if (err) {
		fprintf(stderr, "getaddrinfo %s: %s\n", name, gai_strerror(err));
		exit(20);
	}

	/* Yes, it is a list. We use only the first result. The loop is only
	 * there to document that we know it is a list */
	for (tmp = res; tmp; tmp = tmp->ai_next) {
		memcpy(addr, tmp->ai_addr, sizeof(*addr));
		break;
	}
	freeaddrinfo(res);
	if (0) { /* debug output */
		char ip[INET6_ADDRSTRLEN];
		inet_ntop(AF_INET6, &addr->sin6_addr, ip, sizeof(ip));
		fprintf(stderr, "%s -> %02x %04x %08x %s %08x\n",
				name,
				addr->sin6_family,
				addr->sin6_port,
				addr->sin6_flowinfo,
				ip,
				addr->sin6_scope_id);
	}
}

static unsigned long resolv(const char* name)
{
	unsigned long retval;

	if((retval = inet_addr(name)) == INADDR_NONE ) {
		struct hostent *he;
		he = gethostbyname(name);
		if (!he) {
			fprintf(stderr, "can not resolve the hostname: gethostbyname(%s): %s\n",
					name, hstrerror(h_errno));
			exit(20);
		}
		retval = ((struct in_addr *)(he->h_addr_list[0]))->s_addr;
	}
	return retval;
}

static void split_ipv6_addr(char **address, int *port)
{
	/* ipv6:[fe80::0234:5678:9abc:def1]:8000; */
	char *b = strrchr(*address,']');
	if (address[0][0] != '[' || b == NULL ||
		(b[1] != ':' && b[1] != '\0')) {
		fprintf(stderr, "unexpected ipv6 format: %s\n",
				*address);
		exit(20);
	}

	*b = 0;
	*address += 1; /* skip '[' */
	if (b[1] == ':')
		*port = m_strtoll(b+2,1); /* b+2: "]:" */
	else
		*port = 7788; /* will we ever get rid of that default port? */
}

static void split_address(int *af, char** address, int* port)
{
	static struct { char* text; int af; } afs[] = {
		{ "ipv4:", AF_INET  },
		{ "ipv6:", AF_INET6 },
		{ "sdp:",  AF_INET_SDP },
		{ "ssocks:",  -1 },
	};

	unsigned int i;
	char *b;

	*af=AF_INET;
	for (i=0; i<ARRAY_SIZE(afs); i++) {
		if (!strncmp(*address, afs[i].text, strlen(afs[i].text))) {
			*af = afs[i].af;
			*address += strlen(afs[i].text);
			break;
		}
	}

	if (*af == AF_INET6 && address[0][0] == '[')
		return split_ipv6_addr(address, port);

	if (*af == -1)
		*af = get_af_ssocks(1);

	b=strrchr(*address,':');
	if (b) {
		*b = 0;
		if (*af == AF_INET6) {
			/* compatibility handling of ipv6 addresses,
			 * in the style expected before drbd 8.3.9.
			 * may go wrong without explicit port */
			fprintf(stderr, "interpreting ipv6:%s:%s as ipv6:[%s]:%s\n",
					*address, b+1, *address, b+1);
		}
		*port = m_strtoll(b+1,1);
	} else
		*port = 7788;
}

static int sockaddr_from_str(struct sockaddr_storage *storage, const char *str)
{
	int af, port;
	char *address = strdupa(str);

	split_address(&af, &address, &port);
	if (af == AF_INET6) {
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)storage;

		memset(sin6, 0, sizeof(*sin6));
		resolv6(address, sin6);
		sin6->sin6_port = htons(port);
		/* sin6->sin6_len = sizeof(*sin6); */
		return sizeof(*sin6);
	} else {
		/* AF_INET, AF_SDP, AF_SSOCKS,
		 * all use the IPv4 addressing scheme */
		struct sockaddr_in *sin = (struct sockaddr_in *)storage;

		memset(sin, 0, sizeof(*sin));
		sin->sin_port = htons(port);
		sin->sin_family = af;
		sin->sin_addr.s_addr = resolv(address);
		return sizeof(*sin);
	}
	return 0;
}

static int conv_addr(struct drbd_argument *ad, struct msg_buff *msg,
			  struct drbd_genlmsghdr *dhdr, char* arg)
{
	struct sockaddr_storage x;
	int addr_len;

	if (strncmp(arg, "local:", 6) == 0)
		arg += 6;
	else if (strncmp(arg, "peer:", 5) == 0)
		arg += 5;

	addr_len = sockaddr_from_str(&x, arg);
	if (addr_len == 0) {
		fprintf(stderr, "does not look like an endpoint address '%s'", arg);
		return OTHER_ERROR;
	}

	nla_put(msg, ad->nla_type, addr_len, &x);
	return NO_ERROR;
}


/* It will only print the WARNING if the warn flag is set
   with the _first_ call! */
#define PROC_NET_AF_SCI_FAMILY "/proc/net/af_sci/family"
#define PROC_NET_AF_SSOCKS_FAMILY "/proc/net/af_ssocks/family"

static int get_af_ssocks(int warn_and_use_default)
{
	char buf[16];
	int c, fd;
	static int af = -1;

	if (af > 0)
		return af;

	fd = open(PROC_NET_AF_SSOCKS_FAMILY, O_RDONLY);

	if (fd < 0)
		fd = open(PROC_NET_AF_SCI_FAMILY, O_RDONLY);

	if (fd < 0) {
		if (warn_and_use_default) {
			fprintf(stderr, "open(" PROC_NET_AF_SSOCKS_FAMILY ") "
				"failed: %m\n WARNING: assuming AF_SSOCKS = 27. "
				"Socket creation may fail.\n");
			af = 27;
		}
		return af;
	}
	c = read(fd, buf, sizeof(buf)-1);
	if (c > 0) {
		buf[c] = 0;
		if (buf[c-1] == '\n')
			buf[c-1] = 0;
		af = m_strtoll(buf,1);
	} else {
		if (warn_and_use_default) {
			fprintf(stderr, "read(" PROC_NET_AF_SSOCKS_FAMILY ") "
				"failed: %m\n WARNING: assuming AF_SSOCKS = 27. "
				"Socket creation may fail.\n");
			af = 27;
		}
	}
	close(fd);
	return af;
}

static struct option *make_longoptions(struct drbd_cmd *cm)
{
	static struct option buffer[47];
	int i = 0;
	int primary_force_index = -1;
	int connect_tentative_index = -1;

	if (cm->ctx) {
		struct field_def *field;

		/*
		 * Make sure to keep cm->ctx->fields first: we use the index
		 * returned by getopt_long() to access cm->ctx->fields.
		 */
		for (field = cm->ctx->fields; field->name; field++) {
			assert(i < ARRAY_SIZE(buffer));
			buffer[i].name = field->name;
			buffer[i].has_arg = field->argument_is_optional ?
				optional_argument : required_argument;
			buffer[i].flag = NULL;
			buffer[i].val = 0;
			if (!strcmp(cm->cmd, "primary") && !strcmp(field->name, "force"))
				primary_force_index = i;
			if (!strcmp(cm->cmd, "connect") && !strcmp(field->name, "tentative"))
				connect_tentative_index = i;
			i++;
		}
		assert(field - cm->ctx->fields == i);
	}

	if (cm->options) {
		struct option *option;

		for (option = cm->options; option->name; option++) {
			assert(i < ARRAY_SIZE(buffer));
			buffer[i] = *option;
			i++;
		}
	}

	if (primary_force_index != -1) {
		/*
		 * For backward compatibility, add --overwrite-data-of-peer as
		 * an alias to --force.
		 */
		assert(i < ARRAY_SIZE(buffer));
		buffer[i] = buffer[primary_force_index];
		buffer[i].name = "overwrite-data-of-peer";
		buffer[i].val = 1000 + primary_force_index;
		i++;
	}

	if (connect_tentative_index != -1) {
		/*
		 * For backward compatibility, add --dry-run as an alias to
		 * --tentative.
		 */
		assert(i < ARRAY_SIZE(buffer));
		buffer[i] = buffer[connect_tentative_index];
		buffer[i].name = "dry-run";
		buffer[i].val = 1000 + connect_tentative_index;
		i++;
	}

	if (cm->set_defaults) {
		assert(i < ARRAY_SIZE(buffer));
		buffer[i].name = "set-defaults";
		buffer[i].has_arg = 0;
		buffer[i].flag = NULL;
		buffer[i].val = '(';
		i++;
	}

	assert(i < ARRAY_SIZE(buffer));
	buffer[i].name = NULL;
	buffer[i].has_arg = 0;
	buffer[i].flag = NULL;
	buffer[i].val = 0;

	return buffer;
}

void fprintf_all_cfg_reply_info_text(const char *hdr, struct nlmsghdr *nlh)
{
	struct nlattr *msg_start = nlmsg_attrdata(nlh, GENL_HDRLEN + drbd_genl_family.hdrsize);
	int msg_len = nlmsg_attrlen(nlh, GENL_HDRLEN + drbd_genl_family.hdrsize);

	/* there may be more than one DRBD_NLA_CFG_REPLY,
	 * and more than one T_info_text inside. */
	struct nlattr *o_nla, *nla;
	int o_rem, rem;

	nla_for_each_attr(o_nla, msg_start, msg_len, o_rem) {
		if (nla_type(o_nla) != DRBD_NLA_CFG_REPLY
		|| o_nla->nla_len == 0)
			continue;
		if (hdr && *hdr) {
			fprintf(stderr, "%s", hdr);
			hdr = NULL;
		}
		nla_for_each_nested(nla, o_nla, rem) {
			if (nla_type(nla) == __nla_type(T_info_text))
				fprintf(stderr, "%s\n", (char*)nla_data(nla));
		}
	}
}

/* prepends global objname to output (if any) */
static int check_error(int err_no, char *desc, struct nlattr **tla, struct nlmsghdr *nlh)
{
	int rv = 0;

	if (err_no == NO_ERROR || err_no == SS_SUCCESS) {
		/* drbdsetup primary may produce warnings,
		 * which are no errors (on WinDRBD). */
		if (tla[DRBD_NLA_CFG_REPLY])
			fprintf_all_cfg_reply_info_text("warnings from kernel:\n", nlh);
		return 0;
	}

	if (err_no == OTHER_ERROR) {
		if (desc)
			fprintf(stderr,"%s: %s\n", objname, desc);
		return 20;
	}
	if (err_no == ERR_MODULE_UNLOADED) {
		if (desc)
			fprintf(stderr,"%s: %s\n", objname, desc);
		return ERR_EXIT_MODULE_UNLOADED;
	}

	if ( ( err_no >= AFTER_LAST_ERR_CODE || err_no <= ERR_CODE_BASE ) &&
	     ( err_no > SS_CW_NO_NEED || err_no <= SS_AFTER_LAST_ERROR) ) {
		fprintf(stderr,"%s: Error code %d unknown.\n"
			"You should update the drbd userland tools.\n",
			objname, err_no);
		rv = 20;
	} else {
		if(err_no > ERR_CODE_BASE ) {
			fprintf(stderr,"%s: Failure: (%d) %s\n",
				objname, err_no, desc ?: error_to_string(err_no));
			rv = 10;
		} else if (err_no == SS_UNKNOWN_ERROR) {
			fprintf(stderr,"%s: State change failed: (%d)"
				"unknown error.\n", objname, err_no);
			rv = 11;
		} else if (err_no > SS_TWO_PRIMARIES) {
			// Ignore SS_SUCCESS, SS_NOTHING_TO_DO, SS_CW_Success...
		} else {
			fprintf(stderr,"%s: State change failed: (%d) %s\n",
				objname, err_no, drbd_set_st_err_str(err_no));
			if (err_no == SS_NO_UP_TO_DATE_DISK) {
				/* all available disks are inconsistent,
				 * or I am consistent, but cannot outdate the peer. */
				rv = 17;
			} else if (err_no == SS_LOWER_THAN_OUTDATED) {
				/* was inconsistent anyways */
				rv = 5;
			} else if (err_no == SS_NO_LOCAL_DISK) {
				/* Can not start resync, no local disks, try with drbdmeta */
				rv = 16;
			} else {
				rv = 11;
			}
		}
	}

	if (tla[DRBD_NLA_CFG_REPLY])
		fprintf_all_cfg_reply_info_text("additional info from kernel:\n", nlh);
	return rv;
}

static void warn_print_excess_args(int argc, char **argv, int i)
{
	fprintf(stderr, "Excess arguments:");
	for (; i < argc; i++)
		fprintf(stderr, " %s", argv[i]);
	printf("\n");
}

int drbd_tla_parse(struct nlattr *tla[], struct nlmsghdr *nlh)
{
	return nla_parse(tla, ARRAY_SIZE(drbd_tla_nl_policy)-1,
		nlmsg_attrdata(nlh, GENL_HDRLEN + drbd_genl_family.hdrsize),
		nlmsg_attrlen(nlh, GENL_HDRLEN + drbd_genl_family.hdrsize),
		drbd_tla_nl_policy);
}

#define ASSERT(exp) if (!(exp)) \
		fprintf(stderr,"ASSERT( " #exp " ) in %s:%d\n", __FILE__,__LINE__);

static int _generic_config_cmd(struct drbd_cmd *cm, int argc, char **argv)
{
	struct drbd_argument *ad;
	struct nlattr *nla;
	struct option *options;
	int c, i;
	int rv;
	char *desc = NULL; /* error description from kernel reply message */

	struct nlattr *tla[ARRAY_SIZE(drbd_tla_nl_policy)] = { 0, };
	struct drbd_genlmsghdr *dhdr;
	struct msg_buff *smsg;
	struct iovec iov;
	struct nlmsghdr *nlh = NULL;
	struct drbd_genlmsghdr *dh;
	struct timespec retry_timeout = {
		.tv_nsec = 62500000L,  /* 1/16 second */
	};

	/* pre allocate request message and reply buffer */
	iov.iov_len = DEFAULT_MSG_SIZE;
	iov.iov_base = malloc(iov.iov_len);
	smsg = msg_new(DEFAULT_MSG_SIZE);
	if (!smsg || !iov.iov_base) {
		desc = "could not allocate netlink messages";
		rv = OTHER_ERROR;
		goto error;
	}
	/* reply buffer */
	nlh = (struct nlmsghdr*)iov.iov_base;
	dh = genlmsg_data(nlmsg_data(nlh));

	dhdr = genlmsg_put(smsg, &drbd_genl_family, 0, cm->cmd_id);
	dhdr->minor = -1;
	dhdr->flags = 0;

	if (context & CTX_MINOR)
		dhdr->minor = minor;

	if (context & ~CTX_MINOR) {
		nla = nla_nest_start(smsg, DRBD_NLA_CFG_CONTEXT);
		if (context & CTX_RESOURCE)
			nla_put_string(smsg, T_ctx_resource_name, objname);
		if (context & CTX_PEER_NODE_ID)
			nla_put_u32(smsg, T_ctx_peer_node_id, global_ctx.ctx_peer_node_id);
		if (context & CTX_VOLUME)
			nla_put_u32(smsg, T_ctx_volume, global_ctx.ctx_volume);
		nla_nest_end(smsg, nla);
	}

	nla = NULL;

	options = make_longoptions(cm);
	optind = 0;  /* reset getopt_long() */
	for (;;) {
		int idx;

		c = getopt_long(argc, argv, "(", options, &idx);
		if (c == -1)
			break;
		if (c >= 1000) {
			/* This is a field alias. */
			idx = c - 1000;
			c = 0;
		}
		if (c == 0) {
			struct field_def *field = &cm->ctx->fields[idx];
			assert (field->name == options[idx].name);
			if (!nla) {
				assert (cm->tla_id != NO_PAYLOAD);
				nla = nla_nest_start(smsg, cm->tla_id);
			}
			if (!field->ops->put(cm->ctx, field, smsg, optarg)) {
				fprintf(stderr, "Option --%s: invalid "
					"argument '%s'\n",
					field->name, optarg);
				rv = OTHER_ERROR;
				goto error;
			}
		} else if (c == '(')
			dhdr->flags |= DRBD_GENL_F_SET_DEFAULTS;
		else {
			rv = OTHER_ERROR;
			goto error;
		}
	}

	for (i = optind, ad = cm->drbd_args; ad && ad->name; i++) {
		if (argc < i + 1) {
			fprintf(stderr, "Missing argument '%s'\n", ad->name);
			print_command_usage(cm, FULL);
			rv = OTHER_ERROR;
			goto error;
		}
		if (!nla) {
			assert (cm->tla_id != NO_PAYLOAD);
			nla = nla_nest_start(smsg, cm->tla_id);
		}
		rv = ad->convert_function(ad, smsg, dhdr, argv[i]);
		if (rv != NO_ERROR)
			goto error;
		ad++;
	}
	/* dhdr->minor may have been set by one of the convert functions. */
	minor = dhdr->minor;

	if (nla)
		nla_nest_end(smsg, nla);

	/* argc should be cmd + n options + n args;
	 * if it is more, we did not understand some */
	if (i < argc) {
		warn_print_excess_args(argc, argv, i);
		rv = OTHER_ERROR;
		goto error;
	}

	if (strcmp(cm->cmd, "new-minor") == 0) {
/* HACK */
/* hack around "sysfs: cannot create duplicate filename '/devices/virtual/bdi/147:0'"
 * and subsequent NULL deref in kernel below add_disk(). */
		struct stat sb;
		char buf[PATH_MAX];
		int i;
		int c = 0;
		for (i = 0; i <= 2; i++) {
			if (i == 0) c = snprintf(buf, PATH_MAX, "/sys/devices/virtual/block/drbd%u", minor);
			if (i == 1) c = snprintf(buf, PATH_MAX, "/sys/devices/virtual/bdi/147:%u", minor);
			if (i == 2) c = snprintf(buf, PATH_MAX, "/sys/block/drbd%u", minor);
			if (c < PATH_MAX) {
				if (lstat(buf, &sb) == 0) {
					syslog(LOG_ERR, "new-minor %s %u %u: sysfs node '%s' (already? still?) exists\n",
							objname, minor, global_ctx.ctx_volume, buf);
					fprintf(stderr, "new-minor %s %u %u: sysfs node '%s' (already? still?) exists\n",
							objname, minor, global_ctx.ctx_volume, buf);
					rv = ERR_MINOR_OR_VOLUME_EXISTS;
					desc = NULL;
					goto error;
				}
			}
		}
	}

	for(;;) {
		if (genl_send(drbd_sock, smsg)) {
			desc = "error sending config command";
			rv = OTHER_ERROR;
			goto error;
		}
		do {
			int received;

			/* reduce timeout! limit retries */
			received = genl_recv_msgs(drbd_sock, &iov, &desc, 120000);
			if (received <= 0) {
				if (received == -E_RCV_ERROR_REPLY && !errno)
					continue;
				if (!desc)
					desc = "error receiving config reply";
				rv = OTHER_ERROR;
				goto error;
			}
		} while (false);
		ASSERT(dh->minor == minor);
		rv = dh->ret_code;
		if (rv != SS_IN_TRANSIENT_STATE)
			break;
		nanosleep(&retry_timeout, NULL);
		/* Double the timeout, up to 10 seconds. */
		if (retry_timeout.tv_sec < 10) {
			retry_timeout.tv_sec *= 2;
			retry_timeout.tv_nsec *= 2;
			if (retry_timeout.tv_nsec > 1000000000L) {
				retry_timeout.tv_sec++;
				retry_timeout.tv_nsec -= 1000000000L;
			}
		}
	}
	if (rv == ERR_RES_NOT_KNOWN) {
		if (cm->warn_on_missing && isatty(STDERR_FILENO))
			fprintf(stderr, "Resource unknown\n");

		if (cm->missing_ok)
			rv = NO_ERROR;
	}
	drbd_tla_parse(tla, nlh);

error:
	msg_free(smsg);

	rv = check_error(rv, desc, tla, nlh);
	free(iov.iov_base);
	return rv;
}

static int generic_config_cmd(struct drbd_cmd *cm, int argc, char **argv)
{
	return _generic_config_cmd(cm, argc, argv);
}

static int del_minor_cmd(struct drbd_cmd *cm, int argc, char **argv)
{
	int rv;

	rv = generic_config_cmd(cm, argc, argv);
	if (!rv)
		unregister_minor(minor);
	return rv;
}

static int del_resource_cmd(struct drbd_cmd *cm, int argc, char **argv)
{
	int rv;

	rv = generic_config_cmd(cm, argc, argv);
	if (!rv)
		unregister_resource(objname);
	return rv;
}

static struct drbd_cmd *find_cmd_by_name(const char *name)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (!strcmp(name, commands[i].cmd)) {
			return commands + i;
		}
	}
	return NULL;
}

static void print_options(struct nlattr *attr, struct context_def *ctx, const char *sect_name)
{
	struct nlattr *nested_attr_tb[128];
	struct field_def *field;
	int opened = 0;

	if (!attr)
		return;

	if (drbd_nla_parse_nested(nested_attr_tb, ctx->nla_policy_size - 1,
				  attr, ctx->nla_policy)) {
		fprintf(stderr, "nla_policy violation for %s payload!\n", sect_name);
		/* still, print those that validated ok */
	}

	for (field = ctx->fields; field->name; field++) {
		struct nlattr *nlattr;
		const char *str;
		bool is_default;

		nlattr = nested_attr_tb[__nla_type(field->nla_type)];
		if (!nlattr)
			continue;
		str = field->ops->get(ctx, field, nlattr);
		is_default = field->ops->is_default(field, str);
		if (is_default && !show_defaults)
			continue;
		if (!opened) {
			opened=1;
			printI("%s {\n",sect_name);
			++indent;
		}
		if (field->needs_double_quoting)
			str = double_quote_string(str);
		printI("%-16s\t%s;",field->name, str);
		if (field->unit || is_default) {
				printf(" # ");
			if (field->unit)
				printf("%s", field->unit);
			if (field->unit && is_default)
				printf(", ");
			if (is_default)
				printf("default");
		}
		printf("\n");
	}
	if(opened) {
		--indent;
		printI("}\n");
	}
}

static int nr_printed_opts(struct context_def *ctx, struct field_def *start,
		struct nlattr **nested_attr_tb)
{
	struct field_def *field;
	const char *str;
	bool is_default;

	int nr = -1;
	if (start == NULL)
		return nr;

	for (field = start; field->name; field++) {
		struct nlattr *nlattr = nested_attr_tb[__nla_type(field->nla_type)];
		if (!nlattr)
			continue;
		str = field->ops->get(ctx, field, nlattr);
		is_default = field->ops->is_default(field, str);
		if (is_default && !show_defaults)
			continue;

		nr++;
	}

	return nr;
}

static bool print_options_json(
	struct nlattr *attr,
	struct context_def *ctx,
	const char *sect_name,
	bool comma_before,
	bool comma_after
)
{
	struct nlattr *nested_attr_tb[128];
	struct field_def *field;
	int opened = 0;
	int will_print, printed;

	if (!attr)
		return false;

	if (drbd_nla_parse_nested(nested_attr_tb, ctx->nla_policy_size - 1,
				  attr, ctx->nla_policy)) {
		fprintf(stderr, "nla_policy violation for %s payload!\n", sect_name);
		/* still, print those that validated ok */
	}

	will_print = nr_printed_opts(ctx, ctx->fields, nested_attr_tb);
	printed = 0;
	for (field = ctx->fields; field->name; field++) {
		struct nlattr *nlattr;
		const char *str;
		bool is_default;

		nlattr = nested_attr_tb[__nla_type(field->nla_type)];
		if (!nlattr)
			continue;
		str = field->ops->get(ctx, field, nlattr);
		is_default = field->ops->is_default(field, str);
		if (is_default && !show_defaults)
			continue;
		if (!opened) {
			opened=1;
			if (comma_before)
				printf(",\n");
			printI(QUOTED("%s") ": {\n", sect_name);
			++indent;
		}

		printI(QUOTED("%s") ": ", field->name);
		if (field->ops == &fc_boolean)
		{
			if (strcmp("yes", str) == 0)
				printf("true");
			else
				printf("false");
		}
		else
			printf(QUOTED("%s"), str);
		if (printed < will_print)
			printf(",");
		printf("\n");

		printed++;
	}
	if(opened) {
		--indent;
		if (comma_after)
			printI("},\n");
		else
			printI("}\n");
	}

	return opened > 0;
}

struct choose_timeout_ctx {
	struct drbd_cfg_context ctx;
	struct msg_buff *smsg;
	struct iovec *iov;
	int timeout;
	int wfc_timeout;
	int degr_wfc_timeout;
	int outdated_wfc_timeout;
};

int choose_timeout(struct choose_timeout_ctx *ctx)
{
	struct nlattr *tla[ARRAY_SIZE(drbd_tla_nl_policy)] = { 0, };
	char *desc = NULL;
	struct drbd_genlmsghdr *dhdr;
	struct nlattr *nla;
	int err, rr;

	if (0 < ctx->wfc_timeout &&
	      (ctx->wfc_timeout < ctx->degr_wfc_timeout || ctx->degr_wfc_timeout == 0)) {
		ctx->degr_wfc_timeout = ctx->wfc_timeout;
		fprintf(stderr, "degr-wfc-timeout has to be shorter than wfc-timeout\n"
				"degr-wfc-timeout implicitly set to wfc-timeout (%ds)\n",
				ctx->degr_wfc_timeout);
	}

	if (0 < ctx->degr_wfc_timeout &&
	    (ctx->degr_wfc_timeout < ctx->outdated_wfc_timeout || ctx->outdated_wfc_timeout == 0)) {
		ctx->outdated_wfc_timeout = ctx->wfc_timeout;
		fprintf(stderr, "outdated-wfc-timeout has to be shorter than degr-wfc-timeout\n"
				"outdated-wfc-timeout implicitly set to degr-wfc-timeout (%ds)\n",
				ctx->degr_wfc_timeout);
	}
	dhdr = genlmsg_put(ctx->smsg, &drbd_genl_family, 0, DRBD_ADM_GET_TIMEOUT_TYPE);
	dhdr->minor = -1;
	dhdr->flags = 0;

	nla = nla_nest_start(ctx->smsg, DRBD_NLA_CFG_CONTEXT);
	nla_put_string(ctx->smsg, T_ctx_resource_name, ctx->ctx.ctx_resource_name);
	nla_put_u32(ctx->smsg, T_ctx_peer_node_id, ctx->ctx.ctx_peer_node_id);
	nla_put_u32(ctx->smsg, T_ctx_volume, ctx->ctx.ctx_volume);
	nla_nest_end(ctx->smsg, nla);

	if (genl_send(drbd_sock, ctx->smsg)) {
		desc = "error sending config command";
		goto error;
	}

	rr = genl_recv_msgs(drbd_sock, ctx->iov, &desc, 120000);
	if (rr > 0) {
		struct nlmsghdr *nlh = (struct nlmsghdr*)ctx->iov->iov_base;
		struct genl_info info = {
			.seq = nlh->nlmsg_seq,
			.nlhdr = nlh,
			.genlhdr = nlmsg_data(nlh),
			.userhdr = genlmsg_data(nlmsg_data(nlh)),
			.attrs = tla,
		};
		struct drbd_genlmsghdr *dh = info.userhdr;
		struct timeout_parms parms;
		rr = dh->ret_code;
		if (rr == ERR_MINOR_INVALID) {
			desc = "minor not available";
			goto error;
		}
		if (rr != NO_ERROR)
			goto error;
		err = drbd_tla_parse(info.attrs, nlh);
		if (err) {
			desc = "drbd_tla_parse() failed - "
				"do you need to upgrade your userland tools?";
			goto error;
		}
		err = timeout_parms_from_attrs(&parms, &info);
		if (err) {
			desc = "timeout_parms_from_attrs() failed - "
				"do you need to upgrade your userland tools?";
			goto error;
		}
		rr = parms.timeout_type;
		ctx->timeout =
			(rr == UT_DEGRADED) ? ctx->degr_wfc_timeout :
			(rr == UT_PEER_OUTDATED) ? ctx->outdated_wfc_timeout :
			ctx->wfc_timeout;
		return 0;
	}
error:
	if (!desc)
		desc = "error receiving netlink reply";
	fprintf(stderr, "error determining which timeout to use: %s\n",
			desc);
	return 20;
}

static int shortest_timeout(struct peer_devices_list *peer_devices)
{
	struct peer_devices_list *peer_device;
	int timeout = -1;

	/* There is no point waiting for peers I do not even know about. */
	if (peer_devices == NULL)
		return 1;

	for (peer_device = peer_devices; peer_device; peer_device = peer_device->next) {
		if (peer_device->timeout_ms > 0 &&
		    (peer_device->timeout_ms < timeout || timeout == -1))
			timeout = peer_device->timeout_ms;
	}

	return timeout;
}

static bool update_timeouts(struct peer_devices_list *peer_devices, int elapsed)
{
	struct peer_devices_list *peer_device;
	bool all_expired = true;

	for (peer_device = peer_devices; peer_device; peer_device = peer_device->next) {
		if (peer_device->timeout_ms != -1) {
			peer_device->timeout_ms -= elapsed;
			if (peer_device->timeout_ms < 0)
				peer_device->timeout_ms = 0;
		}
		if (peer_device->timeout_ms != 0)
			all_expired = false;
	}

	return all_expired;
}

static bool parse_color_argument(void)
{
	if (!optarg || !strcmp(optarg, "always"))
		opt_color = ALWAYS_COLOR;
	else if (!strcmp(optarg, "never"))
		opt_color = NEVER_COLOR;
	else if (!strcmp(optarg, "auto"))
		opt_color = AUTO_COLOR;
	else
		return 0;
	return 1;
}

bool opt_now;
bool opt_poll;
int opt_verbose;
bool opt_statistics;
bool opt_timestamps;
bool opt_diff;
bool opt_fullch;

static int generic_send(struct drbd_cmd *cm)
{
	struct drbd_genlmsghdr *dhdr;
	struct msg_buff *smsg;
	int err = 0;

	/* preallocate request message */
	smsg = msg_new(DEFAULT_MSG_SIZE);
	if (!smsg) {
		fprintf(stderr, "%s: could not allocate netlink message\n", objname);
		return 20;
	}

	dhdr = genlmsg_put(smsg, &drbd_genl_family, NLM_F_DUMP, cm->cmd_id);
	dhdr->minor = -1;
	dhdr->flags = 0;
	if (strcmp(objname, "all")) {
		/* Restrict the dump to a single resource. */
		struct nlattr *nla;
		nla = nla_nest_start(smsg, DRBD_NLA_CFG_CONTEXT);
		nla_put_string(smsg, T_ctx_resource_name, objname);
		nla_nest_end(smsg, nla);
	}

	if (genl_send(drbd_sock, smsg)) {
		fprintf(stderr, "%s: error sending config command\n", objname);
		err = 20;
	}

	msg_free(smsg);
	return err;
}

static int generic_recv(struct drbd_cmd *cm, int timeout_arg, void *u_ptr, int extra_poll_fd, bool expect_reply)
{
	struct nlattr *tla[ARRAY_SIZE(drbd_tla_nl_policy)] = { 0, };
	char *desc = NULL;
	struct iovec iov;
	int timeout_ms;
	int rv = NO_ERROR;
	int err = 0;

	/* preallocate reply buffer */
	iov.iov_len = DEFAULT_MSG_SIZE;
	iov.iov_base = malloc(iov.iov_len);
	if (!iov.iov_base) {
		desc = "could not allocate netlink reply buffer";
		rv = OTHER_ERROR;
		goto out;
	}

	/* disable sequence number check in genl_recv_msgs */
	drbd_sock->s_seq_expect = 0;

	for (;;) {
		int received, rem, ret;
		struct nlmsghdr *nlh = (struct nlmsghdr *)iov.iov_base;
		struct timeval before;

		gettimeofday(&before, NULL);

		timeout_ms =
			timeout_arg == MULTIPLE_TIMEOUTS ? shortest_timeout(u_ptr) : timeout_arg;

		/* Wait for new data or error/HUP. We want to receive the full
		 * reply before returning, so only check for data on
		 * extra_poll_fd if we are not still expecting to receive a
		 * reply. */
		ret = poll_hup(drbd_sock, timeout_ms, expect_reply ? -1 : extra_poll_fd);
		if (ret == E_POLL_EXTRA_FD) {
			goto out;
		} else if (ret > 0) { /* failed */
			if (ret == E_POLL_TIMEOUT)
				err = 5;
			goto out;
		}

		/* Linux: At this point we know that there is data available on
		 * drbd_sock. So this will not block. So we do not need to
		 * additionally poll on extra_poll_fd.
		 *
		 * Windows: extra_poll_fd is not supported; this may block. */
		received = genl_recv_msgs(drbd_sock, &iov, &desc, timeout_ms);
		if (received <= 0) {
			switch(received) {
			case E_RCV_TIMEDOUT:
				err = 5;
				goto out;
			case -E_RCV_FAILED:
				err = 20;
				goto out;
			case -E_RCV_NO_SOURCE_ADDR:
				continue; /* ignore invalid message */
			case -E_RCV_SEQ_MISMATCH:
				/* we disabled it, so it should not happen */
				err = 20;
				goto out;
			case -E_RCV_MSG_TRUNC:
				continue;
			case -E_RCV_UNEXPECTED_TYPE:
				continue;
			case -E_RCV_NLMSG_DONE:
				expect_reply = false;
				if (cm->continuous_poll)
					continue;
				err = cm->handle_reply(cm, NULL, u_ptr);
				if (err)
					goto out;
				err = -*(int*)nlmsg_data(nlh);
				if (err &&
				    (err != ENODEV || !cm->missing_ok)) {
					fprintf(stderr, "received netlink error reply: %s\n",
						strerror(err));
					err = 20;
				}
				goto out;
			case -E_RCV_ERROR_REPLY:
				if (!errno) /* positive ACK message */
					continue;
				if (!desc)
					desc = strerror(errno);
				fprintf(stderr, "received netlink error reply: %s\n",
					       desc);
				err = 20;
				goto out;
			default:
				if (!desc)
					desc = "error receiving config reply";
				err = 20;
				goto out;
			}
		}

		if (timeout_ms != -1) {
			struct timeval after;
			int elapsed_ms;
			bool exit;

			gettimeofday(&after, NULL);
			elapsed_ms =
				(after.tv_sec - before.tv_sec) * 1000 +
				(after.tv_usec - before.tv_usec) / 1000;

			if (timeout_arg == MULTIPLE_TIMEOUTS) {
				exit = update_timeouts(u_ptr, elapsed_ms);
			} else {
				timeout_ms -= elapsed_ms;
				exit = timeout_ms <= 0;
			}

			if (exit) {
				err = 5;
				goto out;
			}
		}

		/* There may be multiple messages in one datagram (for dump replies). */
		nlmsg_for_each_msg(nlh, nlh, received, rem) {
			struct drbd_genlmsghdr *dh = genlmsg_data(nlmsg_data(nlh));
			struct genl_info info = (struct genl_info){
				.seq = nlh->nlmsg_seq,
				.nlhdr = nlh,
				.genlhdr = nlmsg_data(nlh),
				.userhdr = genlmsg_data(nlmsg_data(nlh)),
				.attrs = tla,
			};

			dbg(3, "received type:%x\n", nlh->nlmsg_type);
			if (nlh->nlmsg_type < NLMSG_MIN_TYPE) {
				/* Ignore netlink control messages. */
				continue;
			}
			if (nlh->nlmsg_type == GENL_ID_CTRL) {
#ifdef HAVE_CTRL_CMD_DELMCAST_GRP
#define CMD_INDICATING_MODULE_UNLOAD CTRL_CMD_DELMCAST_GRP
#else
#define CMD_INDICATING_MODULE_UNLOAD CTRL_CMD_DELFAMILY
#endif
				dbg(3, "received cmd:%x\n", info.genlhdr->cmd);
				if (info.genlhdr->cmd == CMD_INDICATING_MODULE_UNLOAD) {
					struct nlattr *nla =
						nlmsg_find_attr(nlh, GENL_HDRLEN, CTRL_ATTR_FAMILY_ID);
					if (nla && nla_get_u16(nla) == drbd_genl_family.id) {
						/* FIXME: We could wait for the
						   multicast group to be recreated ... */
						rv = ERR_MODULE_UNLOADED;
						desc = "module unloaded";
						goto out;
					}
				}
				/* Ignore other generic netlink control messages. */
				continue;
			}
			if (nlh->nlmsg_type != drbd_genl_family.id) {
				/* Ignore messages for all other netlink families. */
				continue;
			}

			/* parse early, otherwise drbd_cfg_context_from_attrs
			 * can not work */
			if (drbd_tla_parse(info.attrs, nlh)) {
				/* FIXME
				 * should continuous_poll continue?
				 */
				desc = "reply did not validate - "
					"do you need to upgrade your userland tools?";
				rv = OTHER_ERROR;
				goto out;
			}
			if (cm->continuous_poll || cm->cmd_id == DRBD_ADM_GET_INITIAL_STATE) {
				struct drbd_cfg_context ctx;
				/*
				 * We will receive all events and have to
				 * filter for what we want ourself.
				 */

				err = drbd_cfg_context_from_attrs(&ctx, &info);
				if (!err) {
					switch ((int)cm->ctx_key) {
					case CTX_PEER_DEVICE:
						if (ctx.ctx_volume != global_ctx.ctx_volume)
							continue;
						/* also needs to match the connection, of course */
					case CTX_PEER_NODE:
						if (ctx.ctx_peer_node_id != global_ctx.ctx_peer_node_id)
							continue;
						/* also needs to match the resource, of course */
					case CTX_RESOURCE:
					case CTX_RESOURCE | CTX_ALL:
						if (!strcmp(objname, "all"))
							break;

						if (strcmp(objname, ctx.ctx_resource_name))
							continue;

						break;
					default:
						fprintf(stderr, "DRECK: %x\n", cm->ctx_key);
						assert(0);
					}
				}
			}
			rv = dh->ret_code;
			if (rv == ERR_MINOR_INVALID && cm->missing_ok)
				rv = NO_ERROR;
			if (rv != NO_ERROR)
				goto out;
			err = cm->handle_reply(cm, &info, u_ptr);
			if (err) {
				if (err < 0)
					err = 0;
				goto out;
			}
		}
	}

out:
	if (!err)
		err = check_error(rv, desc, tla, (struct nlmsghdr *)iov.iov_base);
	free(iov.iov_base);
	return err;
}

static int generic_get(struct drbd_cmd *cm, int timeout_arg, void *u_ptr)
{
	int err;

	err = generic_send(cm);
	if (err != 0)
		return err;

	return generic_recv(cm, timeout_arg, u_ptr, -1, true);
}

static int events2_poll(struct drbd_cmd *cm, int timeout_arg, void *u_ptr)
{
	int err;
	bool send_request = true;

	while (true) {
		char c;
		ssize_t bytes_read;

		if (send_request) {
			err = generic_send(cm);
			if (err != 0)
				return err;
		}

		if (opt_now) {
			if (send_request) {
				/* When --now is given, we do not need to poll
				 * on stdin because we only receive until the
				 * reply is done. This is important on Windows
				 * because the extra_poll_fd parameter is not
				 * supported on that platform. */
				err = generic_recv(cm, timeout_arg, u_ptr, -1, send_request);
				if (err != 0)
					return err;
			}
		} else {
			err = generic_recv(cm, timeout_arg, u_ptr, STDIN_FILENO, send_request);
			if (err != 0)
				return err;
		}
		send_request = false;

		bytes_read = read(STDIN_FILENO, &c, 1);
		if (bytes_read < 0) {
			return 20;
		} else if (bytes_read == 0) {
			return 0;
		}

		switch (c) {
			case 'n': /* next */
				if (opt_now)
					events2_reset();
				else
					events2_prepare_update();

				send_request = true;
				continue;
			case '\n':
				continue;
			default:
				return 0;
		}
	}

	return 0;
}

static int generic_events_cmd(struct drbd_cmd *cm, int argc, char **argv)
{
	static struct option no_options[] = { { } };
	struct choose_timeout_ctx timeo_ctx = {
		.wfc_timeout = DRBD_WFC_TIMEOUT_DEF,
		.degr_wfc_timeout = DRBD_DEGR_WFC_TIMEOUT_DEF,
		.outdated_wfc_timeout = DRBD_OUTDATED_WFC_TIMEOUT_DEF,
	};
	int c, timeout_ms, err = NO_ERROR;
	struct peer_devices_list *peer_devices = NULL;
	struct option *options = cm->options ? cm->options : no_options;
	const char *opts = make_optstring(options);

	optind = 0;  /* reset getopt_long() */
	for(;;) {
		c = getopt_long(argc, argv, opts, options, 0);
		if (c == -1)
			break;
		switch(c) {
		default:
		case '?':
			return 20;
		case 't':
			timeo_ctx.wfc_timeout = m_strtoll(optarg, 1);
			if(DRBD_WFC_TIMEOUT_MIN > timeo_ctx.wfc_timeout ||
			   timeo_ctx.wfc_timeout > DRBD_WFC_TIMEOUT_MAX) {
				fprintf(stderr, "wfc_timeout => %d"
					" out of range [%d..%d]\n",
					timeo_ctx.wfc_timeout,
					DRBD_WFC_TIMEOUT_MIN,
					DRBD_WFC_TIMEOUT_MAX);
				return 20;
			}
			break;
		case 'd':
			timeo_ctx.degr_wfc_timeout = m_strtoll(optarg, 1);
			if(DRBD_DEGR_WFC_TIMEOUT_MIN > timeo_ctx.degr_wfc_timeout ||
			   timeo_ctx.degr_wfc_timeout > DRBD_DEGR_WFC_TIMEOUT_MAX) {
				fprintf(stderr, "degr_wfc_timeout => %d"
					" out of range [%d..%d]\n",
					timeo_ctx.degr_wfc_timeout,
					DRBD_DEGR_WFC_TIMEOUT_MIN,
					DRBD_DEGR_WFC_TIMEOUT_MAX);
				return 20;
			}
			break;
		case 'o':
			timeo_ctx.outdated_wfc_timeout = m_strtoll(optarg, 1);
			if(DRBD_OUTDATED_WFC_TIMEOUT_MIN > timeo_ctx.outdated_wfc_timeout ||
			   timeo_ctx.outdated_wfc_timeout > DRBD_OUTDATED_WFC_TIMEOUT_MAX) {
				fprintf(stderr, "outdated_wfc_timeout => %d"
					" out of range [%d..%d]\n",
					timeo_ctx.outdated_wfc_timeout,
					DRBD_OUTDATED_WFC_TIMEOUT_MIN,
					DRBD_OUTDATED_WFC_TIMEOUT_MAX);
				return 20;
			}
			break;

		case 'n':
			opt_now = true;
			break;

		case 'p':
			opt_poll = true;
			break;

		case 's':
			++opt_verbose;
			opt_statistics = true;
			break;

		case 'w':
			if (!optarg || !strcmp(optarg, "yes"))
				wait_after_split_brain = true;
			break;

		case 'D':
			show_defaults = true;
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
			if (!parse_color_argument())
				print_usage_and_exit("unknown --color argument");
			break;
		}
	}
	if (optind < argc) {
		warn_print_excess_args(argc, argv, optind + 1);
		return 20;
	}

	timeout_ms = -1;
	if (cm->handle_reply == &wait_for_family) {
		struct peer_devices_list *peer_device;
		struct msg_buff *smsg;
		struct iovec iov;
		int rr;
		char *res_name = cm->ctx_key & CTX_RESOURCE ? objname : "all";

		peer_devices = list_peer_devices(res_name);

		/* if there are no peer devices, we don't wait by definition */
		if (!peer_devices)
			return 0;

		iov.iov_len = DEFAULT_MSG_SIZE;
		iov.iov_base = malloc(iov.iov_len);
		smsg = msg_new(DEFAULT_MSG_SIZE);
		if (!smsg || !iov.iov_base) {
			msg_free(smsg);
			free(iov.iov_base);
			fprintf(stderr, "could not allocate netlink messages\n");
			return 20;
		}

		timeo_ctx.smsg = smsg;
		timeo_ctx.iov = &iov;

		for (peer_device = peer_devices;
		     peer_device;
		     peer_device = peer_device->next) {

			timeo_ctx.ctx = peer_device->ctx;
			rr = choose_timeout(&timeo_ctx);

			if (rr)
				return rr;

			peer_device->timeout_ms =
				timeo_ctx.timeout ? timeo_ctx.timeout * 1000 : -1;

			/* rewind send message buffer */
			smsg->tail = smsg->data;
		}

		msg_free(smsg);
		free(iov.iov_base);

		timeout_ms = MULTIPLE_TIMEOUTS;
	}

	if (cm->handle_reply == &print_event && opt_now) {
		/* When --now is given, we just need the replies from the
		 * initial state request. So we do not need to subscribe to the
		 * multicast events or try to receive them.
		 *
		 * When --poll is also set, we block at the point where we
		 * attempt to read from stdin. */
		cm->continuous_poll = false;
	} else {
		if (genl_join_mc_group_and_ctrl(drbd_sock, "events")) {
			fprintf(stderr,"%s: unable to join drbd events multicast group\n", objname);
			err = 20;
			goto out;
		}
	}

	if (cm->handle_reply == &print_event && opt_poll)
		err = events2_poll(cm, timeout_ms, peer_devices);
	else
		err = generic_get(cm, timeout_ms, peer_devices);

out:
	free_peer_devices(peer_devices);

	return err;
}

static bool options_empty(struct nlattr *attr, struct context_def *ctx)
{
	struct nlattr *nested_attr_tb[ctx->nla_policy_size];
	struct field_def *field;

	if (!attr)
		return true;

	if (drbd_nla_parse_nested(nested_attr_tb, ctx->nla_policy_size - 1,
				  attr, ctx->nla_policy)) {
		fprintf(stderr, "nla_policy violation\n");
	}

	for (field = ctx->fields; field->name; field++) {
		struct nlattr *nlattr;
		const char *str;
		bool is_default;

		nlattr = nested_attr_tb[__nla_type(field->nla_type)];
		if (!nlattr)
			continue;
		str = field->ops->get(ctx, field, nlattr);
		is_default = field->ops->is_default(field, str);
		if (is_default && !show_defaults)
			continue;
		return false;
	}

	return true;
}

static void show_peer_device(struct peer_devices_list *peer_device)
{
	if (options_empty(peer_device->peer_device_conf, &peer_device_options_ctx))
		return;

	printI("volume %d {\n", peer_device->ctx.ctx_volume);
	++indent;
	print_options(peer_device->peer_device_conf, &peer_device_options_ctx, "disk");
	--indent;
	printI("}\n");
}

static void print_paths(struct connections_list *connection)
{
	char address[ADDRESS_STR_MAX];
	char *colon;
	struct nlattr *nla;
	int tmp;

	if (!connection->path_list)
		return;

	nla_for_each_nested(nla, connection->path_list, tmp) {
		int l = nla_len(nla);

		if (!address_str(address, nla_data(nla), l))
			continue;
		colon = strchr(address, ':');
		if (colon)
			*colon = ' ';
		if (nla->nla_type == T_my_addr) {
			pD("path {\n"); pJ(QUOTED("path") ": {\n");
			++indent;
			pD("_this_host %s;\n", address); pJ(QUOTED("_this_host") ": \"%s\",\n", address);
		}
		if (nla->nla_type == T_peer_addr) {
			pD("_remote_host %s;\n", address); pJ(QUOTED("_remote_host") ": \"%s\"\n", address);
			--indent;
			pD("}\n"); pJ("},\n");
		}
	}
}

static void show_connection(struct connections_list *connection, struct peer_devices_list *peer_devices)
{
	struct peer_devices_list *peer_device;

	printI("connection {\n");
	++indent;
	printI("_peer_node_id %d;\n", connection->ctx.ctx_peer_node_id);

	print_paths(connection);
	if (connection->info.conn_connection_state == C_STANDALONE)
		printI("_is_standalone;\n");
	print_options(connection->net_conf, &show_net_options_ctx, "net");

	for (peer_device = peer_devices; peer_device; peer_device = peer_device->next) {
		if (connection->ctx.ctx_peer_node_id == peer_device->ctx.ctx_peer_node_id)
			show_peer_device(peer_device);
	}

	--indent;
	printI("}\n");
}

static bool connection_has_disk_options(__u32 conn_ctx_peer_node_id, struct peer_devices_list *peer_devices)
{
	struct peer_devices_list *peer_device;
	for (peer_device = peer_devices; peer_device; peer_device = peer_device->next) {
		if (conn_ctx_peer_node_id == peer_device->ctx.ctx_peer_node_id &&
			!options_empty(peer_device->peer_device_conf, &peer_device_options_ctx)
		)
			return true;
	}

	return false;
}

static bool will_show_connection_json(struct connections_list *connection, struct peer_devices_list *peer_device)
{
	if (connection == NULL || peer_device == NULL)
		return false;

	return (connection->ctx.ctx_peer_node_id == peer_device->ctx.ctx_peer_node_id &&
				!options_empty(peer_device->peer_device_conf, &peer_device_options_ctx));

}
static void show_connection_json(struct connections_list *connection, struct peer_devices_list *peer_devices)
{
	struct peer_devices_list *peer_device;

	bool has_disk_options = connection_has_disk_options(connection->ctx.ctx_peer_node_id, peer_devices);

	print_paths(connection);
	if (connection->info.conn_connection_state == C_STANDALONE)
		printI(QUOTED("_is_standalone") ": true,\n");
	print_options_json(connection->net_conf, &show_net_options_ctx, "net", false, true);

	if (has_disk_options)
	{
		int will_print, printed;

		will_print = 0;
		for (peer_device = peer_devices; peer_device; peer_device = peer_device->next) {
			if (will_show_connection_json(connection, peer_device))
				will_print++;
		}

		printed = 0;
		printI(QUOTED("volumes") ": [\n");
		for (peer_device = peer_devices; peer_device; peer_device = peer_device->next) {
			if (will_show_connection_json(connection, peer_device))
			{
				++indent;
				printI("{\n");
				++indent;
				printI(QUOTED("volume_nr") ": %d,\n", peer_device->ctx.ctx_volume);
				print_options_json(peer_device->peer_device_conf, &peer_device_options_ctx, "disk", false, false);
				--indent;
				printI("}%s\n", printed < will_print - 1 ? "," : "");
				--indent;
				printed++;
			}
		}
		printI("],\n");
	}

	printI(QUOTED("_peer_node_id") ": %d\n", connection->ctx.ctx_peer_node_id);
}

static void show_volume_json(struct devices_list *device)
{
	printI("{\n");
	++indent;
	printI(QUOTED("volume_nr") ": %d,\n", device->ctx.ctx_volume);
	printI(QUOTED("device_minor") ": %d", device->minor);
	if (device->disk_conf.backing_dev[0]) {
		printf(",\n");
		printI(QUOTED("backing-disk") ": %s,\n", double_quote_string(device->disk_conf.backing_dev));
		switch(device->disk_conf.meta_dev_idx) {
		case DRBD_MD_INDEX_INTERNAL:
		case DRBD_MD_INDEX_FLEX_INT:
			printI(QUOTED("meta-disk") ": " QUOTED("internal"));
			break;
		case DRBD_MD_INDEX_FLEX_EXT:
			printI(QUOTED("meta-disk") ": %s",
			       double_quote_string(device->disk_conf.meta_dev));
			break;
		default:
			printI(QUOTED("meta-disk") ": %s,\n",
			       double_quote_string(device->disk_conf.meta_dev));
			printI(QUOTED("meta-disk-index") ": %d\n",
			       device->disk_conf.meta_dev_idx);
		}
	} else if (device->info.is_intentional_diskless == 1) {
		printf(",\n");
		printI(QUOTED("disk") ": " QUOTED("none"));
	}

	if (!print_options_json(device->disk_conf_nl, &attach_cmd_ctx, "disk", true, false))
		printf("\n");
	--indent;
	printI("}%s\n", device->next ? "," : ""); /* close volume */
}

static void show_volume(struct devices_list *device)
{
	printI("volume %d {\n", device->ctx.ctx_volume);
	++indent;
	printI("device\t\t\tminor %d;\n", device->minor);
	if (device->disk_conf.backing_dev[0]) {
		printI("disk\t\t\t%s;\n", double_quote_string(device->disk_conf.backing_dev));
		printI("meta-disk\t\t\t");
		switch(device->disk_conf.meta_dev_idx) {
		case DRBD_MD_INDEX_INTERNAL:
		case DRBD_MD_INDEX_FLEX_INT:
			printf("internal;\n");
			break;
		case DRBD_MD_INDEX_FLEX_EXT:
			printf("%s;\n",
			       double_quote_string(device->disk_conf.meta_dev));
			break;
		default:
			printf("%s [ %d ];\n",
			       double_quote_string(device->disk_conf.meta_dev),
			       device->disk_conf.meta_dev_idx);
		}
	} else if (device->info.is_intentional_diskless == 1) {
		printI("disk\t\t\tnone;\n");
	}

	print_options(device->disk_conf_nl, &attach_cmd_ctx, "disk");
	--indent;
	printI("}\n"); /* close volume */
}

static void show_resource_list(struct resources_list *resources_list, char* old_objname)
{
	struct resources_list *resource;

	if (resources_list == NULL)
		printf("# No currently configured DRBD found.\n");

	for (resource = resources_list; resource; resource = resource->next) {
		struct devices_list *devices, *device;
		struct connections_list *connections, *connection;
		struct peer_devices_list *peer_devices = NULL;

		struct nlattr *nla;

		if (strcmp(old_objname, "all") && strcmp(old_objname, resource->name))
			continue;

		devices = list_devices(resource->name);
		connections = sort_connections(list_connections(resource->name));
		if (devices && connections)
			peer_devices = list_peer_devices(resource->name);

		objname = resource->name;

		printI("resource \"%s\" {\n", resource->name);
		++indent;

		print_options(resource->res_opts, &resource_options_ctx, "options");

		printI("_this_host {\n");
		++indent;

		nla = nla_find_nested(resource->res_opts, __nla_type(T_node_id));
		if (nla)
			printI("node-id\t\t\t%d;\n", *(uint32_t *)nla_data(nla));

		for (device = devices; device; device = device->next)
			show_volume(device);

		--indent;
		printI("}\n");

		for (connection = connections; connection; connection = connection->next)
			show_connection(connection, peer_devices);

		--indent;
		printI("}\n\n");

		free_connections(connections);
		free_devices(devices);
		free_peer_devices(peer_devices);
	}
}

static bool will_resource_list_json(struct resources_list *resource, char* old_objname)
{
	if (resource == NULL)
		return false;

	if (strcmp(old_objname, "all") && strcmp(old_objname, resource->name))
		return false;

	return true;
}

static void show_resource_list_json(struct resources_list *resources_list, char* old_objname)
{
	struct resources_list *resource;

	printI("[\n");
	++indent;

	for (resource = resources_list; resource; resource = resource->next) {
		struct devices_list *devices, *device;
		struct connections_list *connections, *connection;
		struct peer_devices_list *peer_devices = NULL;

		struct nlattr *nla;

		if (!will_resource_list_json(resource, old_objname))
			continue;

		devices = list_devices(resource->name);
		connections = sort_connections(list_connections(resource->name));
		if (devices && connections)
			peer_devices = list_peer_devices(resource->name);

		objname = resource->name;

		printI("{\n");
		++indent;
		printI(QUOTED("resource") ": " QUOTED("%s") ",\n", resource->name);

		print_options_json(resource->res_opts, &resource_options_ctx, "options", false, true);

		printI("\"_this_host\": {\n");
		++indent;

		nla = nla_find_nested(resource->res_opts, __nla_type(T_node_id));
		if (nla)
			printI("\"node-id\": %d,\n", *(uint32_t *)nla_data(nla));

		printI(QUOTED("volumes") ": [\n");
		indent++;
		for (device = devices; device; device = device->next)
			show_volume_json(device);

		--indent;
		printI("]\n");

		--indent;
		printI("}");
		if (connections)
			printf(",");
		printf("\n");

		if (connections)
		{
			printI(QUOTED("connections") ": [\n");
			indent++;
			for (connection = connections; connection; connection = connection->next)
			{
				printI("{\n");
				indent++;
				show_connection_json(connection, peer_devices);
				indent--;
				printI("}");
				if (connection->next)
					printf(",");
				printf("\n");
			}

			indent--;
			printI("]\n");
		}

		--indent;
		printI("}%s\n", will_resource_list_json(resource->next, old_objname) ? "," :"");

		free_connections(connections);
		free_devices(devices);
		free_peer_devices(peer_devices);
	}

	indent--;
	printI("]\n");
}

static int show_cmd(struct drbd_cmd *cm, int argc, char **argv)
{
	struct resources_list *resources_list;
	char *old_objname = objname;
	int c;

	optind = 0;  /* reset getopt_long() */
	for (;;) {
		c = getopt_long(argc, argv, "Dj", show_cmd_options, 0);
		if (c == -1)
			break;
		switch(c) {
		default:
		case '?':
			return 20;
		case 'D':
			show_defaults = true;
			break;
		case 'j':
			json_output = true;
			break;
		}
	}

	resources_list = sort_resources(list_resources());

	if (json_output)
		show_resource_list_json(resources_list, old_objname);
	else
		show_resource_list(resources_list, old_objname);

	free(resources_list);
	objname = old_objname;
	return 0;
}

const char *susp_str(struct resource_info *info)
{
	static const char * const strs[] = {
		"no",
		"user",
		"no-data",
		"user,no-data",
		"fencing",
		"user,fencing",
		"no-data,fencing",
		"user,no-data,fencing",
		"quorum",
		"user,quorum",
		"no-data,quorum",
		"user,no-data,quorum",
		"fencing,quorum",
		"user,fencing,quorum",
		"no-data,fencing,quorum",
		"user,no-data,fencing,quorum",
	};
	int index =
		(info->res_susp ? 1 : 0) |
		(info->res_susp_nod ? 2 : 0) |
		(info->res_susp_fen ? 4 : 0) |
		(info->res_susp_quorum ? 8 : 0);

	return strs[index];
}

__attribute__((format(printf, 2, 3)))
int nowrap_printf(int indent, const char *format, ...)
{
	va_list ap;
	int ret;

	va_start(ap, format);
	ret = vprintf(format, ap);
	va_end(ap);

	return ret;
}

void print_resource_statistics(int indent,
			       struct resource_statistics *old,
			       struct resource_statistics *new,
			       wrap_printf_fn_t wrap_printf_f)
{
	static const char *write_ordering_str[] = {
		[WO_NONE] = "none",
		[WO_DRAIN_IO] = "drain",
		[WO_BDEV_FLUSH] = "flush",
		[WO_BIO_BARRIER] = "barrier",
	};
	uint32_t wo = new->res_stat_write_ordering;

	if ((!old ||
	     old->res_stat_write_ordering != wo || opt_fullch) &&
	    wo < ARRAY_SIZE(write_ordering_str) &&
	    write_ordering_str[wo]) {
		wrap_printf_f(indent, " write-ordering:%s", write_ordering_str[wo]);
	}
}

void print_device_statistics(int indent,
			     struct device_statistics *old,
			     struct device_statistics *new,
			     wrap_printf_fn_t wrap_printf)
{
	if (opt_statistics) {
		if (opt_verbose)
			wrap_printf(indent, " size:" U64,
				    (uint64_t)new->dev_size / 2);
		wrap_printf(indent, " read:" U64,
			    (uint64_t)new->dev_read / 2);
		wrap_printf(indent, " written:" U64,
			    (uint64_t)new->dev_write / 2);
		if (opt_verbose) {
			wrap_printf(indent, " al-writes:" U64,
				    (uint64_t)new->dev_al_writes);
			wrap_printf(indent, " bm-writes:" U64,
				    (uint64_t)new->dev_bm_writes);
			wrap_printf(indent, " upper-pending:" U32,
				    new->dev_upper_pending);
			wrap_printf(indent, " lower-pending:" U32,
				    new->dev_lower_pending);
			if (!old ||
			    old->dev_al_suspended != new->dev_al_suspended || opt_fullch)
				wrap_printf(indent, " al-suspended:%s",
					    new->dev_al_suspended ? "yes" : "no");
		}
	}
	if ((!old ||
	     old->dev_upper_blocked != new->dev_upper_blocked ||
	     old->dev_lower_blocked != new->dev_lower_blocked) &&
	    new->dev_size != -1 &&
	    (opt_verbose ||
	     new->dev_upper_blocked ||
	     new->dev_lower_blocked)) {
		const char *x1 = "", *x2 = "";
		bool first = true;

		if (new->dev_upper_blocked) {
			x1 = ",upper" + first;
			first = false;
		}
		if (new->dev_lower_blocked) {
			x2 = ",lower" + first;
			first = false;
		}
		if (first)
			x1 = "no";

		wrap_printf(indent, " blocked:%s%s", x1, x2);
	}
}

void print_connection_statistics(int indent,
				 struct connection_statistics *old,
				 struct connection_statistics *new,
				 wrap_printf_fn_t wrap_printf)
{
	if (!old ||
	    old->conn_congested != new->conn_congested || opt_fullch)
		wrap_printf(indent, " congested:%s", new->conn_congested ? "yes" : "no");
	if (new->ap_in_flight != -1ULL) {
		wrap_printf(indent, " ap-in-flight:"U64, (uint64_t)new->ap_in_flight);
		wrap_printf(indent, " rs-in-flight:"U64, (uint64_t)new->rs_in_flight);
	}
}

static char *bool2json(bool b)
{
	return b ? "true" : "false";
}

static void address_json(void *address, int addr_len, char *indent)
{
	union {
		struct sockaddr     addr;
		struct sockaddr_in  addr4;
		struct sockaddr_in6 addr6;
	} a;

	/* avoid alignment issues on certain platforms (e.g. armel) */
	memset(&a, 0, sizeof(a));
	memcpy(&a.addr, address, addr_len);

	if (a.addr.sa_family == AF_INET
			|| a.addr.sa_family == get_af_ssocks(0)
			|| a.addr.sa_family == AF_INET_SDP) {
		printf("%s\"address\": \"%s\",\n", indent, inet_ntoa(a.addr4.sin_addr));
		printf("%s\"port\": %u,\n", indent, ntohs(a.addr4.sin_port));
	} else if (a.addr.sa_family == AF_INET6) {
		char address_buffer[ADDRESS_STR_MAX];
		address_buffer[0] = 0;
		/* inet_ntop does not include scope info */
		getnameinfo(&a.addr, addr_len, address_buffer, sizeof(address_buffer),
			NULL, 0, NI_NUMERICHOST|NI_NUMERICSERV);
		printf("%s\"address\": \"%s\",\n", indent, address_buffer);
		printf("%s\"port\": %u,\n", indent, ntohs(a.addr6.sin6_port));
	}

	printf("%s\"family\": \"%s\"\n", indent, af_to_str(a.addr.sa_family));
}

static void path_status_json(struct paths_list *path)
{
	printf("        {\n");
	printf("          \"this_host\": {\n");
	address_json(path->ctx.ctx_my_addr, path->ctx.ctx_my_addr_len, "            ");
	printf("          },\n");
	printf("          \"remote_host\": {\n");
	address_json(path->ctx.ctx_peer_addr, path->ctx.ctx_peer_addr_len, "            ");
	printf("          },\n");
	printf("          \"established\": %s\n", bool2json(path->info.path_established));
	printf("        }");
}

static void peer_device_status_json(struct peer_devices_list *peer_device)
{
	struct peer_device_statistics *s = &peer_device->statistics;
	bool sync_details =
		(s->peer_dev_rs_total != 0) &&
		(s->peer_dev_rs_total != -1ULL);
	bool in_resync_without_details =
		(s->peer_dev_rs_total == -1ULL) &&
		(peer_device->info.peer_repl_state >= L_SYNC_SOURCE &&
		 peer_device->info.peer_repl_state <= L_PAUSED_SYNC_T &&
		 peer_device->info.peer_repl_state != L_VERIFY_S &&
		 peer_device->info.peer_repl_state != L_VERIFY_T);

	printf("        {\n"
	       "          \"volume\": %d,\n"
	       "          \"replication-state\": \"%s\",\n"
	       "          \"peer-disk-state\": \"%s\",\n"
	       "          \"peer-client\": %s,\n"
	       "          \"resync-suspended\": \"%s\",\n"
	       "          \"received\": " U64 ",\n"
	       "          \"sent\": " U64 ",\n"
	       "          \"out-of-sync\": " U64 ",\n"
	       "          \"pending\": " U32 ",\n"
	       "          \"unacked\": " U32",\n"
	       "          \"has-sync-details\": %s,\n"
	       "          \"has-online-verify-details\": %s,\n"
	       "          \"percent-in-sync\": %.2f%s\n",
	       peer_device->ctx.ctx_volume,
	       drbd_repl_str(peer_device->info.peer_repl_state),
	       drbd_disk_str(peer_device->info.peer_disk_state),
	       bool2json(peer_device->info.peer_is_intentional_diskless == 1),
	       resync_susp_str(&peer_device->info),
	       (uint64_t)s->peer_dev_received / 2,
	       (uint64_t)s->peer_dev_sent / 2,
	       (uint64_t)s->peer_dev_out_of_sync / 2,
	       s->peer_dev_pending,
	       s->peer_dev_unacked,
	       bool2json(sync_details),
	       bool2json(sync_details && s->peer_dev_ov_left),
	       /* a volume of size 0 is what? 100% in sync? 0% in sync? */
	       peer_device->device->statistics.dev_size == 0 ? 0 :
	       100 * (1 - (double)peer_device->statistics.peer_dev_out_of_sync /
		      (double)peer_device->device->statistics.dev_size),
	       (sync_details || in_resync_without_details) ? "," : "");

	if (sync_details) {
		double db, dt;
		uint64_t sectors_to_go;

		printf("          \"rs-total\": "U64",\n"
		       "          \"rs-dt-start-ms\": "D64",\n"
		       "          \"rs-paused-ms\": "D64",\n"
		       "          \"rs-dt0-ms\": "D64",\n"
		       "          \"rs-db0-sectors\": "D64",\n"
		       "          \"rs-dt1-ms\": "D64",\n"
		       "          \"rs-db1-sectors\": "D64",\n",
		       (uint64_t)s->peer_dev_rs_total,
		       (uint64_t)s->peer_dev_rs_dt_start_ms,
		       (uint64_t)s->peer_dev_rs_paused_ms,
		       (uint64_t)s->peer_dev_rs_dt0_ms,
		       (uint64_t)s->peer_dev_rs_db0_sectors,
		       (uint64_t)s->peer_dev_rs_dt1_ms,
		       (uint64_t)s->peer_dev_rs_db1_sectors);
		if (s->peer_dev_ov_left) {
			printf(
		       "          \"ov-start-sector\": "U64",\n"
		       "          \"ov-stop-sector\": "D64",\n"
		       "          \"ov-position\": "D64",\n"
		       "          \"ov-left\": "U64",\n"
		       "          \"ov-skipped\": "U64",\n",
		       (uint64_t)s->peer_dev_ov_start_sector,
		       (uint64_t)s->peer_dev_ov_stop_sector,
		       (uint64_t)s->peer_dev_ov_position,
		       (uint64_t)s->peer_dev_ov_left,
		       (uint64_t)s->peer_dev_ov_skipped);
		} else {
			printf(
		       "          \"rs-failed\": "U64",\n"
		       "          \"rs-same-csum\": "U64",\n",
		       (uint64_t)s->peer_dev_resync_failed,
		       (uint64_t)s->peer_dev_rs_same_csum);
		}

		printf("          \"want\": %.2f,\n",
				s->peer_dev_rs_c_sync_rate ? s->peer_dev_rs_c_sync_rate / 1024.0 : 0.0);

		db = (int64_t) s->peer_dev_rs_db0_sectors;
		dt = s->peer_dev_rs_dt0_ms ?: 1;
		printf("          \"db0/dt0 [MiB/s]\": %.2f,\n",
			db/dt /* sectors/ms */
			*1000.0/2048.0 /* MiB/s */);
		db = (int64_t) s->peer_dev_rs_db1_sectors;
		dt = s->peer_dev_rs_dt1_ms ?: 1;
		printf("          \"db1/dt1 [MiB/s]\": %.2f,\n", db/dt *1000.0/2048.0);


		sectors_to_go = s->peer_dev_ov_left ?:
			s->peer_dev_out_of_sync - s->peer_dev_resync_failed;

		/* estimate time-to-run, based on "db1/dt1" */
		printf("          \"estimated-seconds-to-finish\": %.0f,\n",
			db <= 0 ? 987654321 : dt * 1e-3 * sectors_to_go / db);

		db = s->peer_dev_rs_total - sectors_to_go;
		dt = s->peer_dev_rs_dt_start_ms - s->peer_dev_rs_paused_ms;
		printf("          \"db/dt [MiB/s]\": %.2f,\n", db/(dt?:1) *1000.0/2048.0);

		printf("          \"percent-resync-done\": %.2f\n",
			s->peer_dev_rs_total == 0 ? 0 :
			100.0 * db/(double)s->peer_dev_rs_total);
	} else if (in_resync_without_details) {
		printf("          \"percent-resync-done\": %.2f\n",
			100 * (1 - (double)peer_device->statistics.peer_dev_out_of_sync /
			(double)peer_device->device->statistics.dev_size));
	}
	printf("        }");
}

static void connection_status_json(struct connections_list *connection,
				   struct peer_devices_list *peer_devices,
				   struct paths_list *paths)
{
	struct peer_devices_list *peer_device;
	struct paths_list *path;
	int path_index = 0;
	int i = 0;
	struct nlattr *tls_nla = nla_find_nested(connection->net_conf, __nla_type(T_tls));

	printf("    {\n"
	       "      \"peer-node-id\": %d,\n"
	       "      \"name\": \"%s\",\n"
	       "      \"connection-state\": \"%s\", \n"
	       "      \"congested\": %s,\n"
	       "      \"peer-role\": \"%s\",\n"
	       "      \"tls\": %s,\n",
	       connection->ctx.ctx_peer_node_id,
	       connection->ctx.ctx_conn_name,
	       drbd_conn_str(connection->info.conn_connection_state),
	       bool2json(connection->statistics.conn_congested),
	       drbd_role_str(connection->info.conn_role),
	       bool2json(tls_nla && *(uint8_t *)nla_data(tls_nla)));

	if (connection->statistics.ap_in_flight != -1ULL) {
		printf("      \"ap-in-flight\": "U64",\n"
		       "      \"rs-in-flight\": "U64",\n",
			(uint64_t)connection->statistics.ap_in_flight,
			(uint64_t)connection->statistics.rs_in_flight);
	}

	if (genl_op_known(drbd_sock->s_family, DRBD_ADM_GET_PATHS)) {
		printf("      \"paths\": [\n");
		for (path = paths; path; path = path->next) {
			if (connection->ctx.ctx_peer_node_id != path->ctx.ctx_peer_node_id)
				continue;
			if (path_index)
				puts(",");
			path_status_json(path);
			path_index++;
		}
		printf(" ],\n");
	}

	printf("      \"peer_devices\": [\n");
	for (peer_device = peer_devices; peer_device; peer_device = peer_device->next) {
		if (connection->ctx.ctx_peer_node_id != peer_device->ctx.ctx_peer_node_id)
			continue;
		if (i)
			puts(",");
		peer_device_status_json(peer_device);
		i++;
	}
	printf(" ]\n    }");
}

static void device_status_json(struct devices_list *device)
{
	enum drbd_disk_state disk_state = device->info.dev_disk_state;
	bool d_statistics = (device->statistics.dev_size != -1);

	printf("    {\n"
	       "      \"volume\": %d,\n"
	       "      \"minor\": %d,\n"
	       "      \"disk-state\": \"%s\",\n"
	       "      \"client\": %s,\n"
	       "      \"quorum\": %s%s\n",
	       device->ctx.ctx_volume,
	       device->minor,
	       drbd_disk_str(disk_state),
	       bool2json(device->info.is_intentional_diskless == 1),
	       bool2json(device->info.dev_has_quorum),
	       d_statistics ? "," : "");

	if (d_statistics) {
		struct device_statistics *s = &device->statistics;

		printf("      \"size\": " U64 ",\n"
		       "      \"read\": " U64 ",\n"
		       "      \"written\": " U64 ",\n"
		       "      \"al-writes\": " U64 ",\n"
		       "      \"bm-writes\": " U64 ",\n"
		       "      \"upper-pending\": " U32 ",\n"
		       "      \"lower-pending\": " U32 "\n",
		       (uint64_t)s->dev_size / 2,
		       (uint64_t)s->dev_read / 2,
		       (uint64_t)s->dev_write / 2,
		       (uint64_t)s->dev_al_writes,
		       (uint64_t)s->dev_bm_writes,
		       s->dev_upper_pending,
		       s->dev_lower_pending);
	}
	printf("    }");
}

static void resource_status_json(struct resources_list *resource)
{
	static const char *write_ordering_str[] = {
		[WO_NONE] = "none",
		[WO_DRAIN_IO] = "drain",
		[WO_BDEV_FLUSH] = "flush",
		[WO_BIO_BARRIER] = "barrier",
	};

	struct nlattr *nla;
	int node_id = -1;
	bool suspended =
		resource->info.res_susp ||
		resource->info.res_susp_nod ||
		resource->info.res_susp_fen ||
		resource->info.res_susp_quorum;

	nla = nla_find_nested(resource->res_opts, __nla_type(T_node_id));
	if (nla)
		node_id = *(uint32_t *)nla_data(nla);

	printf("{\n"
	       "  \"name\": \"%s\",\n"
	       "  \"node-id\": %d,\n"
	       "  \"role\": \"%s\",\n"
	       "  \"suspended\": %s,\n"
	       "  \"suspended-user\": %s,\n"
	       "  \"suspended-no-data\": %s,\n"
	       "  \"suspended-fencing\": %s,\n"
	       "  \"suspended-quorum\": %s,\n"
	       "  \"force-io-failures\": %s,\n"
	       "  \"write-ordering\": \"%s\",\n"
	       "  \"devices\": [\n",
	       resource->name,
	       node_id,
	       drbd_role_str(resource->info.res_role),
	       bool2json(suspended),
	       bool2json(resource->info.res_susp),
	       bool2json(resource->info.res_susp_nod),
	       bool2json(resource->info.res_susp_fen),
	       bool2json(resource->info.res_susp_quorum),
	       bool2json(resource->info.res_fail_io),
	       write_ordering_str[resource->statistics.res_stat_write_ordering]);
}


void print_peer_device_statistics(int indent,
				  struct peer_device_statistics *old,
				  struct peer_device_statistics *s,
				  wrap_printf_fn_t wrap_printf)
{
	double db, dt;
	uint64_t sectors_to_go = 0;
	bool sync_details =
		(s->peer_dev_rs_total != 0) &&
		(s->peer_dev_rs_total != -1ULL);

	if (sync_details)
		sectors_to_go = s->peer_dev_ov_left ?:
			s->peer_dev_out_of_sync - s->peer_dev_resync_failed;

	if (indent == 0) { /* called from print_event() */
		if (sync_details)
			wrap_printf(indent, " done:%.2f", 100.0 *
					(double)(s->peer_dev_rs_total - sectors_to_go) /
					(double)s->peer_dev_rs_total);
		if (!opt_statistics)
			return;
	}
	/* else (indent != 0), called from peer_device_status(),
	 * we printed the "done" percentage already */

	wrap_printf(indent, " received:" U64,
		    (uint64_t)s->peer_dev_received / 2);
	wrap_printf(indent, " sent:" U64,
		    (uint64_t)s->peer_dev_sent / 2);
	if (opt_verbose || s->peer_dev_out_of_sync)
		wrap_printf(indent, " out-of-sync:" U64,
			    (uint64_t)s->peer_dev_out_of_sync / 2);
	if (!opt_verbose)
		return;

	wrap_printf(indent, " pending:" U32,
		    s->peer_dev_pending);
	wrap_printf(indent, " unacked:" U32,
		    s->peer_dev_unacked);

	if (!sync_details)
		return;

	if (opt_verbose > 1) {
		wrap_printf(indent, " rs-total:" U64, (uint64_t) s->peer_dev_rs_total);
		wrap_printf(indent, " rs-dt-start-ms:" D64, (uint64_t) s->peer_dev_rs_dt_start_ms);
		wrap_printf(indent, " rs-paused-ms:" D64, (uint64_t) s->peer_dev_rs_paused_ms);
		wrap_printf(indent, " rs-dt0-ms:" D64, (uint64_t) s->peer_dev_rs_dt0_ms);
		wrap_printf(indent, " rs-db0-sectors:" D64, (uint64_t) s->peer_dev_rs_db0_sectors);
		wrap_printf(indent, " rs-dt1-ms:" D64, (uint64_t) s->peer_dev_rs_dt1_ms);
		wrap_printf(indent, " rs-db1-sectors:" D64, (uint64_t) s->peer_dev_rs_db1_sectors);
		if (s->peer_dev_ov_left) {
			wrap_printf(indent, " ov-start-sector:"U64, (uint64_t)s->peer_dev_ov_start_sector);
			wrap_printf(indent, " ov-stop-sector:"U64, (uint64_t)s->peer_dev_ov_stop_sector);
			wrap_printf(indent, " ov-position:"D64, (uint64_t)s->peer_dev_ov_position);
			wrap_printf(indent, " ov-left:"U64, (uint64_t)s->peer_dev_ov_left);
			wrap_printf(indent, " ov-skipped:"U64, (uint64_t)s->peer_dev_ov_skipped);
		} else {
			wrap_printf(indent, " rs-failed:"U64, (uint64_t)s->peer_dev_resync_failed);
			wrap_printf(indent, " rs-same-csum:"U64, (uint64_t)s->peer_dev_rs_same_csum);
		}

		if (s->peer_dev_rs_c_sync_rate)
			wrap_printf(indent, " want:%.2f", s->peer_dev_rs_c_sync_rate / 1024.0);

		db = s->peer_dev_rs_total - sectors_to_go;
		dt = s->peer_dev_rs_dt_start_ms - s->peer_dev_rs_paused_ms;
		wrap_printf(indent, " dbdt:%.2f", db/(dt?:1) *1000.0/2048.0);

		db = (int64_t) s->peer_dev_rs_db0_sectors;
		dt = s->peer_dev_rs_dt0_ms ?: 1;
		wrap_printf(indent, " dbdt0:%.2f",
				db/dt /* sectors/ms */
				*1000.0/2048.0 /* MiB/s */);
	}

	db = (int64_t) s->peer_dev_rs_db1_sectors;
	dt = s->peer_dev_rs_dt1_ms ?: 1;
	wrap_printf(indent, " dbdt1:%.2f", db/dt *1000.0/2048.0);

	/* estimate time-to-run, based on "db1/dt1" */
	wrap_printf(indent, " eta:%.0f", db > 0 ? dt * 1e-3 * sectors_to_go / db : NAN);
}

void resource_status(struct resources_list *resource)
{
	enum drbd_role role = resource->info.res_role;

	wrap_printf(0, "%s", resource->name);
	if (opt_verbose) {
		struct nlattr *nla;

		nla = nla_find_nested(resource->res_opts, __nla_type(T_node_id));
		if (nla)
			wrap_printf(4, " node-id:%d", *(uint32_t *)nla_data(nla));
	}
	wrap_printf(4, " role:%s%s%s", ROLE_COLOR_STRING(role, true));
	if (opt_verbose ||
	    resource->info.res_susp ||
	    resource->info.res_susp_nod ||
	    resource->info.res_susp_fen ||
	    resource->info.res_susp_quorum)
		wrap_printf(4, " suspended:%s", susp_str(&resource->info));
	if (opt_verbose || resource->info.res_fail_io)
		wrap_printf(4, " force-io-failures:%s%s%s",
			    fail_io_color_start(resource->info.res_fail_io),
			    resource->info.res_fail_io ? "yes" : "no",
			    fail_io_color_stop(resource->info.res_fail_io));
	if (opt_statistics && opt_verbose) {
		wrap_printf(4, "\n");
		print_resource_statistics(4, NULL, &resource->statistics, wrap_printf);
	}
	wrap_printf(0, "\n");
}

/* returns either the backing dev path or "none" if it is empty */
const char *backing_dev_str(struct device_info *info) {
	if (info->backing_dev_path[0] == '\0') {
		return "none";
	}

	return info->backing_dev_path;
}

static void device_status(struct devices_list *device, bool single_device, bool is_a_tty)
{
	enum drbd_disk_state disk_state = device->info.dev_disk_state;
	bool intentional_diskless = device->info.is_intentional_diskless == 1;
	int indent = 2;

	if (opt_verbose || !(single_device && device->ctx.ctx_volume == 0)) {
		wrap_printf(indent, "volume:%u",  device->ctx.ctx_volume);
		indent = 6;
		if (opt_verbose)
			wrap_printf(indent, " minor:%u", device->minor);
	}
	wrap_printf(indent, " disk:%s%s%s", DISK_COLOR_STRING(disk_state, intentional_diskless, true));
	if (disk_state == D_DISKLESS && (opt_verbose || !is_a_tty))
		wrap_printf(indent, " client:%s", intentional_diskless_str(&device->info));
	if (opt_verbose)
		wrap_printf(indent, " backing_dev:%s", backing_dev_str(&device->info));
	if (opt_verbose || !device->info.dev_has_quorum)
		wrap_printf(indent, " quorum:%s%s%s",
			    quorum_color_start(device->info.dev_has_quorum),
			    device->info.dev_has_quorum ? "yes" : "no",
			    quorum_color_stop(device->info.dev_has_quorum));
	indent = 6;
	if (device->statistics.dev_size != -1) {
		if (opt_statistics)
			wrap_printf(indent, "\n");
		print_device_statistics(indent, NULL, &device->statistics, wrap_printf);
	}
	wrap_printf(indent, "\n");
}

static const char *_intentional_diskless_str(unsigned char intentional_diskless) {
	switch (intentional_diskless) {
		case 0:
			return "no";
		case 1:
			return "yes";
		default:
			return "unknown";
	}
}

const char *intentional_diskless_str(struct device_info *info)
{
	return _intentional_diskless_str(info->is_intentional_diskless);
}

const char *peer_intentional_diskless_str(struct peer_device_info *info) {
	return _intentional_diskless_str(info->peer_is_intentional_diskless);
}

const char *resync_susp_str(struct peer_device_info *info)
{
	static const char * const strs[] = {
		"no",
		"user",
		"peer",
		"user,peer",
		"dependency",
		"user,dependency",
		"peer,dependency",
		"user,peer,dependency",
	};
	int index =
		(info->peer_resync_susp_user ? 1 : 0) |
		(info->peer_resync_susp_peer ? 2 : 0) |
		(info->peer_resync_susp_dependency ? 4 : 0);

	return strs[index];
}

static void peer_device_status(struct peer_devices_list *peer_device, bool single_device, bool is_a_tty)
{
	int indent = 4;
	bool intentional_diskless = peer_device->info.peer_is_intentional_diskless == 1;

	if (opt_verbose || !(single_device && peer_device->ctx.ctx_volume == 0)) {
		wrap_printf(indent, "volume:%d", peer_device->ctx.ctx_volume);
		indent = 8;
	}
	if (opt_verbose || peer_device->info.peer_repl_state > L_ESTABLISHED) {
		enum drbd_repl_state repl_state = peer_device->info.peer_repl_state;

		wrap_printf(indent, " replication:%s%s%s",
			    repl_state_color_start(repl_state),
			    drbd_repl_str(repl_state),
			    repl_state_color_stop(repl_state));
		indent = 8;
	}
	if (opt_verbose || opt_statistics ||
	    peer_device->info.peer_repl_state != L_OFF ||
	    peer_device->info.peer_disk_state != D_UNKNOWN) {
		enum drbd_disk_state disk_state = peer_device->info.peer_disk_state;
		struct peer_device_statistics *s = &peer_device->statistics;
		bool sync_details =
			(s->peer_dev_rs_total != 0) &&
			(s->peer_dev_rs_total != -1ULL);
		bool in_resync_without_details =
			(s->peer_dev_rs_total == -1ULL) &&
			(peer_device->info.peer_repl_state >= L_SYNC_SOURCE &&
			 peer_device->info.peer_repl_state <= L_PAUSED_SYNC_T &&
			 peer_device->info.peer_repl_state != L_VERIFY_S &&
			 peer_device->info.peer_repl_state != L_VERIFY_T);

		wrap_printf(indent, " peer-disk:%s%s%s", DISK_COLOR_STRING(disk_state, intentional_diskless, false));
		if (disk_state == D_DISKLESS && (opt_verbose || !is_a_tty))
			wrap_printf(indent, " peer-client:%s", peer_intentional_diskless_str(&peer_device->info));
		indent = 8;

		if (in_resync_without_details) {
			wrap_printf(indent, " done:%.2f", 100 * (1 -
				(double)peer_device->statistics.peer_dev_out_of_sync /
				(double)peer_device->device->statistics.dev_size));
		} else if (sync_details) {
			uint64_t sectors_to_go =
				s->peer_dev_ov_left ?:
				s->peer_dev_out_of_sync - s->peer_dev_resync_failed;
			wrap_printf(indent, " done:%.2f", 100.0 *
					(double)(s->peer_dev_rs_total - sectors_to_go) /
				        (double)s->peer_dev_rs_total);
		}

		if (opt_verbose ||
		    peer_device->info.peer_resync_susp_user ||
		    peer_device->info.peer_resync_susp_peer ||
		    peer_device->info.peer_resync_susp_dependency)
			wrap_printf(indent, " resync-suspended:%s",
				    resync_susp_str(&peer_device->info));
		if (opt_statistics && peer_device->statistics.peer_dev_received != -1) {
			wrap_printf(indent, "\n");
			print_peer_device_statistics(indent, NULL, &peer_device->statistics, wrap_printf);
		}
	}

	wrap_printf(0, "\n");
}

static void peer_devices_status(struct drbd_cfg_context *ctx,
		struct peer_devices_list *peer_devices,
		bool single_device, bool is_a_tty)
{
	struct peer_devices_list *peer_device;

	for (peer_device = peer_devices; peer_device; peer_device = peer_device->next) {
		if (ctx->ctx_peer_node_id != peer_device->ctx.ctx_peer_node_id)
			continue;
		peer_device_status(peer_device, single_device, is_a_tty);
	}
}

static void connection_status(struct connections_list *connection,
			      struct peer_devices_list *peer_devices,
			      bool single_device, bool is_a_tty)
{
	if (connection->ctx.ctx_conn_name_len)
		wrap_printf(2, "%s", connection->ctx.ctx_conn_name);

	if (opt_verbose || connection->ctx.ctx_conn_name_len == 0) {
		int in = connection->ctx.ctx_conn_name_len ? 6 : 2;
		wrap_printf(in, " node-id:%d", connection->ctx.ctx_peer_node_id);
	}
	if (opt_verbose || connection->info.conn_connection_state != C_CONNECTED) {
		enum drbd_conn_state cstate = connection->info.conn_connection_state;
		wrap_printf(6, " connection:%s%s%s",
			    cstate_color_start(cstate),
			    drbd_conn_str(cstate),
			    cstate_color_stop(cstate));
	}
	if (opt_verbose || connection->info.conn_connection_state == C_CONNECTED) {
		enum drbd_role role = connection->info.conn_role;
		wrap_printf(6, " role:%s%s%s",
			    role_color_start(role, false),
			    drbd_role_str(role),
			    role_color_stop(role, false));

		struct nlattr *tls_nla = nla_find_nested(connection->net_conf, __nla_type(T_tls));
		if (opt_verbose || (tls_nla && *(uint8_t *)nla_data(tls_nla)))
			wrap_printf(6, " tls:%s",
				    tls_nla && *(uint8_t *)nla_data(tls_nla) ? "yes" : "no");
	}
	if (opt_verbose || connection->statistics.conn_congested > 0)
		print_connection_statistics(6, NULL, &connection->statistics, wrap_printf);
	wrap_printf(0, "\n");
	if (opt_verbose || opt_statistics || connection->info.conn_connection_state == C_CONNECTED)
		peer_devices_status(&connection->ctx, peer_devices, single_device, is_a_tty);
}

static void stop_colors(int sig)
{
	printf("%s", stop_color_code());
	signal(sig, SIG_DFL);
	raise(sig);
}

static void link_peer_devices_to_devices(struct peer_devices_list *peer_devices, struct devices_list *devices)
{
	struct peer_devices_list *peer_device;
	struct devices_list *device;

	for (peer_device = peer_devices; peer_device; peer_device = peer_device->next) {
		for (device = devices; device; device = device->next) {
			if (peer_device->ctx.ctx_volume == device->ctx.ctx_volume) {
				peer_device->device = device;
				break;
			}
		}
	}
}

static int status_cmd(struct drbd_cmd *cm, int argc, char **argv)
{
	struct resources_list *resources, *resource;
	struct sigaction sa = {
		.sa_handler = stop_colors,
		.sa_flags = SA_RESETHAND,
	};
	bool found = false;
	bool json = false;
	int c;

	optind = 0;  /* reset getopt_long() */
	for (;;) {
		c = getopt_long(argc, argv, make_optstring(cm->options), cm->options, 0);
		if (c == -1)
			break;
		switch(c) {
		default:
		case '?':
			return 20;
		case 'v':
			++opt_verbose;
			break;
		case 's':
			opt_statistics = true;
			break;
		case 'c':
			if (!parse_color_argument())
				print_usage_and_exit("unknown --color argument");
			break;
		case 'j':
			json = true;
			break;
		}
	}

	resources = sort_resources(list_resources());

	if (resources == NULL && !json)
		printf("# No currently configured DRBD found.\n");

	sigaction(SIGHUP, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGPIPE, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	if (json)
		puts("[");

	for (resource = resources; resource; resource = resource->next) {
		struct devices_list *devices, *device;
		struct connections_list *connections, *connection;
		struct peer_devices_list *peer_devices = NULL;
		struct paths_list *paths = NULL;
		bool single_device;
		static bool jsonisfirst = true;

		if (strcmp(objname, "all") && strcmp(objname, resource->name))
			continue;
		if (json)
			jsonisfirst ? jsonisfirst = false : puts(",");

		devices = list_devices(resource->name);
		connections = sort_connections(list_connections(resource->name));
		if (devices && connections)
			peer_devices = list_peer_devices(resource->name);
		if (connections && genl_op_known(drbd_sock->s_family, DRBD_ADM_GET_PATHS))
			paths = list_paths(resource->name);

		link_peer_devices_to_devices(peer_devices, devices);

		if (json) {
			resource_status_json(resource);
			for (device = devices; device; device = device->next) {
				device_status_json(device);
				if (device->next)
					puts(",");
			}
			puts(" ],\n  \"connections\": [");
			for (connection = connections; connection; connection = connection->next) {
				connection_status_json(connection, peer_devices, paths);
				if (connection->next)
					puts(",");
			}
			puts(" ]\n}");
		} else {
			bool is_a_tty = isatty(fileno(stdout));
			resource_status(resource);
			single_device = devices && !devices->next;
			for (device = devices; device; device = device->next)
				device_status(device, single_device, is_a_tty);
			for (connection = connections; connection; connection = connection->next)
				connection_status(connection, peer_devices, single_device, is_a_tty);
			wrap_printf(0, "\n");
		}

		free_connections(connections);
		free_devices(devices);
		free_peer_devices(peer_devices);
		found = true;
	}

	if (json)
		puts("]\n");

	free_resources(resources);
	if (!found && strcmp(objname, "all")) {
		fprintf(stderr, "%s: No such resource\n", objname);
		return 10;
	}
	return 0;
}

static int role_cmd(struct drbd_cmd *cm, int argc, char **argv)
{
	struct resources_list *resources, *resource;
	int ret = ERR_RES_NOT_KNOWN;

	resources = list_resources();

	for (resource = resources; resource; resource = resource->next) {
		if (strcmp(objname, resource->name))
			continue;

		printf("%s\n", drbd_role_str(resource->info.res_role));
		ret = NO_ERROR;
		break;
	}

	free_resources(resources);

	if (ret != NO_ERROR) {
		fprintf(stderr, "%s: %s\n", objname, error_to_string(ret));
		return 10;
	}
	return 0;
}

static int peer_role_cmd(struct drbd_cmd *cm, int argc, char **argv)
{
	struct connections_list *connections, *connection;
	bool found = false;

	connections = list_connections(objname);
	for (connection = connections; connection; connection = connection->next) {
		if (connection->ctx.ctx_peer_node_id != global_ctx.ctx_peer_node_id)
			continue;

		printf("%s\n", drbd_role_str(connection->info.conn_role));
		found = true;
		break;
	}
	free_connections(connections);

	if (!found) {
		fprintf(stderr, "%s: No such connection\n", objname);
		return 10;
	}
	return 0;
}

static int cstate_cmd(struct drbd_cmd *cm, int argc, char **argv)
{
	struct connections_list *connections, *connection;
	bool found = false;

	connections = list_connections(objname);
	for (connection = connections; connection; connection = connection->next) {
		if (connection->ctx.ctx_peer_node_id != global_ctx.ctx_peer_node_id)
			continue;

		printf("%s\n", drbd_conn_str(connection->info.conn_connection_state));
		found = true;
		break;
	}
	free_connections(connections);

	if (!found) {
		fprintf(stderr, "%s: No such connection\n", objname);
		return 10;
	}
	return 0;
}

static int dstate_cmd(struct drbd_cmd *cm, int argc, char **argv)
{
	struct devices_list *devices, *device;
	bool found = false;
	struct peer_devices_list *peer_devices, *peer_device;

	devices = list_devices(NULL);
	for (device = devices; device; device = device->next) {
		if (device->minor != minor)
			continue;

		printf("%s", drbd_disk_str(device->info.dev_disk_state));
		/* printf("%s/%s\n",drbd_disk_str(state.disk),drbd_disk_str(state.pdsk)); */

		peer_devices = list_peer_devices(device->ctx.ctx_resource_name);
		for (peer_device = peer_devices;
				peer_device;
				peer_device = peer_device->next) {

			if (device->ctx.ctx_volume == peer_device->ctx.ctx_volume)
				printf("/%s", drbd_disk_str(peer_device->info.peer_disk_state));
		}
		printf("\n");
		found = true;
		break;
	}
	free_devices(devices);

	if (!found) {
		fprintf(stderr, "%s: No such device\n", objname);
		return 10;
	}
	return 0;
}

static int udev_cmd(struct drbd_cmd *cm, int argc, char **argv)
{
	struct devices_list *devices, *device;
	bool found = false;

	devices = list_devices(NULL);
	for (device = devices; device; device = device->next) {
		if (device->minor != minor)
			continue;

		printf("DEVICE=drbd%u\n", device->minor);
		printf("SYMLINK_BY_RES=drbd/by-res/%s/%u\n", device->ctx.ctx_resource_name, device->ctx.ctx_volume);
		if (!strncmp("/dev/", device->disk_conf.backing_dev, 5))
			printf("SYMLINK_BY_DISK=drbd/by-disk/%s\n", device->disk_conf.backing_dev + 5);
		else if (device->disk_conf.backing_dev[0])
			printf("SYMLINK_BY_DISK=drbd/by-disk/%s\n", device->disk_conf.backing_dev);

		found = true;
		break;
	}
	free_devices(devices);

	if (!found) {
		fprintf(stderr, "%s: No such device\n", objname);
		return 10;
	}
	return 0;
}

static char *af_to_str(int af)
{
	if (af == AF_INET)
		return "ipv4";
	else if (af == AF_INET6)
		return "ipv6";
	/* AF_SSOCKS typically is 27, the same as AF_INET_SDP.
	 * But with warn_and_use_default = 0, it will stay at -1 if not available.
	 * Just keep the test on ssocks before the one on SDP (which is hard-coded),
	 * and all should be fine.  */
	else if (af == get_af_ssocks(0))
		return "ssocks";
	else if (af == AF_INET_SDP)
		return "sdp";
	else return "unknown";
}

char *address_str(char *buffer, void* address, int addr_len)
{
	union {
		struct sockaddr     addr;
		struct sockaddr_in  addr4;
		struct sockaddr_in6 addr6;
	} a;

	/* avoid alignment issues on certain platforms (e.g. armel) */
	memset(&a, 0, sizeof(a));
	memcpy(&a.addr, address, addr_len);
	if (a.addr.sa_family == AF_INET
	|| a.addr.sa_family == get_af_ssocks(0)
	|| a.addr.sa_family == AF_INET_SDP) {
		snprintf(buffer, ADDRESS_STR_MAX, "%s:%s:%u",
			 af_to_str(a.addr4.sin_family),
			 inet_ntoa(a.addr4.sin_addr),
			 ntohs(a.addr4.sin_port));
		return buffer;
	} else if (a.addr.sa_family == AF_INET6) {
		char buf2[ADDRESS_STR_MAX];
		int n;
		buf2[0] = 0;
		/* inet_ntop does not include scope info */
		getnameinfo(&a.addr, addr_len, buf2, sizeof(buf2),
			NULL, 0, NI_NUMERICHOST|NI_NUMERICSERV);
		n = snprintf(buffer, ADDRESS_STR_MAX, "%s:[%s]:%u",
		        af_to_str(a.addr6.sin6_family), buf2,
		        ntohs(a.addr6.sin6_port));
		assert(n > 0);
		assert(n < ADDRESS_STR_MAX); /* there should be no need to truncate */
		return buffer;
	} else
		return NULL;
}

struct resources_list *new_resource_from_info(struct genl_info *info)
{
	struct drbd_cfg_context cfg = { .ctx_volume = -1U, .ctx_peer_node_id = -1U };
	struct nlattr *res_opts = info->attrs[DRBD_NLA_RESOURCE_OPTS];
	struct resources_list *r;

	drbd_cfg_context_from_attrs(&cfg, info);
	r = calloc(1, sizeof(*r));

	r->name = strdup(cfg.ctx_resource_name);
	if (res_opts) {
		int size = nla_total_size(nla_len(res_opts));

		r->res_opts = malloc(size);
		memcpy(r->res_opts, res_opts, size);
	}
	resource_info_from_attrs(&r->info, info);
	memset(&r->statistics, -1, sizeof(r->statistics));
	resource_statistics_from_attrs(&r->statistics, info);

	return r;
}

static int remember_resource(struct drbd_cmd *cmd, struct genl_info *info, void *u_ptr)
{
	struct resources_list ***tail = u_ptr;

	if (info) {
		struct resources_list *r = new_resource_from_info(info);
		PTR_NONNULL_OR_EXIT2(tail);
		**tail = r;
		*tail = &r->next;
	}
	return 0;
}

void free_resources(struct resources_list *resources)
{
	while (resources) {
		struct resources_list *r = resources;
		resources = resources->next;
		free(r->name);
		free(r->res_opts);
		free(r);
	}
}

static int resource_name_cmp(const struct resources_list * const *a, const struct resources_list * const *b)
{
	return strcmp((*a)->name, (*b)->name);
}

static struct resources_list *sort_resources(struct resources_list *resources)
{
	struct resources_list *r;
	int n;

	for (r = resources, n = 0; r; r = r->next)
		n++;
	if (n > 1) {
		struct resources_list **array;

		array = malloc(sizeof(*array) * n);
		for (r = resources, n = 0; r; r = r->next)
			array[n++] = r;
		qsort(array, n, sizeof(*array), (int (*)(const void *, const void *)) resource_name_cmp);
		n--;
		array[n]->next = NULL;
		for (; n > 0; n--)
			array[n - 1]->next = array[n];
		resources = array[0];
		free(array);
	}
	return resources;
}

/*
 * Expects objname to be set to the resource name or "all".
 */
static struct resources_list *list_resources(void)
{
	struct drbd_cmd cmd = {
		.cmd_id = DRBD_ADM_GET_RESOURCES,
		.handle_reply = remember_resource,
		.missing_ok = false,
	};
	struct resources_list *list = NULL, **tail = &list;
	char *old_objname = objname;
	int old_my_addr_len = global_ctx.ctx_my_addr_len;
	int old_peer_addr_len = global_ctx.ctx_peer_addr_len;
	int err;

	objname = "all";
	global_ctx.ctx_my_addr_len = 0;
	global_ctx.ctx_peer_addr_len = 0;
	err = generic_get(&cmd, 120000, &tail);
	objname = old_objname;
	global_ctx.ctx_my_addr_len = old_my_addr_len;
	global_ctx.ctx_peer_addr_len = old_peer_addr_len;
	if (err) {
		free_resources(list);
		list = NULL;
	}

	return list;
}

struct devices_list *new_device_from_info(struct genl_info *info)
{
	struct drbd_cfg_context ctx = { .ctx_volume = -1U, .ctx_peer_node_id = -1U };
	struct nlattr *disk_conf_nl = info->attrs[DRBD_NLA_DISK_CONF];
	struct devices_list *d = NULL;

	drbd_cfg_context_from_attrs(&ctx, info);

	if (ctx.ctx_volume == -1U)
		return NULL;

	d = calloc(1, sizeof(*d));

	d->minor = ((struct drbd_genlmsghdr*)(info->userhdr))->minor;
	d->ctx = ctx;
	if (disk_conf_nl) {
		int size = nla_total_size(nla_len(disk_conf_nl));

		d->disk_conf_nl = malloc(size);
		memcpy(d->disk_conf_nl, disk_conf_nl, size);
	}
	disk_conf_from_attrs(&d->disk_conf, info);
	d->info.dev_disk_state = D_DISKLESS;
	d->info.is_intentional_diskless = IS_INTENTIONAL_DEF;
	device_info_from_attrs(&d->info, info);
	memset(&d->statistics, -1, sizeof(d->statistics));
	device_statistics_from_attrs(&d->statistics, info);

	return d;
}

static int remember_device(struct drbd_cmd *cm, struct genl_info *info, void *u_ptr)
{
	struct devices_list ***tail = u_ptr;

	if (info) {
		struct devices_list *d = new_device_from_info(info);
		PTR_NONNULL_OR_EXIT2(tail);
		**tail = d;
		*tail = &d->next;
	}
	return 0;
}

/*
 * Expects objname to be set to the resource name or "all".
 */
static struct devices_list *list_devices(char *resource_name)
{
	struct drbd_cmd cmd = {
		.cmd_id = DRBD_ADM_GET_DEVICES,
		.handle_reply = remember_device,
		.missing_ok = false,
	};
	struct devices_list *list = NULL, **tail = &list;
	char *old_objname = objname;
	int old_my_addr_len = global_ctx.ctx_my_addr_len;
	int old_peer_addr_len = global_ctx.ctx_peer_addr_len;
	int err;

	objname = resource_name ? resource_name : "all";
	global_ctx.ctx_my_addr_len = 0;
	global_ctx.ctx_peer_addr_len = 0;
	err = generic_get(&cmd, 120000, &tail);
	objname = old_objname;
	global_ctx.ctx_my_addr_len = old_my_addr_len;
	global_ctx.ctx_peer_addr_len = old_peer_addr_len;
	if (err) {
		free_devices(list);
		list = NULL;
	}
	return list;
}

void free_device(struct devices_list *device)
{
	free(device->disk_conf_nl);
	free(device);
}

void free_devices(struct devices_list *devices)
{
	while (devices) {
		struct devices_list *d = devices;
		devices = devices->next;
		free_device(d);
	}
}

struct connections_list *new_connection_from_info(struct genl_info *info)
{
	struct drbd_cfg_context ctx = { .ctx_volume = -1U, .ctx_peer_node_id = -1U };
	struct nlattr *net_conf = info->attrs[DRBD_NLA_NET_CONF];
	struct nlattr *path_list = info->attrs[DRBD_NLA_PATH_PARMS];
	struct connections_list *c;

	drbd_cfg_context_from_attrs(&ctx, info);
	c = calloc(1, sizeof(*c));

	c->ctx = ctx;
	if (net_conf) {
		int size = nla_total_size(nla_len(net_conf));

		c->net_conf = malloc(size);
		memcpy(c->net_conf, net_conf, size);
	}
	if (path_list) {
		int size = nla_total_size(nla_len(path_list));
		c->path_list = malloc(size);
		memcpy(c->path_list, path_list, size);
	}
	connection_info_from_attrs(&c->info, info);
	memset(&c->statistics, -1, sizeof(c->statistics));
	connection_statistics_from_attrs(&c->statistics, info);

	return c;
}

static int remember_connection(struct drbd_cmd *cmd, struct genl_info *info, void *u_ptr)
{
	struct connections_list ***tail = u_ptr;

	if (info) {
		struct connections_list *c = new_connection_from_info(info);
		PTR_NONNULL_OR_EXIT2(tail);
		**tail = c;
		*tail = &c->next;
	}
	return 0;
}

static int connection_name_cmp(const struct connections_list * const *a, const struct connections_list * const *b)
{
	if (!(*a)->ctx.ctx_conn_name_len != !(*b)->ctx.ctx_conn_name_len)
		return !(*b)->ctx.ctx_conn_name_len;
	return strcmp((*a)->ctx.ctx_conn_name, (*b)->ctx.ctx_conn_name);
}

static struct connections_list *sort_connections(struct connections_list *connections)
{
	struct connections_list *c;
	int n;

	for (c = connections, n = 0; c; c = c->next)
		n++;
	if (n > 1) {
		struct connections_list **array;

		array = malloc(sizeof(*array) * n);
		for (c = connections, n = 0; c; c = c->next)
			array[n++] = c;
		qsort(array, n, sizeof(*array), (int (*)(const void *, const void *)) connection_name_cmp);
		n--;
		array[n]->next = NULL;
		for (; n > 0; n--)
			array[n - 1]->next = array[n];
		connections = array[0];
		free(array);
	}
	return connections;
}

/*
 * Expects objname to be set to the resource name or "all".
 */
static struct connections_list *list_connections(char *resource_name)
{
	struct drbd_cmd cmd = {
		.cmd_id = DRBD_ADM_GET_CONNECTIONS,
		.handle_reply = remember_connection,
		.missing_ok = true,
	};
	struct connections_list *list = NULL, **tail = &list;
	char *old_objname = objname;
	int old_my_addr_len = global_ctx.ctx_my_addr_len;
	int old_peer_addr_len = global_ctx.ctx_peer_addr_len;
	int err;

	objname = resource_name ? resource_name : "all";
	global_ctx.ctx_my_addr_len = 0;
	global_ctx.ctx_peer_addr_len = 0;
	err = generic_get(&cmd, 120000, &tail);
	objname = old_objname;
	global_ctx.ctx_my_addr_len = old_my_addr_len;
	global_ctx.ctx_peer_addr_len = old_peer_addr_len;
	if (err) {
		free_connections(list);
		list = NULL;
	}
	return list;
}

void free_connection(struct connections_list *connection)
{
	free(connection->path_list);
	free(connection->net_conf);
	free_peer_devices(connection->peer_devices);
	free_paths(connection->paths);
	free(connection);
}

void free_connections(struct connections_list *connections)
{
	while (connections) {
		struct connections_list *l = connections;
		connections = connections->next;
		free_connection(l);
	}
}

struct peer_devices_list *new_peer_device_from_info(struct genl_info *info)
{
	struct drbd_cfg_context ctx = { .ctx_volume = -1U, .ctx_peer_node_id = -1U };
	struct nlattr *peer_device_conf = info->attrs[DRBD_NLA_PEER_DEVICE_OPTS];
	struct peer_devices_list *p;

	drbd_cfg_context_from_attrs(&ctx, info);
	p = calloc(1, sizeof(*p));
	if (!p)
		exit(20);

	p->ctx = ctx;
	if (peer_device_conf) {
		int size = nla_total_size(nla_len(peer_device_conf));
		p->peer_device_conf = malloc(size);
		memcpy(p->peer_device_conf, peer_device_conf, size);
	}
	p->info.peer_is_intentional_diskless = IS_INTENTIONAL_DEF;
	peer_device_info_from_attrs(&p->info, info);
	memset(&p->statistics, -1, sizeof(p->statistics));
	peer_device_statistics_from_attrs(&p->statistics, info);

	return p;

}
static int remember_peer_device(struct drbd_cmd *cmd, struct genl_info *info, void *u_ptr)
{
	struct peer_devices_list ***tail = u_ptr;

	if (info) {
		struct peer_devices_list *p = new_peer_device_from_info(info);
		PTR_NONNULL_OR_EXIT2(tail);
		**tail = p;
		*tail = &p->next;
	}
	return 0;
}

/*
 * Expects objname to be set to the resource name or "all".
 */
static struct peer_devices_list *list_peer_devices(char *resource_name)
{
	struct drbd_cmd cmd = {
		.cmd_id = DRBD_ADM_GET_PEER_DEVICES,
		.handle_reply = remember_peer_device,
		.missing_ok = false,
	};
	struct peer_devices_list *list = NULL, **tail = &list;
	char *old_objname = objname;
	int old_my_addr_len = global_ctx.ctx_my_addr_len;
	int old_peer_addr_len = global_ctx.ctx_peer_addr_len;
	int err;

	objname = resource_name ? resource_name : "all";
	global_ctx.ctx_my_addr_len = 0;
	global_ctx.ctx_peer_addr_len = 0;
	err = generic_get(&cmd, 120000, &tail);
	objname = old_objname;
	global_ctx.ctx_my_addr_len = old_my_addr_len;
	global_ctx.ctx_peer_addr_len = old_peer_addr_len;
	if (err) {
		free_peer_devices(list);
		list = NULL;
	}
	return list;
}

void free_peer_device(struct peer_devices_list *peer_device)
{
	free(peer_device->peer_device_conf);
	free(peer_device);
}

void free_peer_devices(struct peer_devices_list *peer_devices)
{
	while (peer_devices) {
		struct peer_devices_list *p = peer_devices;
		peer_devices = peer_devices->next;
		free_peer_device(p);
	}
}

struct paths_list *new_path_from_info(struct genl_info *info)
{
	struct drbd_cfg_context ctx = { .ctx_volume = -1U, .ctx_peer_node_id = -1U };
	struct paths_list *p;

	drbd_cfg_context_from_attrs(&ctx, info);
	p = calloc(1, sizeof(*p));
	if (!p)
		exit(20);

	p->ctx = ctx;
	drbd_path_info_from_attrs(&p->info, info);

	return p;
}

static int remember_path(struct drbd_cmd *cmd, struct genl_info *info, void *u_ptr)
{
	struct paths_list ***tail = u_ptr;

	if (info) {
		struct paths_list *p = new_path_from_info(info);
		PTR_NONNULL_OR_EXIT2(tail);
		**tail = p;
		*tail = &p->next;
	}
	return 0;
}

/*
 * Expects objname to be set to the resource name or "all".
 */
static struct paths_list *list_paths(char *resource_name)
{
	struct drbd_cmd cmd = {
		.cmd_id = DRBD_ADM_GET_PATHS,
		.handle_reply = remember_path,
		.missing_ok = false,
	};
	struct paths_list *list = NULL, **tail = &list;
	char *old_objname = objname;
	int old_my_addr_len = global_ctx.ctx_my_addr_len;
	int old_peer_addr_len = global_ctx.ctx_peer_addr_len;
	int err;

	objname = resource_name ? resource_name : "all";
	global_ctx.ctx_my_addr_len = 0;
	global_ctx.ctx_peer_addr_len = 0;
	err = generic_get(&cmd, 120000, &tail);
	objname = old_objname;
	global_ctx.ctx_my_addr_len = old_my_addr_len;
	global_ctx.ctx_peer_addr_len = old_peer_addr_len;
	if (err) {
		free_paths(list);
		list = NULL;
	}
	return list;
}

void free_paths(struct paths_list *paths)
{
	while (paths) {
		struct paths_list *p = paths;
		paths = paths->next;
		free(p);
	}
}

static int check_resize_cmd(struct drbd_cmd *cm, int argc, char **argv)
{
	struct devices_list *devices, *device;
	bool found = false;
	bool ret = 0;
	char *bdev_userland_name;

	devices = list_devices(NULL);
	for (device = devices; device; device = device->next) {
		struct bdev_info bd = { 0, };
		uint64_t bd_size;
		int fd;

		if (device->minor != minor)
			continue;
		found = true;

		if (device->disk_conf.meta_dev_idx >= 0 ||
		    device->disk_conf.meta_dev_idx == DRBD_MD_INDEX_FLEX_EXT) {
			lk_bdev_delete(minor);
			break;
		}

		bdev_userland_name = kernel_device_to_userland_device(device->disk_conf.backing_dev);
		fd = open(bdev_userland_name, O_RDONLY);
		if (fd == -1) {
			fprintf(stderr, "Could not open %s: %m.\n", device->disk_conf.backing_dev);
			ret = 1;
			break;
		}
		bd_size = bdev_size(fd);
		close(fd);

		if (lk_bdev_load(minor, &bd) == 0 &&
		    bd.bd_size == bd_size &&
		    bd.bd_name && !strcmp(bd.bd_name, device->disk_conf.backing_dev))
			break;	/* nothing changed. */

		bd.bd_size = bd_size;
		bd.bd_name = device->disk_conf.backing_dev;
		lk_bdev_save(minor, &bd);
		break;
	}
	free_devices(devices);

	if (!found) {
		fprintf(stderr, "%s: No such device\n", objname);
		return 10;
	}
	return ret;
}

static bool peer_device_ctx_match(struct drbd_cfg_context *a, struct drbd_cfg_context *b)
{
	return
		strcmp(a->ctx_resource_name, b->ctx_resource_name) == 0
	&&	a->ctx_peer_node_id == b->ctx_peer_node_id
	&&	a->ctx_volume == b->ctx_volume;
}

static int show_or_get_gi_cmd(struct drbd_cmd *cm, int argc, char **argv)
{
	struct peer_devices_list *peer_devices, *peer_device;
	struct devices_list *devices = NULL, *device;
	uint64_t uuids[UI_SIZE];
	int ret = 0, i;

	peer_devices = list_peer_devices(NULL);
	for (peer_device = peer_devices; peer_device; peer_device = peer_device->next) {
		if (!peer_device_ctx_match(&global_ctx, &peer_device->ctx))
			continue;

		devices = list_devices(peer_device->ctx.ctx_resource_name);
		for (device = devices; device; device = device->next) {
			if (device->ctx.ctx_volume == global_ctx.ctx_volume)
				goto found;
		}
	}
	fprintf(stderr, "%s: No such peer device\n", objname);
	ret = 10;

out:
	free_devices(devices);
	free_peer_devices(peer_devices);
	return ret;

found:
	if (peer_device->info.peer_repl_state == L_OFF &&
	    device->info.dev_disk_state == D_DISKLESS) {
		fprintf(stderr, "Device is unconfigured\n");
		ret = 1;
		goto out;
	}
	if (device->info.dev_disk_state == D_DISKLESS) {
		/* XXX we could print the exposed_data_uuid anyways: */
		if (0)
			printf(X64(016)"\n", (uint64_t)device->statistics.dev_exposed_data_uuid);
		fprintf(stderr, "Device has no disk\n");
		ret = 1;
		goto out;
	}
	memset(uuids, 0, sizeof(uuids));
	uuids[UI_CURRENT] = device->statistics.dev_current_uuid;
	uuids[UI_BITMAP] = peer_device->statistics.peer_dev_bitmap_uuid;
	i = device->statistics.history_uuids_len / 8;
	if (i >= HISTORY_UUIDS_V08)
		i = HISTORY_UUIDS_V08 - 1;
	for (; i >= 0; i--)
		uuids[UI_HISTORY_START + i] =
			((uint64_t *)device->statistics.history_uuids)[i];
	if(!strcmp(cm->cmd, "show-gi"))
		dt_pretty_print_v9_uuids(uuids, device->statistics.dev_disk_flags,
					 peer_device->statistics.peer_dev_flags);
	else
		dt_print_v9_uuids(uuids, device->statistics.dev_disk_flags,
				  peer_device->statistics.peer_dev_flags);
	goto out;
}

static int down_cmd(struct drbd_cmd *cm, int argc, char **argv)
{
	struct resources_list *resources, *resource;
	char *old_objname;
	int rv = 0;

	if(argc > 2) {
		warn_print_excess_args(argc, argv, 2);
		return OTHER_ERROR;
	}

	old_objname = objname;
	context = CTX_RESOURCE;

	resources = list_resources();
	for (resource = resources; resource; resource = resource->next) {
		struct devices_list *devices;
		int rv2;

		if (strcmp(old_objname, "all") && strcmp(old_objname, resource->name))
			continue;

		objname = resource->name;
		devices = list_devices(objname);
		rv2 = _generic_config_cmd(cm, argc, argv);
		if (!rv2) {
			struct devices_list *device;

			for (device = devices; device; device = device->next)
				unregister_minor(device->minor);
			unregister_resource(objname);
		}
		if (!rv)
			rv = rv2;
		free_devices(devices);
	}
	free_resources(resources);
	return rv;
}

void peer_devices_append(struct peer_devices_list *peer_devices, struct genl_info *info)
{
	struct peer_devices_list *peer_device, **tail = NULL;

	if (!peer_devices)
		return;

	for (peer_device = peer_devices; peer_device; peer_device = peer_device->next)
		tail = &peer_device->next;

	remember_peer_device(NULL, info, &tail);
}

/* Actually waits for all volumes of a connection... */
static int wait_for_family(struct drbd_cmd *cm, struct genl_info *info, void *u_ptr)
{
	struct peer_devices_list *peer_devices = u_ptr;
	struct drbd_cfg_context ctx = { .ctx_volume = -1U, .ctx_peer_node_id = -1U };
	struct drbd_notification_header nh = { .nh_type = -1U };
	struct drbd_genlmsghdr *dh;
	int err;

	if (!info)
		return 0;

	err = drbd_cfg_context_from_attrs(&ctx, info);
	if (err)
		return 0;

	err = drbd_notification_header_from_attrs(&nh, info);
	if (err)
		return 0;

	dh = info->userhdr;
	if (dh->ret_code != NO_ERROR)
		return dh->ret_code;

	if ((nh.nh_type & ~NOTIFY_FLAGS) == NOTIFY_DESTROY)
		return 0;

	switch(info->genlhdr->cmd) {
	case DRBD_CONNECTION_STATE: {
		struct connection_info connection_info;

		if ((nh.nh_type & ~NOTIFY_FLAGS) == NOTIFY_CREATE)
			break; /* Ignore C_STANDALONE while creating it */

		err = connection_info_from_attrs(&connection_info, info);
		if (err) {
			dbg(1, "connection info missing\n");
			break;
		}
		if (connection_info.conn_connection_state < C_UNCONNECTED) {
			if (!wait_after_split_brain)
				return -1;  /* done waiting */

			fprintf(stderr, "\ndrbd %s connection to peer-id %u ('%s') is %s, "
				       "but I'm configured to wait anways (--wait-after-sb)\n",
				       ctx.ctx_resource_name, ctx.ctx_peer_node_id, ctx.ctx_conn_name,
				       drbd_conn_str(connection_info.conn_connection_state));
		}
		break;
	}
	case DRBD_PEER_DEVICE_STATE: {
		struct peer_device_info peer_device_info;
		struct peer_devices_list *peer_device;
		int nr_peer_devices = 0, nr_done = 0;
		bool wait_connect;

		err = peer_device_info_from_attrs(&peer_device_info, info);
		if (err) {
			dbg(1, "peer device info missing\n");
			break;
		}

		wait_connect = strstr(cm->cmd, "sync") == NULL;

		if ((nh.nh_type & ~NOTIFY_FLAGS) == NOTIFY_CREATE)
			peer_devices_append(peer_devices, info);

		for (peer_device = peer_devices;
		     peer_device;
		     peer_device = peer_device->next) {
			enum drbd_repl_state rs;

			if (peer_device_ctx_match(&ctx, &peer_device->ctx))
				peer_device->info = peer_device_info;

			/* wait-*-volume: filter out all but the specific peer device */
			if (cm->ctx_key == CTX_PEER_DEVICE &&
			    !peer_device_ctx_match(&global_ctx, &peer_device->ctx))
				continue;

			/* wait-*-connection: filter out other connections */
			if (cm->ctx_key == CTX_PEER_NODE &&
			    peer_device->ctx.ctx_peer_node_id != global_ctx.ctx_peer_node_id)
				continue;

			/* wait-*-resource: no filter */
			nr_peer_devices++;

			rs = peer_device->info.peer_repl_state;
			if (rs == L_ESTABLISHED ||
			    (wait_connect && rs > L_ESTABLISHED) ||
			    peer_device->timeout_ms == 0)
				nr_done++;
		}

		if (nr_peer_devices == nr_done)
			return -1; /* Done with waiting */

		break;
	}
	}

	return 0;
}

/*
 * Check if an integer is a power of two.
 */
static bool power_of_two(int i)
{
	return i && !(i & (i - 1));
}

static void print_command_usage(struct drbd_cmd *cm, enum usage_type ut)
{
	struct drbd_argument *args;

	if(ut == XML) {
		printf("<command name=\"%s\">\n", cm->cmd);
		if (cm->summary)
			printf("\t<summary>%s</summary>\n", cm->summary);
		if (cm->ctx_key && ut != BRIEF) {
			enum cfg_ctx_key ctx = cm->ctx_key, arg;
			bool more_than_one_choice =
				!power_of_two(ctx & ~CTX_MULTIPLE_ARGUMENTS) &&
				!(ctx & CTX_MULTIPLE_ARGUMENTS);
			const char *indent = "\t\t" + !more_than_one_choice;

			if (more_than_one_choice)
				printf("\t<group>\n");
			ctx |= CTX_MULTIPLE_ARGUMENTS;
			for (arg = ctx_next_arg(&ctx); arg; arg = ctx_next_arg(&ctx))
				printf("%s<argument>%s</argument>\n",
				       indent, ctx_arg_string(arg, ut));
			if (more_than_one_choice)
				printf("\t</group>\n");
		}

		if(cm->drbd_args) {
			for (args = cm->drbd_args; args->name; args++) {
				printf("\t<argument>%s</argument>\n",
				       args->name);
			}
		}

		if (cm->options) {
			struct option *option;

			for (option = cm->options; option->name; option++) {
				/*
				 * The "string" options here really are
				 * timeouts, but we can't describe them
				 * in a resonable way here.
				 */
				printf("\t<option name=\"%s\" type=\"%s\">\n"
				       "\t</option>\n",
				       option->name,
				       option->has_arg == no_argument ?
					 "flag" : "string");
			}
		}

		if (cm->set_defaults)
			printf("\t<option name=\"set-defaults\" type=\"flag\">\n"
			       "\t</option>\n");

		if (cm->ctx) {
			struct field_def *field;

			for (field = cm->ctx->fields; field->name; field++)
				field->ops->describe_xml(field);
		}
		printf("</command>\n");
		return;
	}

	if (ut == BRIEF) {
		wrap_printf(4, "%s - ", cm->cmd);
		if (cm->summary)
			wrap_printf_wordwise(8, cm->summary);
		wrap_printf(4, "\n");
	} else {
		wrap_printf(0, "%s %s", progname, cm->cmd);
		if (cm->summary)
			wrap_printf(4, " - %s", cm->summary);
		wrap_printf(4, "\n\n");

		wrap_printf(0, "USAGE: %s %s", progname, cm->cmd);

		if (cm->ctx_key && ut != BRIEF) {
			enum cfg_ctx_key ctx = cm->ctx_key, arg;
			bool more_than_one_choice =
				!power_of_two(ctx & ~CTX_MULTIPLE_ARGUMENTS) &&
				!(ctx & CTX_MULTIPLE_ARGUMENTS);
			bool first = true;

			if (more_than_one_choice)
				wrap_printf(4, " {");
			ctx |= CTX_MULTIPLE_ARGUMENTS;
			for (arg = ctx_next_arg(&ctx); arg; arg = ctx_next_arg(&ctx)) {
				if (more_than_one_choice && !first)
					wrap_printf(4, " |");
				first = false;
				wrap_printf(4, " %s", ctx_arg_string(arg, ut));
			}
			if (more_than_one_choice)
				wrap_printf(4, " }");
		}

		if (cm->drbd_args) {
			for (args = cm->drbd_args; args->name; args++)
				wrap_printf(4, " {%s}", args->name);
		}

		if (cm->options || cm->set_defaults || cm->ctx)
			wrap_printf(4, "\n");

		if (cm->options) {
			struct option *option;

			for (option = cm->options; option->name; option++)
				wrap_printf(4, " [--%s%s]",
					    option->name,
					    option->has_arg == no_argument ?
					        "" : "=...");
		}

		if (cm->set_defaults)
			wrap_printf(4, " [--set-defaults]");

		if (cm->ctx) {
			struct field_def *field;

			for (field = cm->ctx->fields; field->name; field++) {
				char buffer[300];
				int n;
				n = field->ops->usage(field, buffer, sizeof(buffer));
				assert(n < sizeof(buffer));
				wrap_printf(4, " %s", buffer);
			}
		}
		wrap_printf(4, "\n");
	}
}

static void print_usage_and_exit(const char *addinfo)
{
	size_t i;

	printf("drbdsetup - Configure the DRBD kernel module.\n\n"
	       "USAGE: %s command {arguments} [options]\n"
	       "\nCommands:\n",cmdname);


	for (i = 0; i < ARRAY_SIZE(commands); i++)
		print_command_usage(&commands[i], BRIEF);

	printf("\nUse 'drbdsetup help command' for command-specific help.\n\n");
	if (addinfo)  /* FIXME: ?! */
		printf("\n%s\n", addinfo);

	exit(20);
}

static void maybe_exec_legacy_drbdsetup(char **argv)
{
	const struct version *driver_version = drbd_driver_version(FALLBACK_TO_UTILS);

	if (driver_version->version.major == 8 &&
	    driver_version->version.minor == 4) {
#ifdef DRBD_LEGACY_84
		static const char * const drbdsetup_84 = "drbdsetup-84";

		add_lib_drbd_to_path();
		execvp(drbdsetup_84, argv);
		fprintf(stderr, "execvp() failed to exec %s: %m\n", drbdsetup_84);
#else
		config_help_legacy("drbdsetup", driver_version);
#endif
		exit(20);
	}
}

int drbdsetup_main(int argc, char **argv)
{
	struct drbd_cmd *cmd;
	struct option *options;
	const char *opts;
	int c, rv = 0;
	int longindex, first_optind;

	if (argv == NULL || argc < 1) {
		fputs("drbdsetup: Nonexistent or empty arguments array, aborting.\n", stderr);
		abort();
	}

	initialize_logging();
	progname = basename(argv[0]);

	if (chdir("/")) {
		/* highly unlikely, but gcc is picky */
		perror("cannot chdir /");
		return -111;
	}

	cmdname = strrchr(argv[0],'/');
	if (cmdname)
		argv[0] = ++cmdname;
	else
		cmdname = argv[0];

	if (argc > 2 && (!strcmp(argv[2], "--help")  || !strcmp(argv[2], "-h"))) {
		char *swap = argv[1];
		argv[1] = argv[2];
		argv[2] = swap;
	}

	if (argc > 1 && (!strcmp(argv[1], "help") || !strcmp(argv[1], "xml-help")  ||
			 !strcmp(argv[1], "--help")  || !strcmp(argv[1], "-h"))) {
		enum usage_type usage_type = !strcmp(argv[1], "xml-help") ? XML : FULL;
		if(argc > 2) {
			cmd = find_cmd_by_name(argv[2]);
			if(cmd) {
				print_command_usage(cmd, usage_type);
				exit(0);
			} else
				print_usage_and_exit("unknown command");
		} else
			print_usage_and_exit(NULL);
	}

	/*
	 * drbdsetup previously took the object to operate on as its first argument,
	 * followed by the command.  For backwards compatibility, still support his.
	 */
	if (argc >= 3 && !find_cmd_by_name(argv[1]) && find_cmd_by_name(argv[2])) {
		char *swap = argv[1];
		argv[1] = argv[2];
		argv[2] = swap;
	}

	if (argc < 2)
		print_usage_and_exit(NULL);

	if (!modprobe_drbd()) {
		if (!strcmp(argv[1], "down") ||
		    !strcmp(argv[1], "secondary") ||
		    !strcmp(argv[1], "disconnect") ||
		    !strcmp(argv[1], "detach"))
			return 0; /* "down" succeeds even if drbd is missing */
		return 20;
	}

	maybe_exec_legacy_drbdsetup(argv);

	cmd = find_cmd_by_name(argv[1]);
	if (!cmd)
		print_usage_and_exit("invalid command");

	/* Make argv[0] the command name so that getopt_long() will leave it in
	 * the first position. */
	argv++;
	argc--;

	options = make_longoptions(cmd);
	opts = make_optstring(options);
	for (;;) {
		c = getopt_long(argc, argv, opts, options, &longindex);
		if (c == -1)
			break;
		if (c == '?' || c == ':')
			print_usage_and_exit(NULL);
	}
	/* All non-option arguments now are in argv[optind .. argc - 1]. */
	first_optind = optind;

	if (cmd->continuous_poll && kernel_older_than(2, 6, 23)) {
		/* with newer kernels, we need to use setsockopt NETLINK_ADD_MEMBERSHIP */
		/* maybe more specific: (1 << GENL_ID_CTRL)? */
		drbd_genl_family.nl_groups = -1;
	}
	drbd_sock = genl_connect_to_family(&drbd_genl_family);
	if (!drbd_sock) {
		fprintf(stderr, "Could not connect to 'drbd' generic netlink family\n");
		return 20;
	}

	if (drbd_genl_family.version != GENL_MAGIC_VERSION ||
	    drbd_genl_family.hdrsize != sizeof(struct drbd_genlmsghdr)) {
		fprintf(stderr, "API mismatch!\n\t"
			"API version drbdsetup: %u kernel: %u\n\t"
			"header size drbdsetup: %u kernel: %u\n",
			GENL_MAGIC_VERSION, drbd_genl_family.version,
			(unsigned)sizeof(struct drbd_genlmsghdr),
			drbd_genl_family.hdrsize);
		return 20;
	}

	context = 0;
	enum cfg_ctx_key ctx_key = cmd->ctx_key, next_arg;
	for (next_arg = ctx_next_arg(&ctx_key);
	     next_arg;
	     next_arg = ctx_next_arg(&ctx_key), optind++) {
		if (argc == optind &&
		    !(ctx_key & CTX_MULTIPLE_ARGUMENTS) && (next_arg & CTX_ALL)) {
			context |= CTX_ALL;  /* assume "all" if no argument is given */
			objname = "all";
			break;
		} else if (argc <= optind) {
			fprintf(stderr, "Missing argument %d to command\n", optind);
			print_command_usage(cmd, FULL);
			exit(20);
		} else if (next_arg & (CTX_RESOURCE | CTX_MINOR | CTX_ALL)) {
			ensure_sanity_of_res_name(argv[optind]);
			if (!objname)
				objname = argv[optind];
			if (!strcmp(argv[optind], "all")) {
				if (!(next_arg & CTX_ALL))
					print_usage_and_exit("command does not accept argument 'all'");
				context |= CTX_ALL;
			} else if (next_arg & CTX_MINOR) {
				minor = dt_minor_of_dev(argv[optind]);
				if (minor == -1U && next_arg == CTX_MINOR) {
					fprintf(stderr, "Cannot determine minor device number of "
							"device '%s'\n",
						argv[optind]);
					exit(20);
				}
				context |= CTX_MINOR;
			} else /* not "all", and not a minor number/device name */ {
				if (!(next_arg & CTX_RESOURCE)) {
					fprintf(stderr, "command does not accept argument '%s'\n",
						objname);
					print_command_usage(cmd, FULL);
					exit(20);
				}
				context |= CTX_RESOURCE;
				assert(strlen(objname) < sizeof(global_ctx.ctx_resource_name));
				memset(global_ctx.ctx_resource_name, 0, sizeof(global_ctx.ctx_resource_name));
				global_ctx.ctx_resource_name_len = strlen(objname);
				strcpy(global_ctx.ctx_resource_name, objname);
			}
		} else {
			if (next_arg == CTX_MY_ADDR) {
				const char *str = argv[optind];
				struct sockaddr_storage *x;

				if (strncmp(str, "local:", 6) == 0)
					str += 6;
				assert(sizeof(global_ctx.ctx_my_addr) >= sizeof(*x));
				x = (struct sockaddr_storage *)&global_ctx.ctx_my_addr;
				global_ctx.ctx_my_addr_len = sockaddr_from_str(x, str);
			} else if (next_arg == CTX_PEER_ADDR) {
				const char *str = argv[optind];
				struct sockaddr_storage *x;

				if (strncmp(str, "peer:", 5) == 0)
					str += 5;
				assert(sizeof(global_ctx.ctx_peer_addr) >= sizeof(*x));
				x = (struct sockaddr_storage *)&global_ctx.ctx_peer_addr;
				global_ctx.ctx_peer_addr_len = sockaddr_from_str(x, str);
			} else if (next_arg == CTX_VOLUME) {
				global_ctx.ctx_volume = m_strtoll(argv[optind], 1);
			} else if (next_arg == CTX_PEER_NODE_ID) {
				global_ctx.ctx_peer_node_id = m_strtoll(argv[optind], 1);
			} else
				assert(0);
			context |= next_arg;
		}
	}

	/* Remove the options we have already processed from argv */
	if (first_optind != optind) {
		int n;

		for (n = 0; n < argc - optind; n++)
			argv[first_optind + n] = argv[optind + n];
		argc -= optind - first_optind;
	}

	if (!objname)
		objname = "??";

	if ((context & CTX_MINOR) && !cmd->lockless)
		lock_fd = dt_lock_drbd(minor);

	rv = cmd->function(cmd, argc, argv);

	if ((context & CTX_MINOR) && !cmd->lockless)
		dt_unlock_drbd(lock_fd);
	return rv;
}
