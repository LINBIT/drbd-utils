#define UNICODE 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <wchar.h>

void usage_and_exit(void)
{
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "	windrbd assign-drive-letter <minor> <drive-letter>\n");
	fprintf(stderr, "		Assign drive letter to windrbd device for this user.\n");
	fprintf(stderr, "	windrbd delete-drive-letter <minor> <drive-letter>\n");
	fprintf(stderr, "		Delete drive letter to windrbd device for this user.\n");
	exit(1);
}

enum drive_letter_ops {
	ASSIGN_DRIVE_LETTER, DELETE_DRIVE_LETTER
};

int drive_letter_op(int minor, const char *drive, enum drive_letter_ops op)
{
	wchar_t t_device[100], t_drive[100];
	BOOL ret;
	int flag;

	switch (op) {
	case ASSIGN_DRIVE_LETTER: 
		flag = DDD_RAW_TARGET_PATH;
		break;
	case DELETE_DRIVE_LETTER:
		flag = DDD_EXACT_MATCH_ON_REMOVE | DDD_REMOVE_DEFINITION | DDD_RAW_TARGET_PATH;
		break;
	default:
		fprintf(stderr, "unknown op, don't know what to do.\n");
		usage_and_exit();
	}
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

	wprintf(L"%s drive letter %ls to minor %d (device %ls)\n",
		op == ASSIGN_DRIVE_LETTER ? "Assigning" : "Deleting",
		t_drive, minor, t_device);

	ret = DefineDosDevice(flag, t_drive, t_device);
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

		return drive_letter_op(minor, drive, ASSIGN_DRIVE_LETTER);
	}
	if (strcmp(op, "delete-drive-letter") == 0) {
		if (argc != 4) {
			usage_and_exit();
		}
		int minor = atoi(argv[2]);
		const char *drive = argv[3];

		return drive_letter_op(minor, drive, DELETE_DRIVE_LETTER);
	}

	usage_and_exit();
	return 0;
}
