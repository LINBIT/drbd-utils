#ifndef __DRBD_META_H
#define __DRBD_META_H

#include "config.h"
#include <sys/types.h>
#include "shared_tool.h"
#include <linux/drbd.h>

#ifdef WINDRBD
#include <windows.h>		/* for HANDLE */
#endif

enum md_format {
	DRBD_V06,
	DRBD_V07,
	DRBD_V08,
	DRBD_V09,
	DRBD_UNKNOWN,
};

/* return codes for md_open */
enum {
	VALID_MD_FOUND = 0,
	NO_VALID_MD_FOUND = -1,
	VALID_MD_FOUND_AT_LAST_KNOWN_LOCATION = -2,
};

/* let gcc help us get it right.
 * some explicit endian types */
typedef struct { uint64_t le; } le_u64;
typedef struct { uint64_t be; } be_u64;
typedef struct { uint32_t le; } le_u32;
typedef struct { uint32_t be; } be_u32;
typedef struct { int32_t be; } be_s32;
typedef struct { uint16_t be; } be_u16;
typedef struct { unsigned long le; } le_ulong;
typedef struct { unsigned long be; } be_ulong;

/* NOTE that this structure does not need to be packed,
 * aligned, nor does it need to be in the same order as the on_disk variants.
 */
struct peer_md_cpu {
	uint64_t bitmap_uuid;
	uint64_t bitmap_dagtag;
	uint32_t flags;
	int32_t bitmap_index;
};

struct md_cpu {
	uint64_t current_uuid;
	uint64_t history_uuids[HISTORY_UUIDS];
	/* present since drbd 0.6 */
	uint32_t gc[GEN_CNT_SIZE];	/* generation counter */
	uint32_t magic;
	/* added in drbd 0.7;
	 * 0.7 stores effevtive_size on disk as kb, 0.8 in units of sectors.
	 * we use sectors in our general working structure here */
	uint64_t effective_size;	/* last agreed size */
	uint32_t md_size_sect;
	int32_t al_offset;		/* signed sector offset to this block */
	uint32_t al_nr_extents;	/* important for restoring the AL */
	int32_t bm_offset;		/* signed sector offset to the bitmap, from here */
	/* Since DRBD 0.8 we have uuid instead of gc */
	uint32_t flags;
	uint64_t device_uuid;
	uint32_t bm_bytes_per_bit;
	uint32_t la_peer_max_bio_size;
	/* Since DRBD 9.0 the following new stuff: */
	uint32_t max_peers;
	int32_t node_id;
	struct peer_md_cpu peers[DRBD_PEERS_MAX];
	uint32_t al_stripes;
	uint32_t al_stripe_size_4k;
};

/*
 * drbdmeta specific types
 */

struct format_ops;

struct format {
	const struct format_ops *ops;
	char *md_device_name;	/* well, in 06 it is file name */
	char *drbd_dev_name;
	unsigned minor;		/* cache, determined from drbd_dev_name */
	int lock_fd;
	int drbd_fd;		/* no longer used!   */
	int ll_fd;		/* not yet used here */
#ifdef WINDRBD
	HANDLE disk_handle;
#else
	int md_fd;
#endif

	int md_hard_sect_size;


	/* unused in 06 */
	int md_index;
	unsigned int bm_bytes;
	unsigned int bits_set;	/* 32 bit should be enough. @4k ==> 16TB */
	int bits_counted:1;
	int update_lk_bdev:1;	/* need to update the last known bdev info? */

	struct md_cpu md;

	/* _byte_ offsets of our "super block" and other data, within fd */
	uint64_t md_offset;
	uint64_t al_offset;
	uint64_t bm_offset;

	/* if create_md actually does convert,
	 * we want to wipe the old meta data block _after_ convertion. */
	uint64_t wipe_fixed;
	uint64_t wipe_flex;
	uint64_t wipe_resize;

	/* convenience */
	uint64_t bd_size; /* size of block device for internal meta data */

	/* size limit due to available on-disk bitmap */
	uint64_t max_usable_sect;

	/* last-known bdev info,
	 * to increase the chance of finding internal meta data in case the
	 * lower level device has been resized without telling DRBD.
	 * Loaded from file for internal metadata */
	struct bdev_info lk_bd;
};

/* - parse is expected to exit() if it does not work out.
 * - open is expected to read the respective on_disk members,
 *   and copy the "superblock" meta data into the struct mem_cpu
 * FIXME describe rest of them, and when they should exit,
 * return error or success.
 */
struct format_ops {
	const char *name;
	char **args;
	int (*parse) (struct format *, char **, int, int *);
	int (*open) (struct format *);
	int (*close) (struct format *);
	int (*md_initialize) (struct format *, int do_disk_writes, int max_peers);
	int (*md_disk_to_cpu) (struct format *);
	int (*md_cpu_to_disk) (struct format *);
	void (*get_gi) (struct md_cpu *md, int node_id);
	void (*show_gi) (struct md_cpu *md, int node_id);
	void (*set_gi) (struct md_cpu *md, int node_id, char **argv, int argc);
	int (*outdate_gi) (struct md_cpu *md);
	int (*invalidate_gi) (struct md_cpu *md);
};

/*
 * I think this block of declarations and definitions should be
 * in some common.h, too.
 * {
 */

#ifndef ALIGN
# define ALIGN(x,a) ( ((x) + (a)-1) &~ ((a)-1) )
#endif

#if 0
#define ASSERT(x) ((void)(0))
#define d_expect(x) (x)
#else
#define ASSERT(x) do { if (!(x)) {				\
	fprintf(stderr, "%s:%u:%s: ASSERT(%s) failed.\n",	\
		__FILE__ , __LINE__ , __func__ , #x );		\
	abort(); }						\
	} while (0)
#define d_expect(x) ({						\
	int _x = (x);						\
	if (!_x)						\
		fprintf(stderr, "%s:%u:%s: ASSERT(%s) failed.\n",\
			__FILE__ , __LINE__ , __func__ , #x );	\
	_x; })
#endif

	/* Those have to be implemented by the platform dependent
	 * I/O driver.
	 */

void pread_or_die(struct format *cfg, void *buf, size_t count, off_t offset, const char* tag);
void pwrite_or_die(struct format *cfg, const void *buf, size_t count, off_t offset, const char* tag);

int v06_md_open(struct format *cfg);
int generic_md_close(struct format *cfg);
int zeroout_bitmap_fast(struct format *cfg);
int v07_style_md_open_device(struct format *cfg);

	/* Symbols in drbdmeta.c used by drivers */
void validate_offsets_or_die(struct format *cfg, size_t count, off_t offset, const char* tag);
int is_apply_al_cmd(void);
int confirmed(const char *text);
unsigned long bm_bytes(const struct md_cpu * const md, uint64_t sectors);
enum md_format format_version(struct format *cfg);

extern int opened_odirect;
extern int verbose;
extern int dry_run;
extern int force;

#endif
