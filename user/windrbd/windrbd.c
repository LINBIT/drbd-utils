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
#include <sys/wait.h>

static int quiet = 0;

void usage_and_exit(void)
{
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "	windrbd [opt] assign-drive-letter <minor> <drive-letter>\n");
	fprintf(stderr, "		Assign drive letter to windrbd device for this user.\n");
	fprintf(stderr, "	windrbd [opt] delete-drive-letter <minor> <drive-letter>\n");
	fprintf(stderr, "		Delete drive letter to windrbd device for this user.\n");
	fprintf(stderr, "	windrbd [opt] set-volume-mount-point <GUID> <drive-letter>\n");
	fprintf(stderr, "		Assign mountpoint (drive letter) to volume GUID.\n");
	fprintf(stderr, "	windrbd [opt] delete-volume-mount-point <drive-letter>\n");
	fprintf(stderr, "		Delete mountpoint (drive letter).\n");
	fprintf(stderr, "	windrbd [opt] hide-filesystem <drive-letter>\n");
	fprintf(stderr, "		Prepare existing drive for use as windrbd backing device.\n");
	fprintf(stderr, "	windrbd [opt] show-filesystem <drive-letter>\n");
	fprintf(stderr, "		Make backing device visible to Windows again.\n");
	fprintf(stderr, "		(You cannot use it as backing device after doing that)\n");
	fprintf(stderr, "	windrbd [opt] filesystem-state <drive-letter>\n");
	fprintf(stderr, "		Shows the current filesystem state (windows, windrbd, other)\n");
	fprintf(stderr, "	windrbd [opt] log-server [<log-file>]\n");
	fprintf(stderr, "		Logs windrbd kernel messages to stdout (and optionally to\n");
	fprintf(stderr, "		log-file)\n");
	fprintf(stderr, "Options are:\n");
	fprintf(stderr, "	-q (quiet): be a little less verbose.\n");

	exit(1);
}

	/* TODO: move those to user/shared/windrbd_helper.c */

enum volume_spec { VS_UNKNOWN, VS_DRIVE_LETTER, VS_GUID };

static int is_drive_letter(const char *drive)
{
	if (!isalpha(drive[0]))
		return 0;

	if (drive[1] != '\0') {
		if (drive[1] != ':' || drive[2] != '\0')
			return 0;
	}
	return 1;
}

static int is_guid(const char *arg)
{
        int i;
#define GUID_MASK "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"

        for (i=0;arg[i] != '\0';i++) {
                if (GUID_MASK[i] == 'x' && !isxdigit(arg[i]))
                        return 0;
                if (GUID_MASK[i] == '-' && arg[i] != '-')
                        return 0;
        }
        return arg[i] == '\0';
#undef GUID_MASK
}

static enum volume_spec volume_spec(const char *arg)
{
	if (arg == NULL)
		return VS_UNKNOWN;
	if (is_drive_letter(arg))
		return VS_DRIVE_LETTER;
	if (is_guid(arg))
		return VS_GUID;
	return VS_UNKNOWN;
}

static void check_drive_letter(const char *drive)
{
	if (!is_drive_letter(drive)) {
		fprintf(stderr, "Drive letter (%s) must start with a letter and have a colon.\n", drive);
		usage_and_exit();
	}
}

static enum volume_spec check_drive_letter_or_guid(const char *arg)
{
	enum volume_spec vs = volume_spec(arg);

	if (vs == VS_UNKNOWN) {
		fprintf(stderr, "Argument (%s) must be a drive letter or a GUID.\n", arg);
		usage_and_exit();
	}
	return vs;
}

enum drive_letter_ops {
	ASSIGN_DRIVE_LETTER, DELETE_DRIVE_LETTER
};

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
				if (!quiet)
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

#if 0
static int run_command(const char *command, char *args[])
{
	int ret;

	switch (fork()) {
	case 0:	
		execvp(command, args);
		exit(1);
	case -1:
		perror("fork");
		return 1;
	default:
		wait(&ret);
		return ret;
	}
}
#endif

#if 0
static int remount_volume(const char *drive)
{
	check_drive_letter(drive);


	wchar_t mount_point[10];
	wchar_t guid[80];
	char guid_ascii[80];
	int err;
	char *args[4];
	int i;
	
	swprintf(mount_point, sizeof(mount_point) / sizeof(*mount_point) -1, L"%s\\", drive);
	if (GetVolumeNameForVolumeMountPoint(mount_point, guid, sizeof(guid) / sizeof(*guid) - 1) == 0) {
		err = GetLastError();
		fprintf(stderr, "Couldn't get volume mount point for drive %s, err = %d\n", drive, err);
		return 1;
	}

// wprintf(L"GUID of drive letter %ls is %ls\n", mount_point, guid);

	args[0] = "mountvol";
	args[1] = (char*)drive;
	args[2] = "/p";
	args[3] = NULL;

	if (run_command("mountvol", args) != 0) {
		fprintf(stderr, "Failed to run mountvol\n");
		return 1;
	}
	for (i=0; guid[i] != 0; i++) {
		guid_ascii[i] = guid[i];
	}
	guid_ascii[i] = '\0';
// printf("GUID of drive letter %s is %s\n", drive, guid_ascii);
	args[2] = guid_ascii;
	if (run_command("mountvol", args) != 0) {
		fprintf(stderr, "Failed to run mountvol\n");
		return 1;
	}
	return 0;
#endif

#if 0
	if (DeleteVolumeMountPoint(mount_point) == 0) {
		err = GetLastError();
		fprintf(stderr, "Couldn't delete volume mount point for drive %s, err = %d\n", drive, err);
		return 1;
	}

printf("Mount point %s deleted.\n", drive);
printf("Press enter to continue.\n");
char x[10];
fgets(x, sizeof(x)-1, stdin);

	if (SetVolumeMountPoint(mount_point, guid) == 0) {
		err = GetLastError();
		fprintf(stderr, "Couldn't set volume mount point for drive %s, err = %d\n", drive, err);
		return 1;
	}
printf("Mount point %s set.\n", drive);

	return 0;
}
#endif

