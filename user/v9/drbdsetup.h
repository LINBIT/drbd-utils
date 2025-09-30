/* Function prototypes for platform-specific drbdsetup functions */

#ifndef __DRBD_SETUP_H
#define __DRBD_SETUP_H

#include <stdbool.h>
#include "libgenl.h"
#include <linux/drbd_genl_api.h>
#include <linux/types.h>


#define OTHER_ERROR 900
#define ERR_MODULE_UNLOADED 901

/* EXIT code to map ERR_MODULE_UNLOADED to */
#define ERR_EXIT_MODULE_UNLOADED  121

#define ADDRESS_STR_MAX 256

/* is_intentional is a boolean value we get via nl from kernel. if we use new
 * utils and old kernel we don't get it, so we set this default, get kernel
 * info, and then decide from the value if the kernel was new enough */
#define IS_INTENTIONAL_DEF 3
/* same for DEV_IS_OPEN */
#define DEV_IS_OPEN_UNKNOWN 3


/* Used as base value, for getopt_long()'s options to express
 * old, alternate names for options.
 */
#define OPT_ALT_BASE 1000

/* In the 8.4 time the command understood this option. */
#define OPT_COMPAT84 2000


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
	CTX_RESOURCE = 1,
	CTX_PEER_NODE_ID = 2,
	CTX_MINOR = 4,
	CTX_VOLUME = 8,
	CTX_MY_ADDR = 16,
	CTX_PEER_ADDR = 32,
	CTX_ALL = 64,

	CTX_MULTIPLE_ARGUMENTS = 128,

	/* To identify a connection, we use (resource_name, peer_node_id) */
	CTX_PEER_NODE = CTX_RESOURCE | CTX_PEER_NODE_ID | CTX_MULTIPLE_ARGUMENTS,
	CTX_PEER_DEVICE = CTX_PEER_NODE | CTX_VOLUME,
};

struct drbd_cmd {
	const char* cmd;
	enum cfg_ctx_key ctx_key;
	int cmd_id;
	int tla_id; /* top level attribute id */
	int (*function)(const struct drbd_cmd *, int, char **);
	struct drbd_argument *drbd_args;
	int (*handle_reply)(const struct drbd_cmd*, struct genl_info *, void *u_ptr);
	struct option *options;
	bool missing_ok;
	bool warn_on_missing;
	bool continuous_poll;
	bool set_defaults;
	bool lockless;
	struct context_def *ctx;
	const char *summary;
#ifdef WITH_84_SUPPORT
	struct field_def *compat_84_fields;
#endif
};

enum {
	E_POLL_TIMEOUT = 1,
	E_POLL_ERR,
	E_POLL_EXTRA_FD,
};

struct resources_list {
	struct resources_list *next;
	char *name;
	struct nlattr *res_opts;
	struct resource_info info;
	struct resource_statistics statistics;
	struct rename_resource_info rename_info;
	bool destroyed; /* only used by events2 */
	struct devices_list *devices; /* only used by events2 */
	struct connections_list *connections; /* only used by events2 */
};
struct devices_list {
	struct devices_list *next;
	unsigned minor;
	struct drbd_cfg_context ctx;
	struct nlattr *disk_conf_nl;
	struct disk_conf disk_conf;
	struct nlattr *device_conf_nl;
	struct device_info info;
	struct device_statistics statistics;
};
struct connections_list {
	struct connections_list *next;
	struct drbd_cfg_context ctx;
	struct nlattr *path_list;
	struct nlattr *net_conf;
	struct connection_info info;
	struct connection_statistics statistics;
	struct peer_devices_list *peer_devices; /* only used by events2 */
	struct paths_list *paths; /* only used by events2 */
};
struct peer_devices_list {
	struct peer_devices_list *next;
	struct drbd_cfg_context ctx;
	struct nlattr *peer_device_conf;
	struct peer_device_info info;
	struct peer_device_statistics statistics;
	struct devices_list *device;
	int timeout_ms; /* used only by wait_for_family() */
};
struct paths_list {
	struct paths_list *next;
	struct drbd_cfg_context ctx;
	struct drbd_path_info info;
};
enum usage_type {
	BRIEF,
	FULL,
	XML,
};

