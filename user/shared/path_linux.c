#include "config.h"

char *drbd_lib_dir(void)
{
	return DRBD_LIB_DIR;
}

char *node_id_file(void)
{
	return DRBD_LIB_DIR"/node_id";
}

char *drbd_run_dir(void)
{
	return DRBD_RUN_DIR;
}

char *drbd_run_dir_with_slash(void)
{
	return DRBD_RUN_DIR "/";
}

char *drbd_bin_dir(void)
{
	return DRBD_BIN_DIR;
}

char *drbd_lock_dir(void)
{
	return DRBD_LOCK_DIR;
}

