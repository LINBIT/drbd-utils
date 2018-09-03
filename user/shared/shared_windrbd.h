#include <windows.h>

int is_guid(const char *arg);
HANDLE do_open_root_device(int quiet);
int windrbd_driver_loaded(void);
