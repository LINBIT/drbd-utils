/*
   drbdmeta_thin.c

   This file is part of DRBD by Philipp Reisner and Lars Ellenberg.

   Copyright (C) 2026, LINBIT HA-Solutions GmbH

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

/* The zero extents of cbt-dump on a dm-thin volume.  There is no dm-thin on
 * WinDRBD, drbdmeta_windrbd.c has stubs. */

#define _GNU_SOURCE
#define _XOPEN_SOURCE 600
#define _FILE_OFFSET_BITS 64

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <sys/sysmacros.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "drbdmeta.h"


/* On a dm-thin volume, the mappings of the thin pool tell which blocks read
 * as zeros.  The day0 bitmap counts a discard as a write, the thin pool
 * unmaps the blocks a discard covers.
 * The approach follows thin_send from thin-send-recv: reserve the metadata
 * snapshot of the pool, "thin_dump -m --dev-id", release the snapshot.  The
 * same lock file serializes it with thin_send, a pool has one metadata
 * snapshot only. */
#define THIN_LOCKFILE "/var/run/thin-send-recv.lock"

struct cbt_thin {
	FILE *f;		/* the thin_dump XML */
	uint64_t block_bytes;
	uint64_t begin, end;	/* the current mapped range, in bytes */
	bool eof;
	bool failed;		/* the XML is not what we expect */
	char *line;
	size_t line_size;
};

