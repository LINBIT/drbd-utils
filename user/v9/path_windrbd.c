#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "shared_main.h"
#include "drbdadm.h"
#include "shared_windrbd.h"

char *conf_file[2];

	/* This reads the Registry key (once) and returns it together with the
	 * length. If registry key is not found falls back to pre-1.0.0
	 * location (never returns NULL).
	 * If length_ret is non-NULL returns length including terminating \0.
	 */

#define DEFAULT_ROOT "/cygdrive/c/windrbd"

static const char *windrbd_root(DWORD *length_ret)
{
	static char *root = NULL;
	static DWORD length;
	int err = 0;

	if (root == NULL) {
	        err = windrbd_get_registry_string_value(HKEY_LOCAL_MACHINE, "System\\CurrentControlSet\\Services\\WinDRBD", "WinDRBDRoot", (unsigned char**) &root, &length, 0);
	}
	if (err == 0 && root != NULL) {
		if (length_ret != NULL)
			*length_ret = length;
		return root;
	}
	if (length_ret != NULL)
		*length_ret = strlen(DEFAULT_ROOT)+1;
	return DEFAULT_ROOT;
}

	/* Concatenates $WINDRBD_ROOT and path. Returns result in
	 * buf (no more than buffer_size bytes). Returns NULL if
	 * one of the args is NULL or result exceeds buffer_size
	 * bytes.
	 */

char *relative_to_root(const char *path, char *buf, size_t buffer_size)
{
	DWORD buflen, rootlen;
	const char *root;

	if (path == NULL || buf == NULL) {
		fprintf(stderr, "Invalid argument to relative_to_root function.\n");
		exit(42);
	}

	root = windrbd_root(&buflen);
	rootlen = buflen-1;
	buflen += strlen(path);

	if (buflen > buffer_size-1) {
		fprintf(stderr, "Path length exceeds buffer size (%zd bytes, WinDRBD root is %s)\n", buffer_size, root);
		exit(42);
	}
	strcpy(buf, root);
	strcpy(buf+rootlen, path);

	return buf;
}

void generate_conf_file_locations(void)
{
	static char buf[MAX_PATH];

	conf_file[0] = relative_to_root("/etc/drbd.conf", buf, sizeof(buf));
	conf_file[1] = NULL;
}

char *drbd_lib_dir(void)
{
	static char buf[MAX_PATH];
	char *ret;

	if (buf[0])
		return buf;

	ret = relative_to_root("/var/lib/drbd", buf, sizeof(buf));
	if (ret)
		initialized = true;

	return ret;
}

char *drbd_run_dir(void)
{
	static char buf[MAX_PATH];
	static bool initialized;
	char *ret;

	if (initialized)
		return buf;

	ret = relative_to_root("/var/run/drbd", buf, sizeof(buf));
	if (ret)
		initialized = true;

	return ret;
}

char *drbd_run_dir_with_slash(void)
{
	static char buf[MAX_PATH];
	static bool initialized;
	char *ret;

	if (initialized)
		return buf;

	ret = relative_to_root("/var/run/drbd/", buf, sizeof(buf));
	if (ret)
		initialized = true;

	return ret;
}

char *node_id_file(void)
{
	static char buf[MAX_PATH];
	static bool initialized;
	char *ret;

	if (initialized)
		return buf;

	ret = relative_to_root("/var/lib/drbd/node_id", buf, sizeof(buf));
	if (ret)
		initialized = true;

	return ret;
}

char *drbd_bin_dir(void)
{
	static char buf[MAX_PATH];
	static bool initialized;
	char *ret;

	if (initialized)
		return buf;

	ret = relative_to_root("/usr/sbin", buf, sizeof(buf));
	if (ret)
		initialized = true;

	return ret;
}

char *drbd_lock_dir(void)
{
	static char buf[MAX_PATH];
	static bool initialized;
	char *ret;

	if (initialized)
		return buf;

	ret = relative_to_root("/var/lock", buf, sizeof(buf));
	if (ret)
		initialized = true;

	return ret;
}

