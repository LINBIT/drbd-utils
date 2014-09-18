#!/bin/sh
#
# DRBD fence-peer handler for Pacemaker 1.1 clusters
# (via stonith-ng).
#
# Requires that the cluster is running with STONITH
# enabled, and has configured and functional STONITH
# agents.
#
# Also requires that the DRBD disk fencing policy
# is at least "resource-only", but "resource-and-stonith"
# is more likely to be useful as most people will
# use this in dual-Primary configurations.
#
# Returns 7 on on success (DRBD fence-peer exit code
# for "yes, I've managed to fence this node").
# Returns 1 on any error (undefined generic error code,
# causes DRBD devices with the "resource-and-stonith"
# fencing policy to remain suspended).

log() {
  local msg
  msg="$1"
  logger -i -t "`basename $0`" -s "$msg"
}

die() { log "$*"; exit 1; }

die_unless_all_minors_up_to_date()
{
	set -- $DRBD_MINOR
	local n_minors=$#

	[ $n_minors != 0 ] ||
		die "Resource minor numbers unknown! Unable to proceed."

	# and build a "grep extended regex"
	local _OLDIFS=$IFS
	IFS="|"
	local minor_regex="^ *($*): cs:"
	IFS=$_OLDIFS

	# grep -c -Ee '^ *(m|i|n|o|r|s): cs:.* ds:UpToDate' /proc/drbd
	local proc_drbd=$(cat /proc/drbd)
	local minors_of_resource=$(echo "$proc_drbd" | grep -E -e "$minor_regex")
	local n_up_to_date=$(echo "$minors_of_resource" | grep -c -e "ds:UpToDate")

	log "n_minors: $n_minors; n_up_to_date: $n_up_to_date"
	[ "$n_up_to_date" = "$n_minors" ] ||
		die "$DRBD_RESOURCE(minor $DRBD_MINOR): some minor is not UpToDate, will not fence peer."
}

[ -n "$DRBD_PEERS" ] || die "DRBD_PEERS is empty or unset, cannot continue."
die_unless_all_minors_up_to_date

for p in $DRBD_PEERS; do
  stonith_admin --tolerance 5s --tag drbd --fence $p
  rc=$?
  if [ $rc -eq 0 ]; then
    log "stonith_admin successfully fenced peer $p."
  else
    die "Failed to fence peer $p. stonith_admin returned $rc."
  fi
done

exit 7
