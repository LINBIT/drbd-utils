#include "libgenl.h"

#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <poll.h>
#include <stdbool.h>

/*
 * Stripped down copy from linux-2.6.32/lib/nlattr.c
 * skb -> "msg_buff"
 *	- Lars Ellenberg
 *
 * NETLINK      Netlink attributes
 *
 *		Authors:	Thomas Graf <tgraf@suug.ch>
 *				Alexey Kuznetsov <kuznet@ms2.inr.ac.ru>
 */

#include <string.h>
#include <linux/types.h>

static __u16 nla_attr_minlen[NLA_TYPE_MAX+1] __read_mostly = {
	[NLA_U8]	= sizeof(__u8),
	[NLA_U16]	= sizeof(__u16),
	[NLA_U32]	= sizeof(__u32),
	[NLA_U64]	= sizeof(__u64),
	[NLA_NESTED]	= NLA_HDRLEN,
};

static int validate_nla(struct nlattr *nla, int maxtype,
			const struct nla_policy *policy)
{
	const struct nla_policy *pt;
	int minlen = 0, attrlen = nla_len(nla), type = nla_type(nla);

	if (type <= 0 || type > maxtype)
		return 0;

	pt = &policy[type];

	BUG_ON(pt->type > NLA_TYPE_MAX);

	switch (pt->type) {
	case NLA_FLAG:
		if (attrlen > 0)
			return -ERANGE;
		break;

	case NLA_NUL_STRING:
		if (pt->len)
			minlen = min_t(int, attrlen, pt->len + 1);
		else
			minlen = attrlen;

		if (!minlen || memchr(nla_data(nla), '\0', minlen) == NULL)
			return -EINVAL;
		/* fall through */

	case NLA_STRING:
		if (attrlen < 1)
			return -ERANGE;

		if (pt->len) {
			char *buf = nla_data(nla);

			if (buf[attrlen - 1] == '\0')
				attrlen--;

			if (attrlen > pt->len)
				return -ERANGE;
		}
		break;

	case NLA_BINARY:
		if (pt->len && attrlen > pt->len)
			return -ERANGE;
		break;

	case NLA_NESTED_COMPAT:
		if (attrlen < pt->len)
			return -ERANGE;
		if (attrlen < NLA_ALIGN(pt->len))
			break;
		if (attrlen < NLA_ALIGN(pt->len) + NLA_HDRLEN)
			return -ERANGE;
		nla = nla_data(nla) + NLA_ALIGN(pt->len);
		if (attrlen < NLA_ALIGN(pt->len) + NLA_HDRLEN + nla_len(nla))
			return -ERANGE;
		break;
	case NLA_NESTED:
		/* a nested attributes is allowed to be empty; if its not,
		 * it must have a size of at least NLA_HDRLEN.
		 */
		if (attrlen == 0)
			break;
	default:
		if (pt->len)
			minlen = pt->len;
		else if (pt->type != NLA_UNSPEC)
			minlen = nla_attr_minlen[pt->type];

		if (attrlen < minlen)
			return -ERANGE;
	}

	return 0;
}

/**
 * nla_validate - Validate a stream of attributes
 * @head: head of attribute stream
 * @len: length of attribute stream
 * @maxtype: maximum attribute type to be expected
 * @policy: validation policy
 *
 * Validates all attributes in the specified attribute stream against the
 * specified policy. Attributes with a type exceeding maxtype will be
 * ignored. See documenation of struct nla_policy for more details.
 *
 * Returns 0 on success or a negative error code.
 */
int nla_validate(struct nlattr *head, int len, int maxtype,
		 const struct nla_policy *policy)
{
	struct nlattr *nla;
	int rem, err;

	nla_for_each_attr(nla, head, len, rem) {
		err = validate_nla(nla, maxtype, policy);
		if (err < 0)
			goto errout;
	}

	err = 0;
errout:
	return err;
}

/**
 * nla_policy_len - Determin the max. length of a policy
 * @policy: policy to use
 * @n: number of policies
 *
 * Determines the max. length of the policy.  It is currently used
 * to allocated Netlink buffers roughly the size of the actual
 * message.
 *
 * Returns 0 on success or a negative error code.
 */
int
nla_policy_len(const struct nla_policy *p, int n)
{
	int i, len = 0;

	for (i = 0; i < n; i++, p++) {
		if (p->len)
			len += nla_total_size(p->len);
		else if (nla_attr_minlen[p->type])
			len += nla_total_size(nla_attr_minlen[p->type]);
	}

	return len;
}

