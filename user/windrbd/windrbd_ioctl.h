/* TODO: this file is currently duplicated between windrbd and
 * drbd-utils repos. It finally should reside in drbd-headers
 * (windrbd subdirectory).
 */

#ifndef WINDRBD_IOCTL_H
#define WINDRBD_IOCTL_H

/* For compiling this for drbd-utils when there are no Windows headers
 * installed, we need this (taken from ReactOS): Hopefully this never
 * changes.
 */

#ifndef CTL_CODE
#define CTL_CODE( DeviceType, Function, Method, Access ) (                 \
    ((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method) \
)
#endif

#define WINDRBD_ROOT_DEVICE_NAME "windrbd_control"

/* TODO: are these used by someone else? Doc states that <= 0x8000
 * is reserved by Microsoft, but it does not state how to obtain
 * such a number. Plus the WINDRBD_DEVICEs appear as FILE_DEVICE_DISK.
 */

#define WINDRBD_DEVICE_TYPE 0xab26
#define WINDRBD_ROOT_DEVICE_TYPE 0xab27

#define IOCTL_WINDRBD_ROOT_IS_WINDRBD_ROOT_DEVICE CTL_CODE(WINDRBD_ROOT_DEVICE_TYPE, 1, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_WINDRBD_IS_WINDRBD_DEVICE CTL_CODE(WINDRBD_DEVICE_TYPE, 1, METHOD_BUFFERED, FILE_ANY_ACCESS)

enum fault_injection_location {
	INVALID_FAULT_LOCATION = -1,
	ON_ALL_REQUESTS_ON_REQUEST = 0,
	ON_ALL_REQUESTS_ON_COMPLETION,
	ON_META_DEVICE_ON_REQUEST,
	ON_META_DEVICE_ON_COMPLETION,
	ON_BACKING_DEVICE_ON_REQUEST,
	ON_BACKING_DEVICE_ON_COMPLETION,
	AFTER_LAST_FAULT_LOCATION
};

struct windrbd_ioctl_fault_injection {
		/* Inject faults after this number requests (and keep
		 * injecting faults). If 0, inject now. If < 0 do not
		 * inject faults (any more, this is the default).
		 */
	int after;
	enum fault_injection_location where;
};

#define IOCTL_WINDRBD_ROOT_INJECT_FAULTS CTL_CODE(WINDRBD_ROOT_DEVICE_TYPE, 2, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_WINDRBD_INJECT_FAULTS CTL_CODE(WINDRBD_DEVICE_TYPE, 2, METHOD_BUFFERED, FILE_ANY_ACCESS)

struct windrbd_ioctl_genl_portid {
	u32 portid;
};

struct windrbd_ioctl_genl_portid_and_multicast_group {
	u32 portid;
        char name[GENL_NAMSIZ];
};

/* Send netlink packet(s) to kernel.
 *
 * Input buffer: the netlink packet.
 * Output buffer: none.
 *
 * Call multiple times if there are more than one netlink request.
 * Return packet(s) to be fetched by receive nl packet ioctl().
 */

#define IOCTL_WINDRBD_ROOT_SEND_NL_PACKET CTL_CODE(WINDRBD_ROOT_DEVICE_TYPE, 3, METHOD_BUFFERED, FILE_ANY_ACCESS)

/* Receive netlink packet(s) from kernel.
 *
 * Input buffer: the port id (getpid()) in a struct windrbd_ioctl_genl_portid
 * Output buffer: the netlink reply packet(s).
 *
 * Call multiple times if there are more reply packets than the output buffer
 * can hold. Output buffer should hold at least NLMSG_GOODSIZE bytes,
 * the actual size is returned by the lpBytesReturned parameter to
 * DeviceIoControl().
 *
 * Does not wait for packets to arrive, use POLL ioctl for waiting for
 * packets.
 */

#define IOCTL_WINDRBD_ROOT_RECEIVE_NL_PACKET CTL_CODE(WINDRBD_ROOT_DEVICE_TYPE, 4, METHOD_BUFFERED, FILE_ANY_ACCESS)

/* Add port ID to multicast group.
 *
 * Input buffer: the port id (getpid()) and name of the multicast group
 * 		 in a struct windrbd_ioctl_genl_portid_and_multicast_group
 * Output buffer: none.
 *
 * Adds the portid to multicast group specified in input buffer. As a
 * consequence, everything DRBD sends to that multicast group can be
 * received by the RECEIVE_NL_PACKET ioctl.
 *
 * Currently DRBD only uses the 'events' multicast group, however this
 * may change in the future. Note that WinDRBD has no notion of netlink
 * families since there is only DRBD to support.
 */

#define IOCTL_WINDRBD_ROOT_JOIN_MC_GROUP CTL_CODE(WINDRBD_ROOT_DEVICE_TYPE, 5, METHOD_BUFFERED, FILE_ANY_ACCESS)

/* Something > 0x10, this is the value current kernels (4.1x) use.
 * Do not change.
 */

#define WINDRBD_NETLINK_FAMILY_ID	28

struct windrbd_usermode_helper {
		/* ID given by kernel to find return value request later. */
	int id;

		/* The total size of the helper struct including all data
		 * and this header information. If not enough space
		 * is provided this member contains the space needed
		 */
	size_t total_size;

		/* Since we cannot map a NULL pointer over the ioctl()
		 * interface, we store the number of the args (and env)
		 * in seperate arguments here.
		 */
	int argc;
	int envc;

		/* Data:
		 * cmd<0>arg1<0>arg2<0>...argn<0>env1<0>env2<0> ... envn<0>
		 * the above members determine how many args/how many envs.
		 */
	char data[0];
};

struct windrbd_usermode_helper_return_value {
	int id;

		/* The return value of the handler. As far as I can tell
		 * nothing else is transferred to the kernel (no stdout/
		 * stderr).
		 */
	int retval;
};

/* This is for calling usermode helpers.
 *
 * Input: None
 * Output: a struct windrbd_usermode_helper with variable data member.
 *
 * Linux has a built-in call_usermode_helper() function which we need
 * to emulate. With this ioctl a usermode daemon retrieves commands
 * (with args and env) to run from the kernel (there may be 0-n
 * daemons running). Daemons return the return value of the handler
 * in a IOCTL_WINDRBD_ROOT_SEND_USERMODE_HELPER_RETURN_VALUE later.
 * There is a timeout for sending this (also to handle the case
 * where no daemon is running). Linux DRBD also has this timeout
 * in order to not get stuck on hanging handlers.
 *
 * The size of the output buffer should be at least 8192 bytes, in
 * case the ioctl() returns ERROR_INSUFFICIENT_BUFFER retry
 * with a bigger buffer.
 */

#define IOCTL_WINDRBD_ROOT_RECEIVE_USERMODE_HELPER CTL_CODE(WINDRBD_ROOT_DEVICE_TYPE, 6, METHOD_BUFFERED, FILE_ANY_ACCESS)

/* This is for returning the exit status of usermode helpers to the kernel.
 * Input: a windrbd_usermode_helper_return_value containing id and retvalue.
 * Output: none
 *
 * See IOCTL_WINDRBD_ROOT_RECEIVE_USERMODE_HELPER ioctl for more details.
 */

#define IOCTL_WINDRBD_ROOT_SEND_USERMODE_HELPER_RETURN_VALUE CTL_CODE(WINDRBD_ROOT_DEVICE_TYPE, 7, METHOD_BUFFERED, FILE_ANY_ACCESS)

#endif
