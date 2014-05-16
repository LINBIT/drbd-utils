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
#include <netdb.h>

#include "linux/drbd_config.h"
#include "drbdtool_common.h"
#include "config.h"

static struct version __drbd_driver_version = {};
static struct version __drbd_utils_version = {};

/* In-place unescape double quotes and backslash escape sequences from a
 * double quoted string. Note: backslash is only useful to quote itself, or
 * double quote, no special treatment to any c-style escape sequences. */
void unescape(char *txt)
{
	char *ue, *e;
	e = ue = txt;
	for (;;) {
		if (*ue == '"') {
			ue++;
			continue;
		}
		if (*ue == '\\')
			ue++;
		if (!*ue)
			break;
		*e++ = *ue++;
	}
	*e = '\0';
}


/* input size is expected to be in KB */
char *ppsize(char *buf, unsigned long long size)
{
	/* Needs 9 bytes at max including trailing NUL:
	 * -1ULL ==> "16384 EB" */
	static char units[] = { 'K', 'M', 'G', 'T', 'P', 'E' };
	int base = 0;
	while (size >= 10000 && base < sizeof(units)-1) {
		/* shift + round */
		size = (size >> 10) + !!(size & (1<<9));
		base++;
	}
	sprintf(buf, "%u %cB", (unsigned)size, units[base]);

	return buf;
}

const char *make_optstring(struct option *options)
{
	static char buffer[200];
	char seen[256];
	struct option *opt;
	char *c;

	memset(seen, 0, sizeof(seen));
	opt = options;
	c = buffer;
	while (opt->name) {
		if (0 < opt->val && opt->val < 256) {
			if (seen[opt->val]++) {
				fprintf(stderr, "internal error: --%s has duplicate opt->val '%c'\n",
						opt->name, opt->val);
				abort();
			}
			*c++ = opt->val;
			if (opt->has_arg != no_argument) {
				*c++ = ':';
				if (opt->has_arg == optional_argument)
					*c++ = ':';
			}
		}
		opt++;
	}

	*c = 0;
	return buffer;
}

enum new_strtoll_errs {
	MSE_OK,
	MSE_DEFAULT_UNIT,
	MSE_MISSING_NUMBER,
	MSE_INVALID_NUMBER,
	MSE_INVALID_UNIT,
	MSE_OUT_OF_RANGE,
};

static enum new_strtoll_errs
new_strtoll(const char *s, const char def_unit, unsigned long long *rv)
{
	char unit = 0;
	char dummy = 0;
	int shift, c;

	switch (def_unit) {
	default:
		return MSE_DEFAULT_UNIT;
	case 0:
	case 1:
	case '1':
		shift = 0;
		break;
	case 'K':
	case 'k':
		shift = -10;
		break;
	case 's':
		shift = -9;   // sectors
		break;
		/*
		  case 'M':
		  case 'm':
		  case 'G':
		  case 'g':
		*/
	}

	if (!s || !*s) return MSE_MISSING_NUMBER;

	c = sscanf(s, "%llu%c%c", rv, &unit, &dummy);

	if (c != 1 && c != 2) return MSE_INVALID_NUMBER;

	switch (unit) {
	case 0:
		return MSE_OK;
	case 'K':
	case 'k':
		shift += 10;
		break;
	case 'M':
	case 'm':
		shift += 20;
		break;
	case 'G':
	case 'g':
		shift += 30;
		break;
	case 's':
		shift += 9;
		break;
	default:
		return MSE_INVALID_UNIT;
	}

	/* if shift is negative (e.g. default unit 'K', actual unit 's'),
	 * convert to positive, and shift right, rounding up. */
	if (shift < 0) {
		shift = -shift;
		*rv = (*rv + (1ULL << shift) - 1) >> shift;
		return MSE_OK;
	}

	/* if shift is positive, first check for overflow */
	if (*rv > (~0ULL >> shift))
		return MSE_OUT_OF_RANGE;

	/* then convert */
	*rv = *rv << shift;
	return MSE_OK;
}

unsigned long long
m_strtoll(const char *s, const char def_unit)
{
	unsigned long long r;

	switch(new_strtoll(s, def_unit, &r)) {
	case MSE_OK:
		return r;
	case MSE_DEFAULT_UNIT:
		fprintf(stderr, "unexpected default unit: %d\n",def_unit);
		exit(100);
	case MSE_MISSING_NUMBER:
		fprintf(stderr, "missing number argument\n");
		exit(100);
	case MSE_INVALID_NUMBER:
		fprintf(stderr, "%s is not a valid number\n", s);
		exit(20);
	case MSE_INVALID_UNIT:
		fprintf(stderr, "%s is not a valid number\n", s);
		exit(20);
	case MSE_OUT_OF_RANGE:
		fprintf(stderr, "%s: out of range\n", s);
		exit(20);
	default:
		fprintf(stderr, "m_stroll() is confused\n");
		exit(20);
	}
}

