#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include "drbdtool_common.h"
#include "shared_windrbd.h"

extern struct version __drbd_driver_version;

/* WinDRBD does not serve the 8.4 netlink API, so it has no "core:" field. */
bool get_drbd_core_version(struct version *core)
{
	return false;
}

const struct version *get_drbd_driver_version(void)
{
	char *drbd_version = windrbd_get_drbd_version();

	version_from_str(&__drbd_driver_version, drbd_version);

	return &__drbd_driver_version;
}

