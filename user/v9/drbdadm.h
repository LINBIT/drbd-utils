#ifndef DRBDADM_H
#define DRBDADM_H

#include <stdbool.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <net/if.h>
/* We carry a copy of <sys/queue.h> around,
 * some older versions of it don't have the STAILQ_* variants */
#include "sys_queue.h"
#include <stdint.h>
#include <stdarg.h>
#include <fcntl.h>

#ifndef O_CLOEXEC
#warning "O_CLOEXEC undefined, redefining to 0"
#define O_CLOEXEC 0
#endif

#include "config.h"
#include "shared_main.h"

/* FIXME keep in sync with GENL_MAGIC_VERSION,
 * without including all the genl magic...
 */
#define API_VERSION 2

struct d_name
{
	char *name;
	STAILQ_ENTRY(d_name) link;
};

STAILQ_HEAD(names, d_name);

struct d_option
{
	STAILQ_ENTRY(d_option) link;
	char* name;
	char* value;
	unsigned int mentioned  :1 ; // for the adjust command.
	unsigned int is_escaped :1 ;
	unsigned int adj_skip :1;
	unsigned int inherited :1;
};

STAILQ_HEAD(options, d_option);

struct d_address
{
	char* addr;
	char* port;
	char* af;
	int is_local_address:1;
};

struct d_proxy_info
{
	struct names on_hosts;
	struct d_address inside;
	struct d_address outside;
	struct options options; /* named proxy_options in other places */
	struct options plugins; /* named proxy_plugins in other places */
};

struct connection;
struct d_volume;

struct peer_device
{
	int vnr; /* parsed */
	struct options pd_options; /* parsed */
	int config_line; /* parsed here */

	struct connection *connection;
	/* No pointer to d_volume, because there are two!
	   On each side of the connection!
	   Iterate d_host_info and find the correct volume by matching vnr! */

	STAILQ_ENTRY(peer_device) connection_link; /* added during parsing */
	/* no volume_link !! */

	unsigned int implicit:1; /* Do not dump by default */
};
STAILQ_HEAD(peer_devices, peer_device);

struct d_volume
{
	unsigned vnr;
	char* device;
	unsigned device_minor;
	char* disk;
	char* meta_disk;
	char* meta_index;
	int meta_major;
	int meta_minor;
	STAILQ_ENTRY(d_volume) link;
	struct options disk_options; /* Additional per volume options */
	struct options pd_options; /* peer device options */
	struct peer_devices peer_devices;

	const char *v_config_file;
	int v_device_line;
	int v_disk_line;
	int v_meta_disk_line;

	/* Do not dump an explicit volume section */
	unsigned int implicit :1 ;

	/* flags for "drbdadm adjust" */
	unsigned int adj_del_minor :1;
	unsigned int adj_new_minor :1;
	unsigned int adj_detach :1;
	unsigned int adj_attach :1;
	unsigned int adj_resize :1;
	unsigned int adj_disk_opts :1;
	unsigned int parsed_device :1;
	unsigned int parsed_disk :1;
	unsigned int parsed_meta_disk :1;
};

STAILQ_HEAD(volumes, d_volume);

struct d_host_info
{
	struct names on_hosts;
	struct volumes volumes;
	struct d_address address;
	struct d_proxy_info *proxy_compat_only; /* parsed here, only for 8.4 compatibility */
	STAILQ_ENTRY(d_host_info) link;
	struct d_resource* lower;  /* for device stacking */
	char *lower_name;          /* for device stacking, before bind_stacked_res() */
	int config_line;
	unsigned int implicit:1;   /* Implicitly declared with an host xx address statement*/
	unsigned int by_address:1; /* Match to machines by address, not by names (=on_hosts) */
	unsigned int used_as_me:1; /* May be set in set_me_in_resource() */
	unsigned int require_minor:1; /* Requires device */
	struct options res_options; /* Additional per host options */
	char* node_id;
};

STAILQ_HEAD(hosts, d_host_info);

struct hname_address
{
	char *name;			/* parsed */
	int config_line;		/* parsed here */
	struct d_address address;	/* parsed */
	struct d_proxy_info *proxy;     /* parsed here */
	struct d_host_info *host_info;	/* determined in post_parse */
	unsigned int used_as_me:1;
	unsigned int faked_hostname:1;
	unsigned int by_address:1;
	unsigned int parsed_address:1;
	unsigned int parsed_port:1;
	unsigned int conflicts:1;
	STAILQ_ENTRY(hname_address) link;
};
STAILQ_HEAD(hname_address_pairs, hname_address);

