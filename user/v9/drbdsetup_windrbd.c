#include "drbdsetup.h"
#include <linux/drbd.h>
#include <stdio.h>
#include "shared_tool.h"
#include "libgenl.h"
#include "shared_windrbd.h"
#include <unistd.h>
#include <poll.h>
#include <windrbd/windrbd_ioctl.h>

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

		/* This is larger than PATH_MAX which is 260 */
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

char *kernel_device_to_userland_device(char *kernel_dev)
{
		/* This is larger than PATH_MAX which is 260 */
	static char device[1024];
	size_t n;

	if (strncmp(kernel_dev, "\\DosDevices\\", strlen("\\DosDevices\\")) == 0) {
		n = snprintf(device, sizeof(device)-1, "\\\\.\\%s", &kernel_dev[strlen("\\DosDevices\\")]);
	} else {
			/* just a strcpy ... */
		n = snprintf(device, sizeof(device)-1, "%s", kernel_dev);
	}
	if (n >= sizeof(device)-1) {
		fprintf(stderr, "Device name too long: %s (%zd), please report this.\n", kernel_dev, n);
		device[sizeof(device)-1] = '\0';
	}
	return device;
}

int genl_join_mc_group_and_ctrl(struct genl_sock *s, const char *name)
{
	return genl_join_mc_group(s, name);
}

#define BUSY_POLLING_INTERVAL_MS 100

int poll_hup(struct genl_sock *s, int timeout_ms, int extra_poll_fd)
{
	struct windrbd_ioctl_genl_portid p;
	unsigned int size;
	int err;
	int forever;
	int there_are_netlink_packets;

	int ret;
	struct pollfd pollfds[2];

	p.portid = getpid();
	forever = timeout_ms < 0;
	while (forever || timeout_ms > 0) {
	        if (DeviceIoControl(s->s_handle, IOCTL_WINDRBD_ROOT_ARE_THERE_NL_PACKETS, &p, sizeof(p), &there_are_netlink_packets, sizeof(there_are_netlink_packets), &size, NULL) == 0) {
		        err = GetLastError();
			fprintf(stderr, "DeviceIoControl() failed, error is %d\n", err);
			if (err == ERROR_ACCESS_DENIED) /* 5 */
				fprintf(stderr, "(are you running with Administrator privileges?)\n");
			if (err == ERROR_NO_MORE_ITEMS) /* 259, the driver is about to shut down */
				fprintf(stderr, "(WinDRBD driver is about to shut down)\n");

			return E_POLL_ERR;
		}
		if (there_are_netlink_packets)
			return 0;

		pollfds[0].fd = 1;
		pollfds[0].events = POLLHUP;
		pollfds[1].fd = extra_poll_fd;
		pollfds[1].events = POLLIN;

		ret = poll(pollfds, ARRAY_SIZE(pollfds), BUSY_POLLING_INTERVAL_MS);
		if (ret < 0)
			return E_POLL_ERR;

		if (pollfds[0].revents == POLLERR || pollfds[0].revents == POLLHUP)
			return E_POLL_ERR;
		if (pollfds[1].revents & POLLIN)
			return E_POLL_EXTRA_FD;
		if (pollfds[1].revents & (POLLERR | POLLHUP))
			return E_POLL_ERR;
	}
	if (timeout_ms <= 0)
		return E_POLL_TIMEOUT;

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

