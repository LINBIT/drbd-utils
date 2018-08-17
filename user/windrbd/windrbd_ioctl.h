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

/* Input buffer: the netlink packet. Output buffer: the netlink reply packet.
 * Call multiple times if there are more than one netlink request/reply
 * sequences. Output buffer should hold at least NLMSG_GOODSIZE bytes,
 * the actual size is returned by changing the output size parameter.
 */
#define IOCTL_WINDRBD_ROOT_DRBD_CMD CTL_CODE(WINDRBD_ROOT_DEVICE_TYPE, 3, METHOD_BUFFERED, FILE_ANY_ACCESS)

/* Poll for DRBD event. There may be 0-n processes waiting for an event,
 * in which case all processes get the event delivered. On input, name
 * of the netlink multicast group is expected (only 'events' is currently
 * used, see ./drbd-headers/linux/drbd_genl.h:352 GENL_mc_group(events)).
 */
#define IOCTL_WINDRBD_ROOT_DRBD_EVENT CTL_CODE(WINDRBD_ROOT_DEVICE_TYPE, 4, METHOD_BUFFERED, FILE_ANY_ACCESS)

/* This is for calling usermode helpers. Interface has yet to be defined.
 * (Linux has a built-in call_usermode_helper() function which we need
 * to emulate).
 */
#define IOCTL_WINDRBD_ROOT_DRBD_USERMODE_HELPER CTL_CODE(WINDRBD_ROOT_DEVICE_TYPE, 5, METHOD_BUFFERED, FILE_ANY_ACCESS)

#endif
