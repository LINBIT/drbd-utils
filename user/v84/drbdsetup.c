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
#include <libgen.h>
#include <time.h>
#include <search.h>

#include <linux/netlink.h>
#include <linux/genetlink.h>

#define EXIT_NOMEM 20
#define EXIT_NO_FAMILY 20
#define EXIT_SEND_ERR 20
#define EXIT_RECV_ERR 20
#define EXIT_TIMED_OUT 20
#define EXIT_NOSOCK 30
#define EXIT_THINKO 42

/*
 * We are not using libnl,
 * using its API for the few things we want to do
 * ends up being almost as much lines of code as
 * coding the necessary bits right here.
 */

#include "libgenl.h"
#include "drbd_nla.h"
#include "linux/drbd_config.h"
#include "linux/drbd_genl_api.h"
#include "linux/drbd_limits.h"
#include "linux/genl_magic_func.h"
#include "drbdtool_common.h"
#include "registry.h"
#include "config.h"
#include "config_flags.h"
#include "wrap_printf.h"
#include "drbdsetup_colors.h"
#include "drbd_strings.h"

char *progname;

/* for parsing of messages */
static struct nlattr *global_attrs[128];
/* there is an other table, nested_attr_tb, defined in genl_magic_func.h,
 * which can be used after <struct>_from_attrs,
 * to check for presence of struct fields. */
#define ntb(t)	nested_attr_tb[__nla_type(t)]

#ifdef PRINT_NLMSG_LEN
/* I'm to lazy to check the maximum possible nlmsg length by hand */
int main(void)
{
	static __u16 nla_attr_minlen[NLA_TYPE_MAX+1] __read_mostly = {
		[NLA_U8]        = sizeof(__u8),
		[NLA_U16]       = sizeof(__u16),
		[NLA_U32]       = sizeof(__u32),
		[NLA_U64]       = sizeof(__u64),
		[NLA_NESTED]    = NLA_HDRLEN,
	};
	int i;
	int sum_total = 0;
#define LEN__(policy) do {					\
	int sum = 0;						\
	for (i = 0; i < ARRAY_SIZE(policy); i++) {		\
		sum += nla_total_size(policy[i].len ?:		\
			nla_attr_minlen[policy[i].type]);	\
								\
	}							\
	sum += 4;						\
	sum_total += sum;					\
	printf("%-30s %4u [%4u]\n",				\
			#policy ":", sum, sum_total);		\
} while (0)
#define LEN_(p) LEN__(p ## _nl_policy)
	LEN_(disk_conf);
	LEN_(syncer_conf);
	LEN_(net_conf);
	LEN_(set_role_parms);
	LEN_(resize_parms);
	LEN_(state_info);
	LEN_(start_ov_parms);
	LEN_(new_c_uuid_parms);
	sum_total += sizeof(struct nlmsghdr) + sizeof(struct genlmsghdr)
		+ sizeof(struct drbd_genlmsghdr);
	printf("sum total inclusive hdr overhead: %4u\n", sum_total);
	return 0;
}
#else

#ifndef AF_INET_SDP
#define AF_INET_SDP 27
#define PF_INET_SDP AF_INET_SDP
#endif

/* pretty print helpers */
static int indent = 0;
#define INDENT_WIDTH	4
#define printI(fmt, args... ) printf("%*s" fmt,INDENT_WIDTH * indent,"" , ## args )

enum usage_type {
	BRIEF,
	FULL,
	XML,
};

struct drbd_argument {
	const char* name;
	__u16 nla_type;
	int (*convert_function)(struct drbd_argument *,
				struct msg_buff *,
				struct drbd_genlmsghdr *dhdr,
				char *);
};

/* Configuration requests typically need a context to operate on.
 * Possible keys are device minor/volume id (both fit in the drbd_genlmsghdr),
 * the replication link (aka connection) name,
 * and/or the replication group (aka resource) name */
enum cfg_ctx_key {
	/* Only one of these can be present in a command: */
	CTX_MINOR = 1,
	CTX_RESOURCE = 2,
	CTX_ALL = 4,
	CTX_CONNECTION = 8,

	CTX_RESOURCE_AND_CONNECTION = 16,
};

struct drbd_cmd {
	const char* cmd;
	const enum cfg_ctx_key ctx_key;
	const int cmd_id;
	const int tla_id; /* top level attribute id */
	int (*function)(const struct drbd_cmd *, int, char **);
	struct drbd_argument *drbd_args;
	int (*show_function)(const struct drbd_cmd*, struct genl_info *, void *u_prt);
	struct option *options;
	bool missing_ok;
	bool warn_on_missing;
	bool continuous_poll;
	bool wait_for_connect_timeouts;
	bool set_defaults;
	bool lockless;
	struct context_def *ctx;
};

// other functions
static void print_command_usage(const struct drbd_cmd *cm, enum usage_type);

// command functions
static int generic_config_cmd(const struct drbd_cmd *cm, int argc, char **argv);
static int down_cmd(const struct drbd_cmd *cm, int argc, char **argv);
static int generic_get_cmd(const struct drbd_cmd *cm, int argc, char **argv);
static int del_minor_cmd(const struct drbd_cmd *cm, int argc, char **argv);
static int del_resource_cmd(const struct drbd_cmd *cm, int argc, char **argv);
static int status_cmd(const struct drbd_cmd *cm, int argc, char **argv);

// sub commands for generic_get_cmd
static int print_notifications(const struct drbd_cmd *, struct genl_info *, void *u_ptr);
static int show_scmd(const struct drbd_cmd *cm, struct genl_info *info, void *u_ptr);
static int role_scmd(const struct drbd_cmd *cm, struct genl_info *info, void *u_ptr);
static int sh_status_scmd(const struct drbd_cmd *cm, struct genl_info *info, void *u_ptr);
static int cstate_scmd(const struct drbd_cmd *cm, struct genl_info *info, void *u_ptr);
static int dstate_scmd(const struct drbd_cmd *cm, struct genl_info *info, void *u_ptr);
static int uuids_scmd(const struct drbd_cmd *cm, struct genl_info *info, void *u_ptr);
static int lk_bdev_scmd(const struct drbd_cmd *cm, struct genl_info *info, void *u_ptr);
static int print_broadcast_events(const struct drbd_cmd *, struct genl_info *, void *u_ptr);
static int w_connected_state(const struct drbd_cmd *, struct genl_info *, void *u_ptr);
static int w_synced_state(const struct drbd_cmd *, struct genl_info *, void *u_ptr);

// convert functions for arguments
static int conv_block_dev(struct drbd_argument *ad, struct msg_buff *msg, struct drbd_genlmsghdr *dhdr, char* arg);
static int conv_md_idx(struct drbd_argument *ad, struct msg_buff *msg, struct drbd_genlmsghdr *dhdr, char* arg);
static int conv_resource_name(struct drbd_argument *ad, struct msg_buff *msg, struct drbd_genlmsghdr *dhdr, char* arg);
static int conv_volume(struct drbd_argument *ad, struct msg_buff *msg, struct drbd_genlmsghdr *dhdr, char* arg);
static int conv_minor(struct drbd_argument *ad, struct msg_buff *msg, struct drbd_genlmsghdr *dhdr, char* arg);

struct resources_list {
	struct resources_list *next;
	char *name;
	struct nlattr *res_opts;
	struct resource_info info;
	struct resource_statistics statistics;
};
static struct resources_list *list_resources(void);
static struct resources_list *sort_resources(struct resources_list *);
static void free_resources(struct resources_list *);

struct devices_list {
	struct devices_list *next;
	unsigned minor;
	struct drbd_cfg_context ctx;
	struct disk_conf disk_conf;
	struct device_info info;
	struct device_statistics statistics;
};
static struct devices_list *list_devices(char *);
static void free_devices(struct devices_list *);

struct connections_list {
	struct connections_list *next;
	struct drbd_cfg_context ctx;
	struct nlattr *net_conf;
	struct connection_info info;
	struct connection_statistics statistics;
};
static struct connections_list *sort_connections(struct connections_list *);
static struct connections_list *list_connections(char *);
static void free_connections(struct connections_list *);

struct peer_devices_list {
	struct peer_devices_list *next;
	struct drbd_cfg_context ctx;
	struct peer_device_info info;
	struct peer_device_statistics statistics;
	struct devices_list *device;
	int timeout_ms; /* used only by wait_for_family() */
};
static struct peer_devices_list *list_peer_devices(char *);
static void free_peer_devices(struct peer_devices_list *);

struct option wait_cmds_options[] = {
	{ "wfc-timeout",required_argument, 0, 't' },
	{ "degr-wfc-timeout",required_argument,0,'d'},
	{ "outdated-wfc-timeout",required_argument,0,'o'},
	{ "wait-after-sb",optional_argument,0,'w'},
	{ 0,            0,           0,  0  }
};

struct option events_cmd_options[] = {
	{ "timestamps", no_argument, 0, 'T' },
	{ "statistics", no_argument, 0, 's' },
	{ "now", no_argument, 0, 'n' },
	{ }
};

struct option show_cmd_options[] = {
	{ "show-defaults", no_argument, 0, 'D' },
	{ }
};

static struct option status_cmd_options[] = {
	{ "verbose", no_argument, 0, 'v' },
	{ "statistics", no_argument, 0, 's' },
	{ "color", optional_argument, 0, 'c' },
	{ }
};

#define F_CONFIG_CMD	generic_config_cmd
#define NO_PAYLOAD	0
#define F_GET_CMD(scmd)	DRBD_ADM_GET_STATUS, NO_PAYLOAD, generic_get_cmd, \
			.show_function = scmd
#define F_NEW_EVENTS_CMD(scmd)	DRBD_ADM_GET_INITIAL_STATE, NO_PAYLOAD, generic_get_cmd, \
			.show_function = scmd

