#ifndef DRBDTOOL_COMMON_H
#define DRBDTOOL_COMMON_H

#include "drbd_endian.h"
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <linux/major.h>

#include "shared_tool.h"

#define LANANA_DRBD_MAJOR 147	/* we should get this into linux/major.h */
#ifndef DRBD_MAJOR
#define DRBD_MAJOR LANANA_DRBD_MAJOR
#elif (DRBD_MAJOR != LANANA_DRBD_MAJOR)
# error "FIXME unexpected DRBD_MAJOR"
#endif

#ifndef __packed
#define __packed __attribute__((packed))
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(A) (sizeof(A)/sizeof(A[0]))
#endif

struct option;

extern void dt_release_lockfile(int drbd_fd);
extern void dt_print_uuids(const uint64_t* uuid, unsigned int flags);
extern void dt_pretty_print_uuids(const uint64_t* uuid, unsigned int flags);
extern int fget_token(char *s, int size, FILE* stream);

extern int force; /* global option to force implicit confirmation */
extern int confirmed(const char *text);


#endif
