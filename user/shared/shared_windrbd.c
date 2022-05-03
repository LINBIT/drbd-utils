#include <string.h>
#include <ctype.h>
#include <windows.h>
#include <io.h>
#include <stdint.h>

#include "shared_windrbd.h"
#include <windrbd/windrbd_ioctl.h>
#include <stdio.h>

int is_guid(const char *arg)
{
        int i;
#define GUID_MASK "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"

        for (i=0;arg[i] != '\0';i++) {
                if (GUID_MASK[i] == 'x' && !isxdigit(arg[i]))
                        return 0;
                if (GUID_MASK[i] == '-' && arg[i] != '-')
                        return 0;
        }
        return i == strlen(GUID_MASK) && arg[i] == '\0';
#undef GUID_MASK
}


HANDLE do_open_root_device(int quiet)
{
        HANDLE h;
        DWORD err = ERROR_SUCCESS;

        h = CreateFile("\\\\.\\" WINDRBD_ROOT_DEVICE_NAME, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE) {
	        err = GetLastError();

		if (err == ERROR_ACCESS_DENIED) {
			h = CreateFile("\\\\.\\" WINDRBD_USER_DEVICE_NAME, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			if (h == INVALID_HANDLE_VALUE)
				err = GetLastError();
		}
	}

	if (h == INVALID_HANDLE_VALUE && !quiet) {
	        if (err != ERROR_SUCCESS) {
			fprintf(stderr, "Couldn't open root device, error is %d\n", err);
			switch (err) {
			case ERROR_FILE_NOT_FOUND:
				fprintf(stderr, "(this is most likely because the WinDRBD driver is not loaded).\n");
				break;
			case ERROR_ACCESS_DENIED:
				fprintf(stderr, "(this is most likely because you are not running as Administrator).\n");
				break;
			}
		}
        }
        return h;
}

int windrbd_driver_loaded(void)
{
	HANDLE h = do_open_root_device(0);
	if (h == INVALID_HANDLE_VALUE)
		return 0;

	CloseHandle(h);
	return 1;
}

static char drbd_version[256] = "Unknown DRBD version (driver not loaded, do drbdadm status to load it)";
static char windrbd_version[256] = "Unknown WinDRBD version (driver not loaded, do drbdadm status to load it)";
static int got_version;

static int get_driver_versions(void)
{
	DWORD ret_bytes;

	if (got_version)
		return 0;

        HANDLE h = do_open_root_device(1);
        if (h == INVALID_HANDLE_VALUE)
                return -1;

	if (DeviceIoControl(h, IOCTL_WINDRBD_ROOT_GET_DRBD_VERSION, NULL, 0, drbd_version, sizeof(drbd_version), &ret_bytes, NULL) == 0) {
		fprintf(stderr, "Could not get DRBD version from driver, error is %d\n",  GetLastError());
		CloseHandle(h);
                return -1;
	}

	if (DeviceIoControl(h, IOCTL_WINDRBD_ROOT_GET_WINDRBD_VERSION, NULL, 0, windrbd_version, sizeof(windrbd_version), &ret_bytes, NULL) == 0) {
		fprintf(stderr, "Could not get WinDRBD version from driver, error is %d\n",  GetLastError());
		CloseHandle(h);
                return -1;
	}

	got_version = 1;
        CloseHandle(h);

	return 0;
}

char *windrbd_get_drbd_version(void)
{
	get_driver_versions();
	return drbd_version;
}

char *windrbd_get_windrbd_version(void)
{
	get_driver_versions();
	return windrbd_version;
}

int windrbd_get_registry_string_value(HKEY root_key, const char *key, const char *value_name, unsigned char ** buf_ret, DWORD *buflen_ret, int verbose)
{
	HKEY h;
	unsigned char *buf;
	DWORD buflen;

	DWORD ret = RegOpenKeyEx(root_key, key, 0, KEY_READ, &h);
	DWORD the_type;

	if (ret != ERROR_SUCCESS) {
		if (verbose)
			fprintf(stderr, "Couldn't open registry key error is %d\n", ret);
		return 1;
	}
        ret = RegQueryValueEx(h, value_name, NULL, &the_type, NULL, &buflen);
	if (ret != ERROR_SUCCESS) {
		RegCloseKey(h);
		if (verbose)
			fprintf(stderr, "Couldn't get size of %s value error is %d\n", value_name, ret);
		return 1;
	}
	if (the_type != REG_SZ && the_type != REG_EXPAND_SZ) {
		RegCloseKey(h);
		if (verbose)
			fprintf(stderr, "Type mismatch: %s is not a REG_SZ (simple C string)\n", value_name);
		return 1;
	}
	buf = malloc(buflen);
	if (buf == NULL) {
		RegCloseKey(h);
		if (verbose)
			fprintf(stderr, "Out of memory allocating %d bytes\n", buflen);
		return 1;
	}

        ret = RegQueryValueEx(h, value_name, NULL, NULL, buf, &buflen);
	if (ret != ERROR_SUCCESS) {
		RegCloseKey(h);
		free(buf);
		if (verbose)
			fprintf(stderr, "Couldn't get value %s error is %d\n", value_name, ret);
		return 1;
	}

	RegCloseKey(h);
	if (buf_ret != NULL)
		*buf_ret = buf;
	else
		free(buf);	/* avoid memory leak */

	if (buflen_ret != NULL)
		*buflen_ret = buflen;

	return 0;
}

uint64_t bdev_size_by_handle(HANDLE h)
{
	GET_LENGTH_INFORMATION length_info;
	DWORD size;

	if (h == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "Invalid windows handle in bdev_size_by_handle()\n");
		return 0;
	}
        if (DeviceIoControl(h, IOCTL_DISK_GET_LENGTH_INFO, NULL, 0, &length_info, sizeof(length_info), &size, NULL) == 0) {
                fprintf(stderr, "Failed to get length info: error is %d\n", GetLastError());
                return 0;
        }
		/* Do not close windows handle. Caller might still need it */
	return length_info.Length.QuadPart;
}

uint64_t bdev_size(int fd)
{
	HANDLE h;

	h = (void*) _get_osfhandle(fd);
	if (h == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "Could not convert fd %d to Windows handle\n", fd);
		return 0;
	}
	return bdev_size_by_handle(h);
}