const struct drbd_cmd commands[] = {
	{"primary", CTX_MINOR, DRBD_ADM_PRIMARY, DRBD_NLA_SET_ROLE_PARMS,
		F_CONFIG_CMD,
	 .ctx = &primary_cmd_ctx },

	{"secondary", CTX_MINOR, DRBD_ADM_SECONDARY, NO_PAYLOAD, F_CONFIG_CMD },

	{"attach", CTX_MINOR, DRBD_ADM_ATTACH, DRBD_NLA_DISK_CONF,
		F_CONFIG_CMD,
	 .drbd_args = (struct drbd_argument[]) {
		 { "lower_dev",		T_backing_dev,	conv_block_dev },
		 { "meta_data_dev",	T_meta_dev,	conv_block_dev },
		 { "meta_data_index",	T_meta_dev_idx,	conv_md_idx },
		 { } },
	 .ctx = &attach_cmd_ctx },

	{"disk-options", CTX_MINOR, DRBD_ADM_CHG_DISK_OPTS, DRBD_NLA_DISK_CONF,
		F_CONFIG_CMD,
	 .set_defaults = true,
	 .ctx = &disk_options_ctx },

	{"detach", CTX_MINOR, DRBD_ADM_DETACH, DRBD_NLA_DETACH_PARMS, F_CONFIG_CMD,
	 .ctx = &detach_cmd_ctx },

	{"connect", CTX_RESOURCE_AND_CONNECTION, DRBD_ADM_CONNECT, DRBD_NLA_NET_CONF,
		F_CONFIG_CMD,
	 .ctx = &connect_cmd_ctx },

	{"net-options", CTX_CONNECTION, DRBD_ADM_CHG_NET_OPTS, DRBD_NLA_NET_CONF,
		F_CONFIG_CMD,
	 .set_defaults = true,
	 .ctx = &net_options_ctx },

	{"disconnect", CTX_CONNECTION, DRBD_ADM_DISCONNECT, DRBD_NLA_DISCONNECT_PARMS,
		F_CONFIG_CMD,
	 .ctx = &disconnect_cmd_ctx },

	{"resize", CTX_MINOR, DRBD_ADM_RESIZE, DRBD_NLA_RESIZE_PARMS,
		F_CONFIG_CMD,
	 .ctx = &resize_cmd_ctx },

	{"resource-options", CTX_RESOURCE, DRBD_ADM_RESOURCE_OPTS, DRBD_NLA_RESOURCE_OPTS,
		F_CONFIG_CMD,
	 .set_defaults = true,
	 .ctx = &resource_options_cmd_ctx },

	{"new-current-uuid", CTX_MINOR, DRBD_ADM_NEW_C_UUID, DRBD_NLA_NEW_C_UUID_PARMS,
		F_CONFIG_CMD,
	 .ctx = &new_current_uuid_cmd_ctx },

	{"invalidate", CTX_MINOR, DRBD_ADM_INVALIDATE, NO_PAYLOAD, F_CONFIG_CMD, },
	{"invalidate-remote", CTX_MINOR, DRBD_ADM_INVAL_PEER, NO_PAYLOAD, F_CONFIG_CMD, },
	{"pause-sync", CTX_MINOR, DRBD_ADM_PAUSE_SYNC, NO_PAYLOAD, F_CONFIG_CMD, },
	{"resume-sync", CTX_MINOR, DRBD_ADM_RESUME_SYNC, NO_PAYLOAD, F_CONFIG_CMD, },
	{"suspend-io", CTX_MINOR, DRBD_ADM_SUSPEND_IO, NO_PAYLOAD, F_CONFIG_CMD, },
	{"resume-io", CTX_MINOR, DRBD_ADM_RESUME_IO, NO_PAYLOAD, F_CONFIG_CMD, },
	{"outdate", CTX_MINOR, DRBD_ADM_OUTDATE, NO_PAYLOAD, F_CONFIG_CMD, },
	{"verify", CTX_MINOR, DRBD_ADM_START_OV, DRBD_NLA_START_OV_PARMS,
		F_CONFIG_CMD,
	 .ctx = &verify_cmd_ctx },
	{"down", CTX_RESOURCE, DRBD_ADM_DOWN, NO_PAYLOAD, down_cmd,
		.missing_ok = true,
		.warn_on_missing = true, },
	{"state", CTX_MINOR, F_GET_CMD(role_scmd) },
	{"role", CTX_MINOR, F_GET_CMD(role_scmd),
		.lockless = true, },
	{"sh-status", CTX_MINOR | CTX_RESOURCE | CTX_ALL,
		F_GET_CMD(sh_status_scmd),
		.missing_ok = true,
		.lockless = true, },
	{"cstate", CTX_MINOR, F_GET_CMD(cstate_scmd),
		.lockless = true, },
	{"dstate", CTX_MINOR, F_GET_CMD(dstate_scmd),
		.lockless = true, },
	{"show-gi", CTX_MINOR, F_GET_CMD(uuids_scmd),
		.lockless = true, },
	{"get-gi", CTX_MINOR, F_GET_CMD(uuids_scmd),
		.lockless = true, },
	{"show", CTX_MINOR | CTX_RESOURCE | CTX_ALL, F_GET_CMD(show_scmd),
		.options = show_cmd_options,
		.lockless = true, },
	{"status", CTX_RESOURCE | CTX_ALL, 0, 0, status_cmd,
		.options = status_cmd_options,
		.lockless = true, },
	{"check-resize", CTX_MINOR, F_GET_CMD(lk_bdev_scmd),
		.lockless = true, },
	{"events", CTX_MINOR | CTX_RESOURCE | CTX_ALL, F_GET_CMD(print_broadcast_events),
		.missing_ok = true,
		.continuous_poll = true,
		.lockless = true, },
	{"events2", CTX_RESOURCE | CTX_ALL, F_NEW_EVENTS_CMD(print_notifications),
		.options = events_cmd_options,
		.missing_ok = true,
		.continuous_poll = true,
		.lockless = true },
	{"wait-connect", CTX_MINOR, F_GET_CMD(w_connected_state),
		.options = wait_cmds_options,
		.continuous_poll = true,
		.wait_for_connect_timeouts = true,
		.lockless = true, },
	{"wait-sync", CTX_MINOR, F_GET_CMD(w_synced_state),
		.options = wait_cmds_options,
		.continuous_poll = true,
		.wait_for_connect_timeouts = true,
		.lockless = true, },

	{"new-resource", CTX_RESOURCE, DRBD_ADM_NEW_RESOURCE, DRBD_NLA_RESOURCE_OPTS, F_CONFIG_CMD,
	 .ctx = &resource_options_cmd_ctx },

	/* only payload is resource name and volume number */
	{"new-minor", 0, DRBD_ADM_NEW_MINOR, DRBD_NLA_CFG_CONTEXT,
		F_CONFIG_CMD,
	 .drbd_args = (struct drbd_argument[]) {
		 { "resource", T_ctx_resource_name, conv_resource_name },
		 { "minor", 0, conv_minor },
		 { "volume", T_ctx_volume, conv_volume },
		 { } },
	 .ctx = &new_minor_cmd_ctx },

	{"del-minor", CTX_MINOR, DRBD_ADM_DEL_MINOR, NO_PAYLOAD, del_minor_cmd, },
	{"del-resource", CTX_RESOURCE, DRBD_ADM_DEL_RESOURCE, NO_PAYLOAD, del_resource_cmd, }
};

bool show_defaults;
bool wait_after_split_brain;

#define OTHER_ERROR 900

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
enum cfg_ctx_key context;
char *opt_local_addr, *opt_peer_addr;

int lock_fd;

struct genl_sock *drbd_sock = NULL;
int try_genl = 1;

struct genl_family drbd_genl_family = {
	.name = "drbd",
	.version = GENL_MAGIC_VERSION,
	.hdrsize = GENL_MAGIC_FAMILY_HDRSZ,
};

static bool endpoints_equal(struct drbd_cfg_context *a, struct drbd_cfg_context *b)
{
	return a->ctx_my_addr_len == b->ctx_my_addr_len &&
	       a->ctx_peer_addr_len == b->ctx_peer_addr_len &&
	       !memcmp(a->ctx_my_addr, b->ctx_my_addr, a->ctx_my_addr_len) &&
	       !memcmp(a->ctx_peer_addr, b->ctx_peer_addr, a->ctx_peer_addr_len);
}

static int conv_block_dev(struct drbd_argument *ad, struct msg_buff *msg,
			  struct drbd_genlmsghdr *dhdr, char* arg)
{
	struct stat sb;
	int device_fd;

	if ((device_fd = open(arg,O_RDWR))==-1) {
		PERROR("Can not open device '%s'", arg);
		return OTHER_ERROR;
	}

	if (fstat(device_fd, &sb)) {
		PERROR("fstat(%s) failed", arg);
		return OTHER_ERROR;
	}

	if(!S_ISBLK(sb.st_mode)) {
		fprintf(stderr, "%s is not a block device!\n", arg);
		return OTHER_ERROR;
	}

	close(device_fd);

	nla_put_string(msg, ad->nla_type, arg);

	return NO_ERROR;
}

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

static int conv_resource_name(struct drbd_argument *ad, struct msg_buff *msg,
			      struct drbd_genlmsghdr *dhdr, char* arg)
{
	/* additional sanity checks? */
	nla_put_string(msg, T_ctx_resource_name, arg);
	return NO_ERROR;
}

static int conv_volume(struct drbd_argument *ad, struct msg_buff *msg,
		       struct drbd_genlmsghdr *dhdr, char* arg)
{
	unsigned vol = m_strtoll(arg,1);
	/* sanity check on vol < 256? */
	nla_put_u32(msg, T_ctx_volume, vol);
	return NO_ERROR;
}

static int conv_minor(struct drbd_argument *ad, struct msg_buff *msg,
		      struct drbd_genlmsghdr *dhdr, char* arg)
{
	unsigned minor = dt_minor_of_dev(arg);
	if (minor == -1U) {
		fprintf(stderr, "Cannot determine minor device number of "
				"device '%s'\n",
			arg);
		return OTHER_ERROR;
	}
	dhdr->minor = minor;
	return NO_ERROR;
}

static struct option *make_longoptions(const struct drbd_cmd *cm)
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

