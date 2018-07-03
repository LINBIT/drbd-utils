#define _GNU_SOURCE
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/netlink.h>
#include <linux/genetlink.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "libgenl.h"
#include <linux/drbd.h>
#include <linux/drbd_config.h>
#include <linux/drbd_genl_api.h>
#include <linux/drbd_limits.h>
#include "drbd_nla.h"
#include "linux/genl_magic_func.h"
#include "drbdtool_common.h"
#include "config_flags.h"

#ifndef AF_INET_SDP
#define AF_INET_SDP 27
#define PF_INET_SDP AF_INET_SDP
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

#define NLA_POLICY(p)									\
	.nla_policy = p ## _nl_policy,							\
	.nla_policy_size = ARRAY_SIZE(p ## _nl_policy)

/* ============================================================================================== */

static int enum_string_to_int(const char **map, int size, const char *value,
			      int (*strcmp)(const char *, const char *))
{
	int n;

	if (!value)
		return -1;
	for (n = 0; n < size; n++) {
		if (map[n] && !strcmp(value, map[n]))
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
	len += l; size -= l;
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

/* ---------------------------------------------------------------------------------------------- */
static bool address_is_default(struct field_def *field, const char *value)
{
	return true;
}

static bool address_is_equal(struct field_def *field, const char *a, const char *b)
{
	return !strcmp(a, b);
}

/* It will only print the WARNING if the warn flag is set
   with the _first_ call! */
#define PROC_NET_AF_SCI_FAMILY "/proc/net/af_sci/family"
#define PROC_NET_AF_SSOCKS_FAMILY "/proc/net/af_ssocks/family"

int get_af_ssocks(int warn_and_use_default)
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

void sprint_address(char *buffer, void *address, int addr_len)
{
	union {
		struct sockaddr     addr;
		struct sockaddr_in  addr4;
		struct sockaddr_in6 addr6;
	} a;

	memset(&a, 0, sizeof(a));
	memcpy(&a.addr, address, addr_len);

	if (a.addr.sa_family == AF_INET
	|| a.addr.sa_family == get_af_ssocks(0)
	|| a.addr.sa_family == AF_INET_SDP) {
		sprintf(buffer, "%s %s:%d",
		       af_to_str(a.addr4.sin_family),
		       inet_ntoa(a.addr4.sin_addr),
		       ntohs(a.addr4.sin_port));
	} else if (a.addr.sa_family == AF_INET6) {
		char buf2[ADDRESS_STR_MAX];
		int n;
		buf2[0] = 0;
		getnameinfo(&a.addr, addr_len, buf2, sizeof(buf2),
			NULL, 0, NI_NUMERICHOST|NI_NUMERICSERV);
		n = snprintf(buffer, ADDRESS_STR_MAX, "%s [%s]:%d",
			af_to_str(a.addr6.sin6_family), buf2,
			ntohs(a.addr6.sin6_port));
		assert(n > 0);
		assert(n < ADDRESS_STR_MAX); /* there should be no need to truncate */
	} else {
		sprintf(buffer, "[unknown af=%d, len=%d]", a.addr.sa_family, addr_len);
	}
}

static const char *get_address(struct context_def *ctx, struct field_def *field, struct nlattr *nla)
{
	static char buffer[ADDRESS_STR_MAX];
	sprint_address(buffer, nla_data(nla), nla_len(nla));

	return buffer;
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

int nla_put_address(struct msg_buff *msg, int attrtype, const char *arg)
{
	int af, port;
	char *address = strdupa(arg);

	split_address(&af, &address, &port);
	if (af == AF_INET6) {
		struct sockaddr_in6 addr6;

		memset(&addr6, 0, sizeof(addr6));
		resolv6(address, &addr6);
		addr6.sin6_port = htons(port);
		/* addr6.sin6_len = sizeof(addr6); */
		nla_put(msg, attrtype, sizeof(addr6), &addr6);
	} else {
		/* AF_INET, AF_SDP, AF_SSOCKS,
		 * all use the IPv4 addressing scheme */
		struct sockaddr_in addr;

		memset(&addr, 0, sizeof(addr));
		addr.sin_port = htons(port);
		addr.sin_family = af;
		addr.sin_addr.s_addr = resolv(address);
		nla_put(msg, attrtype, sizeof(addr), &addr);
	}
	return NO_ERROR;
}

static bool put_address(struct context_def *ctx, struct field_def *field,
			struct msg_buff *msg, const char *value)
{
	assert(type_of_field(ctx, field) == NLA_BINARY);
	nla_put_address(msg, field->nla_type, value);
	return true;
}

static int address_usage(struct field_def *field, char *str, int size)
{
	return snprintf(str, size,"[--%s=[{af}:]{local_addr}[:{port}]]",
			field->name);
}

static void address_describe_xml(struct field_def *field)
{
	printf("\t<option name=\"%s\" type=\"address\">\n"
	       "\t</option>\n",
	       field->name);
}


/* ============================================================================================== */

#define ENUM(f, d)									\
	.nla_type = T_ ## f,								\
	.is_default = enum_is_default,							\
	.is_equal = enum_is_equal,							\
	.get = get_enum,								\
	.put = put_enum,								\
	.usage = enum_usage,								\
	.describe_xml = enum_describe_xml,						\
	.u = { .e = {									\
		.map = f ## _map,							\
		.size = ARRAY_SIZE(f ## _map),						\
		.def = DRBD_ ## d ## _DEF } }

#define ENUM_NOCASE(f, d)								\
	.nla_type = T_ ## f,								\
	.is_default = enum_is_default_nocase,						\
	.is_equal = enum_is_equal_nocase,						\
	.get = get_enum,								\
	.put = put_enum_nocase,								\
	.usage = enum_usage,								\
	.describe_xml = enum_describe_xml,						\
	.u = { .e = {									\
		.map = f ## _map,							\
		.size = ARRAY_SIZE(f ## _map),						\
		.def = DRBD_ ## d ## _DEF } }

#define NUMERIC(f, d)									\
	.nla_type = T_ ## f,								\
	.is_default = numeric_is_default,						\
	.is_equal = numeric_is_equal,							\
	.get = get_numeric,								\
	.put = put_numeric,								\
	.usage = numeric_usage,								\
	.describe_xml = numeric_describe_xml,						\
	.u = { .n = {									\
		.min = DRBD_ ## d ## _MIN,						\
		.max = DRBD_ ## d ## _MAX,						\
		.def = DRBD_ ## d ## _DEF,						\
		.is_signed = F_ ## f ## _IS_SIGNED,					\
		.scale = DRBD_ ## d ## _SCALE } }

#define BOOLEAN(f, d)									\
	.nla_type = T_ ## f,								\
	.is_default = boolean_is_default,						\
	.is_equal = boolean_is_equal,							\
	.get = get_boolean,								\
	.put = put_boolean,								\
	.usage = boolean_usage,								\
	.describe_xml = boolean_describe_xml,						\
	.u = { .b = {									\
		.def = DRBD_ ## d ## _DEF } },						\
	.argument_is_optional = true

#define FLAG(f)										\
	.nla_type = T_ ## f,								\
	.is_default = boolean_is_default,						\
	.is_equal = boolean_is_equal,							\
	.get = get_boolean,								\
	.put = put_flag,								\
	.usage = boolean_usage,								\
	.describe_xml = boolean_describe_xml,						\
	.u = { .b = {									\
		.def = false } },							\
	.argument_is_optional = true

#define STRING(f)									\
	.nla_type = T_ ## f,								\
	.is_default = string_is_default,						\
	.is_equal = string_is_equal,							\
	.get = get_string,								\
	.put = put_string,								\
	.usage = string_usage,								\
	.describe_xml = string_describe_xml,						\
	.needs_double_quoting = true

#define ADDRESS(f)									\
	.nla_type = T_ ## f,								\
	.is_default = address_is_default,						\
	.is_equal = address_is_equal,							\
	.get = get_address,								\
	.put = put_address,								\
	.usage = address_usage,								\
	.describe_xml = address_describe_xml,						\
	.needs_double_quoting = false

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

const char *fencing_map[] = {
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
};

const char *on_no_data_map[] = {
	[OND_IO_ERROR]		= "io-error",
	[OND_SUSPEND_IO]	= "suspend-io",
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

#define CHANGEABLE_DISK_OPTIONS								\
	{ "on-io-error", ENUM(on_io_error, ON_IO_ERROR) },				\
	{ "fencing", ENUM(fencing, FENCING) },						\
	{ "disk-barrier", BOOLEAN(disk_barrier, DISK_BARRIER) },			\
	{ "disk-flushes", BOOLEAN(disk_flushes, DISK_FLUSHES) },			\
	{ "disk-drain", BOOLEAN(disk_drain, DISK_DRAIN) },				\
	{ "md-flushes", BOOLEAN(md_flushes, MD_FLUSHES) },				\
	{ "resync-rate", NUMERIC(resync_rate, RESYNC_RATE),				\
          .unit = "bytes/second" },							\
	{ "resync-after", NUMERIC(resync_after, MINOR_NUMBER) },			\
	{ "al-extents", NUMERIC(al_extents, AL_EXTENTS) },				\
	{ "al-updates", BOOLEAN(al_updates, AL_UPDATES) },				\
	{ "discard-zeroes-if-aligned",							\
		BOOLEAN(discard_zeroes_if_aligned, DISCARD_ZEROES_IF_ALIGNED) },	\
	{ "disable-write-same",								\
		BOOLEAN(disable_write_same, DISABLE_WRITE_SAME) },			\
	{ "c-plan-ahead", NUMERIC(c_plan_ahead, C_PLAN_AHEAD),				\
          .unit = "1/10 seconds" },							\
	{ "c-delay-target", NUMERIC(c_delay_target, C_DELAY_TARGET),			\
          .unit = "1/10 seconds" },							\
	{ "c-fill-target", NUMERIC(c_fill_target, C_FILL_TARGET),			\
          .unit = "bytes" },								\
	{ "c-max-rate", NUMERIC(c_max_rate, C_MAX_RATE),				\
          .unit = "bytes/second" },							\
	{ "c-min-rate", NUMERIC(c_min_rate, C_MIN_RATE),				\
	  .unit = "bytes/second" },							\
	{ "disk-timeout", NUMERIC(disk_timeout,	DISK_TIMEOUT),				\
	  .unit = "1/10 seconds" },							\
	{ "read-balancing", ENUM(read_balancing, READ_BALANCING) },			\
	{ "rs-discard-granularity",							\
	  NUMERIC(rs_discard_granularity, RS_DISCARD_GRANULARITY) }			\

#define CHANGEABLE_NET_OPTIONS								\
	{ "protocol", ENUM_NOCASE(wire_protocol, PROTOCOL) },				\
	{ "timeout", NUMERIC(timeout, TIMEOUT),						\
          .unit = "1/10 seconds" },							\
	{ "max-epoch-size", NUMERIC(max_epoch_size, MAX_EPOCH_SIZE) },			\
	{ "max-buffers", NUMERIC(max_buffers, MAX_BUFFERS) },				\
	{ "unplug-watermark", NUMERIC(unplug_watermark, UNPLUG_WATERMARK) },		\
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
	{ "shared-secret", STRING(shared_secret) },					\
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
	{ "socket-check-timeout", NUMERIC(sock_check_timeo, SOCKET_CHECK_TIMEO) }

struct context_def disk_options_ctx = {
	NLA_POLICY(disk_conf),
	.fields = {
		CHANGEABLE_DISK_OPTIONS,
		{ } },
};

struct context_def net_options_ctx = {
	NLA_POLICY(net_conf),
	.fields = {
		CHANGEABLE_NET_OPTIONS,
		{ } },
};

struct context_def primary_cmd_ctx = {
	NLA_POLICY(set_role_parms),
	.fields = {
		{ "force", FLAG(assume_uptodate) },
		{ } },
};

struct context_def attach_cmd_ctx = {
	NLA_POLICY(disk_conf),
	.fields = {
		{ "size", NUMERIC(disk_size, DISK_SIZE),
		  .unit = "bytes" },
		{ "max-bio-bvecs", NUMERIC(max_bio_bvecs, MAX_BIO_BVECS) },
		CHANGEABLE_DISK_OPTIONS,
		/* { "*", STRING(backing_dev) }, */
		/* { "*", STRING(meta_dev) }, */
		/* { "*", NUMERIC(meta_dev_idx, MINOR_NUMBER) }, */
		{ } },
};

struct context_def detach_cmd_ctx = {
	NLA_POLICY(detach_parms),
	.fields = {
		{ "force", FLAG(force_detach) },
		{ }
	},
};

struct context_def connect_cmd_ctx = {
	NLA_POLICY(net_conf),
	.fields = {
		{ "tentative", FLAG(tentative) },
		{ "discard-my-data", FLAG(discard_my_data) },
		{ "alternate-address", ADDRESS(my_addr2) },
		{ "alternate-peer-address", ADDRESS(peer_addr2) },
		CHANGEABLE_NET_OPTIONS,
		{ } },
};

struct context_def disconnect_cmd_ctx = {
	NLA_POLICY(disconnect_parms),
	.fields = {
		{ "force", FLAG(force_disconnect) },
		{ } },
};

struct context_def resize_cmd_ctx = {
	NLA_POLICY(resize_parms),
	.fields = {
		{ "size", NUMERIC(resize_size, DISK_SIZE),
		  .unit = "bytes" },
		{ "assume-peer-has-space", FLAG(resize_force) },
		{ "assume-clean", FLAG(no_resync) },
		{ "al-stripes", NUMERIC(al_stripes, AL_STRIPES) },
		{ "al-stripe-size-kB", NUMERIC(al_stripe_size, AL_STRIPE_SIZE) },
		{ } },
};

struct context_def resource_options_cmd_ctx = {
	NLA_POLICY(res_opts),
	.fields = {
		{ "cpu-mask", STRING(cpu_mask) },
		{ "on-no-data-accessible", ENUM(on_no_data, ON_NO_DATA) },
		{ } },
};

struct context_def new_current_uuid_cmd_ctx = {
	NLA_POLICY(new_c_uuid_parms),
	.fields = {
		{ "clear-bitmap", FLAG(clear_bm) },
		{ } },
};

struct context_def verify_cmd_ctx = {
	NLA_POLICY(start_ov_parms),
	.fields = {
		{ "start", NUMERIC(ov_start_sector, DISK_SIZE),
		  .unit = "bytes" },
		{ "stop", NUMERIC(ov_stop_sector, DISK_SIZE),
		  .unit = "bytes" },
		{ } },
};

struct context_def new_minor_cmd_ctx = {
	NLA_POLICY(drbd_cfg_context),
	.fields = {
		/* { "*", STRING(ctx_resource_name) }, */
		/* { "*", NUMERIC(ctx_volume, >= 0) }, */
		/* { "*", BINARY(ctx_my_addr) }, */
		/* { "*", BINARY(ctx_peer_addr) }, */
		{ } },
};
