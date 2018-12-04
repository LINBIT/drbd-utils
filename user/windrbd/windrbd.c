#define UNICODE 1
#define _GNU_SOURCE 1

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
#include <ctype.h>
#include <sys/queue.h>
#include <assert.h>

#include <winioctl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <dbt.h>

#include "shared_windrbd.h"
#include <windrbd/windrbd_ioctl.h>



static int quiet = 0;
static int force = 0;

void usage_and_exit(void)
{
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "	windrbd [opt] assign-drive-letter <minor> <drive-letter>\n");
	fprintf(stderr, "		Assign drive letter to windrbd device for this user.\n");
	fprintf(stderr, "	windrbd [opt] delete-drive-letter <minor> <drive-letter>\n");
	fprintf(stderr, "		Delete drive letter to windrbd device for this user.\n");
	fprintf(stderr, "	windrbd [opt] set-volume-mount-point <GUID> <drive-letter>\n");
	fprintf(stderr, "		Assign mountpoint (drive letter) to volume GUID. (deprecated)\n");
	fprintf(stderr, "	windrbd [opt] delete-volume-mount-point <drive-letter>\n");
	fprintf(stderr, "		Delete mountpoint (drive letter). (deprecated)\n");
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
	fprintf(stderr, "	windrbd [opt] add-drive-in-explorer <drive-letter>\n");
	fprintf(stderr, "		Tells Windows Explorer that drive has been created.\n");
	fprintf(stderr, "	windrbd [opt] remove-drive-in-explorer <drive-letter>\n");
	fprintf(stderr, "		Tells Windows Explorer that drive has been removed.\n");
	fprintf(stderr, "	windrbd [opt] inject-faults <n> <where> [<drive-letter>]\n");
	fprintf(stderr, "		Inject faults on completion after n requests. Turn off\n");
	fprintf(stderr, "		fault injection if n is negative. Where is anything out of\n");
	fprintf(stderr, "		[all|backing|meta]-[request|completion]. Drive must be\n");
	fprintf(stderr, "		specified unless all is given.\n");
	fprintf(stderr, "	windrbd [opt] user-mode-helper-daemon\n");
	fprintf(stderr, "		Run user mode helper daemon. Receives commands from\n");
	fprintf(stderr, "		kernel driver if something interresting happens, runs\n");
	fprintf(stderr, "		them and returns result to kernel.\n");
	fprintf(stderr, "	windrbd [opt] set-mount-point-for-minor <minor> <mount-point>\n");
	fprintf(stderr, "		Assign mountpoint (drive letter) to DRBD minor.\n");
	fprintf(stderr, "	windrbd [opt] print-exe-path\n");
	fprintf(stderr, "		Print (UNIX) path to this program\n");
	fprintf(stderr, "	windrbd [opt] dump-memory-allocations\n");
	fprintf(stderr, "		Cause kernel to dump (via printk) currently allocated memory\n");
	fprintf(stderr, "Options are:\n");
	fprintf(stderr, "	-q (quiet): be a little less verbose.\n");
	fprintf(stderr, "	-f (force): do it even if it is dangerous.\n");

	exit(1);
}

	/* TODO: move those to user/shared/windrbd_helper.c */

enum volume_spec { VS_UNKNOWN, VS_DRIVE_LETTER, VS_GUID };

