#define UNICODE 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <wchar.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <unistd.h>

void usage_and_exit(void)
{
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "	windrbd assign-drive-letter <minor> <drive-letter>\n");
	fprintf(stderr, "		Assign drive letter to windrbd device for this user.\n");
	fprintf(stderr, "	windrbd delete-drive-letter <minor> <drive-letter>\n");
	fprintf(stderr, "		Delete drive letter to windrbd device for this user.\n");
	fprintf(stderr, "	windrbd hide-filesystem <drive-letter>\n");
	fprintf(stderr, "		Prepare existing drive for use as windrbd backing device.\n");
	fprintf(stderr, "	windrbd show-filesystem <drive-letter>\n");
	fprintf(stderr, "		Make backing device visible to Windows again.\n");
	fprintf(stderr, "		(You cannot use it as backing device after doing that)\n");
	fprintf(stderr, "	windrbd filesystem-state <drive-letter>\n");
	fprintf(stderr, "		Shows the current filesystem state (windows, windrbd, other)\n");
	fprintf(stderr, "	windrbd log-server [<log-file>]\n");
	fprintf(stderr, "		Logs windrbd kernel messages to stdout (and optionally to\n");
	fprintf(stderr, "		log-file)\n");

	exit(1);
}

enum drive_letter_ops {
	ASSIGN_DRIVE_LETTER, DELETE_DRIVE_LETTER
};

static void check_drive_letter(const char *drive)
{
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
}

static int drive_letter_op(int minor, const char *drive, enum drive_letter_ops op)
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
	check_drive_letter(drive);

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

static HANDLE do_open_device(const char *drive)
{
        HANDLE h;
        DWORD err;

        wchar_t fname[100];
        swprintf(fname, sizeof(fname) / sizeof(fname[0]), L"\\\\.\\%s", drive);

        h = CreateFile(fname, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        err = GetLastError();

	if (err != ERROR_SUCCESS) {
		fprintf(stderr, "Couldn't open drive %s, error is %d\n", drive, err);
		return INVALID_HANDLE_VALUE;
	}
        return h;
}

	/* Taken from windrbd drbd_windows.c. See comments there. */

static int patch_boot_sector(char *buffer, int to_fs, int test_mode)
{
        static const char *fs_signatures[][2] = {
                { "NTFS", "DRBD" },
                { "MSDOS5.0", "FATDRBD" },
                { "EXFAT", "EDRBD" },
                { NULL, NULL }};
        int fs;
        int i;

        for (fs=0; fs_signatures[fs][0] != NULL; fs++) {
                for (i=0;fs_signatures[fs][to_fs][i] != '\0';i++) {
                        if (buffer[3+i] != fs_signatures[fs][to_fs][i])
                                break;
                }
                if (fs_signatures[fs][to_fs][i] == '\0') {
                        if (!test_mode) {
                                printf("Patching boot sector from %s to %s\n", fs_signatures[fs][to_fs], fs_signatures[fs][!to_fs]);
                                for (i=0;fs_signatures[fs][to_fs][i] != '\0';i++) {
                                        buffer[3+i] = fs_signatures[fs][!to_fs][i];
                                }
                        } else {
                                printf("File system signature %s found in boot sector\n", fs_signatures[fs][to_fs]);
                        }
                        return 1;
                }
        }
        return 0;
}

enum filesystem_ops {
	HIDE_FILESYSTEM, SHOW_FILESYSTEM, FILESYSTEM_STATE
};

static int patch_bootsector_op(const char *drive, enum filesystem_ops op)
{
	check_drive_letter(drive);

	HANDLE h = do_open_device(drive);
	if (h == INVALID_HANDLE_VALUE)
		return 1;

	char buf[512];
	BOOL ret;
	int err;
	DWORD bytes_read, bytes_written;
	int px;

        ret = ReadFile(h, buf, sizeof(buf), &bytes_read,  NULL);
	if (!ret || bytes_read != sizeof(buf)) {
		err = GetLastError();
		fprintf(stderr, "Couldn't read from drive %s, err = %d bytes_read = %d\n", drive, err, bytes_read);

		CloseHandle(h);
		return 1;
	}

	if (op == FILESYSTEM_STATE) {
		if (patch_boot_sector(buf, 0, 1))
			printf("Windows filesystem found on %s\n", drive);
		else if (patch_boot_sector(buf, 1, 1))
			printf("Windrbd backing device found on %s\n", drive);
		else
			printf("Nothing I recognize on drive %s\n", drive);
	} else {
		if (patch_boot_sector(buf, op == SHOW_FILESYSTEM, 0))
			printf("Filesystem on drive %s %s\n", drive, op == HIDE_FILESYSTEM ? "prepared for use with windrbd." : "reverted to be used directly (you might have to reboot before you can use it).");
		else
			fprintf(stderr, "No %s found on %s\n", op == HIDE_FILESYSTEM ? "Windows filesystem" : "windrbd backing device", drive);

	        px = SetFilePointer(h, 0, NULL, FILE_BEGIN);
		if (px != 0) {
			fprintf(stderr, "Couldn't reset file pointer.\n");
			return 1;
		}
		ret = WriteFile(h, buf, sizeof(buf), &bytes_written,  NULL);
		if (!ret || bytes_written != sizeof(buf)) {
			err = GetLastError();
			fprintf(stderr, "Couldn't write to drive %s, err = %d bytes_written = %d\n", drive, err, bytes_written);

			CloseHandle(h);
			return 1;
		}
	}

	CloseHandle(h);
	return 0;
}


int log_server_op(const char *log_file)
{
	int s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		perror("socket");
		return 1;
	}
	struct sockaddr_in my_addr;
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(514);
	my_addr.sin_addr.s_addr = 0;

	if (bind(s, (struct sockaddr*)&my_addr, sizeof(my_addr)) < 0) {
		perror("bind");
		return 1;
	}
	int fd = -1;
	if (log_file != NULL) {
		if ((fd = open(log_file, O_WRONLY | O_CREAT | O_SYNC | O_APPEND, 0664)) < 0)
			perror("open (ignored)");
	}

		/* See printk routine. We split lines longer than that. */
	char buf[512];
	ssize_t len;

	printf("Waiting for log messages from windrbd kernel driver.\n");
	printf("Press Ctrl-C to stop.\n");
	while ((len = recv(s, buf, sizeof(buf), 0)) >= 0) {
		write(1, buf, len);
		if (fd >= 0) {
			if (write(fd, buf, len) < 0)
				perror("write (ignored)");
		}
	}
	perror("recv");
	return 1;
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
	if (strcmp(op, "hide-filesystem") == 0) {
		if (argc != 3) {
			usage_and_exit();
		}
		const char *drive = argv[2];

		return patch_bootsector_op(drive, HIDE_FILESYSTEM);
	}
	if (strcmp(op, "show-filesystem") == 0) {
		if (argc != 3) {
			usage_and_exit();
		}
		const char *drive = argv[2];

		return patch_bootsector_op(drive, SHOW_FILESYSTEM);
	}
	if (strcmp(op, "filesystem-state") == 0) {
		if (argc != 3) {
			usage_and_exit();
		}
		const char *drive = argv[2];

		return patch_bootsector_op(drive, FILESYSTEM_STATE);
	}
	if (strcmp(op, "log-server") == 0) {
		if (argc < 2 || argc > 3) {
			usage_and_exit();
		}
		const char *log_file = argv[2];

		return log_server_op(log_file);
	}

	usage_and_exit();
	return 0;
}
