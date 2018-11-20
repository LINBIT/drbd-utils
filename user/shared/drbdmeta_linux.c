/*
   drbdmeta.c

   This file is part of DRBD by Philipp Reisner and Lars Ellenberg.

   Copyright (C) 2004-2008, LINBIT Information Technologies GmbH
   Copyright (C) 2004-2008, Philipp Reisner <philipp.reisner@linbit.com>
   Copyright (C) 2004-2008, Lars Ellenberg  <lars.ellenberg@linbit.com>

   drbd is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   drbd is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with drbd; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

 */

/* These are the low-level I/O functions for drbdmeta for the Linux
 * (and maybe later other POSIX-like) platforms.
 */

#define _GNU_SOURCE
#define _XOPEN_SOURCE 600
#define _FILE_OFFSET_BITS 64

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <sys/time.h>

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#include <linux/major.h>
#include <linux/kdev_t.h>
#include <linux/drbd.h>		/* only use DRBD_MAGIC from here! */
#include <linux/fs.h>           /* for BLKFLSBUF */

#include "drbd_endian.h"
#include "drbdtool_common.h"
#include "drbd_strings.h"
#include "drbd_meta_data.h"

#include "drbdmeta_parser.h"
#include "drbdmeta.h"

/* BLKZEROOUT, available on linux-3.6 and later,
 * and maybe backported to distribution kernels,
 * even if they pretend to be older.
 * Yes, we encountered a number of systems that already had it in their
 * kernels, but not yet in the headers used to build userland stuff like this.
 */

#ifndef BLKZEROOUT
# define BLKZEROOUT	_IO(0x12,127)
#endif

/* Do we want to exit() right here,
 * or do we want to duplicate the error handling everywhere? */
void pread_or_die(struct format *cfg, void *buf, size_t count, off_t offset, const char* tag)
{
	int fd = cfg->md_fd;
	ssize_t c = pread(fd, buf, count, offset);

	if (verbose >= 2) {
		fflush(stdout);
		fprintf(stderr, " %-26s: pread(%u, ...,%6lu,%12llu)\n", tag,
			fd, (unsigned long)count, (unsigned long long)offset);
		if (count & ((1<<12)-1))
			fprintf(stderr, "\tcount will cause EINVAL on hard sect size != 512\n");
		if (offset & ((1<<12)-1))
			fprintf(stderr, "\toffset will cause EINVAL on hard sect size != 512\n");
	}
	if (c < 0) {
		fprintf(stderr,"pread(%u,...,%lu,%llu) in %s failed: %s\n",
			fd, (unsigned long)count, (unsigned long long)offset,
			tag, strerror(errno));
		exit(10);
	} else if ((size_t)c != count) {
		fprintf(stderr,"confused in %s: expected to read %d bytes,"
			" actually read %d\n",
			tag, (int)count, (int)c);
		exit(10);
	}
	if (verbose > 10)
		fprintf_hex(stderr, offset, buf, count);
}

static unsigned n_writes = 0;
void pwrite_or_die(struct format *cfg, const void *buf, size_t count, off_t offset, const char* tag)
{
	int fd = cfg->md_fd;
	ssize_t c;

	validate_offsets_or_die(cfg, count, offset, tag);

	++n_writes;
	if (dry_run) {
		fprintf(stderr, " %-26s: pwrite(%u, ...,%6lu,%12llu) SKIPPED DUE TO DRY-RUN\n",
			tag, fd, (unsigned long)count, (unsigned long long)offset);
		if (verbose > 10)
			fprintf_hex(stderr, offset, buf, count);
		return;
	}
	c = pwrite(fd, buf, count, offset);
	if (verbose >= 2) {
		fflush(stdout);
		fprintf(stderr, " %-26s: pwrite(%u, ...,%6lu,%12llu)\n", tag,
			fd, (unsigned long)count, (unsigned long long)offset);
		if (count & ((1<<12)-1))
			fprintf(stderr, "\tcount will cause EINVAL on hard sect size != 512\n");
		if (offset & ((1<<12)-1))
			fprintf(stderr, "\toffset will cause EINVAL on hard sect size != 512\n");
	}
	if (c < 0) {
		fprintf(stderr,"pwrite(%u,...,%lu,%llu) in %s failed: %s\n",
			fd, (unsigned long)count, (unsigned long long)offset,
			tag, strerror(errno));
		exit(10);
	} else if ((size_t)c != count) {
		/* FIXME we might just now have corrupted the on-disk data */
		fprintf(stderr,"confused in %s: expected to write %d bytes,"
			" actually wrote %d\n", tag, (int)count, (int)c);
		exit(10);
	}
}

