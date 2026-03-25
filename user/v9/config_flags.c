#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/genetlink.h>

#ifdef KEYUTILS
#include <keyutils.h>
#endif

#include "libgenl.h"
#include "linux/drbd.h"
#include "linux/drbd_config.h"
#include "linux/drbd_genl_userspace.h"
#include "linux/drbd_limits.h"
#include "drbdtool_common.h"
#include "config_flags.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

#define NLA_POLICY(p)									\
	.nla_policy = drbd_ ## p ## _nl_policy,							\
	.nla_policy_size = ARRAY_SIZE(drbd_ ## p ## _nl_policy)


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

static bool enum_is_default(const struct field_def *field, const char *value)
{
	int n;

	n = enum_string_to_int(field->u.e.map, field->u.e.size, value, strcmp);
	return n == field->u.e.def;
}

static bool enum_is_equal(const struct field_def *field, const char *a, const char *b)
{
	return !strcmp(a, b);
}

static int type_of_field(struct context_def *ctx, const struct field_def *field)
{
	return ctx->nla_policy[field->nla_type].type;
}

/* NLA_U32 and NLA_S32 share the same wire format. */
static bool is_32bit_field(struct context_def *ctx, const struct field_def *field)
{
	int type = type_of_field(ctx, field);

	return type == NLA_U32 || type == NLA_S32;
}

static int len_of_field(struct context_def *ctx, const struct field_def *field)
{
	return ctx->nla_policy[field->nla_type].len;
}

static const char *get_enum(struct context_def *ctx, const struct field_def *field, struct nlattr *nla)
{
	int i;

	assert(type_of_field(ctx, field) == NLA_U32);
	i = nla_get_u32(nla);
	if (i < 0 || i >= field->u.e.size)
		return NULL;
	return field->u.e.map[i];
}

static bool put_enum(struct context_def *ctx, const struct field_def *field,
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

static int enum_usage(const struct field_def *field, char *str, int size)
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

static bool enum_is_default_nocase(const struct field_def *field, const char *value)
{
	int n;

	n = enum_string_to_int(field->u.e.map, field->u.e.size, value, strcasecmp);
	return n == field->u.e.def;
}

static bool enum_is_equal_nocase(const struct field_def *field, const char *a, const char *b)
{
	return !strcasecmp(a, b);
}

static bool put_enum_nocase(struct context_def *ctx, const struct field_def *field,
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

static void enum_describe_xml(const struct field_def *field)
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

static enum check_codes enum_check(const struct field_def *field, const char *value)
{
	int n = enum_string_to_int(field->u.e.map, field->u.e.size, value, strcmp);
	return n == -1 ? CC_NOT_AN_ENUM : CC_OK;
}

static enum check_codes enum_check_nocase(const struct field_def *field, const char *value)
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

static bool numeric_is_default(const struct field_def *field, const char *value)
{
	long long l;

	/* FIXME: unsigned long long values are broken. */
	l = m_strtoll(value, field->u.n.scale);
	return l == field->u.n.def;
}

static bool numeric_is_equal(const struct field_def *field, const char *a, const char *b)
{
	long long la, lb;

	/* FIXME: unsigned long long values are broken. */
	la = m_strtoll(a, field->u.n.scale);
	lb = m_strtoll(b, field->u.n.scale);
	return la == lb;
}

static const char *get_numeric(struct context_def *ctx, const struct field_def *field, struct nlattr *nla)
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
	case NLA_S32:
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
		case NLA_S32:
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

static bool put_numeric(struct context_def *ctx, const struct field_def *field,
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
	case NLA_S32:
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

static int numeric_usage(const struct field_def *field, char *str, int size)
{
        return snprintf(str, size,"[--%s=(%lld ... %lld)]",
			field->name,
			field->u.n.min,
			field->u.n.max);
}

static void numeric_describe_xml(const struct field_def *field)
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

static enum check_codes numeric_check(const struct field_def *field, const char *value)
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

static bool enum_num_is_default(const struct field_def *field, const char *value)
{
	int n;

	n = enum_num_to_int(field->u.en.map, field->u.en.map_size, value, NULL);
	return n == field->u.en.def;
}

static bool enum_num_is_equal(const struct field_def *field, const char *a, const char *b)
{
	return !strcmp(a, b);
}

static const char *enum_num_to_string(const struct field_def *field, int value)
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

static const char *get_enum_num(struct context_def *ctx, const struct field_def *field, struct nlattr *nla)
{
	int n;

	assert(is_32bit_field(ctx, field));
	n = nla_get_u32(nla);

	return enum_num_to_string(field, n);
}

static bool put_enum_num(struct context_def *ctx, const struct field_def *field,
			struct msg_buff *msg, const char *value)
{
	int n;

	n = enum_num_to_int(field->u.en.map, field->u.en.map_size, value, NULL);
	if (n == -1)
		return false;
	assert(is_32bit_field(ctx, field));
	nla_put_u32(msg, field->nla_type, n);
	return true;
}

static int enum_num_usage(const struct field_def *field, char *str, int size)
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

static void enum_num_describe_xml(const struct field_def *field)
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

static enum check_codes enum_num_check(const struct field_def *field, const char *value)
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

static bool boolean_is_default(const struct field_def *field, const char *value)
{
	int yesno;

	yesno = boolean_string_to_int(value);
	return yesno == field->u.b.def;
}

static bool boolean_is_equal(const struct field_def *field, const char *a, const char *b)
{
	return boolean_string_to_int(a) == boolean_string_to_int(b);
}

static const char *get_boolean(struct context_def *ctx, const struct field_def *field, struct nlattr *nla)
{
	int i;

	assert(type_of_field(ctx, field) == NLA_U8);
	i = nla_get_u8(nla);
	return i ? "yes" : "no";
}

static bool put_boolean(struct context_def *ctx, const struct field_def *field,
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

static bool put_flag(struct context_def *ctx, const struct field_def *field,
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

static int boolean_usage(const struct field_def *field, char *str, int size)
{
        return snprintf(str, size,"[--%s={yes|no}]",
			field->name);
}

static void boolean_describe_xml(const struct field_def *field)
{
	printf("\t<option name=\"%s\" type=\"boolean\">\n"
	       "\t\t<default>%s</default>\n"
	       "\t</option>\n",
	       field->name,
	       field->u.b.def ? "yes" : "no");
}

static enum check_codes boolean_check(const struct field_def *field, const char *value)
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

static bool string_is_default(const struct field_def *field, const char *value)
{
	return value && !strcmp(value, "");
}

static bool string_is_equal(const struct field_def *field, const char *a, const char *b)
{
	return !strcmp(a, b);
}

static const char *get_string(struct context_def *ctx, const struct field_def *field, struct nlattr *nla)
{
	char *str;
	int len;

	assert(type_of_field(ctx, field) == NLA_NUL_STRING);
	str = (char *)nla_data(nla);
	len = len_of_field(ctx, field);
	assert(memchr(str, 0, len + 1) != NULL);
	return str;
}

static bool put_string(struct context_def *ctx, const struct field_def *field,
		       struct msg_buff *msg, const char *value)
{
	assert(type_of_field(ctx, field) == NLA_NUL_STRING);
	nla_put_string(msg, field->nla_type, value);
	return true;
}

static int string_usage(const struct field_def *field, char *str, int size)
{
        return snprintf(str, size,"[--%s=<str>]",
			field->name);
}

static void string_describe_xml(const struct field_def *field)
{
	printf("\t<option name=\"%s\" type=\"string\">\n"
	       "\t</option>\n",
	       field->name);
}

static enum check_codes string_check(const struct field_def *field, const char *value)
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

	if (!str)
		return "\"\"";

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

/* ---------------------------------------------------------------------------------------------- */

#ifdef KEYUTILS
static key_serial_t string_to_key_serial(const char *value, const char *key_type)
{
	if (!value)
		return 0;

	return request_key(key_type, value, NULL, 0);
}

static bool key_serial_is_default(const struct field_def *field, const char *value)
{
	return string_to_key_serial(value, field->u.k.type) == 0;
}

static bool key_serial_is_equal(const struct field_def *field, const char *a, const char *b)
{
	return string_to_key_serial(a, field->u.k.type) == string_to_key_serial(b, field->u.k.type);
}

static const char *get_key_serial(struct context_def *ctx, const struct field_def *field, struct nlattr *nla)
{
	char *buf;
	static char description[256];
	int ret;
	key_serial_t serial;

	assert(is_32bit_field(ctx, field));
	serial = nla_get_u32(nla);
	if (serial <= 0)
		return NULL;

	ret = keyctl_describe_alloc(serial, &buf);
	if (ret == -1)
		return NULL;

	ret = sscanf(buf, "%*[^;];%*d;%*d;%*08x;%255s", description);
	free(buf);

	if (ret != 1) {
		return NULL;
	}

	return description;
}

static bool put_key_serial(struct context_def *ctx, const struct field_def *field,
                       struct msg_buff *msg, const char *value)
{
	key_serial_t key = request_key(field->u.k.type, value, "drbd", KEY_SPEC_THREAD_KEYRING);
	if (key == -1)
		return false;

	nla_put_u32(msg, field->nla_type, key);
	return true;
}

static int key_serial_usage(const struct field_def *field, char *str, int size)
{
	return snprintf(str, size,"[--%s=<key-description>]",
	                field->name);
}

static void key_serial_describe_xml(const struct field_def *field)
{
	printf("\t<option name=\"%s\" type=\"string\">\n"
	       "\t</option>\n",
	       field->name);
}

static enum check_codes key_serial_check(const struct field_def *field, const char *value)
{
	return CC_OK;
}

#else

static bool key_serial_is_default(const struct field_def *field, const char *value)
{
	return true;
}

static bool key_serial_is_equal(const struct field_def *field, const char *a, const char *b)
{
	return true;
}

static const char *get_key_serial(struct context_def *ctx, const struct field_def *field, struct nlattr *nla)
{
	static char description[] = "";

	return description;
}

static bool put_key_serial(struct context_def *ctx, const struct field_def *field,
                       struct msg_buff *msg, const char *value)
{
	nla_put_u32(msg, field->nla_type, 0);
	return true;
}

static int key_serial_usage(const struct field_def *field, char *str, int size)
{
	return snprintf(str, size,"[--%s=<n/a>]",
	                field->name);
}

static void key_serial_describe_xml(const struct field_def *field)
{
	printf("\t<option name=\"%s\" type=\"string\">\n"
	       "\t</option>\n",
	       field->name);
}

static enum check_codes key_serial_check(const struct field_def *field, const char *value)
{
	return CC_OK;
}

#endif

struct field_class fc_key_serial = {
	.is_default = key_serial_is_default,
	.is_equal = key_serial_is_equal,
	.get = get_key_serial,
	.put = put_key_serial,
	.usage = key_serial_usage,
	.describe_xml = key_serial_describe_xml,
	.check = key_serial_check,
};


/* ============================================================================================== */

#define ENUM(a, f, d)									\
	.nla_type = a,									\
	.ops = &fc_enum,								\
	.u = { .e = {									\
		.map = f ## _map,							\
		.size = ARRAY_SIZE(f ## _map),						\
		.def = DRBD_ ## d ## _DEF } }

#define ENUM_NOCASE(a, f, d)								\
	.nla_type = a,									\
	.ops = &fc_enum_nocase,								\
	.u = { .e = {									\
		.map = f ## _map,							\
		.size = ARRAY_SIZE(f ## _map),						\
		.def = DRBD_ ## d ## _DEF } }

#define NUMERIC(a, f, d)								\
	.nla_type = a,									\
	.ops = &fc_numeric,								\
	.u = { .n = {									\
		.min = DRBD_ ## d ## _MIN,						\
		.max = DRBD_ ## d ## _MAX,						\
		.def = DRBD_ ## d ## _DEF,						\
		.is_signed = F_ ## f ## _IS_SIGNED,					\
		.scale = DRBD_ ## d ## _SCALE } }

#define BOOLEAN(a, d)									\
	.nla_type = a,									\
	.ops = &fc_boolean,								\
	.u = { .b = {									\
		.def = DRBD_ ## d ## _DEF } },						\
	.argument_is_optional = true

#define FLAG(a)										\
	.nla_type = a,									\
	.ops = &fc_flag,								\
	.u = { .b = {									\
		.def = false } },							\
	.argument_is_optional = true

#define STRING(a)									\
	.nla_type = a,									\
	.ops = &fc_string,								\
	.needs_double_quoting = true

#define STRING_MAX_LEN(a, l)								\
	STRING(a),									\
	.u = { .s = { .max_len = l } }

#define ENUM_NUM(a, f, d, num_min, num_max)			\
	.nla_type = a,						\
	.ops = &fc_enum_num,					\
	.u = { .en = {						\
		.map = f ## _map,				\
		.map_size = ARRAY_SIZE(f ## _map),		\
		.min = num_min,					\
		.max = num_max,					\
		.def = DRBD_ ## d ## _DEF, } }			\

#define KEY_SERIAL(a, key_type)					\
	.nla_type = a,						\
	.ops = &fc_key_serial,					\
	.needs_double_quoting = true,				\
	.u = { .k = {						\
		.type = key_type, } }				\

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
	[ASB_AUTO_DISCARD] = "auto-discard",
};

const char *on_no_data_map[] = {
	[OND_IO_ERROR] = "io-error",
	[OND_SUSPEND_IO] = "suspend-io",
};

#define on_no_quorum_map on_no_data_map
/* ONQ_XX == OND_XX */

const char *on_susp_primary_outdated_map[] = {
	[SPO_DISCONNECT] = "disconnect",
	[SPO_FORCE_SECONDARY] = "force-secondary",
};

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
	{ "on-io-error", ENUM(DRBD_A_DISK_CONF_ON_IO_ERROR, on_io_error, ON_IO_ERROR) },				\
	/*{ "fencing", ENUM(DRBD_A_NET_CONF_FENCING_POLICY, fencing_policy, FENCING) },*/				\
	{ "disk-barrier", BOOLEAN(DRBD_A_DISK_CONF_DISK_BARRIER, DISK_BARRIER) },			\
	{ "disk-flushes", BOOLEAN(DRBD_A_DISK_CONF_DISK_FLUSHES, DISK_FLUSHES) },			\
	{ "disk-drain", BOOLEAN(DRBD_A_DISK_CONF_DISK_DRAIN, DISK_DRAIN) },				\
	{ "md-flushes", BOOLEAN(DRBD_A_DISK_CONF_MD_FLUSHES, MD_FLUSHES) },				\
	{ "resync-after", NUMERIC(DRBD_A_DISK_CONF_RESYNC_AFTER, resync_after, MINOR_NUMBER), .checked_in_postparse = true}, \
	{ "al-extents", NUMERIC(DRBD_A_DISK_CONF_AL_EXTENTS, al_extents, AL_EXTENTS), .implicit_clamp = true, },	\
	{ "al-updates", BOOLEAN(DRBD_A_DISK_CONF_AL_UPDATES, AL_UPDATES) },				\
	{ "discard-zeroes-if-aligned",							\
		BOOLEAN(DRBD_A_DISK_CONF_DISCARD_ZEROES_IF_ALIGNED, DISCARD_ZEROES_IF_ALIGNED) },	\
	{ "disable-write-same",								\
		BOOLEAN(DRBD_A_DISK_CONF_DISABLE_WRITE_SAME, DISABLE_WRITE_SAME) },			\
	{ "disk-timeout", NUMERIC(DRBD_A_DISK_CONF_DISK_TIMEOUT, disk_timeout, DISK_TIMEOUT),				\
	  .unit = "1/10 seconds" },							\
	{ "read-balancing", ENUM(DRBD_A_DISK_CONF_READ_BALANCING, read_balancing, READ_BALANCING) },			\
	{ "rs-discard-granularity",							\
	  NUMERIC(DRBD_A_DISK_CONF_RS_DISCARD_GRANULARITY, rs_discard_granularity, RS_DISCARD_GRANULARITY),			\
	  .unit = "bytes" },								\
	{ "bitmap", BOOLEAN(DRBD_A_DISK_CONF_D_BITMAP, BITMAP) }

#define CHANGEABLE_NET_OPTIONS								\
	{ "protocol", ENUM_NOCASE(DRBD_A_NET_CONF_WIRE_PROTOCOL, wire_protocol, PROTOCOL) },				\
	{ "timeout", NUMERIC(DRBD_A_NET_CONF_TIMEOUT, timeout, TIMEOUT),						\
          .unit = "1/10 seconds" },							\
	{ "max-epoch-size", NUMERIC(DRBD_A_NET_CONF_MAX_EPOCH_SIZE, max_epoch_size, MAX_EPOCH_SIZE) },			\
	{ "connect-int", NUMERIC(DRBD_A_NET_CONF_CONNECT_INT, connect_int, CONNECT_INT),				\
          .unit = "seconds" },								\
	{ "ping-int", NUMERIC(DRBD_A_NET_CONF_PING_INT, ping_int, PING_INT),					\
          .unit = "seconds" },								\
	{ "sndbuf-size", NUMERIC(DRBD_A_NET_CONF_SNDBUF_SIZE, sndbuf_size, SNDBUF_SIZE),				\
          .unit = "bytes" },								\
	{ "rcvbuf-size", NUMERIC(DRBD_A_NET_CONF_RCVBUF_SIZE, rcvbuf_size, RCVBUF_SIZE),				\
          .unit = "bytes" },								\
	{ "ko-count", NUMERIC(DRBD_A_NET_CONF_KO_COUNT, ko_count, KO_COUNT) },					\
	{ "allow-two-primaries", BOOLEAN(DRBD_A_NET_CONF_TWO_PRIMARIES, ALLOW_TWO_PRIMARIES) },		\
	{ "cram-hmac-alg", STRING_MAX_LEN(DRBD_A_NET_CONF_CRAM_HMAC_ALG, SHARED_SECRET_MAX) },		\
	{ "shared-secret", STRING_MAX_LEN(DRBD_A_NET_CONF_SHARED_SECRET, SHARED_SECRET_MAX) },		\
	{ "after-sb-0pri", ENUM(DRBD_A_NET_CONF_AFTER_SB_0P, after_sb_0p, AFTER_SB_0P) },				\
	{ "after-sb-1pri", ENUM(DRBD_A_NET_CONF_AFTER_SB_1P, after_sb_1p, AFTER_SB_1P) },				\
	{ "after-sb-2pri", ENUM(DRBD_A_NET_CONF_AFTER_SB_2P, after_sb_2p, AFTER_SB_2P) },				\
	{ "always-asbp", BOOLEAN(DRBD_A_NET_CONF_ALWAYS_ASBP, ALWAYS_ASBP) },				\
	{ "rr-conflict", ENUM(DRBD_A_NET_CONF_RR_CONFLICT, rr_conflict, RR_CONFLICT) },				\
	{ "ping-timeout", NUMERIC(DRBD_A_NET_CONF_PING_TIMEO, ping_timeo, PING_TIMEO),				\
          .unit = "1/10 seconds" },							\
	{ "data-integrity-alg", STRING_MAX_LEN(DRBD_A_NET_CONF_INTEGRITY_ALG, SHARED_SECRET_MAX) },	\
	{ "tcp-cork", BOOLEAN(DRBD_A_NET_CONF_TCP_CORK, TCP_CORK) },					\
	{ "on-congestion", ENUM(DRBD_A_NET_CONF_ON_CONGESTION, on_congestion, ON_CONGESTION) },			\
	{ "congestion-fill", NUMERIC(DRBD_A_NET_CONF_CONG_FILL, cong_fill, CONG_FILL),				\
          .unit = "bytes" },								\
	{ "congestion-extents", NUMERIC(DRBD_A_NET_CONF_CONG_EXTENTS, cong_extents, CONG_EXTENTS) },			\
	{ "csums-alg", STRING_MAX_LEN(DRBD_A_NET_CONF_CSUMS_ALG, SHARED_SECRET_MAX) },			\
	{ "csums-after-crash-only", BOOLEAN(DRBD_A_NET_CONF_CSUMS_AFTER_CRASH_ONLY, \
						CSUMS_AFTER_CRASH_ONLY) },		\
	{ "verify-alg", STRING_MAX_LEN(DRBD_A_NET_CONF_VERIFY_ALG, SHARED_SECRET_MAX) },		\
	{ "use-rle", BOOLEAN(DRBD_A_NET_CONF_USE_RLE, USE_RLE) },					\
	{ "socket-check-timeout", NUMERIC(DRBD_A_NET_CONF_SOCK_CHECK_TIMEO, sock_check_timeo, SOCKET_CHECK_TIMEO) },	\
	{ "fencing", ENUM(DRBD_A_NET_CONF_FENCING_POLICY, fencing_policy, FENCING) },					\
	{ "max-buffers", NUMERIC(DRBD_A_NET_CONF_MAX_BUFFERS, max_buffers, MAX_BUFFERS) },				\
	{ "allow-remote-read", BOOLEAN(DRBD_A_NET_CONF_ALLOW_REMOTE_READ, ALLOW_REMOTE_READ) },		\
	{ "tls", BOOLEAN(DRBD_A_NET_CONF_TLS, TLS) },							\
	{ "tls-keyring", KEY_SERIAL(DRBD_A_NET_CONF_TLS_KEYRING, "keyring") },				\
	{ "tls-privkey", KEY_SERIAL(DRBD_A_NET_CONF_TLS_PRIVKEY, "user") },				\
	{ "tls-certificate", KEY_SERIAL(DRBD_A_NET_CONF_TLS_CERTIFICATE, "user") },			\
	{ "rdma-ctrl-rcvbuf-size", NUMERIC(DRBD_A_NET_CONF_RDMA_CTRL_RCVBUF_SIZE, rdma_ctrl_rcvbuf_size, RDMA_CTRL_RCVBUF_SIZE) }, \
	{ "rdma-ctrl-sndbuf-size", NUMERIC(DRBD_A_NET_CONF_RDMA_CTRL_SNDBUF_SIZE, rdma_ctrl_sndbuf_size, RDMA_CTRL_SNDBUF_SIZE) }, \
	{ "_name", STRING_MAX_LEN(DRBD_A_NET_CONF_NAME, SHARED_SECRET_MAX) }

#define IMMUTABLE_NET_OPTIONS								\
	{ "transport", STRING_MAX_LEN(DRBD_A_NET_CONF_TRANSPORT_NAME, SHARED_SECRET_MAX) },		\
	{ "load-balance-paths", BOOLEAN(DRBD_A_NET_CONF_LOAD_BALANCE_PATHS, LOAD_BALANCE_PATHS) }


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
		{ "force", FLAG(DRBD_A_SET_ROLE_PARMS_FORCE) },
		{ } },
};

struct context_def secondary_cmd_ctx = {
	NLA_POLICY(set_role_parms),
	.nla_type = DRBD_NLA_SET_ROLE_PARMS,
	.fields = {
		{ "force", FLAG(DRBD_A_SET_ROLE_PARMS_FORCE) },
		{ } },
};

struct context_def attach_cmd_ctx = {
	NLA_POLICY(disk_conf),
	.nla_type = DRBD_NLA_DISK_CONF,
	.fields = {
		{ "size", NUMERIC(DRBD_A_DISK_CONF_DISK_SIZE, disk_size, DISK_SIZE),
		  .unit = "bytes" },
		CHANGEABLE_DISK_OPTIONS,
		/* { "*", STRING(DRBD_A_DISK_CONF_BACKING_DEV) }, */
		/* { "*", STRING(DRBD_A_DISK_CONF_META_DEV) }, */
		/* { "*", NUMERIC(DRBD_A_DISK_CONF_META_DEV_IDX, meta_dev_idx, MINOR_NUMBER) }, */
		{ } },
};

struct context_def detach_cmd_ctx = {
	NLA_POLICY(detach_parms),
	.nla_type = DRBD_NLA_DETACH_PARMS,
	.fields = {
		{ "force", FLAG(DRBD_A_DETACH_PARMS_FORCE_DETACH) },
		{ "diskless", FLAG(DRBD_A_DETACH_PARMS_INTENTIONAL_DISKLESS_DETACH) },
		{ }
	},
};

struct context_def new_peer_cmd_ctx = {
	NLA_POLICY(net_conf),
	.nla_type = DRBD_NLA_NET_CONF,
	.fields = {
		IMMUTABLE_NET_OPTIONS,
		CHANGEABLE_NET_OPTIONS,
		{ } },
};

struct context_def path_cmd_ctx = {
	NLA_POLICY(path_parms),
	.nla_type = DRBD_NLA_PATH_PARMS,
	.fields = { { } },
};

#define CONNECT_CMD_OPTIONS					\
	{ "tentative", FLAG(DRBD_A_CONNECT_PARMS_TENTATIVE) },			\
	{ "discard-my-data", FLAG(DRBD_A_CONNECT_PARMS_DISCARD_MY_DATA) }

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
		IMMUTABLE_NET_OPTIONS,
		CHANGEABLE_NET_OPTIONS,
		{ } },
};

struct context_def disconnect_cmd_ctx = {
	NLA_POLICY(disconnect_parms),
	.nla_type = DRBD_NLA_DISCONNECT_PARMS,
	.fields = {
		{ "force", FLAG(DRBD_A_DISCONNECT_PARMS_FORCE_DISCONNECT) },
		{ } },
};

struct context_def resize_cmd_ctx = {
	NLA_POLICY(resize_parms),
	.nla_type = DRBD_NLA_RESIZE_PARMS,
	.fields = {
		{ "size", NUMERIC(DRBD_A_RESIZE_PARMS_RESIZE_SIZE, resize_size, DISK_SIZE),
		  .unit = "bytes" },
		{ "assume-peer-has-space", FLAG(DRBD_A_RESIZE_PARMS_RESIZE_FORCE) },
		{ "assume-clean", FLAG(DRBD_A_RESIZE_PARMS_NO_RESYNC) },
		{ "al-stripes", NUMERIC(DRBD_A_RESIZE_PARMS_AL_STRIPES, al_stripes, AL_STRIPES) },
		{ "al-stripe-size-kB", NUMERIC(DRBD_A_RESIZE_PARMS_AL_STRIPE_SIZE, al_stripe_size, AL_STRIPE_SIZE) },
		{ } },
};

struct context_def resource_options_ctx = {
	NLA_POLICY(res_opts),
	.nla_type = DRBD_NLA_RESOURCE_OPTS,
	.fields = {
		{ "cpu-mask", STRING_MAX_LEN(DRBD_A_RES_OPTS_CPU_MASK, DRBD_CPU_MASK_SIZE) },
		{ "on-no-data-accessible", ENUM(DRBD_A_RES_OPTS_ON_NO_DATA, on_no_data, ON_NO_DATA) },
		{ "auto-promote", BOOLEAN(DRBD_A_RES_OPTS_AUTO_PROMOTE, AUTO_PROMOTE) },
		{ "peer-ack-window", NUMERIC(DRBD_A_RES_OPTS_PEER_ACK_WINDOW, peer_ack_window, PEER_ACK_WINDOW), .unit = "bytes" },
		{ "peer-ack-delay", NUMERIC(DRBD_A_RES_OPTS_PEER_ACK_DELAY, peer_ack_delay, PEER_ACK_DELAY),
		  .unit = "milliseconds" },
		{ "twopc-timeout", NUMERIC(DRBD_A_RES_OPTS_TWOPC_TIMEOUT, twopc_timeout, TWOPC_TIMEOUT), .unit = "1/10 seconds" },
		{ "twopc-retry-timeout", NUMERIC(DRBD_A_RES_OPTS_TWOPC_RETRY_TIMEOUT, twopc_retry_timeout, TWOPC_RETRY_TIMEOUT),
		  .unit = "1/10 seconds" },
		{ "auto-promote-timeout", NUMERIC(DRBD_A_RES_OPTS_AUTO_PROMOTE_TIMEOUT, auto_promote_timeout, AUTO_PROMOTE_TIMEOUT),
		  .unit = "1/10 seconds"},
		{ "max-io-depth", NUMERIC(DRBD_A_RES_OPTS_NR_REQUESTS, nr_requests, NR_REQUESTS) },
		{ "quorum", ENUM_NUM(DRBD_A_RES_OPTS_QUORUM, quorum, QUORUM, 1, DRBD_PEERS_MAX) },
		{ "on-no-quorum", ENUM(DRBD_A_RES_OPTS_ON_NO_QUORUM, on_no_quorum, ON_NO_QUORUM) },
		{ "quorum-minimum-redundancy", ENUM_NUM(DRBD_A_RES_OPTS_QUORUM_MIN_REDUNDANCY, quorum_min_redundancy, QUORUM, 1, DRBD_PEERS_MAX) },
		{ "on-suspended-primary-outdated", ENUM(DRBD_A_RES_OPTS_ON_SUSP_PRIMARY_OUTDATED, on_susp_primary_outdated, ON_SUSP_PRI_OUTD) },
		{ "drbd8-api-compatibility", BOOLEAN(DRBD_A_RES_OPTS_EXPLICIT_DRBD8_COMPAT, DRBD8_COMPAT_MODE) },
		{ } },
};

struct context_def new_current_uuid_cmd_ctx = {
	NLA_POLICY(new_c_uuid_parms),
	.nla_type = DRBD_NLA_NEW_C_UUID_PARMS,
	.fields = {
		{ "clear-bitmap", FLAG(DRBD_A_NEW_C_UUID_PARMS_CLEAR_BM) },
		{ "force-resync", FLAG(DRBD_A_NEW_C_UUID_PARMS_FORCE_RESYNC) },
		{ } },
};

struct context_def verify_cmd_ctx = {
	NLA_POLICY(start_ov_parms),
	.nla_type = DRBD_NLA_START_OV_PARMS,
	.fields = {
		{ "start", NUMERIC(DRBD_A_START_OV_PARMS_OV_START_SECTOR, ov_start_sector, DISK_SIZE),
		  .unit = "bytes" },
		{ "stop", NUMERIC(DRBD_A_START_OV_PARMS_OV_STOP_SECTOR, ov_stop_sector, DISK_SIZE),
		  .unit = "bytes" },
		{ } },
};

struct context_def device_options_ctx = {
	NLA_POLICY(device_conf),
	.nla_type = DRBD_NLA_DEVICE_CONF,
	.fields = {
		{ "max-bio-size", NUMERIC(DRBD_A_DEVICE_CONF_MAX_BIO_SIZE, max_bio_size, MAX_BIO_SIZE) },
		{ "diskless", FLAG(DRBD_A_DEVICE_CONF_INTENTIONAL_DISKLESS) },
		{ "block-size", NUMERIC(DRBD_A_DEVICE_CONF_BLOCK_SIZE, block_size, BLOCK_SIZE) },
		{ "discard-granularity", NUMERIC(DRBD_A_DEVICE_CONF_DISCARD_GRANULARITY, discard_granularity, DISCARD_GRANULARITY) },
		{ } },
};

#define INVALIDATE_OPTIONS									\
		{ "sync-from-peer-node-id", NUMERIC(DRBD_A_INVALIDATE_PARMS_SYNC_FROM_PEER_NODE_ID, sync_from_peer_node_id, SYNC_FROM_NID) },	\
		{ "reset-bitmap", BOOLEAN(DRBD_A_INVALIDATE_PARMS_RESET_BITMAP, INVALIDATE_RESET_BITMAP) },

struct context_def invalidate_ctx = {
	NLA_POLICY(invalidate_parms),
	.nla_type = DRBD_NLA_INVALIDATE_PARMS,
	.fields = {
		INVALIDATE_OPTIONS
		{ } },
};

struct context_def invalidate_adm_ctx = {
	NLA_POLICY(invalidate_parms),
	.nla_type = DRBD_NLA_INVALIDATE_PARMS,
	.fields = {
		{ "force", .argument_is_optional = true },
		INVALIDATE_OPTIONS
		{ } },
};

struct context_def invalidate_peer_ctx = {
	NLA_POLICY(invalidate_peer_parms),
	.nla_type = DRBD_NLA_INVAL_PEER_PARAMS,
	.fields = {
		{ "reset-bitmap", BOOLEAN(DRBD_A_INVALIDATE_PEER_PARMS_P_RESET_BITMAP, INVALIDATE_RESET_BITMAP) },
		{ } },
};

struct context_def suspend_io_ctx = {
	NLA_POLICY(suspend_io_parms),
	.nla_type = DRBD_NLA_SUSPEND_IO_PARAMS,
	.fields = {
		{ "bdev-freeze", BOOLEAN(DRBD_A_SUSPEND_IO_PARMS_BDEV_FREEZE, SUSPEND_IO_BDEV_FREEZE) },
		{ } },
};

struct context_def peer_device_options_ctx = {
	NLA_POLICY(peer_device_conf),
	.nla_type = DRBD_NLA_PEER_DEVICE_OPTS,
	.fields = {
		{ "resync-rate", NUMERIC(DRBD_A_PEER_DEVICE_CONF_RESYNC_RATE, resync_rate, RESYNC_RATE), .unit = "bytes/second" },
		{ "c-plan-ahead", NUMERIC(DRBD_A_PEER_DEVICE_CONF_C_PLAN_AHEAD, c_plan_ahead, C_PLAN_AHEAD), .unit = "1/10 seconds" },
		{ "c-delay-target", NUMERIC(DRBD_A_PEER_DEVICE_CONF_C_DELAY_TARGET, c_delay_target, C_DELAY_TARGET), .unit = "1/10 seconds" },
		{ "c-fill-target", NUMERIC(DRBD_A_PEER_DEVICE_CONF_C_FILL_TARGET, c_fill_target, C_FILL_TARGET), .unit = "bytes" },
		{ "c-max-rate", NUMERIC(DRBD_A_PEER_DEVICE_CONF_C_MAX_RATE, c_max_rate, C_MAX_RATE), .unit = "bytes/second" },
		{ "c-min-rate", NUMERIC(DRBD_A_PEER_DEVICE_CONF_C_MIN_RATE, c_min_rate, C_MIN_RATE), .unit = "bytes/second" },
		{ "bitmap", BOOLEAN(DRBD_A_PEER_DEVICE_CONF_BITMAP, BITMAP) },
		{ "resync-without-replication", BOOLEAN(DRBD_A_PEER_DEVICE_CONF_RESYNC_WITHOUT_REPLICATION, RESYNC_WITHOUT_REPLICATION) },
		{ "peer-tiebreaker", BOOLEAN(DRBD_A_PEER_DEVICE_CONF_PEER_TIEBREAKER, PEER_TIEBREAKER) },
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
		{ .name = "effective-size", .argument_is_optional = false },
		{ .name = "bitmap-block-size", .argument_is_optional = false },
		{ .name = "diskful-peers", .argument_is_optional = false },
		{ .name = "peers", .argument_is_optional = false },
		{ .name = "bitmap-slots", .argument_is_optional = false },
		{ .name = "initial-current-uuid", .argument_is_optional = false },
		{ .name = "consistent", .argument_is_optional = true },
		{ .name = "uptodate", .argument_is_optional = true },
		{ .name = "peers-outdated", .argument_is_optional = true },
		{ .name = "rotate-uuids", .argument_is_optional = true },
		{ } },
};

struct context_def forceable_ctx = {
       .fields = {
		{ .name = "force", .argument_is_optional = true },
		{ .name = "quiet", .argument_is_optional = true },
		{ } },
};

struct context_def dump_superblock_ctx = {
       .fields = {
		{ .name = "force", .argument_is_optional = true },
		{ .name = "output-format", .argument_is_optional = false },
		{ .name = "quiet", .argument_is_optional = true },
		{ } },
};

struct context_def adjust_ctx = {
	.fields = {
		{ "skip-disk", .argument_is_optional = true },
		{ "skip-net", .argument_is_optional = true },
		CONNECT_CMD_OPTIONS,
		{ } },
};

struct context_def status_ctx = {
	.fields = {
		{ "statistics", .argument_is_optional = true },
		{ } },
};

struct context_def repair_md_ctx = {
       .fields = {
		{ .name = "tentative", .argument_is_optional = true },
		{ .name = "force", .argument_is_optional = true },
		{ .name = "quiet", .argument_is_optional = true },
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
		{ "sndbuf-size", NUMERIC(DRBD_A_NET_CONF_SNDBUF_SIZE, sndbuf_size, SNDBUF_SIZE), .unit = "bytes" },
		{ "rcvbuf-size", NUMERIC(DRBD_A_NET_CONF_RCVBUF_SIZE, rcvbuf_size, RCVBUF_SIZE), .unit = "bytes" },
		{ "ping-timeout", NUMERIC(DRBD_A_NET_CONF_PING_TIMEO, ping_timeo, PING_TIMEO), .unit = "1/10 seconds" },
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
       .fields = { { } },
};

#ifdef WITH_84_SUPPORT
struct field_def attach_compat_84_fields[] = {
	{ "fencing", ENUM(DRBD_A_NET_CONF_FENCING_POLICY, fencing_policy, FENCING) },
	{ "resync-rate", NUMERIC(DRBD_A_PEER_DEVICE_CONF_RESYNC_RATE, resync_rate, RESYNC_RATE), .unit = "bytes/second" },
	{ "c-plan-ahead", NUMERIC(DRBD_A_PEER_DEVICE_CONF_C_PLAN_AHEAD, c_plan_ahead, C_PLAN_AHEAD), .unit = "1/10 seconds" },
	{ "c-delay-target", NUMERIC(DRBD_A_PEER_DEVICE_CONF_C_DELAY_TARGET, c_delay_target, C_DELAY_TARGET), .unit = "1/10 seconds" },
	{ "c-fill-target", NUMERIC(DRBD_A_PEER_DEVICE_CONF_C_FILL_TARGET, c_fill_target, C_FILL_TARGET), .unit = "bytes" },
	{ "c-max-rate", NUMERIC(DRBD_A_PEER_DEVICE_CONF_C_MAX_RATE, c_max_rate, C_MAX_RATE), .unit = "bytes/second" },
	{ "c-min-rate", NUMERIC(DRBD_A_PEER_DEVICE_CONF_C_MIN_RATE, c_min_rate, C_MIN_RATE), .unit = "bytes/second" },
	{ },
};

struct field_def connect_compat_84_fields[] = {
	{ "protocol", ENUM_NOCASE(DRBD_A_NET_CONF_WIRE_PROTOCOL, wire_protocol, PROTOCOL) },
	{ "timeout", NUMERIC(DRBD_A_NET_CONF_TIMEOUT, timeout, TIMEOUT),.unit = "1/10 seconds" },
	{ "max-epoch-size", NUMERIC(DRBD_A_NET_CONF_MAX_EPOCH_SIZE, max_epoch_size, MAX_EPOCH_SIZE) },
	{ "max-buffers", NUMERIC(DRBD_A_NET_CONF_MAX_BUFFERS, max_buffers, MAX_BUFFERS) },
	{ "connect-int", NUMERIC(DRBD_A_NET_CONF_CONNECT_INT, connect_int, CONNECT_INT), .unit = "seconds" },
	{ "ping-int", NUMERIC(DRBD_A_NET_CONF_PING_INT, ping_int, PING_INT), .unit = "seconds" },
	{ "sndbuf-size", NUMERIC(DRBD_A_NET_CONF_SNDBUF_SIZE, sndbuf_size, SNDBUF_SIZE), .unit = "bytes" },
	{ "rcvbuf-size", NUMERIC(DRBD_A_NET_CONF_RCVBUF_SIZE, rcvbuf_size, RCVBUF_SIZE), .unit = "bytes" },
	{ "ko-count", NUMERIC(DRBD_A_NET_CONF_KO_COUNT, ko_count, KO_COUNT) },
	{ "allow-two-primaries", BOOLEAN(DRBD_A_NET_CONF_TWO_PRIMARIES, ALLOW_TWO_PRIMARIES) },
	{ "cram-hmac-alg", STRING(DRBD_A_NET_CONF_CRAM_HMAC_ALG) },
	{ "shared-secret", STRING(DRBD_A_NET_CONF_SHARED_SECRET) },
	{ "after-sb-0pri", ENUM(DRBD_A_NET_CONF_AFTER_SB_0P, after_sb_0p, AFTER_SB_0P) },
	{ "after-sb-1pri", ENUM(DRBD_A_NET_CONF_AFTER_SB_1P, after_sb_1p, AFTER_SB_1P) },
	{ "after-sb-2pri", ENUM(DRBD_A_NET_CONF_AFTER_SB_2P, after_sb_2p, AFTER_SB_2P) },
	{ "always-asbp", BOOLEAN(DRBD_A_NET_CONF_ALWAYS_ASBP, ALWAYS_ASBP) },
	{ "rr-conflict", ENUM(DRBD_A_NET_CONF_RR_CONFLICT, rr_conflict, RR_CONFLICT) },
	{ "ping-timeout", NUMERIC(DRBD_A_NET_CONF_PING_TIMEO, ping_timeo, PING_TIMEO), .unit = "1/10 seconds" },
	{ "data-integrity-alg", STRING(DRBD_A_NET_CONF_INTEGRITY_ALG) },
	{ "tcp-cork", BOOLEAN(DRBD_A_NET_CONF_TCP_CORK, TCP_CORK) },
	{ "on-congestion", ENUM(DRBD_A_NET_CONF_ON_CONGESTION, on_congestion, ON_CONGESTION) },
	{ "congestion-fill", NUMERIC(DRBD_A_NET_CONF_CONG_FILL, cong_fill, CONG_FILL), .unit = "bytes" },
	{ "congestion-extents", NUMERIC(DRBD_A_NET_CONF_CONG_EXTENTS, cong_extents, CONG_EXTENTS) },
	{ "csums-alg", STRING(DRBD_A_NET_CONF_CSUMS_ALG) },
	{ "csums-after-crash-only", BOOLEAN(DRBD_A_NET_CONF_CSUMS_AFTER_CRASH_ONLY, CSUMS_AFTER_CRASH_ONLY) },
	{ "verify-alg", STRING(DRBD_A_NET_CONF_VERIFY_ALG) },
	{ "use-rle", BOOLEAN(DRBD_A_NET_CONF_USE_RLE, USE_RLE) },
	{ "socket-check-timeout", NUMERIC(DRBD_A_NET_CONF_SOCK_CHECK_TIMEO, sock_check_timeo, SOCKET_CHECK_TIMEO) },
	{ },
};

#endif
