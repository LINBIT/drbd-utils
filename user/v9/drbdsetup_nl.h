/*
 * drbdsetup netlink dialects.
 *
 * Everything that knows about the bytes of a generic netlink family lives
 * behind struct drbd_nl_dialect. The command logic only uses the neutral
 * identifiers and records in this header.
 */
#ifndef __DRBDSETUP_NL_H
#define __DRBDSETUP_NL_H

#include <stdbool.h>
#include <linux/types.h>
#include <linux/netlink.h>
#include <linux/genetlink.h>

#include "libgenl.h"
#include "linux/drbd.h"
#include "linux/drbd_genl_userspace.h"

/* Userspace-only result codes, outside the kernel's ERR_* range. */
#define OTHER_ERROR 900
#define ERR_MODULE_UNLOADED 901
/* drbd2: the kernel refused the request with an errno and an extended ACK text */
#define ERR_EXTACK 902

/* is_intentional is a boolean value we get via nl from kernel. if we use new
 * utils and old kernel we don't get it, so we set this default, get kernel
 * info, and then decide from the value if the kernel was new enough */
#define IS_INTENTIONAL_DEF 3
/* same for DEV_IS_OPEN */
#define DEV_IS_OPEN_UNKNOWN 3

/* The commands drbdsetup sends, independent of the family. */
enum drbd_nl_cmd {
	DRBD_NL_CMD_NONE = 0,
	DRBD_NL_CMD_NEW_RESOURCE,
	DRBD_NL_CMD_DEL_RESOURCE,
	DRBD_NL_CMD_RESOURCE_OPTS,
	DRBD_NL_CMD_RENAME_RESOURCE,
	DRBD_NL_CMD_DOWN,
	DRBD_NL_CMD_PRIMARY,
	DRBD_NL_CMD_SECONDARY,
	DRBD_NL_CMD_SUSPEND_IO,
	DRBD_NL_CMD_RESUME_IO,
	DRBD_NL_CMD_NEW_MINOR,
	DRBD_NL_CMD_DEL_MINOR,
	DRBD_NL_CMD_ATTACH,
	DRBD_NL_CMD_DETACH,
	DRBD_NL_CMD_DISK_OPTS,
	DRBD_NL_CMD_RESIZE,
	DRBD_NL_CMD_OUTDATE,
	DRBD_NL_CMD_INVALIDATE,
	DRBD_NL_CMD_NEW_C_UUID,
	DRBD_NL_CMD_NEW_PEER,
	DRBD_NL_CMD_DEL_PEER,
	DRBD_NL_CMD_CONNECT,
	DRBD_NL_CMD_DISCONNECT,
	DRBD_NL_CMD_NET_OPTS,
	DRBD_NL_CMD_FORGET_PEER,
	DRBD_NL_CMD_NEW_PATH,
	DRBD_NL_CMD_DEL_PATH,
	DRBD_NL_CMD_PEER_DEVICE_OPTS,
	DRBD_NL_CMD_INVALIDATE_PEER,
	DRBD_NL_CMD_PAUSE_SYNC,
	DRBD_NL_CMD_RESUME_SYNC,
	DRBD_NL_CMD_START_OV,
	DRBD_NL_CMD_GET_TIMEOUT_TYPE,
	DRBD_NL_CMD_GET_RESOURCES,
	DRBD_NL_CMD_GET_DEVICES,
	DRBD_NL_CMD_GET_CONNECTIONS,
	DRBD_NL_CMD_GET_PEER_DEVICES,
	DRBD_NL_CMD_GET_PATHS,
	DRBD_NL_CMD_GET_INITIAL_STATE,
	__DRBD_NL_CMD_MAX,
};

/* The attribute sets a request may carry; same list as the kernel core. */
enum drbd_nl_attr_set {
	NL_SET_NONE = 0,
	NL_SET_DISK_CONF,
	NL_SET_NET_CONF,
	NL_SET_RES_OPTS,
	NL_SET_PEER_DEVICE_CONF,
	NL_SET_DEVICE_CONF,
	NL_SET_SET_ROLE_PARMS,
	NL_SET_RESIZE_PARMS,
	NL_SET_START_OV_PARMS,
	NL_SET_NEW_C_UUID_PARMS,
	NL_SET_DISCONNECT_PARMS,
	NL_SET_DETACH_PARMS,
	NL_SET_INVALIDATE_PARMS,
	NL_SET_INVALIDATE_PEER_PARMS,
	NL_SET_FORGET_PEER_PARMS,
	NL_SET_CONNECT_PARMS,
	NL_SET_PATH_PARMS,
	NL_SET_RENAME_RESOURCE_PARMS,
	NL_SET_SUSPEND_IO_PARMS,
	__NL_SET_MAX,
};