void alarm_handler(int __attribute((unused)) signo)
{ /* nothing. just interrupt F_SETLKW */ }

/* it is implicitly unlocked when the process dies.
 * but if you want to explicitly unlock it, just close it. */
int unlock_fd(int fd)
{
	return close(fd);
}

int get_fd_lockfile_timeout(const char *path, int seconds)
{
    int fd, err;
    struct sigaction sa,so;
    struct flock fl = {
	.l_type = F_WRLCK,
	.l_whence = 0,
	.l_start = 0,
	.l_len = 0
    };

    if ((fd = open(path, O_RDWR | O_CREAT, 0600)) < 0) {
	fprintf(stderr,"open(%s): %m\n",path);
	return -1;
    }

    if (seconds) {
	sa.sa_handler=alarm_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags=0;
	sigaction(SIGALRM,&sa,&so);
	alarm(seconds);
	err = fcntl(fd,F_SETLKW,&fl);
	if (err) err = errno;
	alarm(0);
	sigaction(SIGALRM,&so,NULL);
    } else {
	err = fcntl(fd,F_SETLK,&fl);
	if (err) err = errno;
    }

    if (!err) return fd;

    if (err != EINTR && err != EAGAIN) {
	close(fd);
	errno = err;
	fprintf(stderr,"fcntl(%s,...): %m\n", path);
	return -1;
    }

    /* do we want to know this? */
    if (!fcntl(fd,F_GETLK,&fl)) {
	fprintf(stderr,"lock on %s currently held by pid:%u\n",
		path, fl.l_pid);
    }
    close(fd);
    return -1;
}

int dt_minor_of_dev(const char *device)
{
	struct stat sb;
	long m;
	int digits_only = only_digits(device);
	const char *c = device;

	/* On udev/devfs based system the device nodes does not
	 * exist before the drbd is created.
	 *
	 * If the device name starts with /dev/drbd followed by
	 * only digits, or if only digits are given,
	 * those digits are the minor number.
	 *
	 * Otherwise, we cannot reliably determine the minor number!
	 *
	 * We allow "arbitrary" device names in drbd.conf,
	 * and those may well contain digits.
	 * Interpreting any digits as minor number is dangerous!
	 */
	if (!digits_only) {
		if (!strncmp("/dev/drbd", device, 9) &&
		    only_digits(device + 9))
			c = device + 9;

		/* if the device node exists,
		 * and is a block device with the correct major,
		 * do not enforce further naming conventions.
		 * people without udev, and not using drbdadm
		 * may do whatever they like. */
		else if (!stat(device,&sb) &&
			 S_ISBLK(sb.st_mode) &&
			 major(sb.st_rdev) == LANANA_DRBD_MAJOR)
			return minor(sb.st_rdev);

		else
			return -1;
	}

	/* ^[0-9]+$ or ^/dev/drbd[0-9]+$ */

	errno = 0;
	m = strtol(c, NULL, 10);
	if (!errno)
		return m;

	return -1;
}

int only_digits(const char *s)
{
	const char *c;
	for (c = s; isdigit(*c); c++)
		;
	return c != s && *c == 0;
}

int dt_lock_drbd(int minor)
{
	int sz, lfd;
	char *lfname;

	/* THINK.
	 * maybe we should also place a fcntl lock on the
	 * _physical_device_ we open later...
	 *
	 * This lock is to prevent a drbd minor from being configured
	 * by drbdsetup while drbdmeta is about to mess with its meta data.
	 *
	 * If you happen to mess with the meta data of one device,
	 * pretending it belongs to an other, you'll screw up completely.
	 *
	 * We should store something in the meta data to detect such abuses.
	 */

	/* NOTE that /var/lock/drbd-*-* may not be "secure",
	 * maybe we should rather use /var/lock/drbd/drbd-*-*,
	 * and make sure that /var/lock/drbd is drwx.-..-. root:root  ...
	 */

	sz = asprintf(&lfname, DRBD_LOCK_DIR "/drbd-%d-%d",
		      LANANA_DRBD_MAJOR, minor);
	if (sz < 0) {
		perror("");
		exit(20);
	}

	lfd = get_fd_lockfile_timeout(lfname, 1);
	free (lfname);
	if (lfd < 0)
		exit(20);
	return lfd;
}

