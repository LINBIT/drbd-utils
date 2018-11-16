/*
   drbdmeta_windrbd.c

   This file is part of DRBD by Philipp Reisner and Lars Ellenberg.

   Copyright (C) 2004-2008, LINBIT Information Technologies GmbH
   Copyright (C) 2004-2008, Philipp Reisner <philipp.reisner@linbit.com>
   Copyright (C) 2004-2008, Lars Ellenberg  <lars.ellenberg@linbit.com>
   Copyright (C) 2017-2018, Johannes Thoma <johannes@johannesthoma.com>

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

/* This file contains the WinDRBD specific block I/O functions. We are
 * not using CygWin's block API (which is quite similar to that of
 * Linux or other UNICES), since the mapping between device files
 * (/dev/sda1 and the like) changes between system reboots (/dev/sda1
 * becoming /dev/sdb1 and vice versa), because they follow the
 * NT internal device scheme (\Devices\Harddisk0\Partition0), which
 * changes between reboots.
 * That's why we implemented accessing block devices using the WIN32
 * native API.
 * We allow for using drive letters (K:, ...) and GUIDs where GUIDs
 * should be preferred over drive letters.
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#include "drbd_endian.h"
#include "drbdtool_common.h"

#include "drbdmeta_parser.h"

#include "drbdmeta.h"

#include <windows.h>
#include <winternl.h>
#include <wchar.h>
#include "shared_windrbd.h"

/*
 * generic helpers
 */

/* Do we want to exit() right here,
 * or do we want to duplicate the error handling everywhere? */
void pread_or_die(struct format *cfg, void *buf, size_t count, off_t offset, const char* tag)
{
	DWORD bytes_read;
	LARGE_INTEGER win_offset;

	win_offset.QuadPart = offset;

	if (verbose >= 2) {
		fflush(stdout);
		fprintf(stderr, " %-26s: ReadFile(%p, ...,%6lu,%12llu)\n", tag,
			cfg->disk_handle, (unsigned long)count, (unsigned long long)offset);
	}
	if (SetFilePointerEx(cfg->disk_handle, win_offset, NULL, FILE_BEGIN) == 0) {
		fprintf(stderr, "Could not set file pointer to position %zd using SetFilePointerEx, error is %d\n", offset, GetLastError());
		exit(10);
	}
	if (ReadFile(cfg->disk_handle, buf, count, &bytes_read, NULL) == 0) {
		fprintf(stderr, "Could not read %zd bytes from position %zd using ReadFile, error is %d\n", count, offset, GetLastError());
		exit(10);
	}
	if (bytes_read != count) {
		fprintf(stderr, "Read %d bytes from position %zd using ReadFile, expected %zd bytes error is %d\n", bytes_read, offset, count, GetLastError());
		fprintf(stderr, "Is this a NTFS partition?\n");
		exit(10);
	}
	if (verbose > 10)
		fprintf_hex(stderr, offset, buf, count);
}

static unsigned n_writes = 0;

void pwrite_or_die(struct format *cfg, const void *buf, size_t count, off_t offset, const char* tag)
{
	DWORD bytes_written;
	LARGE_INTEGER win_offset;

	validate_offsets_or_die(cfg, count, offset, tag);

	++n_writes;
	if (dry_run) {
		fprintf(stderr, " %-26s: WriteFile(%p, ...,%6lu,%12llu) SKIPPED DUE TO DRY-RUN\n",
			tag, cfg->disk_handle, (unsigned long)count, (unsigned long long)offset);
		if (verbose > 10)
			fprintf_hex(stderr, offset, buf, count);
		return;
	}
	if (verbose >= 2) {
		fflush(stdout);
		fprintf(stderr, " %-26s: WriteFile(%p, ...,%6lu,%12llu)\n", tag,
			cfg->disk_handle, (unsigned long)count, (unsigned long long)offset);
	}

	win_offset.QuadPart = offset;
	if (SetFilePointerEx(cfg->disk_handle, win_offset, NULL, FILE_BEGIN) == 0) {
		fprintf(stderr, "Could not set file pointer to position %zd using SetFilePointerEx, error is %d\n", offset, GetLastError());
		exit(10);
	}
	if (WriteFile(cfg->disk_handle, buf, count, &bytes_written, NULL) == 0) {
		fprintf(stderr, "Could not write %zd bytes from position %zd using ReadFile, error is %d\n", count, offset, GetLastError());
		exit(10);
	}
	if (bytes_written != count) {
		fprintf(stderr, "Wrote %d bytes from position %zd using WriteFile, expected %zd bytes error is %d\n", bytes_written, offset, count, GetLastError());
		fprintf(stderr, "Is this a NTFS partition?\n");
		exit(10);
	}
}

