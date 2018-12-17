/* Function prototypes for platform-specific drbdsetup functions */

#ifndef __DRBD_SETUP_H
#define __DRBD_SETUP_H

#include <stdbool.h>
#include "libgenl.h"
#include <linux/drbd_genl_api.h>
#include <linux/types.h>

#define OTHER_ERROR 900

struct drbd_argument {
	const char* name;
	__u16 nla_type;
	int (*convert_function)(struct drbd_argument *,
				struct msg_buff *,
				struct drbd_genlmsghdr *dhdr,
				char *);
};

enum {
	E_POLL_TIMEOUT = 1,
	E_POLL_ERR,
};

bool kernel_older_than(int version, int patchlevel, int sublevel);
int conv_block_dev(struct drbd_argument *ad, struct msg_buff *msg, struct drbd_genlmsghdr *dhdr, char* arg);
int genl_join_mc_group_and_ctrl(struct genl_sock *s, const char *name);
int poll_hup(struct genl_sock *s, int timeout_ms);
int modprobe_drbd(void);

#endif