/* ignore errors */
void dt_unlock_drbd(int lock_fd)
{
	if (lock_fd >= 0)
		unlock_fd(lock_fd);
}

void dt_print_gc(const uint32_t* gen_cnt)
{
	printf("%d:%d:%d:%d:%d:%d:%d:%d\n",
	       gen_cnt[Flags] & MDF_CONSISTENT ? 1 : 0,
	       gen_cnt[HumanCnt],
	       gen_cnt[TimeoutCnt],
	       gen_cnt[ConnectedCnt],
	       gen_cnt[ArbitraryCnt],
	       gen_cnt[Flags] & MDF_PRIMARY_IND ? 1 : 0,
	       gen_cnt[Flags] & MDF_CONNECTED_IND ? 1 : 0,
	       gen_cnt[Flags] & MDF_FULL_SYNC ? 1 : 0);
}

void dt_pretty_print_gc(const uint32_t* gen_cnt)
{
	printf("\n"
	       "                                        WantFullSync |\n"
	       "                                  ConnectedInd |     |\n"
	       "                               lastState |     |     |\n"
	       "                      ArbitraryCnt |     |     |     |\n"
	       "                ConnectedCnt |     |     |     |     |\n"
	       "            TimeoutCnt |     |     |     |     |     |\n"
	       "        HumanCnt |     |     |     |     |     |     |\n"
	       "Consistent |     |     |     |     |     |     |     |\n"
	       "   --------+-----+-----+-----+-----+-----+-----+-----+\n"
	       "       %3s | %3d | %3d | %3d | %3d | %3s | %3s | %3s  \n"
	       "\n",
	       gen_cnt[Flags] & MDF_CONSISTENT ? "1/c" : "0/i",
	       gen_cnt[HumanCnt],
	       gen_cnt[TimeoutCnt],
	       gen_cnt[ConnectedCnt],
	       gen_cnt[ArbitraryCnt],
	       gen_cnt[Flags] & MDF_PRIMARY_IND ? "1/p" : "0/s",
	       gen_cnt[Flags] & MDF_CONNECTED_IND ? "1/c" : "0/n",
	       gen_cnt[Flags] & MDF_FULL_SYNC ? "1/y" : "0/n");
}

void dt_print_uuids(const uint64_t* uuid, unsigned int flags)
{
	int i;
	printf(X64(016)":"X64(016)":",
	       uuid[UI_CURRENT],
	       uuid[UI_BITMAP]);
	for ( i=UI_HISTORY_START ; i<=UI_HISTORY_END ; i++ ) {
		printf(X64(016)":", uuid[i]);
	}
	printf("%d:%d:%d:%d:%d:%d:%d\n",
	       flags & MDF_CONSISTENT ? 1 : 0,
	       flags & MDF_WAS_UP_TO_DATE ? 1 : 0,
	       flags & MDF_PRIMARY_IND ? 1 : 0,
	       flags & MDF_CONNECTED_IND ? 1 : 0,
	       flags & MDF_FULL_SYNC ? 1 : 0,
	       flags & MDF_PEER_OUT_DATED ? 1 : 0,
	       flags & MDF_CRASHED_PRIMARY ? 1 : 0);
}

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
	printf("meta-data: %s\n", (flags & MDF_AL_CLEAN) ? "clean" : "need apply-al");
}

void dt_print_v9_uuids(const uint64_t* uuid, unsigned int mdf_flags, unsigned int mdf_peer_flags)
{
	int i;
	printf(X64(016)":"X64(016)":",
	       uuid[UI_CURRENT],
	       uuid[UI_BITMAP]);
	for ( i=UI_HISTORY_START ; i<=UI_HISTORY_END ; i++ ) {
		printf(X64(016)":", uuid[i]);
	}
	printf("%d:%d:%d:%d:%d:%d",
	       mdf_flags & MDF_CONSISTENT ? 1 : 0,
	       mdf_flags & MDF_WAS_UP_TO_DATE ? 1 : 0,
	       mdf_flags & MDF_PRIMARY_IND ? 1 : 0,
	       mdf_flags & MDF_CRASHED_PRIMARY ? 1 : 0,
	       mdf_flags & MDF_AL_CLEAN ? 1 : 0,
	       mdf_flags & MDF_AL_DISABLED ? 1 : 0);
	printf(":%d:%d:%d:%d\n",
	       mdf_peer_flags & MDF_PEER_CONNECTED ? 1 : 0,
	       mdf_peer_flags & MDF_PEER_OUTDATED ? 1 : 0,
	       mdf_peer_flags & MDF_PEER_FENCING ? 1 : 0,
	       mdf_peer_flags & MDF_PEER_FULL_SYNC ? 1 : 0);
}

