#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <linux/fs.h>

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