/* prepends global objname to output (if any) */
static int print_config_error(int err_no, char *desc)
{
	int rv=0;

	if (err_no == NO_ERROR || err_no == SS_SUCCESS)
		return 0;

	if (err_no == OTHER_ERROR) {
		if (desc)
			fprintf(stderr,"%s: %s\n", objname, desc);
		return 20;
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
	if (global_attrs[DRBD_NLA_CFG_REPLY] &&
	    global_attrs[DRBD_NLA_CFG_REPLY]->nla_len) {
		struct nlattr *nla;
		int rem;
		fprintf(stderr, "additional info from kernel:\n");
		nla_for_each_nested(nla, global_attrs[DRBD_NLA_CFG_REPLY], rem) {
			if (nla_type(nla) == __nla_type(T_info_text))
				fprintf(stderr, "%s\n", (char*)nla_data(nla));
		}
	}
	return rv;
}

static void warn_print_excess_args(int argc, char **argv, int i)
{
	fprintf(stderr, "Excess arguments:");
	for (; i < argc; i++)
		fprintf(stderr, " %s", argv[i]);
	printf("\n");
}

int drbd_tla_parse(struct nlmsghdr *nlh)
{
	return nla_parse(global_attrs, ARRAY_SIZE(drbd_tla_nl_policy)-1,
		nlmsg_attrdata(nlh, GENL_HDRLEN + drbd_genl_family.hdrsize),
		nlmsg_attrlen(nlh, GENL_HDRLEN + drbd_genl_family.hdrsize),
		drbd_tla_nl_policy);
}

#define ASSERT(exp) if (!(exp)) \
		fprintf(stderr,"ASSERT( " #exp " ) in %s:%d\n", __FILE__,__LINE__);

static int _generic_config_cmd(const struct drbd_cmd *cm, int argc,
			       char **argv, int quiet)
{
	struct drbd_argument *ad = cm->drbd_args;
	struct nlattr *nla;
	struct option *options;
	int c, i;
	int rv = NO_ERROR;
	char *desc = NULL; /* error description from kernel reply message */

	struct drbd_genlmsghdr *dhdr;
	struct msg_buff *smsg;
	struct iovec iov;

	/* pre allocate request message and reply buffer */
	iov.iov_len = DEFAULT_MSG_SIZE;
	iov.iov_base = malloc(iov.iov_len);
	smsg = msg_new(DEFAULT_MSG_SIZE);
	if (!smsg || !iov.iov_base) {
		desc = "could not allocate netlink messages";
		rv = OTHER_ERROR;
		goto error;
	}

	dhdr = genlmsg_put(smsg, &drbd_genl_family, 0, cm->cmd_id);
	dhdr->minor = -1;
	dhdr->flags = 0;

	i = 1;
	if (context & (CTX_RESOURCE | CTX_CONNECTION)) {
		nla = nla_nest_start(smsg, DRBD_NLA_CFG_CONTEXT);
		if (context & CTX_RESOURCE)
			nla_put_string(smsg, T_ctx_resource_name, objname);
		if (context & CTX_CONNECTION) {
			nla_put_address(smsg, T_ctx_my_addr, opt_local_addr);
			nla_put_address(smsg, T_ctx_peer_addr, opt_peer_addr);
		}
		nla_nest_end(smsg, nla);
	} else if (context & CTX_MINOR) {
		dhdr->minor = minor;
		i++;
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
			if (!field->put(cm->ctx, field, smsg, optarg)) {
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

	/* argc should be cmd + n options + n args;
	 * if it is more, we did not understand some */
	if (i < argc) {
		warn_print_excess_args(argc, argv, i);
		rv = OTHER_ERROR;
		goto error;
	}

	if (rv == NO_ERROR) {
		int received;

		if (nla)
			nla_nest_end(smsg, nla);
		if (genl_send(drbd_sock, smsg)) {
			desc = "error sending config command";
			rv = OTHER_ERROR;
			goto error;
		}

retry_recv:
		/* reduce timeout! limit retries */
		received = genl_recv_msgs(drbd_sock, &iov, &desc, 120000);
		if (received > 0) {
			struct nlmsghdr *nlh = (struct nlmsghdr*)iov.iov_base;
			struct drbd_genlmsghdr *dh = genlmsg_data(nlmsg_data(nlh));
			ASSERT(dh->minor == minor);
			rv = dh->ret_code;

			if (rv == ERR_RES_NOT_KNOWN) {
				if (cm->warn_on_missing && isatty(STDERR_FILENO))
					fprintf(stderr, "Resource unknown\n");

				if (cm->missing_ok)
					rv = NO_ERROR;
			}

			drbd_tla_parse(nlh);
		} else {
			if (received == -E_RCV_ERROR_REPLY && !errno)
					goto retry_recv;
			if (!desc)
				desc = "error receiving config reply";

			rv = OTHER_ERROR;
		}
	}
error:
	msg_free(smsg);

	if (!quiet)
		rv = print_config_error(rv, desc);
	free(iov.iov_base);
	return rv;
}

static int generic_config_cmd(const struct drbd_cmd *cm, int argc, char **argv)
{
	return _generic_config_cmd(cm, argc, argv, 0);
}

static int del_minor_cmd(const struct drbd_cmd *cm, int argc, char **argv)
{
	int rv;

	rv = generic_config_cmd(cm, argc, argv);
	if (!rv)
		unregister_minor(minor);
	return rv;
}

static int del_resource_cmd(const struct drbd_cmd *cm, int argc, char **argv)
{
	int rv;

	rv = generic_config_cmd(cm, argc, argv);
	if (!rv)
		unregister_resource(objname);
	return rv;
}

static const struct drbd_cmd *find_cmd_by_name(const char *name)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (!strcmp(name, commands[i].cmd)) {
			return commands + i;
		}
	}
	return NULL;
}

static void print_options(const char *cmd_name, const char *sect_name)
{
	const struct drbd_cmd *cmd;
	struct field_def *field;
	int opened = 0;

	cmd = find_cmd_by_name(cmd_name);
	if (!cmd) {
		fprintf(stderr, "%s internal error, no such cmd %s\n",
				cmdname, cmd_name);
		abort();
	}
	if (!global_attrs[cmd->tla_id])
		return;
	if (drbd_nla_parse_nested(nested_attr_tb, cmd->ctx->nla_policy_size - 1,
			     global_attrs[cmd->tla_id], cmd->ctx->nla_policy)) {
		fprintf(stderr, "nla_policy violation for %s payload!\n", sect_name);
		/* still, print those that validated ok */
	}

	if (!cmd->ctx)
		return;
	for (field = cmd->ctx->fields; field->name; field++) {
		struct nlattr *nlattr;
		const char *str;
		bool is_default;

		nlattr = ntb(field->nla_type);
		if (!nlattr)
			continue;
		if (!opened) {
			opened=1;
			printI("%s {\n",sect_name);
			++indent;
		}
		str = field->get(cmd->ctx, field, nlattr);
		is_default = field->is_default(field, str);
		if (is_default && !show_defaults)
			continue;
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

struct choose_timo_ctx {
	unsigned minor;
	struct msg_buff *smsg;
	struct iovec *iov;
	int timeout;
	int wfc_timeout;
	int degr_wfc_timeout;
	int outdated_wfc_timeout;
};

int choose_timeout(struct choose_timo_ctx *ctx)
{
	char *desc = NULL;
	struct drbd_genlmsghdr *dhdr;
	int rr;

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
	dhdr->minor = ctx->minor;
	dhdr->flags = 0;

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
			.attrs = global_attrs,
		};
		struct drbd_genlmsghdr *dh = info.userhdr;
		struct timeout_parms parms;
		ASSERT(dh->minor == ctx->minor);
		rr = dh->ret_code;
		if (rr == ERR_MINOR_INVALID) {
			desc = "minor not available";
			goto error;
		}
		if (rr != NO_ERROR)
			goto error;
		if (drbd_tla_parse(nlh)
		|| timeout_parms_from_attrs(&parms, &info)) {
			desc = "reply did not validate - "
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

#include <sys/utsname.h>
static bool kernel_older_than(int version, int patchlevel, int sublevel)
{
	struct utsname utsname;
	char *rel;
	int l;

	if (uname(&utsname) != 0)
		return false;
	rel = utsname.release;
	l = strtol(rel, &rel, 10);
	if (l > version)
		return false;
	else if (l < version || *rel == 0)
		return true;
	l = strtol(rel + 1, &rel, 10);
	if (l > patchlevel)
		return false;
	else if (l < patchlevel || *rel == 0)
		return true;
	l = strtol(rel + 1, &rel, 10);
	if (l >= sublevel)
		return false;
	return true;
}

static bool opt_now;
static bool opt_verbose;
static bool opt_statistics;
static bool opt_timestamps;

static int generic_get(const struct drbd_cmd *cm, int timeout_arg, void *u_ptr)
{
	char *desc = NULL;
	struct drbd_genlmsghdr *dhdr;
	struct msg_buff *smsg;
	struct iovec iov;
	int timeout_ms, flags;
	int rv = NO_ERROR;
	int err = 0;

	/* pre allocate request message and reply buffer */
	iov.iov_len = DEFAULT_MSG_SIZE;
	iov.iov_base = malloc(iov.iov_len);
	smsg = msg_new(DEFAULT_MSG_SIZE);
	if (!smsg || !iov.iov_base) {
		desc = "could not allocate netlink messages";
		rv = OTHER_ERROR;
		goto out;
	}

	if (cm->continuous_poll) {
		/* also always (try to) listen to nlctrl notify,
		 * so we have a chance to notice rmmod.  */
		int id = GENL_ID_CTRL;
		setsockopt(drbd_sock->s_fd, SOL_NETLINK, NETLINK_ADD_MEMBERSHIP,
					&id, sizeof(id));

		if (genl_join_mc_group(drbd_sock, "events") &&
		    !kernel_older_than(2, 6, 23)) {
			desc = "unable to join drbd events multicast group";
			rv = OTHER_ERROR;
			goto out2;
		}
	}

	flags = 0;
	if (minor == -1U)
		flags |= NLM_F_DUMP;
	dhdr = genlmsg_put(smsg, &drbd_genl_family, flags, cm->cmd_id);
	dhdr->minor = minor;
	dhdr->flags = 0;
	if (minor == -1U && strcmp(objname, "all")) {
		/* Restrict the dump to a single resource. */
		struct nlattr *nla;
		nla = nla_nest_start(smsg, DRBD_NLA_CFG_CONTEXT);
		nla_put_string(smsg, T_ctx_resource_name, objname);
		nla_nest_end(smsg, nla);
	}

	if (genl_send(drbd_sock, smsg)) {
		desc = "error sending config command";
		rv = OTHER_ERROR;
		goto out2;
	}

	/* disable sequence number check in genl_recv_msgs */
	drbd_sock->s_seq_expect = 0;

	for (;;) {
		int received, rem, ret;
		struct nlmsghdr *nlh = (struct nlmsghdr *)iov.iov_base;
		struct timeval before;
		struct pollfd pollfds[2] = {
			[0] = {
				.fd = 1,
				.events = POLLHUP,
			},
			[1] = {
				.fd = drbd_sock->s_fd,
				.events = POLLIN,
			},
		};

		gettimeofday(&before, NULL);

		timeout_ms = timeout_arg;

		ret = poll(pollfds, 2, timeout_arg);
		if (ret == 0) {
			err = 5;
			goto out2;
		}
		if (pollfds[0].revents == POLLERR || pollfds[0].revents == POLLHUP)
			goto out2;

		received = genl_recv_msgs(drbd_sock, &iov, &desc, -1);
		if (received < 0) {
			switch(received) {
			case E_RCV_TIMEDOUT:
				err = 5;
				goto out2;
			case -E_RCV_FAILED:
				err = 20;
				goto out2;
			case -E_RCV_NO_SOURCE_ADDR:
				continue; /* ignore invalid message */
			case -E_RCV_SEQ_MISMATCH:
				/* we disabled it, so it should not happen */
				err = 20;
				goto out2;
			case -E_RCV_MSG_TRUNC:
				continue;
			case -E_RCV_UNEXPECTED_TYPE:
				continue;
			case -E_RCV_NLMSG_DONE:
				if (cm->continuous_poll)
					continue;
				err = cm->show_function(cm, NULL, u_ptr);
				if (err)
					goto out2;
				err = -*(int*)nlmsg_data(nlh);
				if (err &&
				    (err != ENODEV || !cm->missing_ok)) {
					fprintf(stderr, "received netlink error reply: %s\n",
						strerror(err));
					err = 20;
				}
				goto out2;
			case -E_RCV_ERROR_REPLY:
				if (!errno) /* positive ACK message */
					continue;
				if (!desc)
					desc = strerror(errno);
				fprintf(stderr, "received netlink error reply: %s\n",
					       desc);
				err = 20;
				goto out2;
			default:
				if (!desc)
					desc = "error receiving config reply";
				err = 20;
				goto out2;
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

			timeout_ms -= elapsed_ms;
			exit = timeout_ms <= 0;

			if (exit) {
				err = 5;
				goto out2;
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
				.attrs = global_attrs,
			};

			dbg(3, "received type:%x\n", nlh->nlmsg_type);
			if (nlh->nlmsg_type < NLMSG_MIN_TYPE) {
				/* Ignore netlink control messages. */
				continue;
			}
			if (nlh->nlmsg_type == GENL_ID_CTRL) {
#ifdef HAVE_CTRL_CMD_DELMCAST_GRP
				dbg(3, "received cmd:%x\n", info.genlhdr->cmd);
				if (info.genlhdr->cmd == CTRL_CMD_DELMCAST_GRP) {
					struct nlattr *nla =
						nlmsg_find_attr(nlh, GENL_HDRLEN, CTRL_ATTR_FAMILY_ID);
					if (nla && nla_get_u16(nla) == drbd_genl_family.id) {
						/* FIXME: We could wait for the
						   multicast group to be recreated ... */
						goto out2;
					}
				}
#endif
				/* Ignore other generic netlink control messages. */
				continue;
			}
			if (nlh->nlmsg_type != drbd_genl_family.id) {
				/* Ignore messages for all other netlink families. */
				continue;
			}

			/* parse early, otherwise drbd_cfg_context_from_attrs
			 * can not work */
			if (drbd_tla_parse(nlh)) {
				/* FIXME
				 * should continuous_poll continue?
				 */
				desc = "reply did not validate - "
					"do you need to upgrade your userland tools?";
				rv = OTHER_ERROR;
				goto out2;
			}
			if (cm->continuous_poll) {
				struct drbd_cfg_context ctx;
				/*
				 * We will receive all events and have to
				 * filter for what we want ourself.
				 */
				/* FIXME
				 * Do we want to ignore broadcasts until the
				 * initial get/dump requests is done? */

				if (!drbd_cfg_context_from_attrs(&ctx, &info)) {
					switch (context) {
					case CTX_MINOR:
						/* Assert that, for an unicast reply,
						 * reply minor matches request minor.
						 * "unsolicited" kernel broadcasts are "pid=0" (netlink "port id")
						 * (and expected to be genlmsghdr.cmd == DRBD_EVENT) */
						if (minor != dh->minor) {
							if (info.nlhdr->nlmsg_pid != 0)
								dbg(1, "received netlink packet for minor %u, while expecting %u\n",
									dh->minor, minor);
							continue;
						}
						break;

					case CTX_ALL:
						break;

					case CTX_RESOURCE:
						if (strcmp(objname, ctx.ctx_resource_name))
							continue;

						break;
#if 0
					case CTX_CONNECTION:
					case CTX_CONNECTION | CTX_RESOURCE:
						if (!endpoints_equal(&ctx, &global_ctx))
							continue;
						break;
#endif
#if 0
					case CTX_PEER_DEVICE:
						if (!endpoints_equal(&ctx, &global_ctx) ||
						    ctx.ctx_volume != global_ctx.ctx_volume)
							continue;
						break;
#endif
					default:
						assert(0);
					}
				}
			}
			rv = dh->ret_code;
			if (rv == ERR_MINOR_INVALID) {
				if (cm->warn_on_missing)
					fprintf(stderr, "Minor invalid");

				if (cm->missing_ok)
					rv = NO_ERROR;
			}
			if (rv != NO_ERROR)
				goto out2;
			err = cm->show_function(cm, &info, u_ptr);
			if (err) {
				if (err < 0)
					err = 0;
				goto out2;
			}
		}
		if (!cm->continuous_poll && !(flags & NLM_F_DUMP)) {
			/* There will be no more reply packets.  */
			err = cm->show_function(cm, NULL, u_ptr);
			goto out2;
		}
	}

out2:
	msg_free(smsg);

out:
	if (!err)
		err = print_config_error(rv, desc);
	free(iov.iov_base);
	return err;
}

static int generic_get_cmd(const struct drbd_cmd *cm, int argc, char **argv)
{
	static struct option no_options[] = { { } };
	struct choose_timo_ctx timeo_ctx = {
		.wfc_timeout = DRBD_WFC_TIMEOUT_DEF,
		.degr_wfc_timeout = DRBD_DEGR_WFC_TIMEOUT_DEF,
		.outdated_wfc_timeout = DRBD_OUTDATED_WFC_TIMEOUT_DEF,
	};
	int timeout_ms = -1;  /* "infinite" */
	struct option *options = cm->options ? cm->options : no_options;
	const char *opts = make_optstring(options);

	optind = 0;  /* reset getopt_long() */
	for(;;) {
		int c = getopt_long(argc, argv, opts, options, 0);
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

		case 's':
			opt_verbose = true;
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
		}
	}
	if (optind < argc) {
		warn_print_excess_args(argc, argv, optind);
		return 20;
	}

	if (cm->wait_for_connect_timeouts) {
		/* wait-connect, wait-sync */
		struct msg_buff *smsg;
		struct iovec iov;
		int rr;

		iov.iov_len = 8192;
		iov.iov_base = malloc(iov.iov_len);
		smsg = msg_new(DEFAULT_MSG_SIZE);
		if (!smsg || !iov.iov_base) {
			fprintf(stderr, "could not allocate netlink messages\n");
			return 20;
		}

		timeo_ctx.minor = minor;
		timeo_ctx.smsg = smsg;
		timeo_ctx.iov = &iov;
		rr = choose_timeout(&timeo_ctx);
		if (rr)
			return rr;
		if (timeo_ctx.timeout)
			timeout_ms = timeo_ctx.timeout * 1000;

		msg_free(smsg);
		free(iov.iov_base);
	} else if (!cm->continuous_poll)
		/* normal "get" request, or "show" */
		timeout_ms = 120000;
	/* else: events command, defaults to "infinity" */

	return generic_get(cm, timeout_ms, NULL);
}

static void show_address(void* address, int addr_len)
{
	char buffer[ADDRESS_STR_MAX];
	sprint_address(buffer, address, addr_len);
	printI("address\t\t\t%s;\n", buffer);
}

struct minors_list {
	struct minors_list *next;
	unsigned minor;
};
struct minors_list *__remembered_minors;

static int remember_minor(const struct drbd_cmd *cmd, struct genl_info *info, void *u_ptr)
{
	struct drbd_cfg_context cfg = { .ctx_volume = -1U };

	if (!info)
		return 0;

	drbd_cfg_context_from_attrs(&cfg, info);
	if (cfg.ctx_volume != -1U) {
		unsigned minor = ((struct drbd_genlmsghdr*)(info->userhdr))->minor;
		struct minors_list *m = malloc(sizeof(*m));
		m->next = __remembered_minors;
		m->minor = minor;
		__remembered_minors = m;
	}
	return 0;
}

static void free_minors(struct minors_list *minors)
{
	while (minors) {
		struct minors_list *m = minors;
		minors = minors->next;
		free(m);
	}
}

/*
 * Expects objname to be set to the resource name or "all".
 */
static struct minors_list *enumerate_minors(void)
{
	struct drbd_cmd cmd = {
		.cmd_id = DRBD_ADM_GET_STATUS,
		.show_function = remember_minor,
		.missing_ok = true,
	};
	struct minors_list *m;
	int err;

	err = generic_get_cmd(&cmd, 0, NULL);
	m = __remembered_minors;
	__remembered_minors = NULL;
	if (err) {
		free_minors(m);
		m = NULL;
	}
	return m;
}

static int remember_resource(const struct drbd_cmd *cmd, struct genl_info *info, void *u_ptr)
{
	struct resources_list ***tail = u_ptr;
	struct drbd_cfg_context cfg = { .ctx_volume = -1U };

	if (!info)
		return 0;

	drbd_cfg_context_from_attrs(&cfg, info);
	if (cfg.ctx_resource_name) {
		struct resources_list *r = calloc(1, sizeof(*r));
		struct nlattr *res_opts = global_attrs[DRBD_NLA_RESOURCE_OPTS];

		r->name = strdup(cfg.ctx_resource_name);
		if (res_opts) {
			int size = nla_total_size(nla_len(res_opts));

			r->res_opts = malloc(size);
			memcpy(r->res_opts, res_opts, size);
		}
		resource_info_from_attrs(&r->info, info);
		memset(&r->statistics, -1, sizeof(r->statistics));
		resource_statistics_from_attrs(&r->statistics, info);
		**tail = r;
		*tail = &r->next;
	}
	return 0;
}

static void free_resources(struct resources_list *resources)
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
		.show_function = remember_resource,
		.missing_ok = false,
	};
	struct resources_list *list = NULL, **tail = &list;
	char *old_objname = objname;
	unsigned old_minor = minor;
	int err;

	objname = "all";
	minor = -1;
	err = generic_get(&cmd, 120000, &tail);
	objname = old_objname;
	minor = old_minor;
	if (err) {
		free_resources(list);
		list = NULL;
	}

	return list;
}

static int remember_device(const struct drbd_cmd *cm, struct genl_info *info, void *u_ptr)
{
	struct devices_list ***tail = u_ptr;
	struct drbd_cfg_context ctx = { .ctx_volume = -1U };

	if (!info)
		return 0;

	drbd_cfg_context_from_attrs(&ctx, info);

	if (ctx.ctx_volume != -1U) {
		struct devices_list *d = calloc(1, sizeof(*d));

		d->minor =  ((struct drbd_genlmsghdr*)(info->userhdr))->minor;
		d->ctx = ctx;
		disk_conf_from_attrs(&d->disk_conf, info);
		d->info.dev_disk_state = D_DISKLESS;
		device_info_from_attrs(&d->info, info);
		memset(&d->statistics, -1, sizeof(d->statistics));
		device_statistics_from_attrs(&d->statistics, info);
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
		.show_function = remember_device,
		.missing_ok = false,
	};
	struct devices_list *list = NULL, **tail = &list;
	char *old_objname = objname;
	unsigned old_minor = minor;
	int err;

	objname = resource_name ? resource_name : "all";
	minor = -1;
	err = generic_get(&cmd, 120000, &tail);
	objname = old_objname;
	minor = old_minor;
	if (err) {
		free_devices(list);
		list = NULL;
	}
	return list;
}

static void free_devices(struct devices_list *devices)
{
	while (devices) {
		struct devices_list *d = devices;
		devices = devices->next;
		free(d);
	}
}

static int remember_connection(const struct drbd_cmd *cmd, struct genl_info *info, void *u_ptr)
{
	struct connections_list ***tail = u_ptr;
	struct drbd_cfg_context ctx = { .ctx_volume = -1U };

	if (!info)
		return 0;

	drbd_cfg_context_from_attrs(&ctx, info);
	if (ctx.ctx_resource_name) {
		struct connections_list *c = calloc(1, sizeof(*c));
		struct nlattr *net_conf = global_attrs[DRBD_NLA_NET_CONF];

		c->ctx = ctx;
		if (net_conf) {
			int size = nla_total_size(nla_len(net_conf));

			c->net_conf = malloc(size);
			memcpy(c->net_conf, net_conf, size);
		}
		connection_info_from_attrs(&c->info, info);
		memset(&c->statistics, -1, sizeof(c->statistics));
		connection_statistics_from_attrs(&c->statistics, info);
		**tail = c;
		*tail = &c->next;
	}
	return 0;
}

#if 0
static int connection_name_cmp(const struct connections_list * const *a, const struct connections_list * const *b)
{
	if (!(*a)->ctx.ctx_conn_name_len != !(*b)->ctx.ctx_conn_name_len)
		return !(*b)->ctx.ctx_conn_name_len;
	return strcmp((*a)->ctx.ctx_conn_name, (*b)->ctx.ctx_conn_name);
}
#endif

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
#if 0
		qsort(array, n, sizeof(*array), (int (*)(const void *, const void *)) connection_name_cmp);
#endif
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
		.show_function = remember_connection,
		.missing_ok = true,
	};
	struct connections_list *list = NULL, **tail = &list;
	char *old_objname = objname;
	unsigned old_minor = minor;
	int err;

	objname = resource_name ? resource_name : "all";
	minor = -1;
	err = generic_get(&cmd, 120000, &tail);
	objname = old_objname;
	minor = old_minor;
	if (err) {
		free_connections(list);
		list = NULL;
	}
	return list;
}

static void free_connections(struct connections_list *connections)
{
	while (connections) {
		struct connections_list *l = connections;
		connections = connections->next;
		free(l);
	}
}

static int remember_peer_device(const struct drbd_cmd *cmd, struct genl_info *info, void *u_ptr)
{
	struct peer_devices_list ***tail = u_ptr;
	struct drbd_cfg_context ctx = { .ctx_volume = -1U };

	if (!info)
		return 0;

	drbd_cfg_context_from_attrs(&ctx, info);
	if (ctx.ctx_resource_name) {
		struct peer_devices_list *p = calloc(1, sizeof(*p));

		if (!p)
			exit(20);

		p->ctx = ctx;
		peer_device_info_from_attrs(&p->info, info);
		memset(&p->statistics, -1, sizeof(p->statistics));
		peer_device_statistics_from_attrs(&p->statistics, info);
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
		.show_function = remember_peer_device,
		.missing_ok = false,
	};
	struct peer_devices_list *list = NULL, **tail = &list;
	char *old_objname = objname;
	unsigned old_minor = minor;
	int err;

	objname = resource_name ? resource_name : "all";
	minor = -1;
	err = generic_get(&cmd, 120000, &tail);
	objname = old_objname;
	minor = old_minor;
	if (err) {
		free_peer_devices(list);
		list = NULL;
	}
	return list;
}

static void free_peer_devices(struct peer_devices_list *peer_devices)
{
	while (peer_devices) {
		struct peer_devices_list *p = peer_devices;
		peer_devices = peer_devices->next;
		free(p);
	}
}

/* may be called for a "show" of a single minor device.
 * prints all available configuration information in that case.
 *
 * may also be called iteratively for a "show-all", which should try to not
 * print redundant configuration information for the same resource (tconn).
 */
static int show_scmd(const struct drbd_cmd *cm, struct genl_info *info, void *u_ptr)
{
	/* FIXME need some define for max len here */
	static char last_ctx_resource_name[128];
	static int call_count;

	struct drbd_cfg_context cfg = { .ctx_volume = -1U };
	struct disk_conf dc = { .disk_size = 0, };
	struct net_conf nc = { .timeout = 0, };;

	if (!info) {
		if (call_count) {
			--indent;
			printI("}\n"); /* close _this_host */
			--indent;
			printI("}\n"); /* close resource */
		}
		fflush(stdout);
		return 0;
	}
	call_count++;

	/* FIXME: Is the folowing check needed? */
	if (!global_attrs[DRBD_NLA_CFG_CONTEXT])
		dbg(1, "unexpected packet, configuration context missing!\n");

	drbd_cfg_context_from_attrs(&cfg, info);
	disk_conf_from_attrs(&dc, info);
	net_conf_from_attrs(&nc, info);

	if (strncmp(last_ctx_resource_name, cfg.ctx_resource_name, sizeof(last_ctx_resource_name))) {
		if (strncmp(last_ctx_resource_name, "", sizeof(last_ctx_resource_name))) {
			--indent;
			printI("}\n"); /* close _this_host */
			--indent;
			printI("}\n\n");
		}
		strncpy(last_ctx_resource_name, cfg.ctx_resource_name, sizeof(last_ctx_resource_name));

		printI("resource %s {\n", cfg.ctx_resource_name);
		++indent;
		print_options("resource-options", "options");
		print_options("net-options", "net");

		if (cfg.ctx_peer_addr_len) {
			printI("_remote_host {\n");
			++indent;
			show_address(cfg.ctx_peer_addr, cfg.ctx_peer_addr_len);
			--indent;
			printI("}\n");
		}
		printI("_this_host {\n");
		++indent;
		if (cfg.ctx_my_addr_len)
			show_address(cfg.ctx_my_addr, cfg.ctx_my_addr_len);
	}

	if (cfg.ctx_volume != -1U) {
		unsigned minor = ((struct drbd_genlmsghdr*)(info->userhdr))->minor;
		printI("volume %d {\n", cfg.ctx_volume);
		++indent;
		printI("device\t\t\tminor %d;\n", minor);
		if (global_attrs[DRBD_NLA_DISK_CONF]) {
			if (dc.backing_dev[0]) {
				printI("disk\t\t\t\"%s\";\n", dc.backing_dev);
				printI("meta-disk\t\t\t");
				switch(dc.meta_dev_idx) {
				case DRBD_MD_INDEX_INTERNAL:
				case DRBD_MD_INDEX_FLEX_INT:
					printf("internal;\n");
					break;
				case DRBD_MD_INDEX_FLEX_EXT:
					printf("%s;\n",
					       double_quote_string(dc.meta_dev));
					break;
				default:
					printf("%s [ %d ];\n",
					       double_quote_string(dc.meta_dev),
					       dc.meta_dev_idx);
				 }
			}
		}
		print_options("attach", "disk");
		--indent;
		printI("}\n"); /* close volume */
	}

	return 0;
}

static int lk_bdev_scmd(const struct drbd_cmd *cm, struct genl_info *info, void *u_ptr)
{
	unsigned minor;
	struct disk_conf dc = { .disk_size = 0, };
	struct bdev_info bd = { 0, };
	uint64_t bd_size;
	int fd;

	if (!info)
		return 0;

	minor = ((struct drbd_genlmsghdr*)(info->userhdr))->minor;
	disk_conf_from_attrs(&dc, info);
	if (!dc.backing_dev) {
		fprintf(stderr, "Has no disk config, try with drbdmeta.\n");
		return 1;
	}

	if (dc.meta_dev_idx >= 0 || dc.meta_dev_idx == DRBD_MD_INDEX_FLEX_EXT) {
		lk_bdev_delete(minor);
		return 0;
	}

	fd = open(dc.backing_dev, O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "Could not open %s: %m.\n", dc.backing_dev);
		return 1;
	}
	bd_size = bdev_size(fd);
	close(fd);

	if (lk_bdev_load(minor, &bd) == 0 &&
	    bd.bd_size == bd_size &&
	    bd.bd_name && !strcmp(bd.bd_name, dc.backing_dev))
		return 0;	/* nothing changed. */

	bd.bd_size = bd_size;
	bd.bd_name = dc.backing_dev;
	lk_bdev_save(minor, &bd);

	return 0;
}

