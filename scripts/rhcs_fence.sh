#!/bin/bash

Author="Lars Ellenberg <lars.ellenberg@linbit.com>"
Date="2014-09-18"
Version="1.0"
License="GPL v2+"
#
# Inspired by the rhcs_fence perl version
# by Digimer (digimer@alteeve.ca)
# Alteeve's Niche! - https://alteeve.ca/w/
# As found at https://github.com/digimer/rhcs_fence/releases/tag/0.2.8
#
# This program ties Linbit's DRBD into Red Hat's RHCS's fence daemon via the
# 'fence_node' shell call.

# from environment
# if tools do not yet export the plural,
# use the (deprecated) singular.
: ${DRBD_PEERS:=$DRBD_PEER}

PROG=${0##*/}
: ${DEBUG:=1}
LOG_PRIO=daemon.warning
LOG_TAG="$PROG[$$] $DRBD_PEERS: $DRBD_RESOURCE(minor $DRBD_MINOR)"

##############################################################################
# helper functions
##############################################################################

die() { echo "$*"; exit 1; }

all_minors_up_to_date()
{
	set -- $DRBD_MINOR
	local n_minors=$#

	[[ $n_minors != 0 ]] ||
		die "Resource minor numbers unknown! Unable to proceed."

	# build a "grep extended regex"
	local _OLDIFS=$IFS
	IFS="|"
	local minor_regex="^ *($*): cs:"
	IFS=$_OLDIFS

	# grep -c -Ee '^ *(m|i|n|o|r|s): cs:.* ds:UpToDate' /proc/drbd
	local proc_drbd=$(</proc/drbd)
	local minors_of_resource=$(echo "$proc_drbd" | grep -E -e "$minor_regex")
	local n_up_to_date=$(echo "$minors_of_resource" | grep -c -e "ds:UpToDate")

	debug "n_minors: $n_minors; n_up_to_date: $n_up_to_date"
	[[ $n_up_to_date = $n_minors ]] # return code is propagated
}

wait_for_fence_domain_state_change()
{
	local retries=$1 i
	for (( i=0; $i < $retries; i++ )); do
		sleep 1

		# canonicalize white space by word splitting
		# append one space (last line is "members")
		# for easier pattern matching on member id)
		set -- $(fence_tool ls)
		fence_tool_ls="$* "
		debug "$i: fence_tool ls: $fence_tool_ls"

		wait_condition && return 0
	done
	echo "still not met wait condition after $i retries"
	return 1	# timed out
}

wait_for_id_to_become_victim()
{
	wait_condition() [[ $fence_tool_ls = *"victim now $id "* ]]
	echo "waiting for $id to become victim"
	wait_for_fence_domain_state_change 30 # return value propagates
}

wait_for_id_to_drop_out_of_membership()
{
	wait_condition() [[ $fence_tool_ls != *"members"*" $id "* ]]
	echo "waiting for $id to drop out of membership"
	wait_for_fence_domain_state_change 240 || return 1
	echo "successfully fenced $name (by fenced)"
	return 0
}

eject_target()
{
	# tell cman to eject this node
	debug "call: cman_tool kill -n $name"
	cman_tool kill -n $name

	# if it was member in the fence domain,
	# wait for it to become victim,
	# wait for it to drop out of the membership.
	if [[ $fence_tool_ls = *"members"*" $id "* ]]; then
		wait_for_id_to_become_victim &&
		wait_for_id_to_drop_out_of_membership &&
		return 0 # Yes!
	fi
	return 1
}

##############################################################################
# logging preparations
##############################################################################

# Funky redirection to avoid logger feeding its own output to itself accidentally.
# Funky double exec to avoid an intermediate sub-shell.
# Sometimes, the sub-shell lingers around, keeps file descriptors open,
# and logger then won't notice the main script has finished,
# forever waiting for further input.
# The second exec replaces the subshell, and logger will notice directly
# when its stdin is closed once the main script exits.
# This avoids the spurious logger processes.
if test -t 2 ; then
	[[ $DEBUG != 0 ]] &&
	exec 3> >( exec 1>&- logger -s -p ${LOG_PRIO%.*}.debug -t "$LOG_TAG: DEBUG" )
	exec 1> >( exec 1>&- logger -s -p $LOG_PRIO            -t "$LOG_TAG" )
else
	[[ $DEBUG != 0 ]] &&
	exec 3> >( exec 1>&- 2>&- logger -p ${LOG_PRIO%.*}.debug -t "$LOG_TAG: DEBUG" )
	exec 1> >( exec 1>&- 2>&- logger -p $LOG_PRIO            -t "$LOG_TAG" )
fi
# and now point stderr to logger, too
exec 2>&1
if [[ $DEBUG = 0 ]]; then
	debug() { :; }
else
	debug() { echo >&3 "$*" ; }
	if [[ $DEBUG -gt 1 ]]; then
		BASH_XTRACEFD=3
		set -x
	fi
fi

##############################################################################
# "main"
##############################################################################

[[ $DRBD_PEERS ]] || die "No target list specified. You need to pass DRBD_PEERS via environment."

all_minors_up_to_date || die "some minor device is NOT 'UpToDate', will not fence peer"

for peer in $DRBD_PEERS; do
	for name in $peer ${peer%%.*}; do
		set -- $(cman_tool -F id,type nodes -n $name)
		id=$1 state=$2
		[[ $id ]] && break
	done
	if [[ -z $id ]] || [[ $id = *[!0-9]* ]] ; then
		die "could not resolve cman node id of $peer, giving up"
	fi
	echo "resolved $peer as cman node $name, id $id, state $state"

	# record fence domain state now
	set -- $(fence_tool ls)
	fence_tool_ls="$* "
	debug "fence_tool ls: $fence_tool_ls"

	if [[ $state = M ]] ; then
		eject_target && continue # with next peer, if any
	else
		# maybe cman noticed before the handler triggered,
		# and fencing is already active anyways.
		if [[ $fence_tool_ls = *"victim now $id "* ]]; then
			wait_for_id_to_drop_out_of_membership && continue # with next peer, if any
		fi
	fi

	# apparently it was not in the member list.
	# or we timed out waiting for fenced
	debug "trying direct fence of $name"
	dash_v=-v
	[[ $DEBUG -gt 1 ]] && dash_v=-vv
	echo "fence_node $dash_v $name"
	if fence_node $dash_v $name ; then
		echo "successfully fenced $name"
		continue # with next peer, if any
	else
		die "fencing $name failed, giving up"
	fi
done

# if we fenced more than one peer,
# add an other log line
[[ $peer != $DRBD_PEERS ]] &&
echo "SUCCESSFULLY FENCED $DRBD_PEERS"
exit 7
