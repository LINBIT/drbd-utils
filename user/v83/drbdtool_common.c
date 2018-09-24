#define _GNU_SOURCE
#define _XOPEN_SOURCE 600
#define _FILE_OFFSET_BITS 64

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <ctype.h>
#include <linux/drbd.h>
#include <linux/fs.h>           /* for BLKGETSIZE64 */
#include <string.h>

#include "drbdtool_common.h"
#include "config.h"

int force = 0;
int confirmed(const char *text)
{
	const char yes[] = "yes";
	const ssize_t N = sizeof(yes);
	char *answer = NULL;
	size_t n = 0;
	int ok;

	printf("\n%s\n", text);

	if (force) {
	    printf("*** confirmation forced via --force option ***\n");
	    ok = 1;
	}
	else {
	    printf("[need to type '%s' to confirm] ", yes);
	    ok = getline(&answer,&n,stdin) == N &&
		strncmp(answer,yes,N-1) == 0;
	    if (answer) free(answer);
	    printf("\n");
	}
	return ok;
}




/* input size is expected to be in KB */




void dt_pretty_print_uuids(const uint64_t* uuid, unsigned int flags)
{
	printf(
"\n"
"       +--<  Current data generation UUID  >-\n"
"       |               +--<  Bitmap's base data generation UUID  >-\n"
"       |               |                 +--<  younger history UUID  >-\n"
"       |               |                 |         +-<  older history  >-\n"
"       V               V                 V         V\n");
	dt_print_uuids(uuid, flags);
	printf(
"                                                                    ^ ^ ^ ^ ^ ^ ^\n"
"                                      -<  Data consistency flag  >--+ | | | | | |\n"
"                             -<  Data was/is currently up-to-date  >--+ | | | | |\n"
"                                  -<  Node was/is currently primary  >--+ | | | |\n"
"                                  -<  Node was/is currently connected  >--+ | | |\n"
"         -<  Node was in the progress of setting all bits in the bitmap  >--+ | |\n"
"                        -<  The peer's disk was out-dated or inconsistent  >--+ |\n"
"      -<  This node was a crashed primary, and has not seen its peer since   >--+\n"
"\n");
	printf("flags:%s %s, %s, %s%s%s\n",
	       (flags & MDF_CRASHED_PRIMARY) ? " crashed" : "",
	       (flags & MDF_PRIMARY_IND) ? "Primary" : "Secondary",
	       (flags & MDF_CONNECTED_IND) ? "Connected" : "StandAlone",
	       (flags & MDF_CONSISTENT)
			?  ((flags & MDF_WAS_UP_TO_DATE) ? "UpToDate" : "Outdated")
			: "Inconsistent",
	       (flags & MDF_FULL_SYNC) ? ", need full sync" : "",
	       (flags & MDF_PEER_OUT_DATED) ? ", peer Outdated" : "");
}
