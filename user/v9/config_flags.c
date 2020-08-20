#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/genetlink.h>

#include "libgenl.h"
#include "linux/drbd.h"
#include "linux/drbd_config.h"
#include "linux/drbd_genl_api.h"
#include "linux/drbd_limits.h"
#include "drbd_nla.h"
#include "drbdtool_common.h"
#include "linux/genl_magic_func.h"
#include "config_flags.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

#define NLA_POLICY(p)									\
	.nla_policy = p ## _nl_policy,							\
	.nla_policy_size = ARRAY_SIZE(p ## _nl_policy)


struct en_map {
	const char *name;
	int value;
};

/* ============================================================================================== */

static int enum_string_to_int(const char **map, int size, const char *value,
			      int (*strcmp_fn)(const char *, const char *))
{
	int n;

	if (!value)
		return -1;
	for (n = 0; n < size; n++) {
		if (map[n] && !strcmp_fn(value, map[n]))
			return n;
	}
	return -1;
}

static bool enum_is_default(struct field_def *field, const char *value)
{
	int n;

	n = enum_string_to_int(field->u.e.map, field->u.e.size, value, strcmp);
	return n == field->u.e.def;
}

static bool enum_is_equal(struct field_def *field, const char *a, const char *b)
{
	return !strcmp(a, b);
}

static int type_of_field(struct context_def *ctx, struct field_def *field)
{
	return ctx->nla_policy[__nla_type(field->nla_type)].type;
}

static int len_of_field(struct context_def *ctx, struct field_def *field)
{
	return ctx->nla_policy[__nla_type(field->nla_type)].len;
}

static const char *get_enum(struct context_def *ctx, struct field_def *field, struct nlattr *nla)
{
	int i;

	assert(type_of_field(ctx, field) == NLA_U32);
	i = nla_get_u32(nla);
	if (i < 0 || i >= field->u.e.size)
		return NULL;
	return field->u.e.map[i];
}

static bool put_enum(struct context_def *ctx, struct field_def *field,
		     struct msg_buff *msg, const char *value)
{
	int n;

	n = enum_string_to_int(field->u.e.map, field->u.e.size, value, strcmp);
	if (n == -1)
		return false;
	assert(type_of_field(ctx, field) == NLA_U32);
	nla_put_u32(msg, field->nla_type, n);
	return true;
}

static int enum_usage(struct field_def *field, char *str, int size)
{
	const char** map = field->u.e.map;
	char sep = '{';
	int n, len = 0, l;

	l = snprintf(str, size, "[--%s=", field->name);
	len += l; size -= l;
	for (n = 0; n < field->u.e.size; n++) {
		if (!map[n])
			continue;
		l = snprintf(str + len, size, "%c%s", sep, map[n]);
		len += l; size -= l;
		sep = '|';
	}
	assert (sep != '{');
	l = snprintf(str+len, size, "}]");
	len += l;
   /* size -= l; */
	return len;
}

static bool enum_is_default_nocase(struct field_def *field, const char *value)
{
	int n;

	n = enum_string_to_int(field->u.e.map, field->u.e.size, value, strcasecmp);
	return n == field->u.e.def;
}

static bool enum_is_equal_nocase(struct field_def *field, const char *a, const char *b)
{
	return !strcasecmp(a, b);
}

static bool put_enum_nocase(struct context_def *ctx, struct field_def *field,
			    struct msg_buff *msg, const char *value)
{
	int n;

	n = enum_string_to_int(field->u.e.map, field->u.e.size, value, strcasecmp);
	if (n == -1)
		return false;
	assert(type_of_field(ctx, field) == NLA_U32);
	nla_put_u32(msg, field->nla_type, n);
	return true;
}

static void enum_describe_xml(struct field_def *field)
{
	const char **map = field->u.e.map;
	int n;

	printf("\t<option name=\"%s\" type=\"handler\">\n",
	       field->name);
	for (n = 0; n < field->u.e.size; n++) {
		if (!map[n])
			continue;
		printf("\t\t<handler>%s</handler>\n", map[n]);
	}
	printf("\t</option>\n");
}

static enum check_codes enum_check(struct field_def *field, const char *value)
{
	int n = enum_string_to_int(field->u.e.map, field->u.e.size, value, strcmp);
	return n == -1 ? CC_NOT_AN_ENUM : CC_OK;
}

static enum check_codes enum_check_nocase(struct field_def *field, const char *value)
{
	int n = enum_string_to_int(field->u.e.map, field->u.e.size, value, strcasecmp);
	return n == -1 ? CC_NOT_AN_ENUM : CC_OK;
}

struct field_class fc_enum = {
	.is_default = enum_is_default,
	.is_equal = enum_is_equal,
	.get = get_enum,
	.put = put_enum,
	.usage = enum_usage,
	.describe_xml = enum_describe_xml,
	.check = enum_check,
};

struct field_class fc_enum_nocase = {
	.is_default = enum_is_default_nocase,
	.is_equal = enum_is_equal_nocase,
	.get = get_enum,
	.put = put_enum_nocase,
	.usage = enum_usage,
	.describe_xml = enum_describe_xml,
	.check = enum_check_nocase,
};

/* ---------------------------------------------------------------------------------------------- */

static bool numeric_is_default(struct field_def *field, const char *value)
{
	long long l;

	/* FIXME: unsigned long long values are broken. */
	l = m_strtoll(value, field->u.n.scale);
	return l == field->u.n.def;
}

static bool numeric_is_equal(struct field_def *field, const char *a, const char *b)
{
	long long la, lb;

	/* FIXME: unsigned long long values are broken. */
	la = m_strtoll(a, field->u.n.scale);
	lb = m_strtoll(b, field->u.n.scale);
	return la == lb;
}

static const char *get_numeric(struct context_def *ctx, struct field_def *field, struct nlattr *nla)
{
	static char buffer[1 + 20 + 2];
	char scale = field->u.n.scale;
	unsigned long long l;
	int n;

	switch(type_of_field(ctx, field)) {
	case NLA_U8:
		l = nla_get_u8(nla);
		break;
	case NLA_U16:
		l = nla_get_u16(nla);
		break;
	case NLA_U32:
		l = nla_get_u32(nla);
		break;
	case NLA_U64:
		l = nla_get_u64(nla);
		break;
	default:
		return NULL;
	}

	if (field->u.n.is_signed) {
		/* Sign extend.  */
		switch(type_of_field(ctx, field)) {
		case NLA_U8:
			l = (int8_t)l;
			break;
		case NLA_U16:
			l = (int16_t)l;
			break;
		case NLA_U32:
			l = (int32_t)l;
			break;
		case NLA_U64:
			l = (int64_t)l;
			break;
		}
		n = snprintf(buffer, sizeof(buffer), "%lld%c",
			     l, scale == '1' ? 0 : scale);
	} else
		n = snprintf(buffer, sizeof(buffer), "%llu%c",
			     l, scale == '1' ? 0 : scale);

	assert(n < sizeof(buffer));
	return buffer;
}

static bool put_numeric(struct context_def *ctx, struct field_def *field,
			struct msg_buff *msg, const char *value)
{
	long long l;

	/* FIXME: unsigned long long values are broken. */
	l = m_strtoll(value, field->u.n.scale);
	switch(type_of_field(ctx, field)) {
	case NLA_U8:
		nla_put_u8(msg, field->nla_type, l);
		break;
	case NLA_U16:
		nla_put_u16(msg, field->nla_type, l);
		break;
	case NLA_U32:
		nla_put_u32(msg, field->nla_type, l);
		break;
	case NLA_U64:
		nla_put_u64(msg, field->nla_type, l);
		break;
	default:
		return false;
	}
	return true;
}

static int numeric_usage(struct field_def *field, char *str, int size)
{
        return snprintf(str, size,"[--%s=(%lld ... %lld)]",
			field->name,
			field->u.n.min,
			field->u.n.max);
}

static void numeric_describe_xml(struct field_def *field)
{
	printf("\t<option name=\"%s\" type=\"numeric\">\n"
	       "\t\t<min>%lld</min>\n"
	       "\t\t<max>%lld</max>\n"
	       "\t\t<default>%lld</default>\n"
	       "\t\t<unit_prefix>%c</unit_prefix>\n",
	       field->name,
	       field->u.n.min,
	       field->u.n.max,
	       field->u.n.def,
	       field->u.n.scale);
	if(field->unit) {
		printf("\t\t<unit>%s</unit>\n",
		       field->unit);
	}
	printf("\t</option>\n");
}

static enum check_codes numeric_check(struct field_def *field, const char *value)
{
	enum new_strtoll_errs e;
	unsigned long long l;

	e = new_strtoll(value, field->u.n.scale, &l);
	if (e != MSE_OK)
		return CC_NOT_A_NUMBER;
	if (l < field->u.n.min) {
		if (field->implicit_clamp)
			l = field->u.n.min;
		else
			return CC_TOO_SMALL;
	}
	if (l > field->u.n.max) {
		if (field->implicit_clamp)
			l = field->u.n.max;
		else
			return CC_TOO_BIG;
	}
	return CC_OK;
}

struct field_class fc_numeric = {
	.is_default = numeric_is_default,
	.is_equal = numeric_is_equal,
	.get = get_numeric,
	.put = put_numeric,
	.usage = numeric_usage,
	.describe_xml = numeric_describe_xml,
	.check = numeric_check,
};

/* ---------------------------------------------------------------------------------------------- */

static int enum_num_to_int(const struct en_map *map, int map_size, const char *value,
	enum new_strtoll_errs *err_p)
{
	enum new_strtoll_errs e;
	unsigned long long l;
	int i;

	if (!value) {
		if (err_p)
			*err_p = MSE_MISSING_NUMBER;
		return -1;
	}

	for (i = 0; i < map_size; i++) {
		if (!strcmp(value, map[i].name))
			return map[i].value;
	}

	e = new_strtoll(value, 1, &l);
	if (err_p)
		*err_p = e;
	return e == MSE_OK ? l : -1;
}

static bool enum_num_is_default(struct field_def *field, const char *value)
{
	int n;

	n = enum_num_to_int(field->u.en.map, field->u.en.map_size, value, NULL);
	return n == field->u.en.def;
}

static bool enum_num_is_equal(struct field_def *field, const char *a, const char *b)
{
	return !strcmp(a, b);
}

static const char *enum_num_to_string(struct field_def *field, int value)
{
	static char buffer[1 + 10 + 2];
	int i;

	for (i = 0; i < field->u.en.map_size; i++) {
		if (value == field->u.en.map[i].value)
			return field->u.en.map[i].name;
	}

	i = snprintf(buffer, sizeof(buffer), "%d", value);
	assert(i < sizeof(buffer));

	return buffer;
}

static const char *get_enum_num(struct context_def *ctx, struct field_def *field, struct nlattr *nla)
{
	int n;

	assert(type_of_field(ctx, field) == NLA_U32);
	n = nla_get_u32(nla);

	return enum_num_to_string(field, n);
}

static bool put_enum_num(struct context_def *ctx, struct field_def *field,
			struct msg_buff *msg, const char *value)
{
	int n;

	n = enum_num_to_int(field->u.en.map, field->u.en.map_size, value, NULL);
	if (n == -1)
		return false;
	assert(type_of_field(ctx, field) == NLA_U32);
	nla_put_u32(msg, field->nla_type, n);
	return true;
}

static int enum_num_usage(struct field_def *field, char *str, int size)
{
	const struct en_map *map = field->u.en.map;
	char sep = '{';
	int i, len = 0, l;

	l = snprintf(str, size, "[--%s=", field->name);
	len += l; size -= l;
	for (i = 0; i < field->u.en.map_size; i++) {
		l = snprintf(str + len, size, "%c%s", sep, map[i].name);
		len += l; size -= l;
		sep = '|';
	}
	assert (sep != '{');
	l = snprintf(str + len, size, "}|(%d ... %d)]",
		     field->u.en.min, field->u.en.max);
	len += l;
	/* size -= l; */
	return len;
}

static void enum_num_describe_xml(struct field_def *field)
{
	const struct en_map *map = field->u.en.map;
	int i;

	printf("\t<option name=\"%s\" type=\"numeric-or-symbol\">\n"
	       "\t\t<min>%d</min>\n"
	       "\t\t<max>%d</max>\n"
	       "\t\t<default>%s</default>\n",
	       field->name,
	       field->u.en.min,
	       field->u.en.max,
	       enum_num_to_string(field, field->u.en.def));

	for (i = 0; i < field->u.en.map_size; i++)
		printf("\t\t<symbol>%s</symbol>\n", map[i].name);

	printf("\t</option>\n");
}

static enum check_codes enum_num_check(struct field_def *field, const char *value)
{
	enum new_strtoll_errs e = 777;
	int n;

	n = enum_num_to_int(field->u.en.map, field->u.en.map_size, value, &e);

	if (n == -1 && e != MSE_OK)
		return CC_NOT_AN_ENUM_NUM;

	/* n positive, but e not touched -> it was a symbolic value, no range check */
	if (e == 777)
		return CC_OK;

	if (n < field->u.en.min)
		return CC_TOO_SMALL;

	if (n > field->u.en.max)
		return CC_TOO_BIG;

	return CC_OK;
}

struct field_class fc_enum_num = {
	.is_default = enum_num_is_default,
	.is_equal = enum_num_is_equal,
	.get = get_enum_num,
	.put = put_enum_num,
	.usage = enum_num_usage,
	.describe_xml = enum_num_describe_xml,
	.check = enum_num_check,
};

/* ---------------------------------------------------------------------------------------------- */

static int boolean_string_to_int(const char *value)
{
	if (!value || !strcmp(value, "yes"))
		return 1;
	else if (!strcmp(value, "no"))
		return 0;
	else
		return -1;
}

static bool boolean_is_default(struct field_def *field, const char *value)
{
	int yesno;

	yesno = boolean_string_to_int(value);
	return yesno == field->u.b.def;
}

static bool boolean_is_equal(struct field_def *field, const char *a, const char *b)
{
	return boolean_string_to_int(a) == boolean_string_to_int(b);
}

static const char *get_boolean(struct context_def *ctx, struct field_def *field, struct nlattr *nla)
{
	int i;

	assert(type_of_field(ctx, field) == NLA_U8);
	i = nla_get_u8(nla);
	return i ? "yes" : "no";
}

static bool put_boolean(struct context_def *ctx, struct field_def *field,
			struct msg_buff *msg, const char *value)
{
	int yesno;

	yesno = boolean_string_to_int(value);
	if (yesno == -1)
		return false;
	assert(type_of_field(ctx, field) == NLA_U8);
	nla_put_u8(msg, field->nla_type, yesno);
	return true;
}

static bool put_flag(struct context_def *ctx, struct field_def *field,
		     struct msg_buff *msg, const char *value)
{
	int yesno;

	yesno = boolean_string_to_int(value);
	if (yesno == -1)
		return false;
	assert(type_of_field(ctx, field) == NLA_U8);
	if (yesno)
		nla_put_u8(msg, field->nla_type, yesno);
	return true;
}

static int boolean_usage(struct field_def *field, char *str, int size)
{
        return snprintf(str, size,"[--%s={yes|no}]",
			field->name);
}

static void boolean_describe_xml(struct field_def *field)
{
	printf("\t<option name=\"%s\" type=\"boolean\">\n"
	       "\t\t<default>%s</default>\n"
	       "\t</option>\n",
	       field->name,
	       field->u.b.def ? "yes" : "no");
}

static enum check_codes boolean_check(struct field_def *field, const char *value)
{
	int yesno = boolean_string_to_int(value);
	return yesno == -1 ? CC_NOT_A_BOOL : CC_OK;
}

struct field_class fc_boolean = {
	.is_default = boolean_is_default,
	.is_equal = boolean_is_equal,
	.get = get_boolean,
	.put = put_boolean,
	.usage = boolean_usage,
	.describe_xml = boolean_describe_xml,
	.check = boolean_check,
};

struct field_class fc_flag = {
	.is_default = boolean_is_default,
	.is_equal = boolean_is_equal,
	.get = get_boolean,
	.put = put_flag,
	.usage = boolean_usage,
	.describe_xml = boolean_describe_xml,
	.check = boolean_check,
};

/* ---------------------------------------------------------------------------------------------- */

static bool string_is_default(struct field_def *field, const char *value)
{
	return value && !strcmp(value, "");
}

static bool string_is_equal(struct field_def *field, const char *a, const char *b)
{
	return !strcmp(a, b);
}

static const char *get_string(struct context_def *ctx, struct field_def *field, struct nlattr *nla)
{
	char *str;
	int len;

	assert(type_of_field(ctx, field) == NLA_NUL_STRING);
	str = (char *)nla_data(nla);
	len = len_of_field(ctx, field);
	assert(memchr(str, 0, len + 1) != NULL);
	return str;
}

static bool put_string(struct context_def *ctx, struct field_def *field,
		       struct msg_buff *msg, const char *value)
{
	assert(type_of_field(ctx, field) == NLA_NUL_STRING);
	nla_put_string(msg, field->nla_type, value);
	return true;
}

static int string_usage(struct field_def *field, char *str, int size)
{
        return snprintf(str, size,"[--%s=<str>]",
			field->name);
}

static void string_describe_xml(struct field_def *field)
{
	printf("\t<option name=\"%s\" type=\"string\">\n"
	       "\t</option>\n",
	       field->name);
}

static enum check_codes string_check(struct field_def *field, const char *value)
{
	if (field->u.s.max_len) {
		if (strlen(value) >= field->u.s.max_len)
			return CC_STR_TOO_LONG;
	}
	return CC_OK;
}

const char *double_quote_string(const char *str)
{
	static char *buffer;
	const char *s;
	char *b;
	int len = 0;

	for (s = str; *s; s++) {
		if (*s == '\\' || *s == '"')
			len++;
		len++;
	}
	b = realloc(buffer, len + 3);
	if (!b)
		return NULL;
	buffer = b;
	*b++ = '"';
	for (s = str; *s; s++) {
		if (*s == '\\' || *s == '"')
			*b++ = '\\';
		*b++ = *s;
	}
	*b++ = '"';
	*b++ = 0;
	return buffer;
}

struct field_class fc_string = {
	.is_default = string_is_default,
	.is_equal = string_is_equal,
	.get = get_string,
	.put = put_string,
	.usage = string_usage,
	.describe_xml = string_describe_xml,
	.check = string_check,
};

/* ============================================================================================== */

#define ENUM(f, d)									\
	.nla_type = T_ ## f,								\
	.ops = &fc_enum,									\
	.u = { .e = {									\
		.map = f ## _map,							\
		.size = ARRAY_SIZE(f ## _map),						\
		.def = DRBD_ ## d ## _DEF } }

#define ENUM_NOCASE(f, d)								\
	.nla_type = T_ ## f,								\
	.ops = &fc_enum_nocase,								\
	.u = { .e = {									\
		.map = f ## _map,							\
		.size = ARRAY_SIZE(f ## _map),						\
		.def = DRBD_ ## d ## _DEF } }

#define NUMERIC(f, d)									\
	.nla_type = T_ ## f,								\
	.ops = &fc_numeric,								\
	.u = { .n = {									\
		.min = DRBD_ ## d ## _MIN,						\
		.max = DRBD_ ## d ## _MAX,						\
		.def = DRBD_ ## d ## _DEF,						\
		.is_signed = F_ ## f ## _IS_SIGNED,					\
		.scale = DRBD_ ## d ## _SCALE } }

#define BOOLEAN(f, d)									\
	.nla_type = T_ ## f,								\
	.ops = &fc_boolean,								\
	.u = { .b = {									\
		.def = DRBD_ ## d ## _DEF } },						\
	.argument_is_optional = true

#define FLAG(f)										\
	.nla_type = T_ ## f,								\
	.ops = &fc_flag,								\
	.u = { .b = {									\
		.def = false } },							\
	.argument_is_optional = true

#define STRING(f)									\
	.nla_type = T_ ## f,								\
	.ops = &fc_string,								\
	.needs_double_quoting = true

#define STRING_MAX_LEN(f, l)								\
	STRING(f),									\
	.u = { .s = { .max_len = l } } }

#define ENUM_NUM(f, d, num_min, num_max)			\
	.nla_type = T_ ## f,					\
	.ops = &fc_enum_num,					\
	.u = { .en = {						\
		.map = f ## _map,				\
		.map_size = ARRAY_SIZE(f ## _map),		\
		.min = num_min,					\
		.max = num_max,					\
		.def = DRBD_ ## d ## _DEF, } }			\

/* ============================================================================================== */

const char *wire_protocol_map[] = {
	[DRBD_PROT_A] = "A",
	[DRBD_PROT_B] = "B",
	[DRBD_PROT_C] = "C",
};

const char *on_io_error_map[] = {
	[EP_PASS_ON] = "pass_on",
	[EP_CALL_HELPER] = "call-local-io-error",
	[EP_DETACH] = "detach",
};

const char *fencing_policy_map[] = {
	[FP_DONT_CARE] = "dont-care",
	[FP_RESOURCE] = "resource-only",
	[FP_STONITH] = "resource-and-stonith",
};

const char *after_sb_0p_map[] = {
	[ASB_DISCONNECT] = "disconnect",
	[ASB_DISCARD_YOUNGER_PRI] = "discard-younger-primary",
	[ASB_DISCARD_OLDER_PRI] = "discard-older-primary",
	[ASB_DISCARD_ZERO_CHG] = "discard-zero-changes",
	[ASB_DISCARD_LEAST_CHG] = "discard-least-changes",
	[ASB_DISCARD_LOCAL] = "discard-local",
	[ASB_DISCARD_REMOTE] = "discard-remote",
};

const char *after_sb_1p_map[] = {
	[ASB_DISCONNECT] = "disconnect",
	[ASB_CONSENSUS] = "consensus",
	[ASB_VIOLENTLY] = "violently-as0p",
	[ASB_DISCARD_SECONDARY] = "discard-secondary",
	[ASB_CALL_HELPER] = "call-pri-lost-after-sb",
};

const char *after_sb_2p_map[] = {
	[ASB_DISCONNECT] = "disconnect",
	[ASB_VIOLENTLY] = "violently-as0p",
	[ASB_CALL_HELPER] = "call-pri-lost-after-sb",
};

const char *rr_conflict_map[] = {
	[ASB_DISCONNECT] = "disconnect",
	[ASB_VIOLENTLY] = "violently",
	[ASB_CALL_HELPER] = "call-pri-lost",
	[ASB_RETRY_CONNECT] = "retry-connect",
};

const char *on_no_data_map[] = {
	[OND_IO_ERROR] = "io-error",
	[OND_SUSPEND_IO] = "suspend-io",
};

#define on_no_quorum_map on_no_data_map
/* ONQ_XX == OND_XX */

const char *on_congestion_map[] = {
	[OC_BLOCK] = "block",
	[OC_PULL_AHEAD] = "pull-ahead",
	[OC_DISCONNECT] = "disconnect",
};

const char *read_balancing_map[] = {
	[RB_PREFER_LOCAL] = "prefer-local",
	[RB_PREFER_REMOTE] = "prefer-remote",
	[RB_ROUND_ROBIN] = "round-robin",
	[RB_LEAST_PENDING] = "least-pending",
	[RB_CONGESTED_REMOTE] = "when-congested-remote",
	[RB_32K_STRIPING] = "32K-striping",
	[RB_64K_STRIPING] = "64K-striping",
	[RB_128K_STRIPING] = "128K-striping",
	[RB_256K_STRIPING] = "256K-striping",
	[RB_512K_STRIPING] = "512K-striping",
	[RB_1M_STRIPING] = "1M-striping"
};

const struct en_map quorum_map[] = {
	{ "off", QOU_OFF },
	{ "majority", QOU_MAJORITY },
	{ "all", QOU_ALL },
};

#define quorum_min_redundancy_map quorum_map

#define CHANGEABLE_DISK_OPTIONS								\
	{ "on-io-error", ENUM(on_io_error, ON_IO_ERROR) },				\
	/*{ "fencing", ENUM(fencing_policy, FENCING) },*/				\
	{ "disk-barrier", BOOLEAN(disk_barrier, DISK_BARRIER) },			\
	{ "disk-flushes", BOOLEAN(disk_flushes, DISK_FLUSHES) },			\
	{ "disk-drain", BOOLEAN(disk_drain, DISK_DRAIN) },				\
	{ "md-flushes", BOOLEAN(md_flushes, MD_FLUSHES) },				\
	{ "resync-after", NUMERIC(resync_after, MINOR_NUMBER), .checked_in_postparse = true}, \
	{ "al-extents", NUMERIC(al_extents, AL_EXTENTS), .implicit_clamp = true, },	\
	{ "al-updates", BOOLEAN(al_updates, AL_UPDATES) },				\
	{ "discard-zeroes-if-aligned",							\
		BOOLEAN(discard_zeroes_if_aligned, DISCARD_ZEROES_IF_ALIGNED) },	\
	{ "disable-write-same",								\
		BOOLEAN(disable_write_same, DISABLE_WRITE_SAME) },			\
	{ "disk-timeout", NUMERIC(disk_timeout,	DISK_TIMEOUT),				\
	  .unit = "1/10 seconds" },							\
	{ "read-balancing", ENUM(read_balancing, READ_BALANCING) },			\
	{ "rs-discard-granularity",							\
	  NUMERIC(rs_discard_granularity, RS_DISCARD_GRANULARITY),			\
	  .unit = "bytes" }

#define CHANGEABLE_NET_OPTIONS								\
	{ "protocol", ENUM_NOCASE(wire_protocol, PROTOCOL) },				\
	{ "timeout", NUMERIC(timeout, TIMEOUT),						\
          .unit = "1/10 seconds" },							\
	{ "max-epoch-size", NUMERIC(max_epoch_size, MAX_EPOCH_SIZE) },			\
	{ "connect-int", NUMERIC(connect_int, CONNECT_INT),				\
          .unit = "seconds" },								\
	{ "ping-int", NUMERIC(ping_int, PING_INT),					\
          .unit = "seconds" },								\
	{ "sndbuf-size", NUMERIC(sndbuf_size, SNDBUF_SIZE),				\
          .unit = "bytes" },								\
	{ "rcvbuf-size", NUMERIC(rcvbuf_size, RCVBUF_SIZE),				\
          .unit = "bytes" },								\
	{ "ko-count", NUMERIC(ko_count, KO_COUNT) },					\
	{ "allow-two-primaries", BOOLEAN(two_primaries, ALLOW_TWO_PRIMARIES) },		\
	{ "cram-hmac-alg", STRING(cram_hmac_alg) },					\
	{ "shared-secret", STRING_MAX_LEN(shared_secret, SHARED_SECRET_MAX), 		\
	{ "after-sb-0pri", ENUM(after_sb_0p, AFTER_SB_0P) },				\
	{ "after-sb-1pri", ENUM(after_sb_1p, AFTER_SB_1P) },				\
	{ "after-sb-2pri", ENUM(after_sb_2p, AFTER_SB_2P) },				\
	{ "always-asbp", BOOLEAN(always_asbp, ALWAYS_ASBP) },				\
	{ "rr-conflict", ENUM(rr_conflict, RR_CONFLICT) },				\
	{ "ping-timeout", NUMERIC(ping_timeo, PING_TIMEO),				\
          .unit = "1/10 seconds" },							\
	{ "data-integrity-alg", STRING(integrity_alg) },				\
	{ "tcp-cork", BOOLEAN(tcp_cork, TCP_CORK) },					\
	{ "on-congestion", ENUM(on_congestion, ON_CONGESTION) },			\
	{ "congestion-fill", NUMERIC(cong_fill, CONG_FILL),				\
          .unit = "bytes" },								\
	{ "congestion-extents", NUMERIC(cong_extents, CONG_EXTENTS) },			\
	{ "csums-alg", STRING(csums_alg) },						\
	{ "csums-after-crash-only", BOOLEAN(csums_after_crash_only,			\
						CSUMS_AFTER_CRASH_ONLY) },		\
	{ "verify-alg", STRING(verify_alg) },						\
	{ "use-rle", BOOLEAN(use_rle, USE_RLE) },					\
	{ "socket-check-timeout", NUMERIC(sock_check_timeo, SOCKET_CHECK_TIMEO) },	\
	{ "fencing", ENUM(fencing_policy, FENCING) },					\
	{ "max-buffers", NUMERIC(max_buffers, MAX_BUFFERS) },				\
	{ "allow-remote-read", BOOLEAN(allow_remote_read, ALLOW_REMOTE_READ) },					\
	{ "_name", STRING(name) }

struct context_def disk_options_ctx = {
	NLA_POLICY(disk_conf),
	.nla_type = DRBD_NLA_DISK_CONF,
	.fields = {
		CHANGEABLE_DISK_OPTIONS,
		{ } },
};

struct context_def net_options_ctx = {
	NLA_POLICY(net_conf),
	.nla_type = DRBD_NLA_NET_CONF,
	.fields = {
		CHANGEABLE_NET_OPTIONS,
		{ } },
};

struct context_def primary_cmd_ctx = {
	NLA_POLICY(set_role_parms),
	.nla_type = DRBD_NLA_SET_ROLE_PARMS,
	.fields = {
		{ "force", FLAG(assume_uptodate) },
		{ } },
};

struct context_def attach_cmd_ctx = {
	NLA_POLICY(disk_conf),
	.nla_type = DRBD_NLA_DISK_CONF,
	.fields = {
		{ "size", NUMERIC(disk_size, DISK_SIZE),
		  .unit = "bytes" },
		CHANGEABLE_DISK_OPTIONS,
		/* { "*", STRING(backing_dev) }, */
		/* { "*", STRING(meta_dev) }, */
		/* { "*", NUMERIC(meta_dev_idx, MINOR_NUMBER) }, */
		{ } },
};

struct context_def detach_cmd_ctx = {
	NLA_POLICY(detach_parms),
	.nla_type = DRBD_NLA_DETACH_PARMS,
	.fields = {
		{ "force", FLAG(force_detach) },
		{ "diskless", FLAG(intentional_diskless_detach) },
		{ }
	},
};

struct context_def new_peer_cmd_ctx = {
	NLA_POLICY(net_conf),
	.nla_type = DRBD_NLA_NET_CONF,
	.fields = {
		{ "transport", STRING(transport_name) },
		CHANGEABLE_NET_OPTIONS,
		{ } },
};

struct context_def path_cmd_ctx = {
	NLA_POLICY(path_parms),
	.nla_type = DRBD_NLA_PATH_PARMS,
	.fields = { { } },
};

#define CONNECT_CMD_OPTIONS					\
	{ "tentative", FLAG(tentative) },			\
	{ "discard-my-data", FLAG(discard_my_data) }

struct context_def connect_cmd_ctx = {
	NLA_POLICY(connect_parms),
	.nla_type = DRBD_NLA_CONNECT_PARMS,
	.fields = {
		CONNECT_CMD_OPTIONS,
		{ } },
};

struct context_def show_net_options_ctx = {
	NLA_POLICY(net_conf),
	.nla_type = DRBD_NLA_NET_CONF,
	.fields = {
		{ "transport", STRING(transport_name) },
		CHANGEABLE_NET_OPTIONS,
		{ } },
};

struct context_def disconnect_cmd_ctx = {
	NLA_POLICY(disconnect_parms),
	.nla_type = DRBD_NLA_DISCONNECT_PARMS,
	.fields = {
		{ "force", FLAG(force_disconnect) },
		{ } },
};

struct context_def resize_cmd_ctx = {
	NLA_POLICY(resize_parms),
	.nla_type = DRBD_NLA_RESIZE_PARMS,
	.fields = {
		{ "size", NUMERIC(resize_size, DISK_SIZE),
		  .unit = "bytes" },
		{ "assume-peer-has-space", FLAG(resize_force) },
		{ "assume-clean", FLAG(no_resync) },
		{ "al-stripes", NUMERIC(al_stripes, AL_STRIPES) },
		{ "al-stripe-size-kB", NUMERIC(al_stripe_size, AL_STRIPE_SIZE) },
		{ } },
};

struct context_def resource_options_ctx = {
	NLA_POLICY(res_opts),
	.nla_type = DRBD_NLA_RESOURCE_OPTS,
	.fields = {
		{ "cpu-mask", STRING(cpu_mask) },
		{ "on-no-data-accessible", ENUM(on_no_data, ON_NO_DATA) },
		{ "auto-promote", BOOLEAN(auto_promote, AUTO_PROMOTE) },
		{ "peer-ack-window", NUMERIC(peer_ack_window, PEER_ACK_WINDOW), .unit = "bytes" },
		{ "peer-ack-delay", NUMERIC(peer_ack_delay, PEER_ACK_DELAY),
		  .unit = "milliseconds" },
		{ "twopc-timeout", NUMERIC(twopc_timeout, TWOPC_TIMEOUT), .unit = "1/10 seconds" },
		{ "twopc-retry-timeout", NUMERIC(twopc_retry_timeout, TWOPC_RETRY_TIMEOUT),
		  .unit = "1/10 seconds" },
		{ "auto-promote-timeout", NUMERIC(auto_promote_timeout, AUTO_PROMOTE_TIMEOUT),
		  .unit = "1/10 seconds"},
		{ "max-io-depth", NUMERIC(nr_requests, NR_REQUESTS) },
		{ "quorum", ENUM_NUM(quorum, QUORUM, 1, DRBD_PEERS_MAX) },
		{ "on-no-quorum", ENUM(on_no_quorum, ON_NO_QUORUM) },
		{ "quorum-minimum-redundancy", ENUM_NUM(quorum_min_redundancy, QUORUM, 1, DRBD_PEERS_MAX) },
		{ } },
};

struct context_def new_current_uuid_cmd_ctx = {
	NLA_POLICY(new_c_uuid_parms),
	.nla_type = DRBD_NLA_NEW_C_UUID_PARMS,
	.fields = {
		{ "clear-bitmap", FLAG(clear_bm) },
		{ "force-resync", FLAG(force_resync) },
		{ } },
};

struct context_def verify_cmd_ctx = {
	NLA_POLICY(start_ov_parms),
	.nla_type = DRBD_NLA_START_OV_PARMS,
	.fields = {
		{ "start", NUMERIC(ov_start_sector, DISK_SIZE),
		  .unit = "bytes" },
		{ "stop", NUMERIC(ov_stop_sector, DISK_SIZE),
		  .unit = "bytes" },
		{ } },
};

struct context_def device_options_ctx = {
	NLA_POLICY(device_conf),
	.nla_type = DRBD_NLA_DEVICE_CONF,
	.fields = {
		{ "max-bio-size", NUMERIC(max_bio_size, MAX_BIO_SIZE) },
		{ "diskless", FLAG(intentional_diskless) },
		{ } },
};

struct context_def invalidate_ctx = {
	NLA_POLICY(invalidate_parms),
	.nla_type = DRBD_NLA_INVALIDATE_PARMS,
	.fields = {
		{ "sync-from-peer-node-id", NUMERIC(sync_from_peer_node_id, SYNC_FROM_NID) },
		{ } },
};

struct context_def peer_device_options_ctx = {
	NLA_POLICY(peer_device_conf),
	.nla_type = DRBD_NLA_PEER_DEVICE_OPTS,
	.fields = {
		{ "resync-rate", NUMERIC(resync_rate, RESYNC_RATE), .unit = "bytes/second" },
		{ "c-plan-ahead", NUMERIC(c_plan_ahead, C_PLAN_AHEAD), .unit = "1/10 seconds" },
		{ "c-delay-target", NUMERIC(c_delay_target, C_DELAY_TARGET), .unit = "1/10 seconds" },
		{ "c-fill-target", NUMERIC(c_fill_target, C_FILL_TARGET), .unit = "bytes" },
		{ "c-max-rate", NUMERIC(c_max_rate, C_MAX_RATE), .unit = "bytes/second" },
		{ "c-min-rate", NUMERIC(c_min_rate, C_MIN_RATE), .unit = "bytes/second" },
		{ "bitmap", BOOLEAN(bitmap, BITMAP) },
		{ } },
};

// only used in drbdadm:
struct context_def create_md_ctx = {
       .fields = {
		{ .name = "max-peers", .argument_is_optional = false },
		{ .name = "peer-max-bio-size", .argument_is_optional = false },
		{ .name = "al-stripes", .argument_is_optional = false },
		{ .name = "al-stripe-size-kB", .argument_is_optional = false },
		{ .name = "force", .argument_is_optional = true },
		{ } },
};

struct context_def dump_md_ctx = {
       .fields = {
		{ .name = "force", .argument_is_optional = true },
		{ } },
};

struct context_def adjust_ctx = {
	.fields = {
		{ "skip-disk", .argument_is_optional = true },
		{ "skip-net", .argument_is_optional = true },
		CONNECT_CMD_OPTIONS,
		{ } },
};

// only used by drbdadm's config file parser:
struct context_def handlers_ctx = {
	.fields = {
		{ "pri-on-incon-degr", .ops = &fc_string, .needs_double_quoting = true},
		{ "pri-lost-after-sb", .ops = &fc_string, .needs_double_quoting = true},
		{ "pri-lost", .ops = &fc_string, .needs_double_quoting = true},
		{ "initial-split-brain", .ops = &fc_string, .needs_double_quoting = true},
		{ "split-brain", .ops = &fc_string, .needs_double_quoting = true},
		{ "outdate-peer", .ops = &fc_string, .needs_double_quoting = true},
		{ "fence-peer", .ops = &fc_string, .needs_double_quoting = true},
		{ "unfence-peer", .ops = &fc_string, .needs_double_quoting = true},
		{ "local-io-error", .ops = &fc_string, .needs_double_quoting = true},
		{ "before-resync-target", .ops = &fc_string, .needs_double_quoting = true},
		{ "after-resync-target", .ops = &fc_string, .needs_double_quoting = true},
		{ "before-resync-source", .ops = &fc_string, .needs_double_quoting = true},
		{ "out-of-sync", .ops = &fc_string, .needs_double_quoting = true},
		{ "quorum-lost", .ops = &fc_string, .needs_double_quoting = true},
		{ "disconnected", .ops = &fc_string, .needs_double_quoting = true},
		{ } },
};

struct context_def proxy_options_ctx = {
	.fields = {
		{ "memlimit", .ops = &fc_numeric, .u={.n={.min = 0, .max=-1}}},
		{ "read-loops", .ops = &fc_numeric, .u={.n={.min = 0, .max=-1}}},
		{ "compression", .ops = &fc_numeric, .u={.n={.min = 0, .max=-1}}},
		{ "bwlimit", .ops = &fc_numeric, .u={.n={.min = 0, .max=-1}}},
		{ "sndbuf-size", NUMERIC(sndbuf_size, SNDBUF_SIZE), .unit = "bytes" },
		{ "rcvbuf-size", NUMERIC(rcvbuf_size, RCVBUF_SIZE), .unit = "bytes" },
		{ "ping-timeout", NUMERIC(ping_timeo, PING_TIMEO), .unit = "1/10 seconds" },
		{ } },
};

#define ADM_NUMERIC(d)									\
	.ops = &fc_numeric,								\
	.u = { .n = {									\
		.min = DRBD_ ## d ## _MIN,						\
		.max = DRBD_ ## d ## _MAX,						\
		.def = DRBD_ ## d ## _DEF,						\
		.is_signed = false,							\
		.scale = DRBD_ ## d ## _SCALE } }

struct context_def startup_options_ctx = {
	.fields = {
		{ "wfc-timeout", ADM_NUMERIC(WFC_TIMEOUT) },
		{ "degr-wfc-timeout", ADM_NUMERIC(DEGR_WFC_TIMEOUT) },
		{ "outdated-wfc-timeout", ADM_NUMERIC(OUTDATED_WFC_TIMEOUT) },
		{ "wait-after-sb", .ops = &fc_boolean },
		{ } },
};

struct context_def wildcard_ctx = {
};
