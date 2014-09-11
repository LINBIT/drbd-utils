#ifndef TOOL_SHARED_H
#define TOOL_SHARED_H

#include <linux/fs.h>           /* for BLKGETSIZE64 */
#include <inttypes.h>
#include <stdio.h>
#include <fcntl.h>

struct option;

extern const char* shell_escape(const char* s);

extern char* ppsize(char* buf, unsigned long long size);
extern const char* make_optstring(struct option *options);
extern int fget_token(char *s, int size, FILE* stream);
extern int sget_token(char *s, int size, const char** text);
extern uint64_t bdev_size(int fd);

/* In-place unescape double quotes and backslash escape sequences from a
 * double quoted string. Note: backslash is only useful to quote itself, or
 * double quote, no special treatment to any c-style escape sequences. */
extern void unescape(char *txt);


/* If the lower level device is resized,
 * and DRBD did not move its "internal" meta data in time,
 * the next time we try to attach, we won't find our meta data.
 *
 * Some helpers for storing and retrieving "last known"
 * information, to be able to find it regardless,
 * without scanning the full device for magic numbers.
 */

/* We may want to store more things later...  if so, we can easily change to
 * some NULL terminated tag-value list format then.
 * For now: store the last known lower level block device size,
 * and its /dev/<name> */
struct bdev_info {
	uint64_t bd_size;
	uint64_t bd_uuid;
	char *bd_name;
};


/* these return 0 on sucess, error code if something goes wrong. */
/* create (update) the last-known-bdev-info file */
extern int lk_bdev_save(const unsigned minor, const struct bdev_info *bd);
/* we may want to remove all stored information */
extern int lk_bdev_delete(const unsigned minor);
/* load info from that file.
 * caller should free(bd->bd_name) once it is no longer needed. */
extern int lk_bdev_load(const unsigned minor, struct bdev_info *bd);

extern void get_random_bytes(void* buffer, int len);

/* Since glibc 2.8~20080505-0ubuntu7 asprintf() is declared with the
   warn_unused_result attribute.... */
extern int m_asprintf(char **strp, const char *fmt, ...);

extern void fprintf_hex(FILE *fp, off_t file_offset, const void *buf, unsigned len);


extern unsigned long long m_strtoll(const char* s,const char def_unit);
extern int only_digits(const char *s);
extern int dt_lock_drbd(int minor);
extern void dt_unlock_drbd(int lock_fd);
extern int dt_minor_of_dev(const char *device);
extern void dt_print_gc(const uint32_t* gen_cnt);
extern void dt_pretty_print_gc(const uint32_t* gen_cnt);


#endif