struct path
{
	int config_line; /* parsed here */
	struct hname_address_pairs hname_address_pairs; /* parsed here */

	struct d_address *my_address; /* determined in set_me_in_resource() */
	struct d_address *peer_address;
	struct d_address *connect_to;
	struct d_proxy_info *my_proxy;
	struct d_proxy_info *peer_proxy;

	unsigned int implicit:1;
	unsigned int adj_seen:1;
	unsigned int proxy_conn_is_down:1;
	unsigned int ignore:1;
	STAILQ_ENTRY(path) link;
};
STAILQ_HEAD(paths, path);

struct connection
{
	char *name; /* parsed */
	struct paths paths;
	int config_line; /* parsed here */

	struct peer_devices peer_devices;
	struct d_host_info *peer;

	struct options net_options; /* parsed here, inherited from res, used here */
	struct options pd_options; /* parsed here, inherited into the peer_devices */
	unsigned int ignore:1;
	/* ignore_tmp is a flag that has the same semantic as ignore,
	 * but the user is free to manipulate it and run checks on it
	 * e.g., would a connection be enabled multiple times
	 * this avoids direct maniuplation/restore of the ignore flag itself */
	unsigned int ignore_tmp:1;
	unsigned int me:1;
	unsigned int implicit:1;
	unsigned int is_standalone:1;
	/* on_cmdline is set it was explicity asked for on the command line.
	   Not set if only found by iterating over all connextions in resource */
	unsigned int on_cmdline:1;
	STAILQ_ENTRY(connection) link;
};
STAILQ_HEAD(connections, connection);

struct mesh
{
	struct names hosts; /* parsed here. Expanded to connections in post_parse */
	struct options net_options;
	STAILQ_ENTRY(mesh) link;
};
STAILQ_HEAD(meshes, mesh);

struct d_resource
{
	char* name;

	struct d_resource *template;
	struct volumes volumes;
	struct connections connections;

	struct d_host_info* me;
	struct hosts all_hosts;

	struct meshes meshes;

	struct options net_options; /* parsed here, inherited to connections */
	struct options disk_options;
	struct options pd_options; /* peer device options */
	struct options res_options;
	struct options startup_options;
	struct options handlers;
	struct options proxy_options;
	struct options proxy_plugins;
	STAILQ_ENTRY(d_resource) link;
	char *config_file; /* The config file this resource is define in.*/
	int start_line;
	unsigned int stacked_timeouts:1;
	unsigned int ignore:1;
	unsigned int stacked:1;        /* Stacked on this node */
	unsigned int stacked_on_one:1; /* Stacked either on me or on peer */
	unsigned int peers_addrs_set:1; /* all peer addresses set */
	unsigned int no_bitmap_done:1;

	/* if a prerequisite command failed, don't try any further commands.
	 * see run_deferred_cmds() */
	unsigned int skip_further_deferred_command:1;
};

STAILQ_HEAD(resources, d_resource);

struct cfg_ctx;

struct adm_cmd {
	const char *name;
	int (*function) (const struct cfg_ctx *);
	const struct context_def *drbdsetup_ctx;
	/* which level this command is for.
	 * 0: don't show this command, ever
	 * 1: normal administrative commands, shown in normal help
	 * 2-4: shown on "drbdadm hidden-commands"
	 * 2: useful for shell scripts
	 * 3: callbacks potentially called from kernel module on certain events
	 * 4: advanced, experts and developers only */
	unsigned int show_in_usage:3;
	/* if set, command requires an explicit resource name */
	unsigned int res_name_required:1;
	/* if set, the backend command expects the resource name */
	unsigned int backend_res_name:1;
	/* Give the backend(drbdsetup) more time to complete its mission */
	unsigned int takes_long:1;
	/* if set, command requires an explicit volume number as well */
	unsigned int vol_id_required:1;
	/* most commands need to iterate over all volumes in the resource */
	unsigned int iterate_volumes:1;
	unsigned int vol_id_optional:1;
	/* error out if the ip specified is not available/active now */
	unsigned int verify_ips:1;
	/* if set, use the "cache" in /var/lib/drbd to figure out
	 * which config file to use.
	 * This is necessary for handlers (callbacks from kernel) to work
	 * when using "drbdadm -c /some/other/config/file" */
	unsigned int use_cached_config_file:1;
	/* need_peer could also be named iterate_peers */
	unsigned int need_peer:1;
	unsigned int is_proxy_cmd:1;
	unsigned int uc_dialog:1; /* May show usage count dialog */
	unsigned int test_config:1; /* Allow -t option */
	unsigned int disk_required:1; /* cmd needs vol->disk or vol->meta_[disk|index] */
	unsigned int iterate_paths:1; /* cmd needs path set, eventually iterate over all */
};