/* Run argv with stdout to out_fd, return its exit status or -1 */
static int thin_run(char *const argv[], int out_fd)
{
	sigset_t empty;
	int status;
	pid_t pid;

	pid = fork();
	if (pid < 0)
		return -1;
	if (pid == 0) {
		int null_fd = open("/dev/null", O_RDWR);

		sigemptyset(&empty);
		sigprocmask(SIG_SETMASK, &empty, NULL);
		if (null_fd >= 0) {
			dup2(null_fd, 0);
			if (out_fd < 0)
				dup2(null_fd, 1);
		}
		if (out_fd >= 0)
			dup2(out_fd, 1);
		execvp(argv[0], argv);
		_exit(127);
	}
	while (waitpid(pid, &status, 0) < 0) {
		if (errno != EINTR)
			return -1;
	}
	return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

/* "dmsetup" failed with exit status err */
static void thin_dmsetup_failed(const char *what, int err)
{
	if (err == 127)
		fprintf(stderr, "Cannot run dmsetup: is device-mapper installed?\n");
	else
		fprintf(stderr, "%s\n", what);
}

/* The first line of "dmsetup table" of a device in *line.  Returns the exit
 * status of dmsetup, or -1. */
static int thin_dm_table(unsigned int maj, unsigned int min, char **line)
{
	char smaj[16], smin[16];
	char *argv[] = { "dmsetup", "table", "-j", smaj, "-m", smin, NULL };
	size_t size = 0;
	FILE *f = tmpfile();
	int err = -1;

	*line = NULL;
	if (!f)
		return -1;
	snprintf(smaj, sizeof(smaj), "%u", maj);
	snprintf(smin, sizeof(smin), "%u", min);
	err = thin_run(argv, fileno(f));
	if (!err && (fseek(f, 0, SEEK_SET) || getline(line, &size, f) < 0))
		err = -1;
	if (err) {
		free(*line);
		*line = NULL;
	}
	fclose(f);
	return err;
}

/* The device mapper name of a device, NULL if it is not one */
static char *thin_dm_name(unsigned int maj, unsigned int min)
{
	char path[64], name[256];
	FILE *f;

	snprintf(path, sizeof(path), "/sys/dev/block/%u:%u/dm/name", maj, min);
	f = fopen(path, "r");
	if (!f)
		return NULL;
	if (!fgets(name, sizeof(name), f)) {
		fclose(f);
		return NULL;
	}
	fclose(f);
	name[strcspn(name, "\n")] = '\0';
	return name[0] ? strdup(name) : NULL;
}

static bool thin_is_dev(const char *path, unsigned int maj, unsigned int min)
{
	struct stat sb;

	return !stat(path, &sb) && S_ISBLK(sb.st_mode) &&
		major(sb.st_rdev) == maj && minor(sb.st_rdev) == min;
}

/* A path of the block device maj:min: the udev link /dev/block/maj:min,
 * which exists for any block device, else /dev/mapper/<dm name>.  NULL if
 * neither exists. */
static char *thin_dev_path(unsigned int maj, unsigned int min)
{
	char *path = NULL, *name;

	if (asprintf(&path, "/dev/block/%u:%u", maj, min) < 0)
		return NULL;
	if (thin_is_dev(path, maj, min))
		return path;
	free(path);
	path = NULL;
	name = thin_dm_name(maj, min);
	if (name && asprintf(&path, "/dev/mapper/%s", name) < 0)
		path = NULL;
	free(name);
	if (path && !thin_is_dev(path, maj, min)) {
		free(path);
		path = NULL;
	}
	return path;
}

/* Dump the mappings of the thin volume "dev" into a temporary file.  Returns
 * NULL if dev is not a thin volume, or if its mappings are not available. */
static FILE *thin_dump_mappings(const char *dev)
{
	unsigned int pool_maj, pool_min, meta_maj, meta_min, dev_id;
	char *table = NULL, *pool_table = NULL, *pool = NULL;
	char *meta_path = NULL, s_dev_id[16], origin[64];
	char *reserve[] = { "dmsetup", "message", NULL, "0", "reserve_metadata_snap", NULL };
	char *release[] = { "dmsetup", "message", NULL, "0", "release_metadata_snap", NULL };
	char *dump[] = { "thin_dump", "-m", "--dev-id", s_dev_id, NULL, NULL };
	sigset_t block, old;
	struct stat sb;
	FILE *f = NULL;
	int lock_fd = -1, err, n;

	if (stat(dev, &sb) || !S_ISBLK(sb.st_mode))
		return NULL;
	pool = thin_dm_name(major(sb.st_rdev), minor(sb.st_rdev));
	if (!pool)
		return NULL; /* not a device mapper device */
	free(pool);
	pool = NULL;
	err = thin_dm_table(major(sb.st_rdev), minor(sb.st_rdev), &table);
	if (err) {
		thin_dmsetup_failed("dmsetup table failed: is it a thin volume?", err);
		goto fail;
	}
	/* <start> <length> thin <pool dev> <dev id> [<external origin dev>] */
	n = sscanf(table, "%*u %*u thin %u:%u %u %63s",
		   &pool_maj, &pool_min, &dev_id, origin);
	if (n < 3)
		goto out;
	if (n == 4) {
		fprintf(stderr, "The thin volume has an external origin: "
			"taking the zero extents from the day0 bitmap\n");
		goto out;
	}
	pool = thin_dm_name(pool_maj, pool_min);
	err = thin_dm_table(pool_maj, pool_min, &pool_table);
	if (err) {
		thin_dmsetup_failed("dmsetup table of the thin pool failed", err);
		goto fail;
	}
	/* <start> <length> thin-pool <metadata dev> <data dev> ... */
	if (!pool ||
	    sscanf(pool_table, "%*u %*u thin-pool %u:%u", &meta_maj, &meta_min) != 2)
		goto fail;
	meta_path = thin_dev_path(meta_maj, meta_min);
	if (!meta_path) {
		fprintf(stderr, "No device node for the metadata device %u:%u of %s\n",
			meta_maj, meta_min, pool);
		goto fail;
	}
	snprintf(s_dev_id, sizeof(s_dev_id), "%u", dev_id);
	reserve[2] = release[2] = pool;
	dump[4] = meta_path;

	f = tmpfile();
	if (!f) {
		fprintf(stderr, "Cannot create a temporary file: %m\n");
		goto fail;
	}
	lock_fd = open(THIN_LOCKFILE, O_CREAT | O_RDONLY | O_CLOEXEC, 0644);
	if (lock_fd < 0 || flock(lock_fd, LOCK_EX)) {
		fprintf(stderr, "Cannot lock %s: %m\n", THIN_LOCKFILE);
		goto fail;
	}
	/* Do not leave the metadata snapshot reserved when interrupted. */
	sigemptyset(&block);
	sigaddset(&block, SIGINT);
	sigaddset(&block, SIGTERM);
	sigaddset(&block, SIGHUP);
	sigaddset(&block, SIGPIPE);
	sigprocmask(SIG_BLOCK, &block, &old);
	err = thin_run(reserve, -1);
	if (err) {
		sigprocmask(SIG_SETMASK, &old, NULL);
		if (err == 127)
			thin_dmsetup_failed(NULL, err);
		else
			fprintf(stderr, "Cannot reserve the metadata snapshot of %s; if nothing\n"
				"else uses it, free it with \"dmsetup message %s 0 release_metadata_snap\"\n",
				pool, pool);
		goto fail;
	}
	err = thin_run(dump, fileno(f));
	thin_run(release, -1);
	sigprocmask(SIG_SETMASK, &old, NULL);
	if (err) {
		fprintf(stderr, "%s failed%s\n", "thin_dump",
			err == 127 ? ": is thin-provisioning-tools installed?" : "");
		goto fail;
	}
	if (fseek(f, 0, SEEK_SET))
		goto fail;
	if (verbose)
		fprintf(stderr, "Zero extents from the mappings of thin device %u in pool %s\n",
			dev_id, pool);
	goto out;

fail:
	fprintf(stderr, "Taking the zero extents from the day0 bitmap\n");
	if (f)
		fclose(f);
	f = NULL;
out:
	if (lock_fd >= 0)
		close(lock_fd);
	free(table);
	free(pool_table);
	free(pool);
	free(meta_path);
	return f;
}

/* The unsigned value of attribute "name" in an XML line of thin_dump */
static bool thin_attr(const char *line, const char *name, uint64_t *val)
{
	size_t len = strlen(name);
	const char *p = line;
	char *end;

	while ((p = strstr(p, name))) {
		if (p > line && p[-1] == ' ' && p[len] == '=' && p[len + 1] == '"') {
			*val = strtoull(p + len + 2, &end, 10);
			return *end == '"';
		}
		p += len;
	}
	return false;
}

/* The XML is not what we expect.  Never skip what we do not know: a <ref> of
 * thin_dump >= 1.0 stands for shared mappings, skipping it would report
 * mapped blocks as zero. */
static void thin_fail(struct cbt_thin *thin)
{
	if (!thin->failed)
		fprintf(stderr, "Unexpected thin_dump output: %s", thin->line);
	thin->failed = true;
	thin->eof = true;
}

/* Move to the next mapped range */
static void thin_next(struct cbt_thin *thin)
{
	uint64_t begin, length, prev_end = thin->end;

	if (thin->eof)
		return;
	while (getline(&thin->line, &thin->line_size, thin->f) >= 0) {
		bool ok;

		if (strstr(thin->line, "<range_mapping ")) {
			ok = thin_attr(thin->line, "origin_begin", &begin) &&
			     thin_attr(thin->line, "length", &length);
		} else if (strstr(thin->line, "<single_mapping ")) {
			ok = thin_attr(thin->line, "origin_block", &begin);
			length = 1;
		} else if (strstr(thin->line, "</device>")) {
			break;
		} else {
			ok = false;
		}
		if (ok) {
			thin->begin = begin * thin->block_bytes;
			thin->end = (begin + length) * thin->block_bytes;
			ok = thin->begin >= prev_end;
		}
		if (!ok)
			thin_fail(thin);
		return;
	}
	thin->eof = true;
}

/* Read the head of the thin_dump XML, up to the first mapping.  False if the
 * XML or the pool cannot be trusted. */
static bool thin_open(struct cbt_thin *thin)
{
	uint64_t sectors, flags;

	thin->begin = thin->end = 0;
	thin->eof = thin->failed = false;
	if (fseek(thin->f, 0, SEEK_SET) ||
	    getline(&thin->line, &thin->line_size, thin->f) < 0 ||
	    !strstr(thin->line, "<superblock ") ||
	    !thin_attr(thin->line, "data_block_size", &sectors) || !sectors) {
		thin_fail(thin);
		return false;
	}
	/* thin_dump sets flags="1" when the pool metadata needs a check */
	if (thin_attr(thin->line, "flags", &flags) && (flags & 1)) {
		fprintf(stderr, "The metadata of the thin pool needs a check\n");
		thin->failed = thin->eof = true;
		return false;
	}
	thin->block_bytes = sectors * 512;
	if (getline(&thin->line, &thin->line_size, thin->f) < 0 ||
	    !strstr(thin->line, "<device ")) {
		thin_fail(thin);
		return false;
	}
	thin_next(thin);
	return !thin->failed;
}

/* Read all the XML once, before any output, so that unexpected XML can fall
 * back to the day0 bitmap.  Then rewind to the first mapping. */
static bool thin_check(struct cbt_thin *thin)
{
	if (!thin_open(thin))
		return false;
	while (!thin->eof)
		thin_next(thin);
	return !thin->failed && thin_open(thin);
}

/* Is a block of the thin volume between start and end mapped? */
static bool thin_mapped(struct cbt_thin *thin, uint64_t start, uint64_t end)
{
	while (!thin->eof && thin->end <= start)
		thin_next(thin);
	if (thin->failed)
		exit(20); /* thin_check() read the same XML without failing */
	return !thin->eof && thin->begin < end;
}

/* The bits of word k with no mapped block of the thin volume */
uint32_t cbt_thin_zero(struct cbt_thin *thin, uint64_t bm_bytes_per_bit, uint64_t k)
{
	uint64_t start = k * 32 * bm_bytes_per_bit, end = start + 32 * bm_bytes_per_bit;
	uint32_t zero = 0;
	unsigned int b;

	if (!thin_mapped(thin, start, end))
		return ~0U;
	if (thin->begin <= start && thin->end >= end)
		return 0;
	for (b = 0; b < 32; b++, start += bm_bytes_per_bit) {
		if (!thin_mapped(thin, start, start + bm_bytes_per_bit))
			zero |= 1U << b;
	}
	return zero;
}

static struct cbt_thin *thin_new(FILE *f)
{
	struct cbt_thin *thin = calloc(1, sizeof(*thin));

	if (!thin) {
		fprintf(stderr, "Out of memory\n");
		exit(20);
	}
	thin->f = f;
	return thin;
}

/* The mappings of the thin volume "dev", NULL if it is not a thin volume or
 * its mappings are not available or not as expected */
struct cbt_thin *cbt_thin_open_device(const char *dev)
{
	FILE *f = thin_dump_mappings(dev);
	struct cbt_thin *thin;

	if (!f)
		return NULL;
	thin = thin_new(f);
	if (!thin_check(thin)) {
		fprintf(stderr, "Taking the zero extents from the day0 bitmap\n");
		cbt_thin_close(thin);
		return NULL;
	}
	return thin;
}

/* The mappings from a file with the output of "thin_dump -m --dev-id" */
struct cbt_thin *cbt_thin_open_file(const char *path)
{
	FILE *f = fopen(path, "r");
	struct cbt_thin *thin;

	if (!f) {
		fprintf(stderr, "%s: %m\n", path);
		exit(20);
	}
	thin = thin_new(f);
	if (!thin_check(thin))
		exit(20);
	return thin;
}

void cbt_thin_close(struct cbt_thin *thin)
{
	if (!thin)
		return;
	fclose(thin->f);
	free(thin->line);
	free(thin);
}
