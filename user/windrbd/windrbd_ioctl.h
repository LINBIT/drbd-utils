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

#endif
