#include "drbdsetup.h"
#include <linux/drbd.h>
#include <stdio.h>
#include "shared_tool.h"
#include "libgenl.h"
#include "shared_windrbd.h"
#include <unistd.h>

bool kernel_older_than(int version, int patchlevel, int sublevel)
{
	return true;
}

int conv_block_dev(struct drbd_argument *ad, struct msg_buff *msg,
		   struct drbd_genlmsghdr *dhdr, char* arg)
{
	/* we want to do simple conversions
		as C: -> \\DosDevices\\C: and GUIDs to
		\\DosDevices\\Volume{<GUID>} for convenience.
	*/

		/* TODO: PATH_MAX */
	char device[1024];
	size_t n;

	if (isalpha(arg[0]) && arg[1] == ':' && arg[2] == '\0') {
		n = snprintf(device, sizeof(device), "\\DosDevices\\%s", arg);
	} else if (is_guid(arg)) {
		n = snprintf(device, sizeof(device), "\\DosDevices\\Volume{%s}", arg);
	} else {
		n = snprintf(device, sizeof(device), "%s", arg);
	}
	if (n >= sizeof(device)) {
		fprintf(stderr, "Device name too long: %s (%zd), please report this.\n", arg, n);
		return OTHER_ERROR;
	}
	nla_put_string(msg, ad->nla_type, device);

	return NO_ERROR;
}

int genl_join_mc_group_and_ctrl(struct genl_sock *s, const char *name)
{
	return genl_join_mc_group(s, name);
}

int poll_hup(struct genl_sock *s, int timeout_ms)
{
	return 0;
}

static int run_command(const char *command, char *args[])
{
        int ret;

        switch (fork()) {
        case 0:
                execvp(command, args);
		perror("execvp");
                exit(1);
        case -1:
                perror("fork");
                return 1;
        default:
                wait(&ret);
                return ret;
        }
}

int modprobe_drbd(void)
{
	int ret;

	if (!windrbd_driver_loaded()) {
		char *args[] = { "sc", "start", NULL, NULL };
		fprintf(stderr, "WinDRBD driver not found, trying to start it.\n");
		args[2] = "windrbdlog";
		ret = run_command("sc", args);
		if (ret != 0)
			fprintf(stderr, "Couldn't start windrbd logging service.\n");

		args[2] = "windrbdumhelper";
		ret = run_command("sc", args);
		if (ret != 0)
			fprintf(stderr, "Couldn't start windrbd user mode helper service.\n");

		args[2] = "windrbd";
		ret = run_command("sc", args);
		if (ret != 0)
			fprintf(stderr, "Couldn't start windrbd driver.\n");

		if (!windrbd_driver_loaded()) {
			fprintf(stderr, "Start windrbd driver failed, maybe you need to update userland and/or kernel?\nDo you have permissions to start a driver (Administrator?)\nIf you don't have an officially signed driver, try executing bcdedit /set TESTSIGNING ON, reboot and try again.\n");
			return 0;
		} else {
			fprintf(stderr, "WinDRBD driver and services started.\n");
		}
	}
	return 1;
}