static int sh_status_scmd(const struct drbd_cmd *cm __attribute((unused)),
		struct genl_info *info, void *u_ptr)
{
	unsigned minor;
	struct drbd_cfg_context cfg = { .ctx_volume = -1U };
	struct state_info si = { .current_state = 0, };
	union drbd_state state;
	int available = 0;

	if (!info)
		return 0;

	minor = ((struct drbd_genlmsghdr*)(info->userhdr))->minor;
/* variable prefix; maybe rather make that a command line parameter?
 * or use "drbd_sh_status"? */
#define _P ""
	printf("%s_minor=%u\n", _P, minor);

	drbd_cfg_context_from_attrs(&cfg, info);
	if (cfg.ctx_resource_name)
		printf("%s_res_name=%s\n", _P, shell_escape(cfg.ctx_resource_name));
	printf("%s_volume=%d\n", _P, cfg.ctx_volume);

	if (state_info_from_attrs(&si, info) == 0)
		available = 1;
	state.i = si.current_state;

	if (state.conn == C_STANDALONE && state.disk == D_DISKLESS && state.role != R_PRIMARY) {
		printf("%s_known=%s\n\n", _P,
			available ? "Unconfigured"
			          : "NA # not available or not yet created");
		printf("%s_cstate=Unconfigured\n", _P);
		printf("%s_role=\n", _P);
		printf("%s_peer=\n", _P);
		printf("%s_disk=\n", _P);
		printf("%s_pdsk=\n", _P);
		printf("%s_flags_susp=\n", _P);
		printf("%s_flags_aftr_isp=\n", _P);
		printf("%s_flags_peer_isp=\n", _P);
		printf("%s_flags_user_isp=\n", _P);
		printf("%s_resynced_percent=\n", _P);
	} else {
		printf( "%s_known=Configured\n\n"
			/* connection state */
			"%s_cstate=%s\n"
			/* role */
			"%s_role=%s\n"
			"%s_peer=%s\n"
			/* disk state */
			"%s_disk=%s\n"
			"%s_pdsk=%s\n\n",
			_P,
			_P, drbd_conn_str(state.conn),
			_P, drbd_role_str(state.role),
			_P, drbd_role_str(state.peer),
			_P, drbd_disk_str(state.disk),
			_P, drbd_disk_str(state.pdsk));

		/* io suspended ? */
		printf("%s_flags_susp=%s\n", _P, state.susp ? "1" : "");
		/* reason why sync is paused */
		printf("%s_flags_aftr_isp=%s\n", _P, state.aftr_isp ? "1" : "");
		printf("%s_flags_peer_isp=%s\n", _P, state.peer_isp ? "1" : "");
		printf("%s_flags_user_isp=%s\n\n", _P, state.user_isp ? "1" : "");

		printf("%s_resynced_percent=", _P);

		if (ntb(T_bits_rs_total)) {
			uint32_t shift = si.bits_rs_total >= (1ULL << 32) ? 16 : 10;
			uint64_t left = (si.bits_oos - si.bits_rs_failed) >> shift;
			uint64_t total = 1UL + (si.bits_rs_total >> shift);
			uint64_t tmp = 1000UL - left * 1000UL/total;

			unsigned synced = tmp;
			printf("%i.%i\n", synced / 10, synced % 10);
			/* what else? everything available! */
		} else
			printf("\n");
	}
	printf("\n%s_sh_status_process\n\n\n", _P);

