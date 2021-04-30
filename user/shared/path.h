/* Under Windows path names are computed at run time (based upon
 * a registry entry) while on Linux those are configure-time
 * string constants. Those functions here return the correct
 * path (in POSIX style format, e.g. /cygdrive/x/... under Windows).
 */

char *drbd_lib_dir(void);
char *node_id_file(void);
char *drbd_run_dir(void);
char *drbd_run_dir_with_slash(void);
char *drbd_bin_dir(void);
char *drbd_lock_dir(void);

