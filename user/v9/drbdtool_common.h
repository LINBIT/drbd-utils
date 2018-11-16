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

/**
 * BUILD_BUG_ON - break compile if a condition is true.
 * @condition: the condition which the compiler should know is false.
 *
 * If you have some code which relies on certain constants being equal, or
 * other compile-time-evaluated condition, you should use BUILD_BUG_ON to
 * detect if someone changes it.
 *
 * The implementation uses gcc's reluctance to create a negative array, but
 * gcc (as of 4.4) only emits that error for obvious cases (eg. not arguments
 * to inline functions).  So as a fallback we use the optimizer; if it can't
 * prove the condition is false, it will cause a link error on the undefined
 * "__build_bug_on_failed".  This error message can be harder to track down
 * though, hence the two different methods.
 */
#ifndef __OPTIMIZE__
#define BUILD_BUG_ON(condition) ((void)sizeof(char[1 - 2*!!(condition)]))
#else
extern int __build_bug_on_failed;
#define BUILD_BUG_ON(condition)                                 \
	do {                                                    \
		((void)sizeof(char[1 - 2*!!(condition)]));      \
		if (condition) __build_bug_on_failed = 1;       \
	} while(0)
#endif

/* Flags which used to be in enum mdf_flag before version 09 */
enum mdf_flag_08 {
	MDF_CONNECTED_IND =  1 << 2,
	MDF_FULL_SYNC =      1 << 3,
	MDF_PEER_OUT_DATED = 1 << 5,
	MDF_FENCING_IND =    1 << 8,
};

struct option;

extern void dt_release_lockfile(int drbd_fd);
extern unsigned long long m_strtoll(const char* s,const char def_unit);
extern void dt_print_uuids(const uint64_t* uuid, unsigned int flags);
extern void dt_pretty_print_uuids(const uint64_t* uuid, unsigned int flags);

void dt_print_v9_uuids(const uint64_t*, unsigned int, unsigned int);
void dt_pretty_print_v9_uuids(const uint64_t*, unsigned int, unsigned int);

const char *get_hostname(void);

#define GIT_HASH_BYTE   20
#define SRCVERSION_BYTE 12     /* actually 11 and a half. */
#define SRCVERSION_PAD (GIT_HASH_BYTE - SRCVERSION_BYTE)
#define SVN_STYLE_OD  16

struct version {
	uint32_t svn_revision;
	char git_hash[GIT_HASH_BYTE];
	struct {
		unsigned major, minor, sublvl;
	} version;
	unsigned version_code;
};

/* Windows km/miniport.h has a #define STRICT */
#ifdef STRICT
#undef STRICT
#endif

enum driver_version_policy {
	STRICT,
	FALLBACK_TO_UTILS
};
extern const struct version *drbd_driver_version(enum driver_version_policy fallback);
extern const struct version *drbd_utils_version(void);
extern const char *escaped_version_code_kernel(void);
extern int version_code_kernel(void);
extern int version_code_userland(void);
extern int version_equal(const struct version *rev1, const struct version *rev2);
extern void config_help_legacy(const char * const tool, const struct version * const driver_version);
extern void add_component_to_path(const char *path);
extern void add_lib_drbd_to_path(void);
extern uint32_t crc32c(uint32_t crc, const uint8_t *data, unsigned int length);

extern void parse_version(struct version *rel, const char *text);
extern void version_from_str(struct version *rel, const char *token);

/* This is platform-specific */
extern const struct version *get_drbd_driver_version(void);

#endif
