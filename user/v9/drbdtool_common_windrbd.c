#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include "drbdtool_common.h"
#include "shared_windrbd.h"

extern struct version __drbd_driver_version;

const struct version *get_drbd_driver_version(void)
{
	char *drbd_version = windrbd_get_drbd_version();

	version_from_str(&__drbd_driver_version, drbd_version);

	return &__drbd_driver_version;
}