void dt_pretty_print_v9_uuids(const uint64_t* uuid, unsigned int mdf_flags, unsigned int mdf_peer_flags)
{
	printf(
"\n"
"       +--<  Current data generation UUID  >-\n"
"       |               +--<  Bitmap's base data generation UUID  >-\n"
"       |               |                 +--<  younger history UUID  >-\n"
"       |               |                 |         +-<  older history  >-\n"
"       V               V                 V         V\n");
	dt_print_v9_uuids(uuid, mdf_flags, mdf_peer_flags);
	printf(
"                                                                    ^ ^ ^ ^ ^ ^ ^ ^ ^ ^\n"
"                                      -<  Data consistency flag  >--+ | | | | | | | | |\n"
"                             -<  Data was/is currently up-to-date  >--+ | | | | | | | |\n"
"                                  -<  Node was/is currently primary  >--+ | | | | | | |\n"
" -<  This node was a crashed primary, and has not seen its peer since  >--+ | | | | | |\n"
"             -<  The activity-log was applied, the disk can be attached  >--+ | | | | |\n"
"        -<  The activity-log was disabled, peer is completely out of sync  >--+ | | | |\n"
"                                        -<  Node was/is currently connected  >--+ | | |\n"
"                            -<  The peer's disk was out-dated or inconsistent  >--+ | |\n"
"                               -<   A fence policy other the dont-care was used  >--+ |\n"
"                -<  Node was in the progress of marking all blocks as out of sync  >--+\n"
"\n");
}

/*    s: token buffer
 * size: size of s, _including_ the terminating NUL
 * stream: to read from.
 * s is guaranteed to be NUL terminated
 * if a token (including the NUL) needs more size bytes,
 * s will contain only a truncated token, and the next call will
 * return the next size-1 non-white-space bytes of stream.
 */
int fget_token(char *s, int size, FILE* stream)
{
	int c;
	char* sp = s;

	*sp = 0; /* terminate even if nothing is found */
	--size;  /* account for the terminating NUL */
	do { // eat white spaces in front.
		c = getc(stream);
		if( c == EOF) return EOF;
	} while (!isgraph(c));

	do { // read the first word into s
		*sp++ = c;
		c = getc(stream);
		if ( c == EOF) break;
	} while (isgraph(c) && --size);

	*sp=0;
	return 1;
}

int sget_token(char *s, int size, const char** text)
{
	int c;
	char* sp = s;

	*sp = 0; /* terminate even if nothing is found */
	--size;  /* account for the terminating NUL */
	do { // eat white spaces in front.
		c = *(*text)++;
		if( c == 0) return EOF;
	} while (!isgraph(c));

	do { // read the first word into s
		*sp++ = c;
		c = *(*text)++;
		if ( c == 0) break;
	} while (isgraph(c) && --size);

	*sp=0;
	return 1;
}

uint64_t bdev_size(int fd)
{
	uint64_t size64;		/* size in byte. */
	long size;		/* size in sectors. */
	int err;

	err = ioctl(fd, BLKGETSIZE64, &size64);
	if (err) {
		if (errno == EINVAL) {
			printf("INFO: falling back to BLKGETSIZE\n");
			err = ioctl(fd, BLKGETSIZE, &size);
			if (err) {
				perror("ioctl(,BLKGETSIZE,) failed");
				exit(20);
			}
			size64 = (uint64_t)512 *size;
		} else {
			perror("ioctl(,BLKGETSIZE64,) failed");
			exit(20);
		}
	}

	return size64;
}

char *lk_bdev_path(unsigned minor)
{
	char *path;
	m_asprintf(&path, "%s/drbd-minor-%d.lkbd", DRBD_LIB_DIR, minor);
	return path;
}

/* If the lower level device is resized,
 * and DRBD did not move its "internal" meta data in time,
 * the next time we try to attach, we won't find our meta data.
 *
 * Some helpers for storing and retrieving "last known"
 * information, to be able to find it regardless,
 * without scanning the full device for magic numbers.
 */

/* these return 0 on sucess, error code if something goes wrong. */

/* NOTE: file format for now:
 * one line, starting with size in byte, followed by tab,
 * followed by device name, followed by newline. */