struct cfg_ctx {
	/* res == NULL: does not care for resources, or iterates over all
	 * resources in the global "struct d_resource *config" */
	struct d_resource *res;
	/* vol == NULL: operate on the resource itself, or iterates over all
	 * volumes in res */
	struct d_volume *vol;

	struct connection *conn;

	struct path *path;

	const struct adm_cmd *cmd;
};


extern char *canonify_path(char *path);
extern int pushd(const char *path);
extern void popd(int fd);

enum {
	ADJUST_DISK = 1,
	ADJUST_NET = 2,
	ADJUST_SKIP_CHECKED = 4,
};
extern int _adm_adjust(const struct cfg_ctx *ctx, int flags);
extern int is_equal(struct context_def *ctx, struct d_option *a, struct d_option *b);

extern struct adm_cmd new_minor_cmd;
extern struct adm_cmd new_resource_cmd;
extern struct adm_cmd res_options_cmd;
extern struct adm_cmd res_options_defaults_cmd;
extern struct adm_cmd attach_cmd;
extern struct adm_cmd disk_options_cmd;
extern struct adm_cmd disk_options_defaults_cmd;
extern struct adm_cmd resize_cmd;
extern struct adm_cmd new_peer_cmd;
extern struct adm_cmd del_peer_cmd;
extern struct adm_cmd new_path_cmd;
extern struct adm_cmd del_path_cmd;
extern struct adm_cmd connect_cmd;
extern struct adm_cmd net_options_cmd;
extern struct adm_cmd net_options_defaults_cmd;
extern struct adm_cmd peer_device_options_defaults_cmd;
extern struct adm_cmd disconnect_cmd;
extern struct adm_cmd detach_cmd;
extern struct adm_cmd del_minor_cmd;
extern struct adm_cmd proxy_conn_down_cmd;
extern struct adm_cmd proxy_conn_up_cmd;
extern struct adm_cmd proxy_conn_plugins_cmd;
extern struct adm_cmd proxy_reconf_cmd;

struct d_name *find_backend_option(const char *opt_name);
extern int adm_create_md(const struct cfg_ctx *);
extern int _adm_drbdmeta(const struct cfg_ctx *, int flags, char *argument);

extern struct d_option *find_opt(struct options *base, const char *name);
extern bool del_opt(struct options *base, const char * const name);

/* stages of configuration, as performed on "drbdadm up"
 * or "drbdadm adjust":
 */
enum drbd_cfg_stage {
	/* prerequisite stage: create objects, start daemons, ... */
	CFG_PREREQ,

	/* run time changeable settings of resources */
	CFG_RESOURCE,

	/* detach/attach local disks, */
	/* detach, del-minor */
	CFG_DISK_PREP_DOWN,
	/* new-minor */
	CFG_DISK_PREP_UP,

	/* disconnect */
	CFG_NET_DISCONNECT,
	/* down,  del-peer, proxy down, del-path */
	CFG_NET_PREP_DOWN,
	/* add-peer, proxy up */
	CFG_NET_PREP_UP,

	/* add-path */
	CFG_NET_PATH,

	/* discard/set connection parameters */
	CFG_NET,

	/* peer device options */
	CFG_PEER_DEVICE,

	/* attach, disk-options, resize */
	CFG_DISK,

	/* actually start with connection attempts */
	CFG_NET_CONNECT,

	__CFG_LAST
};

#define SCHEDULE_ONCE 0x1000