	fflush(stdout);
	return 0;
#undef _P
}

static int role_scmd(const struct drbd_cmd *cm __attribute((unused)),
		struct genl_info *info, void *u_ptr)
{
	union drbd_state state = { .i = 0 };

	if (!strcmp(cm->cmd, "state")) {
		fprintf(stderr, "'%s ... state' is deprecated, use '%s ... role' instead.\n",
			cmdname, cmdname);
	}

	if (!info)
		return 0;

	if (global_attrs[DRBD_NLA_STATE_INFO]) {
		drbd_nla_parse_nested(nested_attr_tb,
				      ARRAY_SIZE(state_info_nl_policy) - 1,
				      global_attrs[DRBD_NLA_STATE_INFO],
				      state_info_nl_policy);
		if (ntb(T_current_state))
			state.i = nla_get_u32(ntb(T_current_state));
	}
	if (state.conn == C_STANDALONE &&
	    state.disk == D_DISKLESS) {
		printf("Unconfigured\n");
	} else {
		printf("%s/%s\n",drbd_role_str(state.role),drbd_role_str(state.peer));
	}
	return 0;
}

static int cstate_scmd(const struct drbd_cmd *cm __attribute((unused)),
		struct genl_info *info, void *u_ptr)
{
	union drbd_state state = { .i = 0 };

	if (!info)
		return 0;

	if (global_attrs[DRBD_NLA_STATE_INFO]) {
		drbd_nla_parse_nested(nested_attr_tb,
				      ARRAY_SIZE(state_info_nl_policy) - 1,
				      global_attrs[DRBD_NLA_STATE_INFO],
				      state_info_nl_policy);
		if (ntb(T_current_state))
			state.i = nla_get_u32(ntb(T_current_state));
	}
	if (state.conn == C_STANDALONE &&
	    state.disk == D_DISKLESS) {
		printf("Unconfigured\n");
	} else {
		printf("%s\n",drbd_conn_str(state.conn));
	}
	return 0;
}

static int dstate_scmd(const struct drbd_cmd *cm __attribute((unused)),
		struct genl_info *info, void *u_ptr)
{
	union drbd_state state = { .i = 0 };

	if (!info)
		return 0;

	if (global_attrs[DRBD_NLA_STATE_INFO]) {
		drbd_nla_parse_nested(nested_attr_tb,
				      ARRAY_SIZE(state_info_nl_policy)-1,
				      global_attrs[DRBD_NLA_STATE_INFO],
				      state_info_nl_policy);
		if (ntb(T_current_state))
			state.i = nla_get_u32(ntb(T_current_state));
	}
	if ( state.conn == C_STANDALONE &&
	     state.disk == D_DISKLESS) {
		printf("Unconfigured\n");
	} else {
		printf("%s/%s\n",drbd_disk_str(state.disk),drbd_disk_str(state.pdsk));
	}
	return 0;
}

static int uuids_scmd(const struct drbd_cmd *cm,
		struct genl_info *info, void *u_ptr)
{
	union drbd_state state = { .i = 0 };
	uint64_t ed_uuid;
	uint64_t *uuids = NULL;
	int flags = flags;

	if (!info)
		return 0;

	if (global_attrs[DRBD_NLA_STATE_INFO]) {
		drbd_nla_parse_nested(nested_attr_tb,
				      ARRAY_SIZE(state_info_nl_policy)-1,
				      global_attrs[DRBD_NLA_STATE_INFO],
				      state_info_nl_policy);
		if (ntb(T_current_state))
			state.i = nla_get_u32(ntb(T_current_state));
		if (ntb(T_uuids))
			uuids = nla_data(ntb(T_uuids));
		if (ntb(T_disk_flags))
			flags = nla_get_u32(ntb(T_disk_flags));
		if (ntb(T_ed_uuid))
			ed_uuid = nla_get_u64(ntb(T_ed_uuid));
	}
	if (state.conn == C_STANDALONE &&
	    state.disk == D_DISKLESS) {
		fprintf(stderr, "Device is unconfigured\n");
		return 1;
	}
	if (state.disk == D_DISKLESS) {
		/* XXX we could print the ed_uuid anyways: */
		if (0)
			printf(X64(016)"\n", ed_uuid);
		fprintf(stderr, "Device has no disk\n");
		return 1;
	}
	if (uuids) {
		if(!strcmp(cm->cmd,"show-gi")) {
			dt_pretty_print_uuids(uuids,flags);
		} else if(!strcmp(cm->cmd,"get-gi")) {
			dt_print_uuids(uuids,flags);
		} else {
			ASSERT( 0 );
		}
	} else {
		fprintf(stderr, "No uuids found in reply!\n"
			"Maybe you need to upgrade your userland tools?\n");
	}
	return 0;
}

static int down_cmd(const struct drbd_cmd *cm, int argc, char **argv)
{
	struct minors_list *minors, *m;
	int rv;
	int success;

	if(argc > 2) {
		warn_print_excess_args(argc, argv, 2);
		return OTHER_ERROR;
	}

	minors = enumerate_minors();
	rv = _generic_config_cmd(cm, argc, argv, 1);
	success = (rv >= SS_SUCCESS && rv < ERR_CODE_BASE) || rv == NO_ERROR;
	if (success) {
		for (m = minors; m; m = m->next)
			unregister_minor(m->minor);
		free_minors(minors);
		unregister_resource(objname);
	} else {
		free_minors(minors);
		return print_config_error(rv, NULL);
	}
	return 0;
}

static const char *susp_str(struct resource_info *info)
{
	static char buffer[32];

	*buffer = 0;
	if (info->res_susp)
		strcat(buffer, ",user" + (*buffer == 0));
	if (info->res_susp_nod)
		strcat(buffer, ",no-data" + (*buffer == 0));
	if (info->res_susp_fen)
		strcat(buffer, ",fencing" + (*buffer == 0));
	if (*buffer == 0)
		strcat(buffer, "no");

	return buffer;
}

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
			       int (*wrap_printf)(int, const char *, ...))
{
	static const char *write_ordering_str[] = {
		[WO_NONE] = "none",
		[WO_DRAIN_IO] = "drain",
		[WO_BDEV_FLUSH] = "flush",
		[WO_BIO_BARRIER] = "barrier",
	};
	uint32_t wo = new->res_stat_write_ordering;

	if ((!old ||
	     old->res_stat_write_ordering != wo) &&
	    wo < ARRAY_SIZE(write_ordering_str) &&
	    write_ordering_str[wo]) {
		wrap_printf(indent, " write-ordering:%s", write_ordering_str[wo]);
	}
}

