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

#define COMM_TIMEOUT 120

/* MetaDataIndex for v06 / v07 style meta data blocks */
enum MetaDataIndex {
	Flags,			/* Consistency flag,connected-ind,primary-ind */
	HumanCnt,		/* human-intervention-count */
	TimeoutCnt,		/* timout-count */
	ConnectedCnt,		/* connected-count */
	ArbitraryCnt,		/* arbitrary-count */
	GEN_CNT_SIZE		/* MUST BE LAST! (and Flags must stay first...) */
};

/*
#define PERROR(fmt, args...) \
do { fprintf(stderr,fmt ": " , ##args); perror(0); } while (0)
*/
#define PERROR(fmt, args...) fprintf(stderr, fmt ": %m\n" , ##args);

enum new_strtoll_errs {
	MSE_OK,
	MSE_DEFAULT_UNIT,
	MSE_MISSING_NUMBER,
	MSE_INVALID_NUMBER,
	MSE_INVALID_UNIT,
	MSE_OUT_OF_RANGE,
};

struct option;

extern void dt_release_lockfile(int drbd_fd);
extern int new_strtoll(const char *s, const char def_unit, unsigned long long *rv);
extern void dt_print_uuids(const uint64_t* uuid, unsigned int flags);
extern void dt_pretty_print_uuids(const uint64_t* uuid, unsigned int flags);
extern int fget_token(char *s, int size, FILE* stream);

extern int force; /* global option to force implicit confirmation */
extern int confirmed(const char *text);


#endif