int lk_bdev_save(const unsigned minor, const struct bdev_info *bd)
{
	FILE *fp;
	char *path = lk_bdev_path(minor);
	int ok = 0;

	fp = fopen(path, "w");
	if (!fp)
		goto fail;

	ok = fprintf(fp, "%llu\t%s\n",
		(unsigned long long) bd->bd_size, bd->bd_name);
	if (ok <= 0)
		goto fail;
	if (bd->bd_uuid)
		fprintf(fp, "uuid:\t"X64(016)"\n", bd->bd_uuid);
	ok =       0 == fflush(fp);
	ok = ok && 0 == fsync(fileno(fp));
	ok = ok && 0 == fclose(fp);

	if (!ok)
fail:		/* MAYBE: unlink. But maybe partial info is better than no info? */
		fprintf(stderr, "lk_bdev_save(%s) failed: %m\n", path);

	free(path);
	return ok <= 0 ? -1 : 0;
}

/* we may want to remove all stored information */
int lk_bdev_delete(const unsigned minor)
{
	char *path = lk_bdev_path(minor);
	int rc = unlink(path);
	if (rc && errno != ENOENT)
		fprintf(stderr, "lk_bdev_delete(%s) failed: %m\n", path);
	free(path);
	return rc;
}

/* load info from that file.
 * caller should free(bd->bd_name) once it is no longer needed. */
int lk_bdev_load(const unsigned minor, struct bdev_info *bd)
{
	FILE *fp;
	char *path;
	char *bd_name;
	unsigned long long bd_size;
	unsigned long long bd_uuid;
	char nl[2];
	int rc = -1;

	if (!bd)
		return -1;

	path = lk_bdev_path(minor);
	fp = fopen(path, "r");
	if (!fp) {
		if (errno != ENOENT)
			fprintf(stderr, "lk_bdev_load(%s) failed: %m\n", path);
		goto out;
	}

	/* GNU format extension: %as:
	 * malloc buffer space for the resulting char */
	rc = fscanf(fp, "%llu %as%[\n]uuid: %llx%[\n]",
			&bd_size, &bd_name, nl,
			&bd_uuid, nl);
	/* rc == 5: successfully converted two lines.
	 *    == 4: newline not found, possibly truncated uuid
	 *    == 3: first line complete, uuid missing.
	 *    == 2: new line not found, possibly truncated pathname,
	 *          or early whitespace
	 *    == 1: found some number, but no more.
	 *          incomplete file? try anyways.
	 */
	bd->bd_uuid = (rc >= 4) ? bd_uuid : 0;
	bd->bd_name = (rc >= 2) ? bd_name : NULL;
	bd->bd_size = (rc >= 1) ? bd_size : 0;
	if (rc < 1) {
		fprintf(stderr, "lk_bdev_load(%s): parse error\n", path);
		rc = -1;
	} else
		rc = 0;

	fclose(fp);
out:
	free(path);
	return rc;
}

void get_random_bytes(void* buffer, int len)
{
	int fd;

	fd = open("/dev/urandom",O_RDONLY);
	if( fd == -1) {
		perror("Open of /dev/urandom failed");
		exit(20);
	}
	if(read(fd,buffer,len) != len) {
		fprintf(stderr,"Reading from /dev/urandom failed\n");
		exit(20);
	}
	close(fd);
}

const char* shell_escape(const char* s)
{
	/* ugly static buffer. so what. */
	static char buffer[1024];
	char *c = buffer;

	if (s == NULL)
		return s;

	while (*s) {
		if (buffer + sizeof(buffer) < c+2)
			break;

		switch(*s) {
		/* set of 'clean' characters */
		case '%': case '+': case '-': case '.': case '/':
		case '0' ... '9':
		case ':': case '=': case '@':
		case 'A' ... 'Z':
		case '_':
		case 'a' ... 'z':
			break;
		/* escape everything else */
		default:
			*c++ = '\\';
		}
		*c++ = *s++;
	}
	*c = '\0';
	return buffer;
}

int m_asprintf(char **strp, const char *fmt, ...)
{
	int r;
	va_list ap;

	va_start(ap, fmt);
	r = vasprintf(strp, fmt, ap);
	va_end(ap);

	if (r == -1) {
		fprintf(stderr, "vasprintf() failed. Out of memory?\n");
		exit(10);
	}

	return r;
}

/* print len bytes from buf in the format of well known "hd",
 * adjust displayed offset by file_offset */