static int is_drive_letter(const char *drive)
{
	if (drive == NULL)
		return 0;

	if (!isalpha(drive[0]))
		return 0;

	if (drive[1] != '\0') {
		if (drive[1] != ':' || drive[2] != '\0')
			return 0;
	} else
		return 0;

	return 1;
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

static enum fault_injection_location str_to_fault_location(const char *s)
{
	static char *str[] = {
		[ON_ALL_REQUESTS_ON_REQUEST] = "all-request",
		[ON_ALL_REQUESTS_ON_COMPLETION] = "all-completion",
		[ON_META_DEVICE_ON_REQUEST] = "meta-request",
		[ON_META_DEVICE_ON_COMPLETION] = "meta-completion",
		[ON_BACKING_DEVICE_ON_REQUEST] = "backing-request",
		[ON_BACKING_DEVICE_ON_COMPLETION] = "backing-completion"
	};
	enum fault_injection_location i;

	for (i=0; i<AFTER_LAST_FAULT_LOCATION; i++)
		if (strcmp(str[i], s) == 0)
			return i;

	return INVALID_FAULT_LOCATION;
}

static int is_windrbd_device(HANDLE h)
{
        DWORD size;
        BOOL ret;
        int err;

        ret = DeviceIoControl(h, IOCTL_WINDRBD_IS_WINDRBD_DEVICE, NULL, 0, NULL, 0, &size, NULL);
	if (ret)
		return 1;

        err = GetLastError();
	if (err != ERROR_INVALID_FUNCTION)
		printf("Warning: device returned strange error code %d\n", err);

	return 0;
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

	if (!quiet)
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
	enum volume_spec vs = check_drive_letter_or_guid(drive);
        HANDLE h;
        DWORD err;

        wchar_t fname[100];
	if (vs == VS_GUID)
	        swprintf(fname, sizeof(fname) / sizeof(fname[0]), L"\\\\?\\Volume{%s}", drive);
	else
	        swprintf(fname, sizeof(fname) / sizeof(fname[0]), L"\\\\.\\%s", drive);

        h = CreateFile(fname, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        err = GetLastError();

	if (err != ERROR_SUCCESS) {
		fprintf(stderr, "Couldn't open drive %s, error is %d\n", drive, err);
		if (err == ERROR_ACCESS_DENIED)
			fprintf(stderr, "You have to be administrator to do that\n");

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
	HANDLE h = do_open_device(drive);
	if (h == INVALID_HANDLE_VALUE)
		return 1;

	if (op != FILESYSTEM_STATE && !force && is_windrbd_device(h)) {
		printf("This is a windrbd device. Patching the bootsector on\n");
		printf("windrbd devices is neither sane nor does it work nor\n");
		printf("does it make any sense to do so. You will not be able to\n");
		printf("mount this filesystem on peers if you do this. Furthermore,\n");
		printf("a hide-filesytem cannot be undone (except by formatting the\n");
		printf("device).\n\n");
		printf("If you really want to do this, use -f\n");

		return 1;
	}

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
			printf("Filesystem on drive %s %s\n", drive, op == HIDE_FILESYSTEM ? "prepared for use with windrbd." : "reverted to be used directly.");
			if (op == HIDE_FILESYSTEM) {
				printf("You can access your data via the DRBD device (specified in the device parameter\n");
				printf("of your drbd.conf file). Do not access the backing device directly, always use\n");
				printf("the DRBD device to access the data. To make it visible again (only if you\n");
				printf("don't want to use DRBD on that device ever again), do a\n");
				printf("	windrbd show-filesystem %s\n", drive);
			}
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

// Converts UNIX-style line breaks (\n) to DOS-style line breaks (\r\n)
//
// Line breaks that are already in DOS-style (\r\n) are not converted
//
// src_bfr is only scanned for the first src_bfr_len bytes,
// or up to the first '\0', whichever comes first.
//
// Parameters:
//     src_bfr		Source buffer containing the text to convert
//     src_bfr_len      Size of the source buffer
//     dst_bfr          Destination buffer; receives converted output text
//     dst_bfr_len      Size of the destination buffer
//     truncated        Pointer to a bool variable or NULL pointer
//                      If non-NULL, will be set to indicate whether the output was truncated
//                      true if truncated, false otherwise
static size_t unix_to_dos(
	const char* const   src_bfr,
	const size_t        src_bfr_len,
	char *const         dst_bfr,
	const size_t        dst_bfr_len,
	bool *const         truncated
)
{
	assert(dst_bfr_len >= 1);

	size_t src_idx = 0;
	size_t dst_idx = 0;
	bool cr_flag = false;

	while (src_idx < src_bfr_len && dst_idx < dst_bfr_len - 1 && src_bfr[src_idx] != '\0') {
		if (src_bfr[src_idx] == '\n' && !cr_flag) {
			dst_bfr[dst_idx] = '\r';
			cr_flag = true;
		} else {
			cr_flag = src_bfr[src_idx] == '\r';
			dst_bfr[dst_idx] = src_bfr[src_idx];
			++src_idx;
		}
		++dst_idx;
	}
	dst_bfr[dst_idx] = '\0';

	if (truncated != NULL) {
		*truncated = src_idx < src_bfr_len && src_bfr[src_idx] != '\0';
	}

	return dst_idx;
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
	my_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

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
	char dosbuf[1024];
	ssize_t len;
	size_t doslen;

	printf("Waiting for log messages from windrbd kernel driver.\r\n");
	printf("Press Ctrl-C to stop.\r\n");
	while ((len = recv(s, buf, sizeof(buf)-1, 0)) >= 0) {
		if (len > sizeof(buf)-1)	/* just to be sure ... */
			len = sizeof(buf)-1;

		buf[len] = '\0';
		doslen = unix_to_dos(buf, len, dosbuf, sizeof(dosbuf), NULL);
		write(1, dosbuf, doslen);
		if (fd >= 0) {
			if (write(fd, dosbuf, doslen) < 0)
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

enum explorer_ops {
	ADD_DRIVE, REMOVE_DRIVE
};

int notify_explorer(const char *drive, enum explorer_ops op)
{
	wchar_t t_drive[100];
	DEV_BROADCAST_VOLUME dev_broadcast_volume = {
		sizeof(DEV_BROADCAST_VOLUME),
		DBT_DEVTYP_VOLUME
	};
	DWORD_PTR dwp;

	check_drive_letter(drive);
	if (islower(drive[0]))
		dev_broadcast_volume.dbcv_unitmask = 1 << (drive[0] - 'a');
	else
		dev_broadcast_volume.dbcv_unitmask = 1 << (drive[0] - 'A');

	swprintf(t_drive, sizeof(t_drive) / sizeof(*t_drive) -1, L"%s", drive);

	switch (op) {
	case ADD_DRIVE:

			/* Taken from imdisk source: cpl/drvio.c:1576 */

		SHChangeNotify(SHCNE_DRIVEADD, SHCNF_PATH, t_drive, NULL);

		SendMessageTimeout(HWND_BROADCAST,
			WM_DEVICECHANGE,
			DBT_DEVICEARRIVAL,
			(LPARAM)&dev_broadcast_volume,
			SMTO_BLOCK | SMTO_ABORTIFHUNG,
			4000,
			&dwp);

		dev_broadcast_volume.dbcv_flags = DBTF_MEDIA;

		SendMessageTimeout(HWND_BROADCAST,
			WM_DEVICECHANGE,
			DBT_DEVICEARRIVAL,
			(LPARAM)&dev_broadcast_volume,
			SMTO_BLOCK | SMTO_ABORTIFHUNG,
			4000,
			&dwp);

		SendMessageTimeout(HWND_BROADCAST,
			WM_DEVICECHANGE,
			DBT_DEVNODES_CHANGED,
			(LPARAM)0,
			SMTO_BLOCK | SMTO_ABORTIFHUNG,
			4000,
			&dwp);

		break;

	case REMOVE_DRIVE:

			/* Taken from imdisk source: cpl/drvio.c:735 */

		SHChangeNotify(SHCNE_DRIVEREMOVED, SHCNF_PATH, t_drive, NULL);

		SendMessageTimeout(HWND_BROADCAST,
			WM_DEVICECHANGE,
			DBT_DEVICEREMOVECOMPLETE,
			(LPARAM)&dev_broadcast_volume,
			SMTO_BLOCK | SMTO_ABORTIFHUNG,
			4000,
			&dwp);

		SendMessageTimeout(HWND_BROADCAST,
			WM_DEVICECHANGE,
			DBT_DEVNODES_CHANGED,
			0,
			SMTO_BLOCK | SMTO_ABORTIFHUNG,
			4000,
			&dwp);

		break;

	default:
		fprintf(stderr, "not implemented\n");
	}
	return 0;
}

static int inject_faults(const char *drive, enum fault_injection_location where, int after, const char *s)
{
        DWORD size;
        BOOL ret;
        int err;
	int root = where <= ON_ALL_REQUESTS_ON_COMPLETION;
	int req;
	struct windrbd_ioctl_fault_injection after_struct;
	HANDLE h;

	if (root) {
		h = do_open_root_device(quiet);
		req = IOCTL_WINDRBD_ROOT_INJECT_FAULTS;
	} else {
		h = do_open_device(drive);
		req = IOCTL_WINDRBD_INJECT_FAULTS;
	}

	if (h == INVALID_HANDLE_VALUE)
		return 1;

	after_struct.after = after;
	after_struct.where = where;
        ret = DeviceIoControl(h, req, &after_struct, sizeof(after_struct), NULL, 0, &size, NULL);

	if (!quiet) {
		if (ret) {
			if (after < 0)
				printf("Turned off faults injection on %s.\n", s);
			else
				printf("Injected faults on %s after %d requests.\n", s, after);
		} else {
			err = GetLastError();
			printf("Could not set fault injection (error code %d), is this a WinDRBD device? Does the backing device exist (not Diskless)?\n", err);
		}
	}
	CloseHandle(h);

	return !ret;
}

#define USER_MODE_HELPER_POLLING_INTERVAL_MS 100

static HANDLE um_root_dev_handle;

struct process {
	pid_t pid;
	struct windrbd_usermode_helper *cmd;
	LIST_ENTRY(process) list_entry;
};

static LIST_HEAD(process_head, process) process_head =
	LIST_HEAD_INITIALIZER(process_head);

static int check_for_retvals(void)
{
	int retval;
	pid_t child_pid;
	struct process *p;
	struct windrbd_usermode_helper_return_value rv;
	DWORD unused;
	BOOL ret;
	int err;

	while (1) {
		child_pid = waitpid(-1, &retval, WNOHANG);
		if (child_pid < 0) {
			if (errno != ECHILD) {
				perror("wait");
				printf("Error waiting for child process in signal handler.\n");
				return -1;
			}
			return 0;
		}
		if (child_pid == 0)
			return 0;

		LIST_FOREACH(p, &process_head, list_entry) {
			if (p->pid == child_pid) {
				rv.id = p->cmd->id;
				if (WIFSIGNALED(retval))
						/* Linux does it that way */
					rv.retval = WTERMSIG(retval)+128;
				else
					rv.retval = retval;

				free(p->cmd);
				LIST_REMOVE(p, list_entry);
				free(p);

				if (!quiet) {
					if (WIFSIGNALED(retval))
						printf("handler was terminated by signal %d\n", WTERMSIG(retval));
					else
						printf("handler terminated and returned exit status %d\n", retval);
				}
				ret = DeviceIoControl(um_root_dev_handle, IOCTL_WINDRBD_ROOT_SEND_USERMODE_HELPER_RETURN_VALUE, &rv, sizeof(rv), NULL, 0, &unused, NULL);
				if (!ret) {
					err = GetLastError();
					printf("Error in sending ioctl to kernel, err is %d\n", err);
				}
				break;
			}
		}
		if (p == NULL)
			printf("Warning: Process %d not found on process list\n", child_pid);
	}
	return 0;
}

static int exec_command(struct windrbd_usermode_helper *next_cmd)
{
	char **argv;
	char **envp;
	char *cmd;
	int i;

	char *s;

	argv = malloc((next_cmd->argc+1)*sizeof(argv[0]));
	if (argv == NULL)
		return -ENOMEM;
	envp = malloc((next_cmd->envc+1)*sizeof(envp[0]));
	if (envp == NULL) {
		free(argv);
		return -ENOMEM;
	}

/* later: due to installation problems we cannot do that now:
	cmd = drbdadm_path;
	s = &next_cmd->data[0];
*/
	cmd = &next_cmd->data[0];
	s = cmd;

	for (i=0;i<next_cmd->argc;i++) {
		while (*s) s++;
		s++;
		argv[i] = s;
	}
	argv[i] = NULL;
	for (i=0;i<next_cmd->envc;i++) {
		while (*s) s++;
		s++;

			/* TODO: later we want a cygrunsrv fixed that can
			 * handle pathes with whitespace. For now we can
			 * live with that hardcoded path.
			 */

		if (strncmp(s, "PATH=", 5) == 0)
			envp[i]="PATH=/cygdrive/c/Program Files/WinDRBD:/cygdrive/c/windrbd/usr/sbin:/cygdrive/c/Windows/system32:/cygdrive/c/Windows:/cygdrive/c/Windows/System32/Wbem:/cygdrive/c/Windows/System32/WindowsPowerShell/v1.0";
		else
			envp[i] = s;
	}
	envp[i] = NULL;

	if (!quiet) {
		printf("about to exec %s ...\n", cmd);
		for (i=0;argv[i]!=NULL;i++)
			printf("%s ", argv[i]);
		printf("\nEnvironment: \n");
		for (i=0;envp[i]!=NULL;i++)
			printf("%s\n", envp[i]);
		printf("\n(pid is %d)\n", getpid());
	}
	execvpe(cmd, argv, envp);
	perror("execvpe");
	printf("Could not exec %s\n", cmd);
	exit(102);
}

static struct process *add_command_to_process_list(struct windrbd_usermode_helper *next_cmd)
{
	struct process *p;

	p = malloc(sizeof(*p));
	if (p == NULL) {
		printf("Could not allocate memory for process struct\n");
		return NULL;
	}
	p->cmd = next_cmd;
	LIST_INSERT_HEAD(&process_head, p, list_entry);

	return p;
}

static int fork_and_exec_command(struct windrbd_usermode_helper *next_cmd)
{
	pid_t pid;
	struct process *p = add_command_to_process_list(next_cmd);

	switch (pid = fork()) {
	case 0:
		exec_command(next_cmd);
		exit(101);

	case -1:
		perror("fork");
		printf("Cannot fork process\n");
		return -1;

	default:
		p->pid = pid;
	}
	return 0;
}

static int get_exe_path(char *buf, size_t bufsize)
{
	int fd = open("/proc/self/exename", O_RDONLY);
	size_t len;

	if (fd < 0) {
		perror("open /proc/self/exename");
		fprintf(stderr, "Could not open /proc/self/exename, does /proc exist?\n");

		return -1;
	}
	len = read(fd, buf, bufsize-1);
	if (len < 0) {
		perror("read /proc/self/exename");
		fprintf(stderr, "Could not read /proc/self/exename\n");
		close(fd);

		return -1;
	} else {
		buf[len] = 0;
	}
	close(fd);

	return 0;
}

static int print_exe_path(void)
{
	char buf[4096];
	int ret;

	ret = get_exe_path(buf, sizeof(buf));
	if (ret == 0)
		printf("%s\n", buf);

	return ret;
}

#if 0

/* This is currently defined to be the drbdadm in the same directory
 * as this windrbd utility is running. The value that comes out of
 * the kernel is currently ignored. This is good enough (c) for
 * now, later, we may implement an ioctl for the kernel module
 * to set the path (linux does this by module parameters) and
 * use that value again.
 *
 * Update: currently not used, installation problems. Binary path
 * hardcoded to /cygdrive/c/windrbd/usr/sbin
 */

static char drbdadm_path[4096];

/* Set user mode helper to the same path this program is located.
 * This is currently disabled, since we have C:\windrbd\usr\sbin
 * hardcoded anyway (for a couple of other reasons). Uncomment
 * this code if the feature is eventually needed.
 */

static int set_um_helper(void)
{
	int ret;
	char *p;

	ret = get_exe_path(drbdadm_path, sizeof(drbdadm_path));
	if (ret == 0) {
		p = strrchr(drbdadm_path, '/');
		if (p == NULL)
			p = drbdadm_path;
		else
			p++;
		strcpy(p, "drbdadm");
	} else {
		fprintf(stderr, "Couldn't get path to drbdadm, trying default\n");
		strcpy(drbdadm_path, "/cygdrive/c/windrbd/usr/sbin/drbdadm");
	}

	if (!quiet)
		printf("We will use %s for user mode helper (drbdadm)\n", drbdadm_path);

	return ret;
}

#endif

static int user_mode_helper_daemon(void)
{
	struct windrbd_usermode_helper get_size;
	struct windrbd_usermode_helper *next_cmd;
	DWORD size, size2;
	int err;
	BOOL ret;

	if (!quiet) {
		printf("Starting WinDRBD user mode helper daemon\n");
		printf("Press Ctrl-C to stop.\n");
	}

		/* We might be started when the driver isn't started
		 * yet. Keep on trying until we succeed or get killed.
		 */

	while (1) {
		um_root_dev_handle = do_open_root_device(quiet);
		if (um_root_dev_handle != INVALID_HANDLE_VALUE)
			break;
		sleep(1);
	}
	if (!quiet)
		printf("Connected to WinDRBD kernel driver\n");

/* Later: */
/*	set_um_helper(); */

	while (1) {
		ret = DeviceIoControl(um_root_dev_handle, IOCTL_WINDRBD_ROOT_RECEIVE_USERMODE_HELPER, NULL, 0, &get_size, sizeof(get_size), &size, NULL);
		if (!ret) {
			err = GetLastError();
			printf("Error in sending ioctl to kernel, err is %d\n", err);
			break;
		}
		if (size > 0) {
			size_t req_size = get_size.total_size;

			next_cmd = malloc(req_size);
			if (next_cmd == NULL) {
				printf("Could not alloc %zd bytes for command, aborting\n", req_size);
				break;
			}
			ret = DeviceIoControl(um_root_dev_handle, IOCTL_WINDRBD_ROOT_RECEIVE_USERMODE_HELPER, NULL, 0, next_cmd, req_size, &size2, NULL);
			if (!ret) {
				err = GetLastError();
				printf("Error in sending ioctl to kernel, err is %d\n", err);
				break;
			}
			if (size2 != req_size) {
				printf("Size mismatch from ioctl: expected %zd actual %d\n", req_size, size2);
				break;
			}
			fork_and_exec_command(next_cmd);
		} /* else nothing to do, wait a little and poll again */

			/* This checks for terminated child processes and
			 * sends their return values to the windrbd driver.
			 */
		check_for_retvals();

		usleep(USER_MODE_HELPER_POLLING_INTERVAL_MS*1000);
	}

		/* We are not cleaning up here, since the handlers might
		 * do something useful. If user pressed Ctrl-C, however
		 * handlers will get killed (by SIGINT), since they are
		 * also attached to the terminal.
		 */

	return -1;
}

/* mount_point is in UTF-8 encoding */
int set_mount_point_for_minor(int minor, const char *mount_point)
{
	int wcchars;
	int wcchars2;
	int mmp_len;
	DWORD unused;

	struct windrbd_minor_mount_point *mmp;
	HANDLE root_dev;
	BOOL ret;
	int err;

	root_dev = do_open_root_device(quiet);
	if (root_dev == INVALID_HANDLE_VALUE)
		return 1;

	if (mount_point == NULL)
		mount_point = "";

		/* do not pass strlen(mount_point) as length, if you do,
		 * the resulting string will not be zero-terminated.
		 */
	wcchars = MultiByteToWideChar(CP_UTF8, 0, mount_point, -1, NULL, 0);

	mmp_len = wcchars*sizeof(wchar_t) + sizeof(*mmp);
	mmp = malloc(mmp_len);
	if (mmp == NULL) {
		fprintf(stderr, "Could not allocate buffer for ioctl\n");
		return -1;
	}
	wcchars2 = MultiByteToWideChar(CP_UTF8, 0, mount_point, -1, &mmp->mount_point[0], wcchars);

	if (wcchars2 != wcchars) {
		fprintf(stderr, "Conversion error\n");
		free(mmp);
		return -1;
	}
	mmp->minor = minor;

	ret = DeviceIoControl(root_dev, IOCTL_WINDRBD_ROOT_SET_MOUNT_POINT_FOR_MINOR, mmp, mmp_len, NULL, 0, &unused, NULL);
	if (!ret) {
		err = GetLastError();
		if (err == ERROR_BUSY)
			fprintf(stderr, "Device is mounted, please do a drbdadm secondary to change the mount point.\n");
		else
			fprintf(stderr, "Error in sending ioctl to kernel, err is %d\n", err);

		free(mmp);
		return -1;
	}
	if (!quiet) {
		if (mount_point[0] != '\0')
			printf("Mount point for minor %d set to %ls\n", minor, mmp->mount_point);
		else
			printf("Not mounting minor %d\n", minor);
	}

	free(mmp);
	return 0;
}

int dump_memory_allocations(void)
{
	HANDLE root_dev;
	DWORD unused;
	BOOL ret;
	int err;

	root_dev = do_open_root_device(quiet);
	if (root_dev == INVALID_HANDLE_VALUE)
		return 1;

	ret = DeviceIoControl(root_dev, IOCTL_WINDRBD_ROOT_DUMP_ALLOCATED_MEMORY, NULL, 0, NULL, 0, &unused, NULL);
	if (!ret) {
		err = GetLastError();
		fprintf(stderr, "Error in sending ioctl to kernel, err is %d\n", err);
		return -1;
	}
	return 0;
}

int main(int argc, char ** argv)
{
	const char *op;
	char c;

		/* When running as service, we are redirected to
		 * files, always flush printf's.
		 */

	setbuf(stdout, NULL);
	while ((c = getopt(argc, argv, "qf")) != -1) {
		switch (c) {
			case 'q': quiet = 1; break;
			case 'f': force = 1; break;
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
	if (strcmp(op, "add-drive-in-explorer") == 0) {
		if (argc != optind+2) {
			usage_and_exit();
		}
		const char *drive = argv[optind+1];

		return notify_explorer(drive, ADD_DRIVE);
	}
	if (strcmp(op, "remove-drive-in-explorer") == 0) {
		if (argc != optind+2) {
			usage_and_exit();
		}
		const char *drive = argv[optind+1];

		return notify_explorer(drive, REMOVE_DRIVE);
	}
	if (strcmp(op, "inject-faults") == 0) {
		if (argc != optind+3 && argc != optind+4) {
			usage_and_exit();
		}
		int after = atoi(argv[optind+1]);
		const char *where_str = argv[optind+2];
		enum fault_injection_location where = str_to_fault_location(where_str);
		const char *drive = argv[optind+3];

		if (where == INVALID_FAULT_LOCATION) {
			usage_and_exit();
		}
		return inject_faults(drive, where, after, where_str);
	}
	if (strcmp(op, "user-mode-helper-daemon") == 0)
		return user_mode_helper_daemon();

	if (strcmp(op, "set-mount-point-for-minor") == 0) {
		if (argc != optind+2 && argc != optind+3) {
			usage_and_exit();
		}
		int minor = atoi(argv[optind+1]);
		const char *mount_point = argv[optind+2];

		return set_mount_point_for_minor(minor, mount_point);
	}
	if (strcmp(op, "print-exe-path") == 0)
		return print_exe_path();

	if (strcmp(op, "dump-memory-allocations") == 0)
		return dump_memory_allocations();

	usage_and_exit();
	return 0;
}
