#include "config.h"
#include "drbdadm.h"
#include "drbdtool_common.h"
#include "drbdadm_parser.h"

#include <string.h>
#include <stdlib.h>

/* This file contains hooks for things that do something with
 * WinDRBD. Most of them are empty for Linux.
 */

/* List of commands executed by drbdadm */

struct cmd_helper helpers[] = {
	{"drbdsetup", &drbdsetup},
	{"drbdmeta", &drbdmeta},
	{"drbd-proxy-ctl", &drbd_proxy_ctl},
	{"drbdadm-83", &drbdadm_83},
	{"drbdadm-84", &drbdadm_84},
	{NULL, NULL}
};

/* Which shell we are using for khelpers. */

char *khelper_argv[] = { "/bin/sh", "-c", NULL, NULL };

void maybe_add_bin_dir_to_path(void)
{
}

int before_attach(const struct cfg_ctx *ctx)
{
	return 0;
}

int after_new_minor(const struct cfg_ctx *ctx)
{
	return 0;
}

int after_primary(const struct cfg_ctx *ctx)
{
	return 0;
}

int after_secondary(const struct cfg_ctx *ctx)
{
	return 0;
}

extern void check_minor_nonsense(const char *devname, const int explicit_minor);
extern void pe_expected(const char *exp);

void parse_device(struct names* on_hosts, struct d_volume *vol)
{
	struct d_name *h;
	int m;

	switch (yylex()) {
	case TK_STRING:
		if (!strncmp("drbd", yylval.txt, 4)) {
			m_asprintf(&vol->device, "/dev/%s", yylval.txt);
			free(yylval.txt);
		} else
			vol->device = yylval.txt;
		if ((on_hosts == NULL || hostname_in_list(hostname, on_hosts))
			&& strncmp("/dev/drbd", vol->device, 9)
		) {
			err("%s:%d: device name must start with /dev/drbd\n"
			    "\t(/dev/ is optional, but drbd is required)\n",
			    config_file, fline);
			config_valid = 0;
			/* no goto out yet,
			 * as that would additionally throw a parse error */
		}
		switch (yylex()) {
		default:
			pe_expected("minor | ;");
			/* fall through */
		case ';':
			m = dt_minor_of_dev(vol->device);
			if (m < 0) {
				err("%s:%d: no minor given nor device name contains a minor number\n",
				    config_file, fline);
				config_valid = 0;
			}
			vol->device_minor = m;
			goto out;
		case TK_MINOR:
			; /* double fall through */
		}
	case TK_MINOR:
		EXP(TK_INTEGER);
		vol->device_minor = atoi(yylval.txt);
		EXP(';');

		/* if both device name and minor number are explicitly given,
		 * force /dev/drbd<minor-number> or /dev/drbd_<arbitrary> */
		if (on_hosts == NULL || hostname_in_list(hostname, on_hosts))
			check_minor_nonsense(vol->device, vol->device_minor);
	}
out:
	if (!on_hosts)
		return;

	STAILQ_FOREACH(h, on_hosts, link) {
		check_uniq_file_line(vol->v_config_file, vol->v_device_line,
			"device-minor", "device-minor:%s:%u", h->name, vol->device_minor);
		if (vol->device)
			check_uniq_file_line(vol->v_config_file, vol->v_device_line,
				"device", "device:%s:%s", h->name, vol->device);
	}
}

void print_platform_specific_versions(void)
{
}