int v06_md_open(struct format *cfg)
{
	fprintf(stderr, "v06_md_open: Not supported with WinDRBD.\n");
	return -1;
}

int generic_md_close(struct format *cfg)
{
	if (CloseHandle(cfg->disk_handle) == 0) {
		fprintf(stderr, "CloseHandle() failed, error is %d\n", GetLastError());
		return -1;
	}
	return 0;
}

int zeroout_bitmap_fast(struct format *cfg)
{
	return -ENOTSUP;

		/* Well .. there is such a thing under MS Windows,
		 * maybe one day we'll implement it.
		 */
}

/* Uses Win32 API (CreateFile) to open the disk. We do not
 * use CygWin (UNIX type: /dev/sdXN) API, since that follows the
 * symbolic links in \\Device\\Harddisk<n>\\Partition<n> which
 * changes between reboots (\\Device\\Harddisk0 becomes
 * \\Device\\Harddisk1 and vice versa). That way, we allow the
 * user to use GUIDs (\\\\.\\Volumes{<GID>}) or drive letters
 * (\\\\.\\x:). Specifying \\Device\\HarddiskVolume<n> directly
 * does not work, but it is not needed. Most recommended way
 * is to use the GUIDs.
 *
 * This function writes results directly into the cfg passed as
 * a parameter.
 */

HANDLE open_windows_device(const char *arg)
{
	HANDLE hdisk = NULL;

        /* We want to do simple conversions
                as C: -> \\\\.\\C: and GUIDs to
                \\\\.\\Volume{<GUID>} for convenience.
        */

        char device[1024];
        size_t n;

        if (isalpha(arg[0]) && arg[1] == ':' && arg[2] == '\0') {
                n = snprintf(device, sizeof(device), "\\\\.\\%s", arg);
        } else if (is_guid(arg)) {
                n = snprintf(device, sizeof(device), "\\\\.\\Volume{%s}", arg);
        } else {
                n = snprintf(device, sizeof(device), "%s", arg);
        }
        if (n >= sizeof(device)) {
                fprintf(stderr, "Device name too long: %s (%zd), please report this.\n", arg, n);
                return INVALID_HANDLE_VALUE;
        }

	if (verbose > 2) {
		fprintf(stderr, "Converted %s to %s\n", arg, device);
	}
	hdisk = CreateFile(
		device,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL,
		OPEN_EXISTING,
		FILE_SYNCHRONOUS_IO_NONALERT,
		NULL
	);
	if (hdisk == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "Couldn't open disk %s with CreateFile: Error is %d\n", device, GetLastError());
	}
	return hdisk;
}

	/* Now, get the disk parameters (sector size and total size) */

static int get_windows_device_geometry(HANDLE hdisk, int *md_hard_sect_size, uint64_t *bd_size)
{
	DISK_GEOMETRY_EX geometry;
	DWORD ret_bytes;
	PARTITION_INFORMATION_EX partition_info;

	if (DeviceIoControl(hdisk, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, NULL, 0, &geometry, sizeof(geometry), &ret_bytes, NULL) == 0) {
		fprintf(stderr, "Failed to get disk geometry: error is %d\n", GetLastError());
		return -1;
	}
	if (verbose >= 1) {
		printf("%d bytes per sector, total disk size %lld\n", geometry.Geometry.BytesPerSector, geometry.DiskSize.QuadPart);
	}

	if (DeviceIoControl(hdisk, IOCTL_DISK_GET_PARTITION_INFO_EX, NULL, 0, &partition_info, sizeof(partition_info), &ret_bytes, NULL) == 0) {
		fprintf(stderr, "Failed to get partition info: error is %d\n", GetLastError());
		return -1;
	}
	if (verbose >= 1) {
		printf("partition size %lld\n", partition_info.PartitionLength.QuadPart);
	}

	*md_hard_sect_size = geometry.Geometry.BytesPerSector;
	*bd_size = partition_info.PartitionLength.QuadPart;

	return 0;
}

int v07_style_md_open_device(struct format *cfg)
{
	cfg->disk_handle = open_windows_device(cfg->md_device_name);
	if (cfg->disk_handle == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "Could not open Windows device %s\n", cfg->md_device_name);
		exit(20);
	}
	if (get_windows_device_geometry(cfg->disk_handle, &cfg->md_hard_sect_size, &cfg->bd_size) < 0) {
		CloseHandle(cfg->disk_handle);
		fprintf(stderr, "Could not open Windows device %s\n", cfg->md_device_name);
		exit(20);
	}

	return 0;
}
