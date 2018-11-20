#ifndef __SHARED_WINDRBD_H
#define __SHARED_WINDRBD_H

#include <windows.h>

int is_guid(const char *arg);
HANDLE do_open_root_device(int quiet);
int windrbd_driver_loaded(void);

char *windrbd_get_drbd_version(void);
char *windrbd_get_windrbd_version(void);

#endif
