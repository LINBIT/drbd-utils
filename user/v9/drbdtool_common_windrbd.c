#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include "drbdtool_common.h"

extern struct version __drbd_driver_version;

const struct version *get_drbd_driver_version(void)
{
		/* TODO: have an ioctl returning it */
	version_from_str(&__drbd_driver_version, "9.0.14");

	return &__drbd_driver_version;
}

