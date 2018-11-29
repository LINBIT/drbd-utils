#include <string.h>
#include <ctype.h>
#include <windows.h>

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
        DWORD err;

        h = CreateFile("\\\\.\\" WINDRBD_ROOT_DEVICE_NAME, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE && !quiet) {
	        err = GetLastError();

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