/**
 * nla_parse - Parse a stream of attributes into a tb buffer
 * @tb: destination array with maxtype+1 elements
 * @maxtype: maximum attribute type to be expected
 * @head: head of attribute stream
 * @len: length of attribute stream
 * @policy: validation policy
 *
 * Parses a stream of attributes and stores a pointer to each attribute in
 * the tb array accessable via the attribute type. Attributes with a type
 * exceeding maxtype will be silently ignored for backwards compatibility
 * reasons. policy may be set to NULL if no validation is required.
 *
 * Returns 0 on success or a negative error code.
 */
int nla_parse(struct nlattr *tb[], int maxtype, struct nlattr *head, int len,
	      const struct nla_policy *policy)
{
	struct nlattr *nla;
	int rem, err;

	memset(tb, 0, sizeof(struct nlattr *) * (maxtype + 1));

	nla_for_each_attr(nla, head, len, rem) {
		__u16 type = nla_type(nla);

		if (type > 0 && type <= maxtype) {
			if (policy) {
				err = validate_nla(nla, maxtype, policy);
				if (err < 0)
					goto errout;
			}

			tb[type] = nla;
		}
	}

	if (unlikely(rem > 0))
		dbg(1, "netlink: %d bytes leftover after parsing "
		       "attributes.\n", rem);

	err = 0;
errout:
	if (err)
		dbg(1, "netlink: policy violation t:%d[%x] e:%d\n",
				nla_type(nla), nla->nla_type, err);
	return err;
}

/**
 * nla_find - Find a specific attribute in a stream of attributes
 * @head: head of attribute stream
 * @len: length of attribute stream
 * @attrtype: type of attribute to look for
 *
 * Returns the first attribute in the stream matching the specified type.
 */
struct nlattr *nla_find(struct nlattr *head, int len, int attrtype)
{
	struct nlattr *nla;
	int rem;

	nla_for_each_attr(nla, head, len, rem)
		if (nla_type(nla) == attrtype)
			return nla;

	return NULL;
}

/**
 * nla_strlcpy - Copy string attribute payload into a sized buffer
 * @dst: where to copy the string to
 * @nla: attribute to copy the string from
 * @dstsize: size of destination buffer
 *
 * Copies at most dstsize - 1 bytes into the destination buffer.
 * The result is always a valid NUL-terminated string. Unlike
 * strlcpy the destination buffer is always padded out.
 *
 * Returns the length of the source buffer.
 */
size_t nla_strlcpy(char *dst, const struct nlattr *nla, size_t dstsize)
{
	size_t srclen = nla_len(nla);
	char *src = nla_data(nla);

	if (srclen > 0 && src[srclen - 1] == '\0')
		srclen--;

	if (dstsize > 0) {
		size_t len = (srclen >= dstsize) ? dstsize - 1 : srclen;

		memset(dst, 0, dstsize);
		memcpy(dst, src, len);
	}

	return srclen;
}

/**
 * nla_memcpy - Copy a netlink attribute into another memory area
 * @dest: where to copy to memcpy
 * @src: netlink attribute to copy from
 * @count: size of the destination area
 *
 * Note: The number of bytes copied is limited by the length of
 *       attribute's payload. memcpy
 *
 * Returns the number of bytes copied.
 */
int nla_memcpy(void *dest, const struct nlattr *src, int count)
{
	int minlen = min_t(int, count, nla_len(src));

	memcpy(dest, nla_data(src), minlen);

	return minlen;
}

/**
 * nla_memcmp - Compare an attribute with sized memory area
 * @nla: netlink attribute
 * @data: memory area
 * @size: size of memory area
 */
int nla_memcmp(const struct nlattr *nla, const void *data,
			     size_t size)
{
	int d = nla_len(nla) - size;

	if (d == 0)
		d = memcmp(nla_data(nla), data, size);

	return d;
}

/**
 * nla_strcmp - Compare a string attribute against a string
 * @nla: netlink string attribute
 * @str: another string
 */
int nla_strcmp(const struct nlattr *nla, const char *str)
{
	int len = strlen(str) + 1;
	int d = nla_len(nla) - len;

	if (d == 0)
		d = memcmp(nla_data(nla), str, len);

	return d;
}

/**
 * __nla_reserve - reserve room for attribute on the msg
 * @msg: message buffer to reserve room on
 * @attrtype: attribute type
 * @attrlen: length of attribute payload
 *
 * Adds a netlink attribute header to a message buffer and reserves
 * room for the payload but does not copy it.
 *
 * The caller is responsible to ensure that the msg provides enough
 * tailroom for the attribute header and payload.
 */
struct nlattr *__nla_reserve(struct msg_buff *msg, int attrtype, int attrlen)
{
	struct nlattr *nla;

