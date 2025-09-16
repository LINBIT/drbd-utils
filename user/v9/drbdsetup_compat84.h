#ifndef __DRBD_SETUP_COMPAT84_H
#define __DRBD_SETUP_COMPAT84_H

#include "drbdsetup.h"

#ifdef WITH_84_SUPPORT
int drbd8_compat_connect_or_disconnect(int argc, char **argv, const struct drbd_cmd *cmd);
int drbd8_compat_attach(int argc, char **argv);
void drbd8_compat_new_minor(const char *resname, const char *minor_str, const char *vol_str);
#else
static inline int drbd8_compat_connect_or_disconnect(int argc, char **argv, const struct drbd_cmd *cmd)
{
	return 0;
}
static int drbd8_compat_attach(int argc, char **argv)
{
	return 0;
}
static void drbd8_compat_new_minor(const char *resname, const char *minor_str, const char* vol_str) {}
#endif

#endif /* __DRBD_SETUP_COMPAT84_H */