void fprintf_hex(FILE *fp, off_t file_offset, const void *buf, unsigned len)
{
	const unsigned char *c = buf;
	unsigned o;
	int skipped = 0;

	for (o = 0; o + 16 < len; o += 16, c += 16) {
		if (o && !memcmp(c - 16, c, 16)) {
			skipped = 1;
			continue;
		}
		if (skipped) {
			skipped = 0;
			fprintf(fp, "*\n");
		}
		/* no error check here, don't know what to do about errors */
		fprintf(fp,
			/* offset */
			"%08llx"
			/* two times 8 byte as byte stream, on disk order */
			"  %02x %02x %02x %02x %02x %02x %02x %02x"
			"  %02x %02x %02x %02x %02x %02x %02x %02x"
			/* the same as printable char or '.' */
			"  |%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c|\n",
			(unsigned long long)o + file_offset,
			c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7],
			c[8], c[9], c[10], c[11], c[12], c[13], c[14], c[15],

#define p_(x)	(isprint(x) ? x : '.')
#define p(a,b,c,d,e,f,g,h) \
		p_(a), p_(b), p_(c), p_(d), p_(e), p_(f), p_(g), p_(h)
			p(c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7]),
			p(c[8], c[9], c[10], c[11], c[12], c[13], c[14], c[15])
		       );
	}
	if (skipped) {
		skipped = 0;
		fprintf(fp, "*\n");
	}
	if (o < len) {
		unsigned remaining = len - o;
		unsigned i;
		fprintf(fp, "%08llx ", (unsigned long long)o + file_offset);
		for (i = 0; i < remaining; i++) {
			if (i == 8)
				fprintf(fp, " ");
			fprintf(fp, " %02x", c[i]);
		}
		fprintf(fp, "%*s  |", (16 - i)*3 + (i < 8), "");
		for (i = 0; i < remaining; i++)
			fprintf(fp, "%c", p_(c[i]));
#undef p
#undef p_
		fprintf(fp, "|\n");
	}
	fprintf(fp, "%08llx\n", (unsigned long long)len + file_offset);
}

const char *get_hostname(void)
{
	static char *s_hostname;

	if (!s_hostname) {
		char hostname[HOST_NAME_MAX];

		if (gethostname(hostname, sizeof(hostname))) {
			perror(hostname);
			exit(20);
		}
		s_hostname = strdup(hostname);
	}
	return s_hostname;
}


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

static void read_hex(char *dst, char *src, int dst_size, int src_size)
{
	int dst_i, u, src_i=0;

	for (dst_i=0; dst_i < dst_size; dst_i++) {
		if (src[src_i] == 0) break;
		if (src_size - src_i < 2) {
			sscanf(src+src_i, "%1x", &u);
			dst[dst_i] = u << 4;
		} else {
			sscanf(src+src_i, "%2x", &u);
			dst[dst_i] = u;
		}
		if (++src_i >= src_size)
			break;
		if (src[src_i] == 0)
			break;
		if (++src_i >= src_size)
			break;
	}
}

static void version_from_str(struct version *rel, const char *token)
{
	char *dot;
	long maj, min, sub;
	maj = strtol(token, &dot, 10);
	if (*dot != '.')
		return;
	min = strtol(dot+1, &dot, 10);
	if (*dot != '.')
		return;
	sub = strtol(dot+1, &dot, 10);
	/* don't check on *dot == 0,
	 * we may want to add some extraversion tag sometime
	if (*dot != 0)
		return;
	*/

	rel->version.major = maj;
	rel->version.minor = min;
	rel->version.sublvl = sub;

	rel->version_code = (maj << 16) + (min << 8) + sub;
}

static void parse_version(struct version *rel, const char *text)
{
	char token[80];
	int plus=0;
	enum { BEGIN, F_VER, F_SVN, F_REV, F_GIT, F_SRCV } ex = BEGIN;

	while (sget_token(token, sizeof(token), &text) != EOF) {
		switch(ex) {
		case BEGIN:
			if (!strcmp(token, "version:"))
				ex = F_VER;
			if (!strcmp(token, "SVN"))
				ex = F_SVN;
			if (!strcmp(token, "GIT-hash:"))
				ex = F_GIT;
			if (!strcmp(token, "srcversion:"))
				ex = F_SRCV;
			break;
		case F_VER:
			if (!strcmp(token, "plus")) {
				plus = 1;
				/* still waiting for version */
			} else {
				version_from_str(rel, token);
				ex = BEGIN;
			}
			break;
		case F_SVN:
			if (!strcmp(token,"Revision:"))
				ex = F_REV;
			break;
		case F_REV:
			rel->svn_revision = atol(token) * 10;
			if (plus)
				rel->svn_revision += 1;
			memset(rel->git_hash, 0, GIT_HASH_BYTE);
			return;
		case F_GIT:
			read_hex(rel->git_hash, token, GIT_HASH_BYTE, strlen(token));
			rel->svn_revision = 0;
			return;
		case F_SRCV:
			memset(rel->git_hash, 0, SRCVERSION_PAD);
			read_hex(rel->git_hash + SRCVERSION_PAD, token, SRCVERSION_BYTE, strlen(token));
			rel->svn_revision = 0;
			return;
		}
	}
}

