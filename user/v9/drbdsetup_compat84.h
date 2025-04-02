#ifndef __DRBD_SETUP_COMPAT84_H
#define __DRBD_SETUP_COMPAT84_H

#include "drbdsetup.h"

#ifdef WITH_84_SUPPORT
int drbd8_compat_connect_or_disconnect(int argc, char **argv, const struct drbd_cmd *cmd);
#else
static inline int drbd8_compat_connect_or_disconnect(int argc, char **argv, const struct drbd_cmd *cmd)
{
	return 0;
}
#endif

#endif /* __DRBD_SETUP_COMPAT84_H */
