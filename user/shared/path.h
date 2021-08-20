/* Under Windows path names are computed at run time (based upon
 * a registry entry) while on Linux those are configure-time
 * string constants. Those functions here return the correct
 * path (in POSIX style format, e.g. /cygdrive/x/... under Windows).
 */

const char *drbd_lib_dir(void);
const char *node_id_file(void);
const char *drbd_run_dir(void);
const char *drbd_run_dir_with_slash(void);
const char *drbd_bin_dir(void);
const char *drbd_lock_dir(void);

