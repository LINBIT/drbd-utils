#!/bin/bash
#
# notify.sh -- a notification handler for various DRBD events.
# This is meant to be invoked via a symlink in /usr/lib/drbd,
# by drbdadm's userspace callouts.

# try to get possible output on stdout/err to syslog
PROG=${0##*/}

redirect_to_logger()
{
	local lf=${1:-local5}
	case $lf in
	# do we want to exclude some?
	auth|authpriv|cron|daemon|ftp|kern|lpr|mail|news|syslog|user|uucp|local[0-7])
		: OK ;;
	*)
		echo >&2 "invalid logfacility: $lf"
		return
		;;
	esac

	# Funky redirection to avoid logger feeding its own output to itself accidentally.
	# Funky double exec to avoid an intermediate sub-shell.
	# Sometimes, the sub-shell lingers around, keeps file descriptors open,
	# and logger then won't notice the main script has finished,
	# forever waiting for further input.
	# The second exec replaces the subshell, and logger will notice directly
	# when its stdin is closed once the main script exits.
	# This avoids the spurious logger processes.
	exec > >( exec 1>&- 2>&- logger -t "$PROG[$$]" -p $lf.info ) 2>&1
}

if [[ $- != *x* ]]; then
	# you may override with --logfacility below
	redirect_to_logger local5
fi

TEMP=$(getopt -o l: --long logfacility: -- "$@")
eval set -- "$TEMP"
while [[ $# != 0 ]]; do
	case $1 in
	-l|--logfacility)
		redirect_to_logger $2
		shift
		;;
	--)
		;;
	*)
		RECIPIENT=${1:-root}
		;;
	esac
	shift
done

# Default to sending email to root, unless otherwise specified
: ${RECIPIENT:=root}

if [[ $DRBD_VOLUME ]]; then
	pretty_print="$DRBD_RESOURCE/$DRBD_VOLUME (drbd$DRBD_MINOR)"
else
	pretty_print="$DRBD_RESOURCE"
fi

echo "invoked for $pretty_print"

# check arguments specified on command line
if [ -z "$RECIPIENT" ]; then
	echo "You must specify a notification recipient when using this handler." >&2
	exit 1
fi

# check envars normally passed in by drbdadm
for var in DRBD_RESOURCE DRBD_PEER; do
	if [ -z "${!var}" ]; then
		echo "Environment variable \$$var not found (this is normally passed in by drbdadm)." >&2
		exit 1
	fi
done

: ${DRBD_CONF:="usually /etc/drbd.conf"}

DRBD_LOCAL_HOST=$(hostname)

case "$0" in
	*split-brain.sh)
		SUBJECT="DRBD split brain on resource $pretty_print"
		BODY="
DRBD has detected split brain on resource $pretty_print
between $DRBD_LOCAL_HOST and $DRBD_PEER.
Please rectify this immediately.
Please see http://www.drbd.org/users-guide/s-resolve-split-brain.html for details on doing so."
		;;
	*out-of-sync.sh)
		SUBJECT="DRBD resource $pretty_print has out-of-sync blocks"
		BODY="
DRBD has detected out-of-sync blocks on resource $pretty_print
between $DRBD_LOCAL_HOST and $DRBD_PEER.
Please see the system logs for details."
		;;
    *io-error.sh)
		SUBJECT="DRBD resource $pretty_print detected a local I/O error"
		BODY="
DRBD has detected an I/O error on resource $pretty_print
on $DRBD_LOCAL_HOST.
Please see the system logs for details."
		;;
	*pri-lost.sh)
		SUBJECT="DRBD resource $pretty_print is currently Primary, but is to become SyncTarget on $DRBD_LOCAL_HOST"
		BODY="
The DRBD resource $pretty_print is currently in the Primary
role on host $DRBD_LOCAL_HOST, but lost the SyncSource election
process."
		;;
	*pri-lost-after-sb.sh)
		SUBJECT="DRBD resource $pretty_print is currently Primary, but lost split brain auto recovery on $DRBD_LOCAL_HOST"
		BODY="
The DRBD resource $pretty_print is currently in the Primary
role on host $DRBD_LOCAL_HOST, but was selected as the split
brain victim in a post split brain auto-recovery."
		;;
	*pri-on-incon-degr.sh)
		SUBJECT="DRBD resource $pretty_print no longer has access to valid data on $DRBD_LOCAL_HOST"
		BODY="
DRBD has detected that the resource $pretty_print
on $DRBD_LOCAL_HOST has lost access to its backing device,
and has also lost connection to its peer, $DRBD_PEER.
This resource now no longer has access to valid data."
		;;
	*emergency-reboot.sh)
		SUBJECT="DRBD initiating emergency reboot of node $DRBD_LOCAL_HOST"
		BODY="
Due to an emergency condition, DRBD is about to issue a reboot
of node $DRBD_LOCAL_HOST. If this is unintended, please check
your DRBD configuration file ($DRBD_CONF)."
		;;
	*emergency-shutdown.sh)
		SUBJECT="DRBD initiating emergency shutdown of node $DRBD_LOCAL_HOST"
		BODY="
Due to an emergency condition, DRBD is about to shut down
node $DRBD_LOCAL_HOST. If this is unintended, please check
your DRBD configuration file ($DRBD_CONF)."
		;;
	*)
		SUBJECT="Unspecified DRBD notification"
		BODY="
DRBD on $DRBD_LOCAL_HOST was configured to launch a notification handler
for resource $pretty_print,
but no specific notification event was set.
This is most likely due to DRBD misconfiguration.
Please check your configuration file ($DRBD_CONF)."
		;;
esac

echo "$BODY" | mail -s "$SUBJECT" $RECIPIENT