/* What one received message describes. */
enum drbd_nl_obj {
	NL_OBJ_RESOURCE,
	NL_OBJ_DEVICE,
	NL_OBJ_CONNECTION,
	NL_OBJ_PEER_DEVICE,
	NL_OBJ_PATH,
	NL_OBJ_HELPER,
	NL_OBJ_INITIAL_STATE_DONE,
};

/* The result of a "do" request. */
struct drbd_nl_outcome {
	int ret_code;		/* NO_ERROR, ERR_*, SS_*, OTHER_ERROR, ERR_EXTACK */
	int errnum;		/* errno of an NLMSG_ERROR reply, else 0 */
	const char *desc;	/* extack text or strerror(), else NULL */
	const char *info;	/* NUL separated info texts from the kernel */
	int info_len;		/* bytes in info including the final NUL */
	int timeout_type;	/* UT_* for get-timeout-type, else -1 */
};

/*
 * One dump message or notification, in the representation the command
 * logic works with. Option nests (opts, opts2, paths) are in the legacy
 * attribute numbering, whatever the family; they point into a buffer that
 * is only valid until the next parse_msg(), copy them to keep them.
 */
struct drbd_nl_event {
	enum drbd_nl_obj kind;
	enum drbd_notification_type action;	/* NOTIFY_* | NOTIFY_CONTINUES */
	unsigned int seq;			/* nlmsg_seq, for events2 ordering */
	unsigned int minor;			/* -1U when unknown */
	int ret_code;				/* legacy per-message ret_code */
	struct drbd_cfg_context ctx;
	bool have_info;
	union {
		struct resource_info resource;
		struct device_info device;
		struct connection_info connection;
		struct peer_device_info peer_device;
		struct drbd_path_info path;
	} info;
	union {
		struct resource_statistics resource;
		struct device_statistics device;
		struct connection_statistics connection;
		struct peer_device_statistics peer_device;
	} stats;				/* all bytes -1 when absent */
	struct rename_resource_info rename;
	struct drbd_helper_info helper;
	struct nlattr *opts;	/* resource-opts, disk-conf, net-conf, peer-device-conf */
	struct nlattr *opts2;	/* device-conf of a device */
	struct nlattr *paths;	/* DRBD_NLA_PATH_PARMS-shaped list of address blobs */
};

enum drbd_nl_msg {
	NL_MSG_SKIP,	/* not for us, or nothing the command logic needs */
	NL_MSG_DONE,	/* NLMSG_DONE */
	NL_MSG_EVENT,	/* *ev filled */
	NL_MSG_INVALID,	/* a message of our family that did not validate */
};

struct drbd_nl_dialect {
	const char *name;
	struct genl_family *family;

	/* Does the connected kernel implement this command? */
	bool (*cmd_known)(enum drbd_nl_cmd cmd);

	/* Request side. "what" holds enum cfg_ctx_key bits (drbdsetup.h). */
	int (*put_request)(struct msg_buff *msg, enum drbd_nl_cmd cmd, int nlflags);
	int (*put_context)(struct msg_buff *msg, const struct drbd_cfg_context *ctx,
			   unsigned int minor, unsigned int what,
			   enum drbd_nl_attr_set set);
	struct nlattr *(*nest_start)(struct msg_buff *msg, enum drbd_nl_attr_set set);
	int (*attr_id)(enum drbd_nl_attr_set set, int field_id);
	const struct nla_policy *(*policy)(enum drbd_nl_attr_set set, int *maxtype);
	void (*put_set_defaults)(struct msg_buff *msg);

	/* The reply of a "do" request or an NLMSG_ERROR. */
	int (*recv_outcome)(struct nlmsghdr *nlh, struct drbd_nl_outcome *out);

	/* Dumps and notifications. "cmd" is the command being served. */
	enum drbd_nl_msg (*parse_msg)(struct nlmsghdr *nlh, enum drbd_nl_cmd cmd,
				      struct drbd_nl_event *ev);
};

/* The active dialect. Defined in config_flags.c (the lowest common object
 * of drbdsetup and drbdadm); NULL in drbdadm, which never marshals. */
extern const struct drbd_nl_dialect *nl;

extern const struct drbd_nl_dialect legacy_dialect;
#ifndef WINDRBD
extern const struct drbd_nl_dialect drbd2_dialect;
#endif

/* Common helpers, drbdsetup_nl.c */
void drbd_nl_event_init(struct drbd_nl_event *ev, enum drbd_nl_obj kind);
struct drbd_nl_event *drbd_nl_event_copy(const struct drbd_nl_event *ev);
void drbd_nl_event_free(struct drbd_nl_event *ev);
int drbd_nl_disk_conf_from_nest(struct disk_conf *dc, struct nlattr *nest);

/* Platform specific, drbdsetup_linux.c / drbdsetup_windrbd.c: connect to
 * the family the kernel offers and set "nl". NULL on failure. */
struct genl_sock *drbd_nl_connect(struct genl_connect_options *opts);

#endif /* __DRBDSETUP_NL_H */