	nla = (struct nlattr *) msg_put(msg, nla_total_size(attrlen));
	nla->nla_type = attrtype;
	nla->nla_len = nla_attr_size(attrlen);

	memset((unsigned char *) nla + nla->nla_len, 0, nla_padlen(attrlen));

	return nla;
}

/**
 * __nla_reserve_nohdr - reserve room for attribute without header
 * @msg: message buffer to reserve room on
 * @attrlen: length of attribute payload
 *
 * Reserves room for attribute payload without a header.
 *
 * The caller is responsible to ensure that the msg provides enough
 * tailroom for the payload.
 */
void *__nla_reserve_nohdr(struct msg_buff *msg, int attrlen)
{
	void *start;

	start = msg_put(msg, NLA_ALIGN(attrlen));
	memset(start, 0, NLA_ALIGN(attrlen));

	return start;
}

/**
 * nla_reserve - reserve room for attribute on the msg
 * @msg: message buffer to reserve room on
 * @attrtype: attribute type
 * @attrlen: length of attribute payload
 *
 * Adds a netlink attribute header to a message buffer and reserves
 * room for the payload but does not copy it.
 *
 * Returns NULL if the tailroom of the msg is insufficient to store
 * the attribute header and payload.
 */
struct nlattr *nla_reserve(struct msg_buff *msg, int attrtype, int attrlen)
{
	if (unlikely(msg_tailroom(msg) < nla_total_size(attrlen)))
		return NULL;

	return __nla_reserve(msg, attrtype, attrlen);
}

/**
 * nla_reserve_nohdr - reserve room for attribute without header
 * @msg: message buffer to reserve room on
 * @attrlen: length of attribute payload
 *
 * Reserves room for attribute payload without a header.
 *
 * Returns NULL if the tailroom of the msg is insufficient to store
 * the attribute payload.
 */
void *nla_reserve_nohdr(struct msg_buff *msg, int attrlen)
{
	if (unlikely(msg_tailroom(msg) < NLA_ALIGN(attrlen)))
		return NULL;

	return __nla_reserve_nohdr(msg, attrlen);
}

/**
 * __nla_put - Add a netlink attribute to a message buffer
 * @msg: message buffer to add attribute to
 * @attrtype: attribute type
 * @attrlen: length of attribute payload
 * @data: head of attribute payload
 *
 * The caller is responsible to ensure that the msg provides enough
 * tailroom for the attribute header and payload.
 */
void __nla_put(struct msg_buff *msg, int attrtype, int attrlen,
			     const void *data)
{
	struct nlattr *nla;

	nla = __nla_reserve(msg, attrtype, attrlen);
	memcpy(nla_data(nla), data, attrlen);
}

/**
 * __nla_put_nohdr - Add a netlink attribute without header
 * @msg: message buffer to add attribute to
 * @attrlen: length of attribute payload
 * @data: head of attribute payload
 *
 * The caller is responsible to ensure that the msg provides enough
 * tailroom for the attribute payload.
 */
void __nla_put_nohdr(struct msg_buff *msg, int attrlen, const void *data)
{
	void *start;

	start = __nla_reserve_nohdr(msg, attrlen);
	memcpy(start, data, attrlen);
}

/**
 * nla_put - Add a netlink attribute to a message buffer
 * @msg: message buffer to add attribute to
 * @attrtype: attribute type
 * @attrlen: length of attribute payload
 * @data: head of attribute payload
 *
 * Returns -EMSGSIZE if the tailroom of the msg is insufficient to store
 * the attribute header and payload.
 */
int nla_put(struct msg_buff *msg, int attrtype, int attrlen, const void *data)
{
	if (unlikely(msg_tailroom(msg) < nla_total_size(attrlen)))
		return -EMSGSIZE;

	__nla_put(msg, attrtype, attrlen, data);
	return 0;
}

/* TODO add an architecture/platform blacklist */
#define CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS 1

#define IS_ALIGNED(x, a)                (((x) & ((typeof(x))(a) - 1)) == 0)
/**
 * nla_need_padding_for_64bit - test 64-bit alignment of the next attribute
 * @msg: message buffer the message is stored in
 *
 * Return true if padding is needed to align the next attribute (nla_data()) to
 * a 64-bit aligned area.
 */
static inline bool nla_need_padding_for_64bit(struct msg_buff *msg)
{
#ifndef CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS
	/* The nlattr header is 4 bytes in size, that's why we test
	 * if the msg->data _is_ aligned.  A NOP attribute, plus
	 * nlattr header for next attribute, will make nla_data()
	 * 8-byte aligned.
	 */
	if (IS_ALIGNED((unsigned long)msg_tail_pointer(msg), 8))
		return true;
#endif
	return false;
}