extern void schedule_deferred_cmd(struct adm_cmd *, const struct cfg_ctx *, enum drbd_cfg_stage);
extern void maybe_exec_legacy_drbdadm(char **argv);
extern void uc_node(enum usage_count_type type);
extern int have_ip(const char *af, const char *ip);
extern void free_opt(struct d_option *item);
typedef enum {SETUP_MULTI, CTX_FIRST, WOULD_ENABLE_DISABLED, WOULD_ENABLE_MULTI_TIMES} checks;
extern int ctx_by_name(struct cfg_ctx *ctx, const char *id, checks check);
enum pr_flags {
	NO_HOST_SECT_ALLOWED  = 4,
	PARSE_FOR_ADJUST = 8
};

extern int check_uniq(const char *what, const char *fmt, ...);
extern int check_uniq_file_line(const char *file, const int line, const char *what, const char *fmt, ...);
extern struct d_resource* parse_resource_for_adjust(const struct cfg_ctx *ctx);
extern struct d_resource* parse_resource(char*, enum pr_flags);
extern void post_parse(struct resources *, enum pp_flags);
extern struct connection *alloc_connection();
extern struct path *alloc_path();
extern struct d_volume *alloc_volume(void);
extern struct peer_device *alloc_peer_device();
extern void free_connection(struct connection *connection);
extern void expand_common(void);
extern void global_validate_maybe_expand_die_if_invalid(int expand, enum pp_flags flags);
extern struct d_option *new_opt(char *name, char *value);
extern int hostname_in_list(const char *name, struct names *names);
extern char *_names_to_str(char* buffer, struct names *names);
extern char *_names_to_str_c(char* buffer, struct names *names, char c);
#define NAMES_STR_SIZE 255
#define names_to_str(N) _names_to_str(alloca(NAMES_STR_SIZE+1), N)
#define names_to_str_c(N, C) _names_to_str_c(alloca(NAMES_STR_SIZE+1), N, C)
extern struct d_name *names_from_str(char* str);
extern struct d_volume *volume_by_vnr(struct volumes *volumes, int vnr);
extern void free_names(struct names *names);
extern void set_me_in_resource(struct d_resource* res, int match_on_proxy);
extern void set_peer_in_resource(struct d_resource* res, int peer_required);
extern void set_on_hosts_in_res(struct d_resource *res);
extern void set_disk_in_res(struct d_resource *res);
extern int _proxy_connect_name_len(const struct d_resource *res, const struct connection *conn);
extern char *_proxy_connection_name(char *conn_name, const struct d_resource *res, const struct connection *conn);
#define proxy_connection_name(RES, CONN) \
	_proxy_connection_name(alloca(_proxy_connect_name_len(RES, CONN)), RES, CONN)
extern struct d_resource *res_by_name(const char *name);
extern struct d_host_info *find_host_info_by_name(struct d_resource* res, char *name);
extern int addresses_cmp(struct d_address *addr1, struct d_address *addr2);
extern bool addresses_equal(struct d_address *addr1, struct d_address *addr2);
extern int btree_key_cmp(const void *a, const void *b);
int parse_proxy_options_section(struct d_proxy_info **proxy);
/* conn_name is optional and mostly for compatibility with dcmd */
struct peer_device *find_peer_device(struct connection *conn, int vnr);
bool peer_diskless(struct peer_device *peer_device);

extern char *config_file;
extern char *config_save;
extern int config_valid;
extern struct resources config;
extern struct d_resource* common;
extern int line, fline;
extern void *global_btree;

extern int no_tty;
extern int dry_run;
extern int verbose;
extern int adjust_more_than_one_resource;
extern char* drbdsetup;
extern char* drbdmeta;
extern char* drbd_proxy_ctl;
extern char* drbdadm_83;
extern char* drbdadm_84;
extern char ss_buffer[1024];
extern const char *hostname;
extern struct names backend_options;

/* ssprintf() places the result of the printf in the current stack
   frame and sets ptr to the resulting string. If the current stack
   frame is destroyed (=function returns), the allocated memory is
   freed automatically */

/*
  // This is the nicer version, that does not need the ss_buffer.
  // But it only works with very new glibcs.

#define ssprintf(...) \
	 ({ int _ss_size = snprintf(0, 0, ##__VA_ARGS__);        \
	 char *_ss_ret = __builtin_alloca(_ss_size+1);           \
	 snprintf(_ss_ret, _ss_size+1, ##__VA_ARGS__);           \
	 _ss_ret; })
*/

