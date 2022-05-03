#ifndef __SHARED_WINDRBD_H
#define __SHARED_WINDRBD_H

#include <stdint.h>
#include <windows.h>

int is_guid(const char *arg);
HANDLE do_open_root_device(int quiet);
int windrbd_driver_loaded(void);

char *windrbd_get_drbd_version(void);
char *windrbd_get_windrbd_version(void);

int windrbd_get_registry_string_value(HKEY root_key, const char *key, const char *value_name, unsigned char ** buf_ret, DWORD *buflen_ret, int verbose);

uint64_t bdev_size_by_handle(HANDLE h);
uint64_t bdev_size(int fd);

#endif
