#define UNICODE 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <wchar.h>

void usage_and_exit(void)
{
	fprintf(stderr, "Usage: windrbd assign-drive-letter <minor> <drive-letter>\n");
	exit(1);
}

int assign_drive_letter(int minor, const char *drive)
{
	wchar_t t_device[100], t_drive[100];
	BOOL ret;

	if (!isalpha(drive[0])) {
		fprintf(stderr, "Drive letter (%s) must start with a letter.\n", drive);
		usage_and_exit();
	}
	if (drive[1] != '\0') {
		if (drive[1] != ':' || drive[2] != '\0') {
			fprintf(stderr, "Drive letter (%s) must end with a colon\n", drive);
			usage_and_exit();
		}
	}
	swprintf(t_device, sizeof(t_device) / sizeof(*t_device) -1, L"\\Device\\Drbd%d", minor);
	swprintf(t_drive, sizeof(t_drive) / sizeof(*t_drive) -1, L"%s", drive);

	wprintf(L"Assigning drive letter %ls to minor %d (device %ls)\n", t_drive, minor, t_device);

	ret = DefineDosDevice(DDD_RAW_TARGET_PATH, t_drive, t_device);
	if (!ret) {
		int err;
		err = GetLastError();
		fprintf(stderr, "Sorry that didn't work out. DefineDosDevice failed with error %d\n", err);
		return 1;
	}

	return 0;
}

int main(int argc, char ** argv)
{
	const char *op;

	if (argc < 2) {
		usage_and_exit();
	}
	op = argv[1];
	if (strcmp(op, "assign-drive-letter") == 0) {
		if (argc != 4) {
			usage_and_exit();
		}
		int minor = atoi(argv[2]);
		const char *drive = argv[3];

		return assign_drive_letter(minor, drive);
	}

	usage_and_exit();
	return 0;
}