const struct version *drbd_driver_version(enum driver_version_policy fallback)
{
	char* version_txt;

	if (__drbd_driver_version.version_code)
		return &__drbd_driver_version;

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

	if (fallback == FALLBACK_TO_UTILS)
		return drbd_utils_version();

	return NULL;
}

const struct version *drbd_utils_version(void)
{
	if (!__drbd_utils_version.version_code) {
		version_from_str(&__drbd_utils_version, PACKAGE_VERSION);
		parse_version(&__drbd_utils_version, drbd_buildtag());
	}

	return &__drbd_utils_version;
}

int version_code_kernel(void)
{
	const struct version *driver_version = drbd_driver_version(STRICT);
	return driver_version ? driver_version->version_code : 0;
}

int version_code_userland(void)
{
	const struct version *utils_version = drbd_utils_version();
	return utils_version->version_code;
}

int version_equal(const struct version *rev1, const struct version *rev2)
{
	if( rev1->svn_revision || rev2->svn_revision ) {
		return rev1->svn_revision == rev2->svn_revision;
	} else {
		return !memcmp(rev1->git_hash,rev2->git_hash,GIT_HASH_BYTE);
	}
}

void add_lib_drbd_to_path(void)
{
	char *new_path = NULL;
	char *old_path = getenv("PATH");

	m_asprintf(&new_path, "%s%s%s",
			old_path,
			old_path ? ":" : "",
			"/lib/drbd");
	setenv("PATH", new_path, 1);
}

