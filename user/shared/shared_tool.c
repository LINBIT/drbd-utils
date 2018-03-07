/* These are common functions that are used across _all_ userspace,
 * ie. even across 9, 84, and 83.
 *
 * Deduplicated here for easier maintenance. */

#define _GNU_SOURCE
#define _XOPEN_SOURCE 600
#define _FILE_OFFSET_BITS 64

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <fnmatch.h>
#include <features.h>

#include "config.h"
#include "drbdadm.h"
#include "drbd_endian.h"
#include "linux/drbd.h"

#include "drbdtool_common.h"
#include "shared_tool.h"
#include "shared_main.h"

#ifdef HAVE_GETENTROPY
#include <sys/random.h>
#endif

const char *IPV4_STR = "ipv4";
const char *IPV6_STR = "ipv6";

const char* shell_escape(const char* s)
{
	/* ugly static buffer. so what. */
	static char buffer[1024];
	char *c = buffer;

	if (s != NULL) {
		/* reserve space for a possible escape character and
		 * the terminating null character */
		char *max_c = buffer + sizeof (buffer) - 2;
		while (*s != '\0' && c < max_c) {
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
	}
	*c = '\0';
	return buffer;
}


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
	/* Do not use stderr (or stdout) while having any FD open for write.
	   We might get called with stdin, stdout and stderr closed. then
	   fp might be on FD 2. Using stderr would send the text to the file.
	   Work around: Error messages after closing the writebale FD. */
	if (!fp)
		goto fail_no_fp;

	ok = fprintf(fp, "%llu\t%s\n",
		(unsigned long long) bd->bd_size, bd->bd_name);
	if (ok <= 0)
		goto fail;
	if (bd->bd_uuid)
		fprintf(fp, "uuid:\t"X64(016)"\n", bd->bd_uuid);

 fail:
	fflush(fp);
	fsync(fileno(fp));
	fclose(fp);

 fail_no_fp:
	if (ok <= 0)
		/* MAYBE: unlink. But maybe partial info is better than no info? */
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

	rc = fscanf(fp, "%llu %ms%[\n]uuid: %llx%[\n]",
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

bool random_by_dev_urandom(void *buffer, size_t len)
{
	int fd;
	bool ok = true;

	fd = open("/dev/urandom", O_RDONLY);
	if (fd == -1) {
		perror("Open of /dev/urandom failed");
		return false;
	}

	if (read(fd, buffer, len) != len) {
		ok = false;
		fprintf(stderr, "Reading from /dev/urandom failed\n");
	}
	close(fd);

	return ok;
}

void get_random_bytes(void *buffer, size_t len)
{
	bool ok;

#ifdef HAVE_GETENTROPY
	ok = (getentropy(buffer, len) == 0);
	if (ok)
		return;

	perror("Could not get random data from getentropy(). Fallback to /dev/urandom");
	/* fallback to /dev/urandom */
#endif
	ok = random_by_dev_urandom(buffer, len);

	if (!ok)
		exit(20);
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


void ensure_sanity_of_res_name(char *stg)
{
    unsigned code;
    if (!*stg) {
	fprintf(stderr, "Resource name is empty.\n");
	exit(1);
    }

    while (*stg) {
	/* No, we won't verify valid UTF-8, and neither check for unicode
	 * control sequences. */
	/* Only works for ASCII derived code sets. */
	code = * (unsigned char*) stg;
	if (code < ' ' || code == '\x7f')
	{
	    fprintf(stderr, "Resource name is invalid - please don't use control characters.\n");
	    exit(1);
	}

	stg++;
    }
    return;
}

bool addr_scope_local(const char *input)
{
	struct in_addr addr4;
	struct in6_addr addr6;

	if (inet_pton(AF_INET6, input, &addr6) == 1)
		return IN6_IS_ADDR_LOOPBACK(&addr6);
	else if (inet_pton(AF_INET, input, &addr4) == 1)
		return IN_IS_ADDR_LOOPBACK(&addr4);

	return false;
}

bool addresses_match(const char *const af_1st, const char *const addr_1st,
                     const char *const af_2nd, const char *const addr_2nd)
{
	bool match = false;

	if (strcasecmp(af_1st, IPV4_STR) == 0 &&
	    strcasecmp(af_2nd, IPV4_STR) == 0) {
		// Both addresses are IPv4, match the translated IPv4 addresses
		match = ipv4_addresses_match(addr_1st, addr_2nd);
	} else if (strcasecmp(af_1st, IPV6_STR) == 0 &&
	           strcasecmp(af_2nd, IPV6_STR) == 0) {
		// Both addresses are IPv6, match the translated IPv6 addresses
		match = ipv6_addresses_match(addr_1st, addr_2nd);
	} else {
		// Address families either mismatch or they are neither
		// IPv4 nor IPv6 type addresses
		// Fall back to string comparison to cover those cases
		match = strcmp(af_1st, af_2nd) == 0 &&
		        strcmp(addr_1st, addr_2nd) == 0;
	}

	return match;
}

bool ipv4_addresses_match(const char *const addr_1st,
                          const char *const addr_2nd)
{
	bool match = false;

	struct in_addr v4_addr_1st;
	struct in_addr v4_addr_2nd;
	if (inet_pton(AF_INET, addr_1st, &v4_addr_1st) == 1 &&
	    inet_pton(AF_INET, addr_2nd, &v4_addr_2nd) == 1) {
		match = v4_addr_1st.s_addr == v4_addr_2nd.s_addr;
	}

	return match;
}

bool ipv6_addresses_match(const char *const addr_1st,
                          const char *const addr_2nd)
{
	bool match = false;

	struct in6_addr v6_addr_1st;
	struct in6_addr v6_addr_2nd;
	if (inet_pton(AF_INET6, addr_1st, &v6_addr_1st) == 1 &&
	    inet_pton(AF_INET6, addr_2nd, &v6_addr_2nd) == 1) {
		size_t length = sizeof (v6_addr_1st.s6_addr);
		match = memcmp(v6_addr_1st.s6_addr,
		               v6_addr_2nd.s6_addr,
		               length) == 0;
	}

	return match;
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


volatile int alarm_raised;
/* nothing. just interrupt F_SETLKW */
void alarm_handler(int __attribute((unused)) signo)
{
	alarm_raised = 1;
}

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

enum new_strtoll_errs
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

struct err_state {
	unsigned int stderr_available:1;
};

static struct err_state err_state = { /* all zero */ };

/* Call initialize_err() before creating any FD */
void initialize_err(void)
{
	int err;

	err = fcntl(STDERR_FILENO, F_GETFL);
	if (err < 0 && errno == EBADF) {
		err_state.stderr_available = 0;
		openlog(NULL, LOG_PID, LOG_SYSLOG);
	} else {
		err_state.stderr_available = 1;
	}
}

int err(const char *format, ...)
{
	va_list ap;
	int n;

	va_start(ap, format);

	if (err_state.stderr_available) {
		n = vfprintf(stderr, format, ap);
	} else {
		vsyslog(LOG_ERR, format, ap);
		n = 1;
	}

	va_end(ap);

	return n;
}

/* if @str is NULL or the empty string, return "";
 * if @str contains ' ', '\t' or '\\',
 * surround it with ", and escape space, tab, backslash and double-quote with backslash.
 * This escape is for escaping tokens for the drbdadm config parser,
 * so any legal input do drbdadm "drbdadm dump" should result in output, which,
 * if fed into an additional "drbdadm dump" should give the same output again.
 */
const char *esc(char *str)
{
	static char buffer[1024];
	char *ue = str, *e = buffer;

	if (!str || !str[0]) {
		return "\"\"";
	}
	if (0 == fnmatch("*[!a-zA-Z0-9/._-]*", str, 0) || strlen(str) > 80) {
		*e++ = '"';
		while (*ue) {
			if (*ue == '"' || *ue == '\\') {
				*e++ = '\\';
			}
			if (e - buffer >= 1022) {
				err("string too long.\n");
				exit(E_SYNTAX);
			}
			*e++ = *ue++;
			if (e - buffer >= 1022) {
				err("string too long.\n");
				exit(E_SYNTAX);
			}
		}
		*e++ = '"';
		*e++ = '\0';
		return buffer;
	}
	return str;
}

/* escape a few things that are not legal in xml content; good enough for our
 * purposes, but likely not "academically correct" resp.  "complete". */
const char *esc_xml(char *str)
{
	static char buffer[1024];
	char *ue = str, *e = buffer;

	if (!str || !str[0]) {
		return "";
	}
	if (strchr(str, '"') || strchr(str, '\'') || strchr(str, '<') ||
	    strchr(str, '>') || strchr(str, '&') || strchr(str, '\\')) {
		while (*ue) {

#define XML_ENCODE_SPECIAL(_ch, _repl) \
			case _ch: \
				if (e - buffer >= sizeof(buffer)-1-strlen(_repl)) goto too_long; \
				strcpy(e, _repl); e += strlen(_repl); break;

			switch (*ue) {
				XML_ENCODE_SPECIAL('\'', "&apos;");
				XML_ENCODE_SPECIAL('"',  "&quot;");
				XML_ENCODE_SPECIAL('<',  "&lt;");
				XML_ENCODE_SPECIAL('>',  "&gt;");
				XML_ENCODE_SPECIAL('&',  "&amp;");
				default:
				*e++ = *ue;
				if (e - buffer >= 1022)
					goto too_long;
			}
			ue++;
		}
		*e++ = '\0';
		return buffer;
	}
	return str;

too_long:
	err("string too long.\n");
	exit(E_SYNTAX);
}