int v06_md_open(struct format *cfg)
{
	struct stat sb;

	cfg->md_fd = open(cfg->md_device_name, O_RDWR);

	if (cfg->md_fd == -1) {
		PERROR("open(%s) failed", cfg->md_device_name);
		return NO_VALID_MD_FOUND;
	}

	if (fstat(cfg->md_fd, &sb)) {
		PERROR("fstat() failed");
		return NO_VALID_MD_FOUND;
	}

	if (!S_ISREG(sb.st_mode)) {
		fprintf(stderr, "'%s' is not a plain file!\n",
			cfg->md_device_name);
		return NO_VALID_MD_FOUND;
	}

	if (cfg->ops->md_disk_to_cpu(cfg)) {
		return NO_VALID_MD_FOUND;
	}

	return VALID_MD_FOUND;
}

int generic_md_close(struct format *cfg)
{
	/* On /dev/ram0 we may not use O_SYNC for some kernels (eg. RHEL6 2.6.32),
	 * and fsync() returns EIO, too. So we don't do error checking here. */
	fsync(cfg->md_fd);
	if (close(cfg->md_fd)) {
		PERROR("close() failed");
		return -1;
	}
	return 0;
}

int zeroout_bitmap_fast(struct format *cfg)
{
	const size_t bitmap_bytes =
		ALIGN(bm_bytes(&cfg->md, cfg->bd_size >> 9), cfg->md_hard_sect_size);

	uint64_t range[2];
	int err;

	range[0] = cfg->bm_offset; /* start offset */
	range[1] = bitmap_bytes; /* len */

	err = ioctl(cfg->md_fd, BLKZEROOUT, &range);
	if (!err)
		return 0;

	PERROR("ioctl(%s, BLKZEROOUT, [%llu, %llu]) failed", cfg->md_device_name,
			(unsigned long long)range[0], (unsigned long long)range[1]);
	fprintf(stderr, "Using slow(er) fallback.\n");

	return -1;
}

int v07_style_md_open_device(struct format *cfg)
{
	struct stat sb;
	unsigned int hard_sect_size = 0;
	int ioctl_err;
	int open_flags = O_RDWR | O_DIRECT;

	/* For old-style fixed size indexed external meta data,
	 * we cannot really use O_EXCL, we have to trust the given minor.
	 *
	 * For internal, or "flexible" external meta data, we open O_EXCL to
	 * avoid accidentally damaging otherwise in-use data, just because
	 * someone had a typo in the command line.
	 */
	if (cfg->md_index < 0)
		open_flags |= O_EXCL;

 retry:
	cfg->md_fd = open(cfg->md_device_name, open_flags );

	if (cfg->md_fd == -1) {
		int save_errno = errno;
		PERROR("open(%s) failed", cfg->md_device_name);
		if (save_errno == EBUSY && (open_flags & O_EXCL)) {
			if ((!force && is_apply_al_cmd()) ||
			    !confirmed("Exclusive open failed. Do it anyways?"))
			{
				printf("Operation canceled.\n");
				exit(20);
			}
			open_flags &= ~O_EXCL;
			goto retry;
		}
		if (save_errno == EINVAL && (open_flags & O_DIRECT)) {
			/* shoo. O_DIRECT is not supported?
			 * retry, but remember this, so we can
			 * BLKFLSBUF appropriately */
			fprintf(stderr, "could not open with O_DIRECT, retrying without\n");
			open_flags &= ~O_DIRECT;
			opened_odirect = 0;
			goto retry;
		}
		exit(20);
	}

	if (fstat(cfg->md_fd, &sb)) {
		PERROR("fstat(%s) failed", cfg->md_device_name);
		exit(20);
	}

	if (!S_ISBLK(sb.st_mode)) {
		if (!force) {
			fprintf(stderr, "'%s' is not a block device!\n",
				cfg->md_device_name);
			exit(20);
		}
		cfg->bd_size = sb.st_size;
	}

	if (format_version(cfg) >= DRBD_V08) {
		ASSERT(cfg->md_index != DRBD_MD_INDEX_INTERNAL);
	}
	ioctl_err = ioctl(cfg->md_fd, BLKSSZGET, &hard_sect_size);
	if (ioctl_err) {
		fprintf(stderr, "ioctl(md_fd, BLKSSZGET) returned %d, "
			"assuming hard_sect_size is 512 Byte\n", ioctl_err);
		cfg->md_hard_sect_size = 512;
	} else {
		cfg->md_hard_sect_size = hard_sect_size;
		if (verbose >= 2)
			fprintf(stderr, "hard_sect_size is %d Byte\n",
				cfg->md_hard_sect_size);
	}

	if (!cfg->bd_size)
		cfg->bd_size = bdev_size(cfg->md_fd);

	if (!opened_odirect &&
	    (MAJOR(sb.st_rdev) != RAMDISK_MAJOR)) {
		ioctl_err = ioctl(cfg->md_fd, BLKFLSBUF);
		/* report error, but otherwise ignore.  we could not open
		 * O_DIRECT, it is a "strange" device anyways. */
		if (ioctl_err)
			fprintf(stderr, "ioctl(md_fd, BLKFLSBUF) returned %d, "
					"we may read stale data\n", ioctl_err);
	}

	return 0;
}