/* from linux/crypto/crc32.c */
static const uint32_t crc32c_table[256] = {
	0x00000000L, 0xF26B8303L, 0xE13B70F7L, 0x1350F3F4L,
	0xC79A971FL, 0x35F1141CL, 0x26A1E7E8L, 0xD4CA64EBL,
	0x8AD958CFL, 0x78B2DBCCL, 0x6BE22838L, 0x9989AB3BL,
	0x4D43CFD0L, 0xBF284CD3L, 0xAC78BF27L, 0x5E133C24L,
	0x105EC76FL, 0xE235446CL, 0xF165B798L, 0x030E349BL,
	0xD7C45070L, 0x25AFD373L, 0x36FF2087L, 0xC494A384L,
	0x9A879FA0L, 0x68EC1CA3L, 0x7BBCEF57L, 0x89D76C54L,
	0x5D1D08BFL, 0xAF768BBCL, 0xBC267848L, 0x4E4DFB4BL,
	0x20BD8EDEL, 0xD2D60DDDL, 0xC186FE29L, 0x33ED7D2AL,
	0xE72719C1L, 0x154C9AC2L, 0x061C6936L, 0xF477EA35L,
	0xAA64D611L, 0x580F5512L, 0x4B5FA6E6L, 0xB93425E5L,
	0x6DFE410EL, 0x9F95C20DL, 0x8CC531F9L, 0x7EAEB2FAL,
	0x30E349B1L, 0xC288CAB2L, 0xD1D83946L, 0x23B3BA45L,
	0xF779DEAEL, 0x05125DADL, 0x1642AE59L, 0xE4292D5AL,
	0xBA3A117EL, 0x4851927DL, 0x5B016189L, 0xA96AE28AL,
	0x7DA08661L, 0x8FCB0562L, 0x9C9BF696L, 0x6EF07595L,
	0x417B1DBCL, 0xB3109EBFL, 0xA0406D4BL, 0x522BEE48L,
	0x86E18AA3L, 0x748A09A0L, 0x67DAFA54L, 0x95B17957L,
	0xCBA24573L, 0x39C9C670L, 0x2A993584L, 0xD8F2B687L,
	0x0C38D26CL, 0xFE53516FL, 0xED03A29BL, 0x1F682198L,
	0x5125DAD3L, 0xA34E59D0L, 0xB01EAA24L, 0x42752927L,
	0x96BF4DCCL, 0x64D4CECFL, 0x77843D3BL, 0x85EFBE38L,
	0xDBFC821CL, 0x2997011FL, 0x3AC7F2EBL, 0xC8AC71E8L,
	0x1C661503L, 0xEE0D9600L, 0xFD5D65F4L, 0x0F36E6F7L,
	0x61C69362L, 0x93AD1061L, 0x80FDE395L, 0x72966096L,
	0xA65C047DL, 0x5437877EL, 0x4767748AL, 0xB50CF789L,
	0xEB1FCBADL, 0x197448AEL, 0x0A24BB5AL, 0xF84F3859L,
	0x2C855CB2L, 0xDEEEDFB1L, 0xCDBE2C45L, 0x3FD5AF46L,
	0x7198540DL, 0x83F3D70EL, 0x90A324FAL, 0x62C8A7F9L,
	0xB602C312L, 0x44694011L, 0x5739B3E5L, 0xA55230E6L,
	0xFB410CC2L, 0x092A8FC1L, 0x1A7A7C35L, 0xE811FF36L,
	0x3CDB9BDDL, 0xCEB018DEL, 0xDDE0EB2AL, 0x2F8B6829L,
	0x82F63B78L, 0x709DB87BL, 0x63CD4B8FL, 0x91A6C88CL,
	0x456CAC67L, 0xB7072F64L, 0xA457DC90L, 0x563C5F93L,
	0x082F63B7L, 0xFA44E0B4L, 0xE9141340L, 0x1B7F9043L,
	0xCFB5F4A8L, 0x3DDE77ABL, 0x2E8E845FL, 0xDCE5075CL,
	0x92A8FC17L, 0x60C37F14L, 0x73938CE0L, 0x81F80FE3L,
	0x55326B08L, 0xA759E80BL, 0xB4091BFFL, 0x466298FCL,
	0x1871A4D8L, 0xEA1A27DBL, 0xF94AD42FL, 0x0B21572CL,
	0xDFEB33C7L, 0x2D80B0C4L, 0x3ED04330L, 0xCCBBC033L,
	0xA24BB5A6L, 0x502036A5L, 0x4370C551L, 0xB11B4652L,
	0x65D122B9L, 0x97BAA1BAL, 0x84EA524EL, 0x7681D14DL,
	0x2892ED69L, 0xDAF96E6AL, 0xC9A99D9EL, 0x3BC21E9DL,
	0xEF087A76L, 0x1D63F975L, 0x0E330A81L, 0xFC588982L,
	0xB21572C9L, 0x407EF1CAL, 0x532E023EL, 0xA145813DL,
	0x758FE5D6L, 0x87E466D5L, 0x94B49521L, 0x66DF1622L,
	0x38CC2A06L, 0xCAA7A905L, 0xD9F75AF1L, 0x2B9CD9F2L,
	0xFF56BD19L, 0x0D3D3E1AL, 0x1E6DCDEEL, 0xEC064EEDL,
	0xC38D26C4L, 0x31E6A5C7L, 0x22B65633L, 0xD0DDD530L,
	0x0417B1DBL, 0xF67C32D8L, 0xE52CC12CL, 0x1747422FL,
	0x49547E0BL, 0xBB3FFD08L, 0xA86F0EFCL, 0x5A048DFFL,
	0x8ECEE914L, 0x7CA56A17L, 0x6FF599E3L, 0x9D9E1AE0L,
	0xD3D3E1ABL, 0x21B862A8L, 0x32E8915CL, 0xC083125FL,
	0x144976B4L, 0xE622F5B7L, 0xF5720643L, 0x07198540L,
	0x590AB964L, 0xAB613A67L, 0xB831C993L, 0x4A5A4A90L,
	0x9E902E7BL, 0x6CFBAD78L, 0x7FAB5E8CL, 0x8DC0DD8FL,
	0xE330A81AL, 0x115B2B19L, 0x020BD8EDL, 0xF0605BEEL,
	0x24AA3F05L, 0xD6C1BC06L, 0xC5914FF2L, 0x37FACCF1L,
	0x69E9F0D5L, 0x9B8273D6L, 0x88D28022L, 0x7AB90321L,
	0xAE7367CAL, 0x5C18E4C9L, 0x4F48173DL, 0xBD23943EL,
	0xF36E6F75L, 0x0105EC76L, 0x12551F82L, 0xE03E9C81L,
	0x34F4F86AL, 0xC69F7B69L, 0xD5CF889DL, 0x27A40B9EL,
	0x79B737BAL, 0x8BDCB4B9L, 0x988C474DL, 0x6AE7C44EL,
	0xBE2DA0A5L, 0x4C4623A6L, 0x5F16D052L, 0xAD7D5351L
};

/*
 * Steps through buffer one byte at at time, calculates reflected
 * crc using table.
 */

uint32_t crc32c(uint32_t crc, const uint8_t *data, unsigned int length)
{
	while (length--)
		crc = crc32c_table[(crc ^ *data++) & 0xFFL] ^ (crc >> 8);

	return crc;
}
