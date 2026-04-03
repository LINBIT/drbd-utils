#ifndef __DRBD_NLA_H
#define __DRBD_NLA_H

/* genl_magic_func.h still references drbd_nla_parse_nested() */
#define drbd_nla_parse_nested(tb, maxtype, nla, policy) \
	nla_parse_nested(tb, maxtype, nla, policy)

#endif  /* __DRBD_NLA_H */
