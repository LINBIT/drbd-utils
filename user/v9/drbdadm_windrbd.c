#include "config.h"

#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include "shared_main.h"
#include "drbdtool_common.h"
#include "drbdadm.h"
#include "drbdadm_parser.h"
#include "shared_windrbd.h"

/* Name of the windrbd command, including path of drbdadm binary */

static char *windrbd = NULL;

/* List of commands executed by drbdadm */

struct cmd_helper helpers[] = {
	{"drbdsetup", &drbdsetup},
	{"drbdmeta", &drbdmeta},
	{"drbd-proxy-ctl", &drbd_proxy_ctl},
	{"drbdadm-83", &drbdadm_83},
	{"drbdadm-84", &drbdadm_84},
	{"windrbd", &windrbd},
	{NULL, NULL}
};

/* Which shell we are using for khelpers.
 *
 * We are not using PowerShell since it is way too slow... (and
 * returns strange exit codes sometimes).
 */

char *khelper_argv[] = { "/cygdrive/c/windows/system32/cmd", "/c", NULL, NULL };

static int is_driveletter(const char *drive)
{
        if (!isalpha(drive[0])) return 0;
        if (drive[1] != '\0') {
                if (drive[1] != ':' || drive[2] != '\0') {
			return 0;
                }
        }
	return 1;
}

static int call_windrbd(char *res_name, int flags, char *path, ...)
{
        char *argv[40];
	int argc = 0;
	va_list ap;
	char *arg;

	argv[argc++] = path;
	va_start(ap, path);
	while (argc < sizeof(argv)/sizeof(*argv)-1 &&
	       (arg = va_arg(ap, char*)) != NULL)
		argv[argc++] = arg;

	if (argc >= sizeof(argv)/sizeof(*argv)-1) {
		printf("Warning: argument list too long (%d elements)\n", argc);
		va_end(ap);
		return E_THINKO;
	}

	argv[argc++] = NULL;
	va_end(ap);

        return m_system_ex(argv, flags, res_name);
}

int before_attach(const struct cfg_ctx *ctx)
{
	int ret;
	ret = call_windrbd(ctx->res->name, SLEEPS_SHORT, windrbd, "-q", "hide-filesystem", ctx->vol->disk, NULL);
	if (ret) {
		printf("windrbd -q hide-filesystem returned exit status %d\n", ret);
		printf("(you normally can ignore this)\n");
	}
	return 0;
}

	/* Starting from WinDRBD 1.1.4 this just prints a warning.
           It now can present a partition as a disk with a virtual
           GPT partition table. Windows thinks it is a disk with
           one big partition, so we don't need the extra drive letter
           any more, since the drive letter can be assigned with the
           standard Windows tools.
	 */

int after_new_minor(const struct cfg_ctx *ctx)
{
	if (is_driveletter(ctx->vol->device)) {
		fprintf(stderr, "Warning: Starting from WinDRBD 1.1.4 the drive letter (%s) has to be assigned\nwith Windows tools (partition manager, diskpart). The drive letter setting in\nthe WinDRBD config file is ignored.\nTo get rid of this warning remove the drive letter setting in the WinDRBD\nconfig file.\n", ctx->vol->device);
	}

	return 0;
}

int after_primary(const struct cfg_ctx *ctx)
{
	struct d_volume *vol;

	for_each_volume(vol, &ctx->res->me->volumes) {
		if (is_driveletter(vol->device)) {
			call_windrbd(ctx->res->name, SLEEPS_SHORT, windrbd, "-q", "add-drive-in-explorer", vol->device, NULL);
		}
	}

	return 0;
}

int after_secondary(const struct cfg_ctx *ctx)
{
	struct d_volume *vol;

	for_each_volume(vol, &ctx->res->me->volumes)
		if (is_driveletter(vol->device))
			call_windrbd(ctx->res->name, SLEEPS_SHORT, windrbd, "-q", "remove-drive-in-explorer", vol->device, NULL);

	return 0;
}

void parse_device(struct names* on_hosts, struct d_volume *vol)
{
	struct d_name *h;
	int m;

	switch (yylex()) {
	case TK_STRING:
		vol->device = yylval.txt;
		if ((on_hosts == NULL || hostname_in_list(hostname, on_hosts))
			&& !is_driveletter(vol->device)
			&& vol->device[0] != '\0'
		) {
			log_err("%s:%d: device name must be either empty or a drive letter\n",
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
				log_err("%s:%d: no minor given nor device name contains a minor number\n",
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
	}
out:
	if (!on_hosts)
		return;

	STAILQ_FOREACH(h, on_hosts, link) {
		check_uniq_file_line(vol->v_config_file, vol->v_device_line,
			"device-minor", "device-minor:%s:%u", h->name, vol->device_minor);
		if (vol->device && vol->device[0] != '\0')
			check_uniq_file_line(vol->v_config_file, vol->v_device_line,
				"device", "device:%s:%s", h->name, vol->device);
	}
}

void maybe_add_bin_dir_to_path(void)
{
        add_component_to_path(drbd_bin_dir());
}

void print_platform_specific_versions(void)
{
	char *windrbd_version = windrbd_get_windrbd_version();
	printf("WINDRBD_VERSION=%s\n", shell_escape(windrbd_version));
}

void assign_default_device(struct d_volume *vol)
{
	if (vol == NULL) {
		fprintf(stderr, "BUG: vol is NULL in assign_default_device()\n");
		exit(E_THINKO);
	}
	if (!vol->device)
		m_asprintf(&vol->device, "");
}

void disk_to_kernel_path(const char *disk, char *buf, size_t buflen)
{
	size_t n;

        if (isalpha(disk[0]) && disk[1] == ':' && disk[2] == '\0') {
                n = snprintf(buf, buflen, "\\DosDevices\\%s", disk);
        } else if (is_guid(disk)) {
                n = snprintf(buf, buflen, "\\DosDevices\\Volume{%s}", disk);
        } else {
                n = snprintf(buf, buflen, "%s", disk);
        }
	if (n >= buflen) {
		fprintf(stderr, "Warning: Device name too long: %s (%zd), please report this.\n", disk, n);
		exit(E_THINKO);
	}
}

int is_same_disk(const char *a, const char *b)
{
		/* Typically these hold values like \\DosDevices\\X: or
		 * \\DosDevices\\Volume{fae34719-85ac-11eb-a4c7-080027305288}\\
		 * so 1K buffer size should be enough. We exit gracefully if
		 * it isn't.
		 */
	char buf_a[1024], buf_b[1024];

	disk_to_kernel_path(a, buf_a, sizeof(buf_a));
	disk_to_kernel_path(b, buf_b, sizeof(buf_b));

	return !strcmp(buf_a, buf_b);
}

