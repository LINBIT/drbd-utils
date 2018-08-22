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

struct windrbd_ioctl_genl_portid_and_timeout {
	u32 portid;
	s32 timeout;
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

/* Poll for netlink packets.
 *
 * Input buffer: the port id (getpid()) and timeout (in milliseconds) in a
 * 		 struct windrbd_ioctl_genl_portid_and_timeout
 * Output buffer: none.
 *
 * Use this as a replacement to poll(2) for polling for new netlink packets
 * to arrive from DRBD kernel. TODO: somehow check for signals (POLLHUP).
 */

#define IOCTL_WINDRBD_ROOT_POLL_NL_PACKET CTL_CODE(WINDRBD_ROOT_DEVICE_TYPE, 5, METHOD_BUFFERED, FILE_ANY_ACCESS)

/* Add port ID to multicast group.
 *
 * Input buffer: the port id (getpid()) and name of the multicast group
 * 		 in a struct windrbd_ioctl_genl_portid_and_multicast_group
 * Output buffer: none.
 *
 * Adds the portid to multicast group specified in input buffer. As a
 * consequence, everything DRBD sends to that multicast group can be
 * received by the RECEIVE_NL_PACKET ioctl and be polled for with
 * the POLL_NL_PACKET ioctl.
 *
 * Currently DRBD only uses the 'events' multicast group, however this
 * may change in the future. Note that WinDRBD has no notion of netlink
 * families since there is only DRBD to support.
 */

#define IOCTL_WINDRBD_ROOT_JOIN_MC_GROUP CTL_CODE(WINDRBD_ROOT_DEVICE_TYPE, 6, METHOD_BUFFERED, FILE_ANY_ACCESS)

/* This is for calling usermode helpers. Interface has yet to be defined.
 * (Linux has a built-in call_usermode_helper() function which we need
 * to emulate).
 */
#define IOCTL_WINDRBD_ROOT_DRBD_USERMODE_HELPER CTL_CODE(WINDRBD_ROOT_DEVICE_TYPE, 7, METHOD_BUFFERED, FILE_ANY_ACCESS)

#endif