void print_device_statistics(int indent,
			     struct device_statistics *old,
			     struct device_statistics *new,
			     int (*wrap_printf)(int, const char *, ...))
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
			    old->dev_al_suspended != new->dev_al_suspended)
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
				 int (*wrap_printf)(int, const char *, ...))
{
	if (!old ||
	    old->conn_congested != new->conn_congested)
		wrap_printf(indent, " congested:%s", new->conn_congested ? "yes" : "no");
}

void print_peer_device_statistics(int indent,
				  struct peer_device_statistics *old,
				  struct peer_device_statistics *new,
				  int (*wrap_printf)(int, const char *, ...))
{
	wrap_printf(indent, " received:" U64,
		    (uint64_t)new->peer_dev_received / 2);
	wrap_printf(indent, " sent:" U64,
		    (uint64_t)new->peer_dev_sent / 2);
	if (opt_verbose || new->peer_dev_out_of_sync)
		wrap_printf(indent, " out-of-sync:" U64,
			    (uint64_t)new->peer_dev_out_of_sync / 2);
	if (opt_verbose) {
		wrap_printf(indent, " pending:" U32,
			    new->peer_dev_pending);
		wrap_printf(indent, " unacked:" U32,
			    new->peer_dev_unacked);
	}
}

void resource_status(struct resources_list *resource)
{
	enum drbd_role role = resource->info.res_role;

	wrap_printf(0, "%s", resource->name);
#if 0
	if (opt_verbose) {
		struct nlattr *nla;

		nla = nla_find_nested(resource->res_opts, __nla_type(T_node_id));
		if (nla)
			wrap_printf(4, " node-id:%d", *(uint32_t *)nla_data(nla));
	}
#endif
	wrap_printf(4, " role:%s%s%s",
		    role_color_start(role, true),
		    drbd_role_str(role),
		    role_color_stop(role, true));
	if (opt_verbose ||
	    resource->info.res_susp ||
	    resource->info.res_susp_nod ||
	    resource->info.res_susp_fen)
		wrap_printf(4, " suspended:%s", susp_str(&resource->info));
#if 0
	if (opt_verbose || resource->info.res_weak)
		wrap_printf(4, " weak:%s",
			    resource->info.res_weak ? "yes" : "no");
#endif
	if (opt_statistics && opt_verbose) {
		wrap_printf(4, "\n");
		print_resource_statistics(4, NULL, &resource->statistics, wrap_printf);
	}
	wrap_printf(0, "\n");
}

static void device_status(struct devices_list *device, bool single_device)
{
	enum drbd_disk_state disk_state = device->info.dev_disk_state;
	int indent = 2;

	if (opt_verbose || !(single_device && device->ctx.ctx_volume == 0)) {
		wrap_printf(indent, "volume:%u",  device->ctx.ctx_volume);
		indent = 6;
		if (opt_verbose)
			wrap_printf(indent, " minor:%u", device->minor);
	}
	wrap_printf(indent, " disk:%s%s%s",
		    disk_state_color_start(disk_state, true),
		    drbd_disk_str(disk_state),
		    disk_state_color_stop(disk_state, true));
	indent = 6;
	if (device->statistics.dev_size != -1) {
		if (opt_statistics)
			wrap_printf(indent, "\n");
		print_device_statistics(indent, NULL, &device->statistics, wrap_printf);
	}
	wrap_printf(indent, "\n");
}

static const char *resync_susp_str(struct peer_device_info *info)
{
	static char buffer[64];

	*buffer = 0;
	if (info->peer_resync_susp_user)
		strcat(buffer, ",user" + (*buffer == 0));
	if (info->peer_resync_susp_peer)
		strcat(buffer, ",peer" + (*buffer == 0));
	if (info->peer_resync_susp_dependency)
		strcat(buffer, ",dependency" + (*buffer == 0));
	if (*buffer == 0)
		strcat(buffer, "no");

	return buffer;
}

const char *drbd_repl_str9(enum drbd_conns s)
{
	static const char *n[] = {
		[C_WF_REPORT_PARAMS] = "Off",
		[C_CONNECTED] = "Established",
	};

	return (s == C_WF_REPORT_PARAMS || s == C_CONNECTED) ? n[s] : drbd_conn_str(s);
}

const char *drbd_conn_str9(enum drbd_conns s)
{
	static const char *n[] = {
		[C_WF_CONNECTION] = "Connecting",
		[C_WF_REPORT_PARAMS] = "Connected",
	};

	return (s == C_WF_CONNECTION || s == C_WF_REPORT_PARAMS) ? n[s] : drbd_conn_str(s);
}