/**
 * nla_align_64bit - 64-bit align the nla_data() of next attribute
 * @msg: message buffer the message is stored in
 * @padattr: attribute type for the padding
 *
 * Conditionally emit a padding netlink attribute in order to make
 * the next attribute we emit have a 64-bit aligned nla_data() area.
 * This will only be done in architectures which do not have
 * CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS defined.
 *
 * Returns zero on success or a negative error code.
 */
static inline int nla_align_64bit(struct msg_buff *msg, int padattr)
{
	if (nla_need_padding_for_64bit(msg) &&
	    !nla_reserve(msg, padattr, 0))
		return -EMSGSIZE;

	return 0;
}

/**
 * nla_total_size_64bit - total length of attribute including padding
 * @payload: length of payload
 */
static inline int nla_total_size_64bit(int payload)
{
	return NLA_ALIGN(nla_attr_size(payload))
#ifndef CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS
		+ NLA_ALIGN(nla_attr_size(0))
#endif
		;
}

/**
 * __nla_reserve_64bit - reserve room for attribute on the msg and align it
 * @msg: message buffer to reserve room on
 * @attrtype: attribute type
 * @attrlen: length of attribute payload
 * @padattr: attribute type for the padding
 *
 * Adds a netlink attribute header to a socket buffer and reserves
 * room for the payload but does not copy it. It also ensure that this
 * attribute will have a 64-bit aligned nla_data() area.
 *
 * The caller is responsible to ensure that the msg provides enough
 * tailroom for the attribute header and payload.
 */
struct nlattr *__nla_reserve_64bit(struct msg_buff *msg, int attrtype,
				   int attrlen, int padattr)
{
	if (nla_need_padding_for_64bit(msg))
		nla_align_64bit(msg, padattr);

	return __nla_reserve(msg, attrtype, attrlen);
}

/**
 * __nla_put_64bit - Add a netlink attribute to a socket buffer and align it
 * @msg: message buffer to add attribute to
 * @attrtype: attribute type
 * @attrlen: length of attribute payload
 * @data: head of attribute payload
 * @padattr: attribute type for the padding
 *
 * The caller is responsible to ensure that the msg provides enough
 * tailroom for the attribute header and payload.
 */
void __nla_put_64bit(struct msg_buff *msg, int attrtype, int attrlen,
		     const void *data, int padattr)
{
	struct nlattr *nla;

	nla = __nla_reserve_64bit(msg, attrtype, attrlen, padattr);
	memcpy(nla_data(nla), data, attrlen);
}

/**
 * nla_put_64bit - Add a netlink attribute to a socket buffer and align it
 * @msg: message buffer to add attribute to
 * @attrtype: attribute type
 * @attrlen: length of attribute payload
 * @data: head of attribute payload
 * @padattr: attribute type for the padding
 *
 * Returns -EMSGSIZE if the tailroom of the msg is insufficient to store
 * the attribute header and payload.
 */
int nla_put_64bit(struct msg_buff *msg, int attrtype, int attrlen,
		  const void *data, int padattr)
{
	size_t len;

	if (nla_need_padding_for_64bit(msg))
		len = nla_total_size_64bit(attrlen);
	else
		len = nla_total_size(attrlen);
	if (unlikely(msg_tailroom(msg) < len))
		return -EMSGSIZE;

	__nla_put_64bit(msg, attrtype, attrlen, data, padattr);
	return 0;
}

/**
 * nla_put_nohdr - Add a netlink attribute without header
 * @msg: message buffer to add attribute to
 * @attrlen: length of attribute payload
 * @data: head of attribute payload
 *
 * Returns -EMSGSIZE if the tailroom of the msg is insufficient to store
 * the attribute payload.
 */
int nla_put_nohdr(struct msg_buff *msg, int attrlen, const void *data)
{
	if (unlikely(msg_tailroom(msg) < NLA_ALIGN(attrlen)))
		return -EMSGSIZE;

	__nla_put_nohdr(msg, attrlen, data);
	return 0;
}

/**
 * nla_append - Add a netlink attribute without header or padding
 * @msg: message buffer to add attribute to
 * @attrlen: length of attribute payload
 * @data: head of attribute payload
 *
 * Returns -EMSGSIZE if the tailroom of the msg is insufficient to store
 * the attribute payload.
 */
int nla_append(struct msg_buff *msg, int attrlen, const void *data)
{
	if (unlikely(msg_tailroom(msg) < NLA_ALIGN(attrlen)))
		return -EMSGSIZE;

	memcpy(msg_put(msg, attrlen), data, attrlen);
	return 0;
}