typedef
__attribute__((format(printf, 2, 3)))
int (*wrap_printf_fn_t)(int indent, const char *format, ...);

extern char *progname;
typedef int (*fake_generic_get_t)(const struct drbd_cmd *cm, int timeout_arg, void *u_ptr);
/* Used by drbdsetup_instrumented to redirect calls to generic_get() */
extern fake_generic_get_t fake_generic_get;
extern char *objname;
extern bool opt_now;
extern bool opt_poll;
extern int opt_verbose;
extern bool opt_statistics;
extern bool opt_timestamps;
extern bool opt_diff;
extern bool opt_fullch;
extern struct drbd_cfg_context global_ctx;
extern enum cfg_ctx_key context;
extern unsigned int minor;
extern const struct drbd_cmd new_resource_cmd;
extern const struct drbd_cmd new_minor_cmd;
extern const struct drbd_cmd attach_cmd;
extern const struct drbd_cmd connect_cmd;
extern const struct drbd_cmd new_peer_cmd;
extern const struct drbd_cmd del_peer_cmd;
extern const struct drbd_cmd new_path_cmd;
extern const struct drbd_cmd del_path_cmd;
extern const struct drbd_cmd disconnect_cmd;
extern const struct drbd_cmd peer_device_options_cmd;

struct option *make_longoptions(const struct drbd_cmd *cm, bool accept_84_compat);
int _generic_config_cmd(const struct drbd_cmd *cm, int argc, char **argv);
void print_command_usage(const struct drbd_cmd *cm, enum usage_type);
int sockaddr_from_str(struct sockaddr_storage *storage, const char *str);

bool kernel_older_than(int version, int patchlevel, int sublevel);
int conv_block_dev(struct drbd_argument *ad, struct msg_buff *msg, struct drbd_genlmsghdr *dhdr, char* arg);
char *kernel_device_to_userland_device(char *kernel_dev);
int genl_join_mc_group_and_ctrl(struct genl_sock *s, const char *name);
int poll_hup(struct genl_sock *s, int timeout_ms, int extra_poll_fd);
int modprobe_drbd(void);
char *address_str(char *buffer, void* address, int addr_len);
const char *susp_str(struct resource_info *info);
const char *resync_susp_str(struct peer_device_info *info);
const char *no_yes_unknown_str(unsigned char almost_bool);
const char *intentional_diskless_str(struct device_info *info);
const char *peer_intentional_diskless_str(struct peer_device_info *info);
const char *backing_dev_str(struct device_info *info);
void print_resource_statistics(int indent,
			       struct resource_statistics *old,
			       struct resource_statistics *new,
			       wrap_printf_fn_t wrap_printf);
void print_device_statistics(int indent,
			     struct device_statistics *old,
			     struct device_statistics *new,
			     wrap_printf_fn_t wrap_printf);
void print_connection_statistics(int indent,
				 struct connection_statistics *old,
				 struct connection_statistics *new,
				 wrap_printf_fn_t wrap_printf);
void print_peer_device_statistics(int indent,
				  struct peer_device_statistics *old,
				  struct peer_device_statistics *s,
				  wrap_printf_fn_t wrap_printf);
__attribute__((format(printf, 2, 3)))
int nowrap_printf(int indent, const char *format, ...);
struct resources_list *new_resource_from_info(struct genl_info *info);
struct devices_list *new_device_from_info(struct genl_info *info);
struct connections_list *new_connection_from_info(struct genl_info *info);
struct peer_devices_list *new_peer_device_from_info(struct genl_info *info);
struct paths_list *new_path_from_info(struct genl_info *info);
void free_resources(struct resources_list *);
void free_device(struct devices_list *);
void free_devices(struct devices_list *);
void free_connection(struct connections_list *);
void free_connections(struct connections_list *);
void free_peer_device(struct peer_devices_list *);
void free_peer_devices(struct peer_devices_list *);
void free_paths(struct paths_list *);
int drbd_tla_parse(struct nlattr *tla[], struct nlmsghdr *nlh);

int drbdsetup_main(int argc, char **argv);

#endif
