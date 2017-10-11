#ifndef DRBDSETUP_COLORS_H
#define DRBDSETUP_COLORS_H

#include <linux/drbd.h>

enum when_color { NEVER_COLOR = -1, AUTO_COLOR = 0, ALWAYS_COLOR = 1 };
extern enum when_color opt_color;

extern const char *stop_color_code(void);
extern const char *role_color_start(enum drbd_role, bool);
extern const char *role_color_stop(enum drbd_role, bool);
extern const char *cstate_color_start(enum drbd_conn_state);
extern const char *cstate_color_stop(enum drbd_conn_state);
extern const char *repl_state_color_start(enum drbd_repl_state);
extern const char *repl_state_color_stop(enum drbd_repl_state);
extern const char *disk_state_color_start(enum drbd_disk_state, bool intentional, bool);
extern const char *disk_state_color_stop(enum drbd_disk_state, bool);
extern const char *quorum_color_start(bool);
extern const char *quorum_color_stop(bool);

#define REPL_COLOR_STRING(__r)  \
	repl_state_color_start(__r), drbd_repl_str(__r), repl_state_color_stop(__r)

#define DISK_COLOR_STRING(__d, __intentional, __local) \
	disk_state_color_start(__d, __intentional, __local), drbd_disk_str(__d), disk_state_color_stop(__d, __local)

#define ROLE_COLOR_STRING(__r, __local) \
	role_color_start(__r, __local), drbd_role_str(__r), role_color_stop(__r, __local)

#define CONN_COLOR_STRING(__c) \
	cstate_color_start(__c), drbd_conn_str(__c), cstate_color_stop(__c)

#endif  /* DRBDSETUP_COLORS_H */
