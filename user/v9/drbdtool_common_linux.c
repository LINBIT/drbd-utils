#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include "drbdtool_common.h"

extern struct version __drbd_driver_version;

/* For our purpose (finding the revision) SLURP_SIZE is always enough.
 */
static char *slurp_proc_drbd()
{
	const int SLURP_SIZE = 4096;
	char *buffer;
	int rr, fd;

	fd = open("/proc/drbd",O_RDONLY);
	if (fd == -1)
		return NULL;

	buffer = malloc(SLURP_SIZE);
	if(!buffer)
		goto fail;

	rr = read(fd, buffer, SLURP_SIZE-1);
	if (rr == -1) {
		free(buffer);
		buffer = NULL;
		goto fail;
	}

	buffer[rr]=0;
fail:
	close(fd);

	return buffer;
}

const struct version *get_drbd_driver_version(void)
{
	char *version_txt;

	version_txt = slurp_proc_drbd();
	if (version_txt) {
		parse_version(&__drbd_driver_version, version_txt);
		free(version_txt);
		return &__drbd_driver_version;
	} else {
		FILE *in = popen("modinfo -F version drbd", "r");
		if (in) {
			char buf[32];
			int c = fscanf(in, "%30s", buf);
			pclose(in);
			if (c == 1) {
				version_from_str(&__drbd_driver_version, buf);
				return &__drbd_driver_version;
			}
		}
	}
	return NULL;
}