static int dismount_volume(HANDLE h)
{
	BOOL ret;
	DWORD size;
	int err;

	ret = DeviceIoControl(h, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &size, NULL);
	if (!ret) {
		err = GetLastError();
		fprintf(stderr, "DeviceIoControl(.., FSCTL_DISMOUNT_VOLUME, ..) failed with error %d\n", err);
		return -1;
	}
	return 0;
}

	

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
	int patched;

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
		if ((patched = patch_boot_sector(buf, op == SHOW_FILESYSTEM, 0))) {
			if (!quiet)
				printf("Filesystem on drive %s %s\n", drive, op == HIDE_FILESYSTEM ? "prepared for use with windrbd." : "reverted to be used directly.");
		} else
			if (!quiet)
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
		if (patched && op == HIDE_FILESYSTEM) {
			if (dismount_volume(h) != 0) {
				fprintf(stderr, "Couldn't dismount volume, please reboot before using this drive (%s)\n", drive);

				CloseHandle(h);
				return 1;
			}
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

int delete_mountpoint(const char *drive)
{
	wchar_t t_mountpoint[100];
	int err;
	BOOL ret;

	check_drive_letter(drive);
	swprintf(t_mountpoint, sizeof(t_mountpoint) / sizeof(*t_mountpoint) -1, L"%s\\", drive);

	ret = DeleteVolumeMountPoint(t_mountpoint);
	if (!ret) {
		err = GetLastError();
		fprintf(stderr, "DeleteVolumeMountPoint(%ls) failed with error %d\n", t_mountpoint, err);
		return 1;
	}
	return 0;
}

int set_mountpoint(const char *drive, const char *guid)
{
	wchar_t t_mountpoint[100];
	wchar_t t_guid[100];
	int err;
	BOOL ret;

	check_drive_letter(drive);
	swprintf(t_mountpoint, sizeof(t_mountpoint) / sizeof(*t_mountpoint) -1, L"%s\\", drive);
	swprintf(t_guid, sizeof(t_guid) / sizeof(*t_guid) -1, L"\\\\?\\Volume{%s}\\", guid);

	ret = SetVolumeMountPoint(t_mountpoint, t_guid);
	if (!ret) {
		err = GetLastError();
		fprintf(stderr, "SetVolumeMountPoint(%ls, %ls) failed with error %d\n", t_mountpoint, t_guid, err);
		return 1;
	}
	return 0;
}

int main(int argc, char ** argv)
{
	const char *op;
	char c;

	while ((c = getopt(argc, argv, "q")) != -1) {
		switch (c) {
			case 'q': quiet = 1; break;
			default: usage_and_exit();
		}
	}
	if (argc < optind+1) {
		usage_and_exit();
	}
	op = argv[optind];

	if (strcmp(op, "assign-drive-letter") == 0) {
		if (argc != optind+3) {
			usage_and_exit();
		}
		int minor = atoi(argv[optind+1]);
		const char *drive = argv[optind+2];

		return drive_letter_op(minor, drive, ASSIGN_DRIVE_LETTER);
	}
	if (strcmp(op, "delete-drive-letter") == 0) {
		if (argc != optind+3) {
			usage_and_exit();
		}
		int minor = atoi(argv[optind+1]);
		const char *drive = argv[optind+2];

		return drive_letter_op(minor, drive, DELETE_DRIVE_LETTER);
	}
	if (strcmp(op, "delete-volume-mount-point") == 0) {
		if (argc != optind+2) {
			usage_and_exit();
		}
		const char *mount_point = argv[optind+1];

		return delete_mountpoint(mount_point);
	}
	if (strcmp(op, "set-volume-mount-point") == 0) {
		if (argc != optind+3) {
			usage_and_exit();
		}
		const char *mount_point = argv[optind+1];
		const char *guid = argv[optind+2];

		return set_mountpoint(mount_point, guid);
	}
	if (strcmp(op, "hide-filesystem") == 0) {
		if (argc != optind+2) {
			usage_and_exit();
		}
		const char *drive = argv[optind+1];

		return patch_bootsector_op(drive, HIDE_FILESYSTEM);
	}
	if (strcmp(op, "show-filesystem") == 0) {
		if (argc != optind+2) {
			usage_and_exit();
		}
		const char *drive = argv[optind+1];

		return patch_bootsector_op(drive, SHOW_FILESYSTEM);
	}
	if (strcmp(op, "filesystem-state") == 0) {
		if (argc != optind+2) {
			usage_and_exit();
		}
		const char *drive = argv[optind+1];

		return patch_bootsector_op(drive, FILESYSTEM_STATE);
	}
	if (strcmp(op, "log-server") == 0) {
		if (argc < optind+1 || argc > optind+2) {
			usage_and_exit();
		}
		const char *log_file = argv[optind+1];

		return log_server_op(log_file);
	}

	usage_and_exit();
	return 0;
}