#define ssprintf(...) \
	strcpy(alloca(snprintf(ss_buffer,sizeof(ss_buffer),##__VA_ARGS__)+1),ss_buffer)

#ifndef offsetof
/* I do not care about non GCC compilers */
#define offsetof(type, member)  __builtin_offsetof(type, member)
#endif

/* Linux's edition of sys/queue.h misses STAILQ_LAST */
#ifndef STAILQ_LAST
#define STAILQ_LAST(head, field)					\
	(STAILQ_EMPTY((head)) ?						\
	 NULL :								\
	 ((typeof((head)->stqh_first))					\
	  ((char *)((head)->stqh_last) - offsetof(typeof(*(head)->stqh_first), field))))
#endif

#define STAILQ_INSERT_ORDERED(head, elem, field) do {			\
	typeof(*elem) *e = (elem); /* evaluate once */			\
	typeof(*elem) *t = STAILQ_LAST(head, field);			\
	if (t == NULL) { /* STAILQ is empty */				\
		STAILQ_INSERT_HEAD(head, e, field);			\
	} else if (t->vnr <= e->vnr) {					\
		STAILQ_INSERT_TAIL(head, e, field);			\
	} else {							\
		typeof(*elem) *p = NULL;				\
		STAILQ_FOREACH(t, head, field) {			\
			if (t->vnr > e->vnr) {				\
				if (p == NULL)				\
					STAILQ_INSERT_HEAD(head, e, field); \
				else					\
					STAILQ_INSERT_AFTER(head, p, e, field); \
				break;					\
			}						\
			p = t;						\
		}							\
	}								\
} while (0)

/* CAUTION: arguments may not have side effects! */
#define for_each_resource(var, head) STAILQ_FOREACH(var, head, link)
#define for_each_volume(var, head) STAILQ_FOREACH(var, head, link)
#define for_each_volume_safe(var, next, head) STAILQ_FOREACH_SAFE(var, next, head, link)
#define for_each_host(var, head) STAILQ_FOREACH(var, head, link)
#define for_each_connection(var, head) STAILQ_FOREACH(var, head, link)
#define for_each_path(var, head) STAILQ_FOREACH(var, head, link)

#define insert_volume(head, elem) STAILQ_INSERT_ORDERED(head, elem, link)

#define insert_tail(head, elem) do {			\
	typeof(*elem) *e = (elem); /* evaluate once */	\
	STAILQ_INSERT_TAIL(head, e, link);		\
} while (0)

#define insert_head(head, elem) do {			\
	typeof(*elem) *e = (elem); /* evaluate once */	\
	STAILQ_INSERT_HEAD(head, e, link);		\
} while (0)

#define PARSER_CHECK_PROXY_KEYWORD (1)
#define PARSER_STOP_IF_INVALID (2)

#ifndef STAILQ_CONCAT
/* compat for older libc sys/queue.h */
#define	STAILQ_CONCAT(head1, head2) do {				\
	if (!STAILQ_EMPTY((head2))) {					\
		*(head1)->stqh_last = (head2)->stqh_first;		\
		(head1)->stqh_last = (head2)->stqh_last;		\
		STAILQ_INIT((head2));					\
	}								\
} while (/*CONSTCOND*/0)
#endif

/* Hooks that do something when compiled --with-windrbd, noops for
 * the Linux version.
 */

void maybe_add_bin_dir_to_path(void);
int before_attach(const struct cfg_ctx *ctx);
int after_new_minor(const struct cfg_ctx *ctx);
int after_primary(const struct cfg_ctx *ctx);
int after_secondary(const struct cfg_ctx *ctx);

void parse_device(struct names* on_hosts, struct d_volume *vol);

/* Command helpers common to all platforms. Platform specific code
 * might add utilities to that list (like windrbd for Windows)
 */

extern char *drbdsetup;
extern char *drbdmeta;
extern char *drbdadm_83;
extern char *drbdadm_84;
extern char *drbd_proxy_ctl;

struct cmd_helper {
	char *name;
	char **var;
};

extern struct cmd_helper helpers[];
extern char *khelper_argv[];

void print_platform_specific_versions(void);
void assign_default_device(struct d_volume *vol);

#endif

