#ifndef __DRBDADM_WINDRBD_H
#define __DRBDADM_WINDRBD_H

int is_driveletter(const char *drive);
int call_windrbd(char *res_name, char *path, ...);

#endif
