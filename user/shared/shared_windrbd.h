#ifndef __SHARED_WINDRBD_H
#define __SHARED_WINDRBD_H

#include <windows.h>

int is_guid(const char *arg);
HANDLE do_open_root_device(int quiet);
int windrbd_driver_loaded(void);

#endif
