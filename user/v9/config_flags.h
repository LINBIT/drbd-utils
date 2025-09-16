#ifndef __DRBD_CONFIG_FLAGS_H
#define __DRBD_CONFIG_FLAGS_H

struct msg_buff;
struct nlattr;

struct context_def;
struct field_def;
struct en_map;

enum check_codes {
	CC_OK,
	CC_NOT_AN_ENUM,
	CC_NOT_A_BOOL,
	CC_NOT_A_NUMBER,
	CC_TOO_SMALL,
	CC_TOO_BIG,
	CC_STR_TOO_LONG,
	CC_NOT_AN_ENUM_NUM,
};

struct field_class {
	bool (*is_default)(const struct field_def *, const char *);
	bool (*is_equal)(const struct field_def *, const char *, const char *);
	const char *(*get)(struct context_def *, const struct field_def *, struct nlattr *);
	bool (*put)(struct context_def *, const struct field_def *, struct msg_buff *, const char *);
	int (*usage)(const struct field_def *, char *, int);
	void (*describe_xml)(const struct field_def *);
	enum check_codes (*check)(const struct field_def *, const char*);
};

struct field_def {
	const char *name;
	unsigned short nla_type;
	const struct field_class *ops;
	union {
		struct {
			const char **map;
			int size;
			int def;
		} e;  /* ENUM, ENUM_NOCASE */
		struct {
			long long min;
			long long max;
			long long def;
			bool is_signed;
			char scale;
		} n;  /* NUMERIC */
		struct {
			bool def;
		} b;  /* BOOLEAN */
		struct {
			unsigned max_len;
		} s; /* string */
		struct {
			const struct en_map *map;
			int map_size;
			int min;
			int max;
			int def;
		} en; /* ENUM_NUM */
		struct {
			const char *type;
		} k;
	} u;
	bool needs_double_quoting;
	bool argument_is_optional;
	bool checked_in_postparse; /* Do not check in drbdadm_parse.c
				      It gets checked and converted later*/
	bool implicit_clamp;
	const char *unit;
};

struct context_def {
	struct nla_policy *nla_policy;
	int nla_policy_size;
	int nla_type;
	struct field_def fields[];
};

extern struct field_class fc_enum;
extern struct field_class fc_enum_nocase;
extern struct field_class fc_numeric;
extern struct field_class fc_boolean;
extern struct field_class fc_flag;
extern struct field_class fc_string;
extern struct field_class fc_enum_num;

extern struct context_def disk_options_ctx;
extern struct context_def net_options_ctx;
extern struct context_def show_net_options_ctx;
extern struct context_def primary_cmd_ctx;
extern struct context_def secondary_cmd_ctx;
extern struct context_def attach_cmd_ctx;
extern struct context_def detach_cmd_ctx;
extern struct context_def connect_cmd_ctx;
extern struct context_def new_peer_cmd_ctx;
extern struct context_def path_cmd_ctx;
extern struct context_def disconnect_cmd_ctx;
extern struct context_def resize_cmd_ctx;
extern struct context_def resource_options_ctx;
extern struct context_def new_current_uuid_cmd_ctx;
extern struct context_def verify_cmd_ctx;
extern struct context_def device_options_ctx;
extern struct context_def invalidate_ctx;
extern struct context_def invalidate_adm_ctx;
extern struct context_def invalidate_peer_ctx;
extern struct context_def create_md_ctx;
extern struct context_def forceable_ctx;
extern struct context_def adjust_ctx;
extern struct context_def status_ctx;
extern struct context_def repair_md_ctx;
extern struct context_def peer_device_options_ctx;
extern struct context_def handlers_ctx;
extern struct context_def proxy_options_ctx;
extern struct context_def startup_options_ctx;
extern struct context_def wildcard_ctx;

#ifdef WITH_84_SUPPORT
extern struct field_def attach_compat_84_fields[];
#endif

extern const char *double_quote_string(const char *str);

#endif  /* __DRBD_CONFIG_FLAGS_H */