static void peer_device_status(struct peer_devices_list *peer_device, bool single_device)
{
	int indent = 4;

	if (opt_verbose || !(single_device && peer_device->ctx.ctx_volume == 0)) {
		wrap_printf(indent, "volume:%d", peer_device->ctx.ctx_volume);
		indent = 8;
	}
	/* this > C_WF_REPORT_PARAMS is > L_ESTABLISHED in DRBD 9 */
	if (opt_verbose || peer_device->info.peer_repl_state > C_WF_REPORT_PARAMS) {
		enum drbd_conns repl_state = peer_device->info.peer_repl_state;

		wrap_printf(indent, " replication:%s%s%s",
			    repl_state_color_start(repl_state),
			    drbd_repl_str9(repl_state),
			    repl_state_color_stop(repl_state));
		indent = 8;
	}
	/* this C_WF_REPORT_PARAMS is C_CONNECTED resp. L_OFF in DRBD 9 */
	if (opt_verbose || opt_statistics ||
	    peer_device->info.peer_repl_state != C_WF_REPORT_PARAMS ||
	    peer_device->info.peer_disk_state != D_UNKNOWN) {
		enum drbd_disk_state disk_state = peer_device->info.peer_disk_state;

		wrap_printf(indent, " peer-disk:%s%s%s",
			    disk_state_color_start(disk_state, false),
			    drbd_disk_str(disk_state),
			    disk_state_color_stop(disk_state, false));
		indent = 8;
		if (peer_device->info.peer_repl_state >= C_SYNC_SOURCE &&
		    peer_device->info.peer_repl_state <= C_PAUSED_SYNC_T) {
			wrap_printf(indent, " done:%.2f", 100 * (1 -
				(double)peer_device->statistics.peer_dev_out_of_sync /
				(double)peer_device->device->statistics.dev_size));
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

static void peer_devices_status(struct drbd_cfg_context *ctx, struct peer_devices_list *peer_devices, bool single_device)
{
	struct peer_devices_list *peer_device;

	for (peer_device = peer_devices; peer_device; peer_device = peer_device->next) {
		if (!endpoints_equal(ctx, &peer_device->ctx))
			continue;
		peer_device_status(peer_device, single_device);
	}
}

static void connection_status(struct connections_list *connection,
			      struct peer_devices_list *peer_devices,
			      bool single_device)
{
	wrap_printf(2, "%s", "peer" /* connection->ctx.ctx_conn_name */);

	/* We do not want the IP-pair information */
	/* We do not have any node-id information */

	/* this C_WF_REPORT_PARAMS is C_CONNECTED in DRBD 9 */
	if (opt_verbose || connection->info.conn_connection_state < C_WF_REPORT_PARAMS) {
		enum drbd_conns cstate = connection->info.conn_connection_state;
		wrap_printf(6, " connection:%s%s%s",
			    cstate_color_start(cstate),
			    drbd_conn_str9(cstate),
			    cstate_color_stop(cstate));
	}
	if (opt_verbose || connection->info.conn_connection_state == C_WF_REPORT_PARAMS) {
		enum drbd_role role = connection->info.conn_role;
		wrap_printf(6, " role:%s%s%s",
			    role_color_start(role, false),
			    drbd_role_str(role),
			    role_color_stop(role, false));
	}
	if (opt_verbose || connection->statistics.conn_congested > 0)
		print_connection_statistics(6, NULL, &connection->statistics, wrap_printf);
	wrap_printf(0, "\n");
	if (opt_verbose || opt_statistics || connection->info.conn_connection_state == C_WF_REPORT_PARAMS)
		peer_devices_status(&connection->ctx, peer_devices, single_device);
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

static void print_usage_and_exit(const char *addinfo);

static int status_cmd(const struct drbd_cmd *cm, int argc, char **argv)
{
	struct resources_list *resources, *resource;
	struct sigaction sa = {
		.sa_handler = stop_colors,
		.sa_flags = SA_RESETHAND,
	};
	bool found = false;
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
			opt_verbose = true;
			break;
		case 's':
			opt_statistics = true;
			break;
		case 'c':
			if (!optarg || !strcmp(optarg, "always"))
				opt_color = ALWAYS_COLOR;
			else if (!strcmp(optarg, "never"))
				opt_color = NEVER_COLOR;
			else if (!strcmp(optarg, "auto"))
				opt_color = AUTO_COLOR;
			else
				print_usage_and_exit("unknown --color argument");
			break;
		}
	}

	resources = sort_resources(list_resources());

	sigaction(SIGHUP, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGPIPE, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	for (resource = resources; resource; resource = resource->next) {
		struct devices_list *devices, *device;
		struct connections_list *connections, *connection;
		struct peer_devices_list *peer_devices = NULL;
		bool single_device;

		if (strcmp(objname, "all") && strcmp(objname, resource->name))
			continue;

		devices = list_devices(resource->name);
		connections = sort_connections(list_connections(resource->name));
		if (devices && connections)
			peer_devices = list_peer_devices(resource->name);

		link_peer_devices_to_devices(peer_devices, devices);

		resource_status(resource);
		single_device = devices && !devices->next;
		for (device = devices; device; device = device->next)
			device_status(device, single_device);
		for (connection = connections; connection; connection = connection->next)
			connection_status(connection, peer_devices, single_device);
		wrap_printf(0, "\n");

		free_connections(connections);
		free_devices(devices);
		free_peer_devices(peer_devices);
		found = true;
	}

	free_resources(resources);
	if (!found && strcmp(objname, "all")) {
		fprintf(stderr, "%s: No such resource\n", objname);
		return 10;
	}
	return 0;
}

static int event_key(char *key, int size, const char *name, unsigned minor,
		     struct drbd_cfg_context *ctx)
{
	int ret, pos = 0;

	ret = snprintf(key + pos, size,
		       "%s", name);
	if (ret < 0)
		return ret;
	pos += ret;
	if (size)
		size -= ret;
	if (ctx->ctx_resource_name) {
		ret = snprintf(key + pos, size,
			       " name:%s", ctx->ctx_resource_name);
		if (ret < 0)
			return ret;
		pos += ret;
		if (size)
			size -= ret;
	}
	/* 8.4 drbd_cfg_context does not provide ctx->ctx_peer_node_id
	 * check the corresponding name and fake it to 0 */
	if (!strcmp(name, "connection") || !strcmp(name, "peer-device") || !strcmp(name, "helper")) {
		ret = snprintf(key + pos, size,
			      " peer-node-id:%d", 0);
		if (ret < 0)
			return ret;
		pos += ret;
		if (size)
			size -= ret;
	}
	/* Always use "peer" as connection name,
	 * and print it if ctx has peer address set.
	 * Do not show IP address pairs */
	if (ctx->ctx_peer_addr_len) {
		ret = snprintf(key + pos, size, " conn-name:%s", "peer");
		if (ret < 0)
			return ret;
		pos += ret;
		if (size)
			size -= ret;
	}
	if (ctx->ctx_volume != -1U) {
		ret = snprintf(key + pos, size,
			      " volume:%u", ctx->ctx_volume);
		if (ret < 0)
			return ret;
		pos += ret;
		if (size)
			size -= ret;
	}
	if (minor != -1U) {
		ret = snprintf(key + pos, size,
			      " minor:%u", minor);
		if (ret < 0)
			return ret;
		pos += ret;
		/* if (size) */
		/* 	size -= ret; */
	}
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

static int print_notifications(const struct drbd_cmd *cm, struct genl_info *info, void *u_ptr)
{
	static const char *action_name[] = {
		[NOTIFY_EXISTS] = "exists",
		[NOTIFY_CREATE] = "create",
		[NOTIFY_CHANGE] = "change",
		[NOTIFY_DESTROY] = "destroy",
		[NOTIFY_CALL] = "call",
		[NOTIFY_RESPONSE] = "response",
	};
	static char *object_name[] = {
		[DRBD_RESOURCE_STATE] = "resource",
		[DRBD_DEVICE_STATE] = "device",
		[DRBD_CONNECTION_STATE] = "connection",
		[DRBD_PEER_DEVICE_STATE] = "peer-device",
		[DRBD_HELPER] = "helper",
	};
	static uint32_t last_seq;
	static bool last_seq_known;
	static struct timeval tv;
	static bool keep_tv;

	struct drbd_cfg_context ctx = { .ctx_volume = -1U };
	struct drbd_notification_header nh = { .nh_type = -1U };
	enum drbd_notification_type action;
	struct drbd_genlmsghdr *dh;
	char *key = NULL;

	if (!info) {
		keep_tv = false;
		return 0;
	}

	dh = info->userhdr;
	if (dh->ret_code == ERR_MINOR_INVALID && cm->missing_ok)
		return 0;
	if (dh->ret_code != NO_ERROR)
		return dh->ret_code;

	if (drbd_notification_header_from_attrs(&nh, info))
		return 0;
	action = nh.nh_type & ~NOTIFY_FLAGS;
	if (action >= ARRAY_SIZE(action_name) ||
	    !action_name[action]) {
		dbg(1, "unknown notification type\n");
		goto out;
	}

	if (opt_now && action != NOTIFY_EXISTS)
		return 0;

	if (info->genlhdr->cmd != DRBD_INITIAL_STATE_DONE) {
		if (drbd_cfg_context_from_attrs(&ctx, info))
			return 0;
		if (info->genlhdr->cmd >= ARRAY_SIZE(object_name) ||
		    !object_name[info->genlhdr->cmd]) {
			dbg(1, "unknown notification\n");
			goto out;
		}
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
	if (info->genlhdr->cmd != DRBD_INITIAL_STATE_DONE) {
		const char *name = object_name[info->genlhdr->cmd];
		int size;

		size = event_key(NULL, 0, name, dh->minor, &ctx);
		if (size < 0)
			goto fail;
		key = malloc(size + 1);
		if (!key)
			goto fail;
		event_key(key, size + 1, name, dh->minor, &ctx);
	}
	printf("%s %s",
	       action_name[action],
	       key ? key : "-");

	switch(info->genlhdr->cmd) {
	case DRBD_RESOURCE_STATE:
		if (action != NOTIFY_DESTROY) {
			bool have_new_stats = true;
			struct {
				struct resource_info i;
				struct resource_statistics s;
			} *old, new;

			if (resource_info_from_attrs(&new.i, info)) {
				dbg(1, "resource info missing\n");
				goto nl_out;
			}
			memset(&new.s, -1, sizeof(new.s));
			if (resource_statistics_from_attrs(&new.s, info)) {
				dbg(1, "resource statistics missing\n");
				have_new_stats = false;
			}
			old = update_info(&key, &new, sizeof(new));
			if (old && !have_new_stats)
				new.s = old->s;

			if (!old || new.i.res_role != old->i.res_role)
				printf(" role:%s",
				       drbd_role_str(new.i.res_role));
			if (!old ||
			    new.i.res_susp != old->i.res_susp ||
			    new.i.res_susp_nod != old->i.res_susp_nod ||
			    new.i.res_susp_fen != old->i.res_susp_fen)
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

			if (device_info_from_attrs(&new.i, info)) {
				dbg(1, "device info missing\n");
				goto nl_out;
			}
			memset(&new.s, -1, sizeof(new.s));
			if (device_statistics_from_attrs(&new.s, info)) {
				dbg(1, "device statistics missing\n");
				have_new_stats = false;
			}
			old = update_info(&key, &new, sizeof(new));
			if (old && !have_new_stats)
				new.s = old->s;
			if (!old || new.i.dev_disk_state != old->i.dev_disk_state)
				printf(" disk:%s",
				       drbd_disk_str(new.i.dev_disk_state));
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

			if (connection_info_from_attrs(&new.i, info)) {
				dbg(1, "connection info missing\n");
				goto nl_out;
			}
			memset(&new.s, -1, sizeof(new.s));
			if (connection_statistics_from_attrs(&new.s, info)) {
				dbg(1, "connection statistics missing\n");
				have_new_stats = false;
			}
			old = update_info(&key, &new, sizeof(new));
			if (old && !have_new_stats)
				new.s = old->s;
			if (!old ||
			    new.i.conn_connection_state != old->i.conn_connection_state)
				printf(" connection:%s",
				       drbd_conn_str9(new.i.conn_connection_state));
			if (!old ||
			    new.i.conn_role != old->i.conn_role)
				printf(" role:%s",
				       drbd_role_str(new.i.conn_role));
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

			if (peer_device_info_from_attrs(&new.i, info)) {
				dbg(1, "peer device info missing\n");
				goto nl_out;
			}
			memset(&new.s, -1, sizeof(new.s));
			if (peer_device_statistics_from_attrs(&new.s, info)) {
				dbg(1, "peer device statistics missing\n");
				have_new_stats = false;
			}
			old = update_info(&key, &new, sizeof(new));
			if (old && !have_new_stats)
				new.s = old->s;

			if (!old || new.i.peer_repl_state != old->i.peer_repl_state)
				printf(" replication:%s",
				       drbd_repl_str9(new.i.peer_repl_state));
			if (!old || new.i.peer_disk_state != old->i.peer_disk_state)
				printf(" peer-disk:%s",
				       drbd_disk_str(new.i.peer_disk_state));
			if (!old ||
			    new.i.peer_resync_susp_user != old->i.peer_resync_susp_user ||
			    new.i.peer_resync_susp_peer != old->i.peer_resync_susp_peer ||
			    new.i.peer_resync_susp_dependency != old->i.peer_resync_susp_dependency)
				printf(" resync-suspended:%s",
				       resync_susp_str(&new.i));
			if (opt_statistics && have_new_stats)
				print_peer_device_statistics(0, old ? &old->s : NULL,
							     &new.s, nowrap_printf);
			free(old);
		} else
			update_info(&key, NULL, 0);
		break;
	case DRBD_HELPER: {
		struct drbd_helper_info helper_info;

		if (!drbd_helper_info_from_attrs(&helper_info, info)) {
			printf(" helper:%s", helper_info.helper_name);
			if (action == NOTIFY_RESPONSE)
				printf(" status:%u", helper_info.helper_status);
		} else {
			dbg(1, "helper info missing\n");
			goto nl_out;
		}
		}
		break;
	case DRBD_INITIAL_STATE_DONE:
		break;
	}

nl_out:
	printf("\n");
out:
	free(key);
	fflush(stdout);
	if (opt_now && info->genlhdr->cmd == DRBD_INITIAL_STATE_DONE)
		return -1;
	return 0;

fail:
	perror(progname);
	exit(20);
}

/* printf format for minor, resource name, volume */
#define MNV_FMT	"%d,%s[%d]"
static void print_state(char *tag, unsigned seq, unsigned minor,
		const char *resource_name, unsigned vnr, __u32 state_i)
{
	union drbd_state s = { .i = state_i };
	printf("%u %s " MNV_FMT " { cs:%s ro:%s/%s ds:%s/%s %c%c%c%c }\n",
	       seq,
	       tag,
	       minor, resource_name, vnr,
	       drbd_conn_str(s.conn),
	       drbd_role_str(s.role),
	       drbd_role_str(s.peer),
	       drbd_disk_str(s.disk),
	       drbd_disk_str(s.pdsk),
	       s.susp ? 's' : 'r',
	       s.aftr_isp ? 'a' : '-',
	       s.peer_isp ? 'p' : '-',
	       s.user_isp ? 'u' : '-' );
}

static int print_broadcast_events(const struct drbd_cmd *cm, struct genl_info *info, void *u_ptr)
{
	struct drbd_cfg_context cfg = { .ctx_volume = -1U };
	struct state_info si = { .current_state = 0 };
	struct disk_conf dc = { .disk_size = 0, };
	struct net_conf nc = { .timeout = 0, };
	struct drbd_genlmsghdr *dh;

	/* End of initial dump. Ignore. Maybe: print some marker? */
	if (!info)
		return 0;

	dh = info->userhdr;
	if (dh->ret_code == ERR_MINOR_INVALID && cm->missing_ok)
		return 0;

	if (drbd_cfg_context_from_attrs(&cfg, info)) {
		dbg(1, "unexpected packet, configuration context missing!\n");
		/* keep running anyways. */
		struct nlattr *nla = NULL;
		if (info->attrs[DRBD_NLA_CFG_REPLY])
			nla = drbd_nla_find_nested(ARRAY_SIZE(drbd_cfg_reply_nl_policy) - 1,
						   info->attrs[DRBD_NLA_CFG_REPLY], T_info_text);
		if (nla) {
			char *txt = nla_data(nla);
			char *c;
			for (c = txt; *c; c++)
				if (*c == '\n')
					*c = '_';
			printf("%u # %s\n", info->seq, txt);
		}
		goto out;
	}

	if (state_info_from_attrs(&si, info)) {
		/* this is a DRBD_ADM_GET_STATUS reply
		 * with information about a resource without any volumes */
		printf("%u R - %s\n", info->seq, cfg.ctx_resource_name);
		goto out;
	}

	disk_conf_from_attrs(&dc, info);
	net_conf_from_attrs(&nc, info);

	switch (si.sib_reason) {
	case SIB_STATE_CHANGE:
		print_state("ST-prev", info->seq,
				dh->minor, cfg.ctx_resource_name, cfg.ctx_volume,
				si.prev_state);
		print_state("ST-new", info->seq,
				dh->minor, cfg.ctx_resource_name, cfg.ctx_volume,
				si.new_state);
		/* fall through */
	case SIB_GET_STATUS_REPLY:
		print_state("ST", info->seq,
				dh->minor, cfg.ctx_resource_name, cfg.ctx_volume,
				si.current_state);
		break;
	case SIB_HELPER_PRE:
		printf("%u UH " MNV_FMT " %s\n", info->seq,
				dh->minor, cfg.ctx_resource_name, cfg.ctx_volume,
				si.helper);
		break;
	case SIB_HELPER_POST:
		printf("%u UH-post " MNV_FMT " %s 0x%04x\n", info->seq,
				dh->minor, cfg.ctx_resource_name, cfg.ctx_volume,
				si.helper, si.helper_exit_code);
		break;
	case SIB_SYNC_PROGRESS:
		{
		uint32_t shift = si.bits_rs_total >= (1ULL << 32) ? 16 : 10;
		uint64_t left = (si.bits_oos - si.bits_rs_failed) >> shift;
		uint64_t total = 1UL + (si.bits_rs_total >> shift);
		uint64_t tmp = 1000UL - left * 1000UL/total;

		unsigned synced = tmp;
		printf("%u SP " MNV_FMT " %i.%i\n", info->seq,
				dh->minor, cfg.ctx_resource_name, cfg.ctx_volume,
				synced / 10, synced % 10);
		}
		break;
	default:
		/* we could add the si.reason */
		printf("%u ?? " MNV_FMT " <other message, state info broadcast reason:%u>\n",
				info->seq,
				dh->minor, cfg.ctx_resource_name, cfg.ctx_volume,
				si.sib_reason);
		break;
	}
out:
	fflush(stdout);

	return 0;
}

static int w_connected_state(const struct drbd_cmd *cm, struct genl_info *info, void *u_ptr)
{
	struct state_info si = { .current_state = 0 };
	union drbd_state state;

	if (!info)
		return 0;

	if (!global_attrs[DRBD_NLA_STATE_INFO])
		return 0;

	if (state_info_from_attrs(&si, info)) {
		fprintf(stderr,"nla_policy violation!?\n");
		return 0;
	}

	if (si.sib_reason != SIB_STATE_CHANGE &&
	    si.sib_reason != SIB_GET_STATUS_REPLY)
		return 0;

	state.i = si.current_state;
	if (state.conn >= C_CONNECTED)
		return -1;  /* done waiting */
	if (state.conn < C_UNCONNECTED) {
		struct drbd_genlmsghdr *dhdr = info->userhdr;
		struct drbd_cfg_context cfg = { .ctx_volume = -1U };

		if (!wait_after_split_brain)
			return -1;  /* done waiting */
		drbd_cfg_context_from_attrs(&cfg, info);

		fprintf(stderr, "\ndrbd%u (%s[%u]) is %s, "
			       "but I'm configured to wait anways (--wait-after-sb)\n",
			       dhdr->minor,
			       cfg.ctx_resource_name, cfg.ctx_volume,
			       drbd_conn_str(state.conn));
	}

	return 0;
}

static int w_synced_state(const struct drbd_cmd *cm, struct genl_info *info, void *u_ptr)
{
	struct state_info si = { .current_state = 0 };
	union drbd_state state;

	if (!info)
		return 0;

	if (!global_attrs[DRBD_NLA_STATE_INFO])
		return 0;

	if (state_info_from_attrs(&si, info)) {
		fprintf(stderr,"nla_policy violation!?\n");
		return 0;
	}

	if (si.sib_reason != SIB_STATE_CHANGE &&
	    si.sib_reason != SIB_GET_STATUS_REPLY)
		return 0;

	state.i = si.current_state;

	if (state.conn == C_CONNECTED)
		return -1;  /* done waiting */

	if (!wait_after_split_brain && state.conn < C_UNCONNECTED)
		return -1;  /* done waiting */

	return 0;
}

/*
 * Check if an integer is a power of two.
 */
static bool power_of_two(int i)
{
	return i && !(i & (i - 1));
}

static void print_command_usage(const struct drbd_cmd *cm, enum usage_type ut)
{
	struct drbd_argument *args;

	if(ut == XML) {
		enum cfg_ctx_key ctx = cm->ctx_key;

		printf("<command name=\"%s\">\n", cm->cmd);
		if (ctx & CTX_RESOURCE_AND_CONNECTION)
			ctx = CTX_RESOURCE | CTX_CONNECTION;
		if (ctx & (CTX_RESOURCE | CTX_MINOR | CTX_ALL)) {
			bool more_than_one_choice =
				!power_of_two(ctx & (CTX_RESOURCE | CTX_MINOR | CTX_ALL));
			const char *indent = "\t\t" + !more_than_one_choice;
			if (more_than_one_choice)
				printf("\t<group>\n");
			if (ctx & CTX_RESOURCE)
				printf("%s<argument>resource</argument>\n", indent);
			if (ctx & CTX_MINOR)
				printf("%s<argument>minor</argument>\n", indent);
			if (ctx & CTX_ALL)
				printf("%s<argument>all</argument>\n", indent);
			if (more_than_one_choice)
				printf("\t</group>\n");
		}
		if (ctx & CTX_CONNECTION) {
			printf("\t<argument>local_addr</argument>\n");
			printf("\t<argument>remote_addr</argument>\n");
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
				field->describe_xml(field);
		}
		printf("</command>\n");
		return;
	}

	if (ut == BRIEF)
		wrap_printf(4, "%-18s  ", cm->cmd);
	else {
		wrap_printf(0, "USAGE:\n");

		wrap_printf(1, "%s %s", progname, cm->cmd);
		if (cm->ctx_key && ut != BRIEF) {
			enum cfg_ctx_key ctx = cm->ctx_key;

			if (ctx & CTX_RESOURCE_AND_CONNECTION)
				ctx = CTX_RESOURCE | CTX_CONNECTION;
			if (ctx & (CTX_RESOURCE | CTX_MINOR | CTX_ALL)) {
				bool first = true;

				wrap_printf(4, " {");
				if (ctx & CTX_RESOURCE) {
					wrap_printf(4, "%s", "|resource" + first);
					first = false;
				}
				if (ctx & CTX_MINOR) {
					wrap_printf(4, "%s", "|minor" + first);
					first = false;
				}
				if (ctx & CTX_ALL) {
					wrap_printf(4, "%s", "|all" + first);
					first = false;
				}
				wrap_printf(4, "}");
			}
			if (ctx & CTX_CONNECTION) {
				wrap_printf(4, " [{af}:]{local_addr}[:{port}]");
				wrap_printf(4, " [{af}:]{remote_addr}[:{port}]");
			}
		}

		if (cm->drbd_args) {
			for (args = cm->drbd_args; args->name; args++)
				wrap_printf(4, " {%s}", args->name);
		}

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
				n = field->usage(field, buffer, sizeof(buffer));
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

	printf("\nUSAGE: %s command device arguments options\n\n"
	       "Device is usually /dev/drbdX or /dev/drbd/X.\n"
	       "\nCommands are:\n",cmdname);


	for (i = 0; i < ARRAY_SIZE(commands); i++)
		print_command_usage(&commands[i], BRIEF);

	printf("\n\n"
	       "To get more details about a command issue "
	       "'drbdsetup help cmd'.\n"
	       "\n");
	/*
	printf("\n\nVersion: "PACKAGE_VERSION" (api:%d)\n%s\n",
	       API_VERSION, drbd_buildtag());
	*/
	if (addinfo)
		printf("\n%s\n", addinfo);

	exit(20);
}

static int modprobe_drbd(void)
{
	struct stat sb;
	int ret, retries = 10;

	ret = stat("/proc/drbd", &sb);
	if (ret && errno == ENOENT && 0 == system("/sbin/modprobe drbd")) {
		for(;;) {
			struct timespec ts = {
				.tv_nsec = 1000000,
			};

			ret = stat("/proc/drbd", &sb);
			if (!ret || retries-- == 0)
				break;
			nanosleep(&ts, NULL);
		}
	}
	if (ret) {
		fprintf(stderr, "Could not stat /proc/drbd: %m\n");
		fprintf(stderr, "Make sure that the DRBD kernel module is installed "
				"and can be loaded!\n");
	}
	return ret == 0;
}

void exec_legacy_drbdsetup(char **argv)
{
#ifdef DRBD_LEGACY_83
	static const char * const legacy_drbdsetup = "drbdsetup-83";
	char *progname, *drbdsetup;

	/* in case drbdsetup is called with an absolute or relative pathname
	 * look for the v83 drbdsetup binary in the same location,
	 * otherwise, just let execvp sort it out... */
	if ((progname = strrchr(argv[0], '/')) == 0) {
		drbdsetup = strdup(legacy_drbdsetup);
	} else {
		size_t len_dir, l;

		++progname;
		len_dir = progname - argv[0];

		l = len_dir + strlen(legacy_drbdsetup) + 1;
		drbdsetup = malloc(l);
		if (!drbdsetup) {
			fprintf(stderr, "Malloc() failed\n");
			exit(20);
		}
		strncpy(drbdsetup, argv[0], len_dir);
		strcpy(drbdsetup + len_dir, legacy_drbdsetup);
	}
	execvp(drbdsetup, argv);
#else
	fprintf(stderr, "This drbdsetup was not built with support for drbd-8.3\n"
		"Consider to rebuild with ./configure --with-83-support\n");
#endif
}

int main(int argc, char **argv)
{
	const struct drbd_cmd *cmd;
	struct option *options;
	int c, rv = 0;
	int longindex, first_optind;

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

	cmd = find_cmd_by_name(argv[1]);
	if (!cmd)
		print_usage_and_exit("invalid command");

	if (!modprobe_drbd()) {
		if (!strcmp(argv[0], "down") ||
		    !strcmp(argv[0], "secondary") ||
		    !strcmp(argv[0], "disconnect") ||
		    !strcmp(argv[0], "detach"))
			return 0; /* "down" succeeds even if drbd is missing */
		return 20;
	}

	if (try_genl) {
		if (cmd->continuous_poll && kernel_older_than(2, 6, 23))
			drbd_genl_family.nl_groups = -1;
		drbd_sock = genl_connect_to_family(&drbd_genl_family);
		if (!drbd_sock) {
			try_genl = 0;
			exec_legacy_drbdsetup(argv);
			/* Only reached in case exec() failed... */
			fprintf(stderr, "Could not connect to 'drbd' generic netlink family\n");
			return 20;
		}
		if (drbd_genl_family.version != API_VERSION ||
		    drbd_genl_family.hdrsize != sizeof(struct drbd_genlmsghdr)) {
			fprintf(stderr, "API mismatch!\n\t"
				"API version drbdsetup: %u kernel: %u\n\t"
				"header size drbdsetup: %u kernel: %u\n",
				API_VERSION, drbd_genl_family.version,
				(unsigned)sizeof(struct drbd_genlmsghdr),
				drbd_genl_family.hdrsize);
			return 20;
		}
	}

	/* Make argv[0] the command name so that getopt_long() will leave it in
	 * the first position. */
	argv++;
	argc--;

	options = make_longoptions(cmd);
	for (;;) {
		c = getopt_long(argc, argv, "(", options, &longindex);
		if (c == -1)
			break;
		if (c == '?' || c == ':')
			print_usage_and_exit(NULL);
	}
	/* All non-option arguments now are in argv[optind .. argc - 1]. */
	first_optind = optind;

	context = 0;
	if (cmd->ctx_key & (CTX_MINOR | CTX_RESOURCE | CTX_ALL | CTX_RESOURCE_AND_CONNECTION)) {
		if (argc == optind &&
		    !(cmd->ctx_key & (CTX_RESOURCE_AND_CONNECTION | CTX_CONNECTION)) &&
		    (cmd->ctx_key & CTX_ALL)) {
			context |= CTX_ALL;  /* assume "all" if no argument is given */
			objname = "all";
		} else {
			if (argc <= optind) {
				fprintf(stderr, "Missing first argument\n");
				print_command_usage(cmd, FULL);
				exit(20);
			}
			objname = argv[optind++];
			ensure_sanity_of_res_name(objname);
			if (!strcmp(objname, "all")) {
				if (!(cmd->ctx_key & CTX_ALL))
					print_usage_and_exit("command does not accept argument 'all'");
				context = CTX_ALL;
			} else if (cmd->ctx_key & CTX_MINOR) {
				minor = dt_minor_of_dev(objname);
				if (minor != -1U)
					context = CTX_MINOR;
				else if (!(cmd->ctx_key &
						(CTX_RESOURCE | CTX_RESOURCE_AND_CONNECTION))) {
					fprintf(stderr, "Cannot determine minor device number of "
							"device '%s'\n",
						objname);
					exit(20);
				}
			}
			/* It could have been "all", but was not.
			 * It could have been a minor number (or device node name), but was not.
			 * So it has to be a resource,
			 * or a resource and possibly connection specification.
			 * (CTX_CONNECTION alone will not enter this branch). */
			if (!context)
				context = CTX_RESOURCE;
		}
	}
	if (cmd->ctx_key & (CTX_CONNECTION | CTX_RESOURCE_AND_CONNECTION)) {
		if (argc <= optind + 1) {
			fprintf(stderr, "Missing connection endpoint argument\n");
			print_command_usage(cmd, FULL);
			exit(20);
		}
		opt_local_addr = argv[optind++];
		opt_peer_addr = argv[optind++];
		context |= CTX_CONNECTION;
	}

	/* Remove the options we have already processed from argv */
	if (first_optind != optind) {
		int n;

		for (n = 0; n < argc - optind; n++)
			argv[first_optind + n] = argv[optind + n];
		argc -= optind - first_optind;
	}

	if (objname == NULL)
		objname = "??";

	if ((context & CTX_MINOR) && !cmd->lockless)
		lock_fd = dt_lock_drbd(minor);

	rv = cmd->function(cmd, argc, argv);

	if ((context & CTX_MINOR) && !cmd->lockless)
		dt_unlock_drbd(lock_fd);
	return rv;
}
#endif
