#!/bin/bash
# ^^ for better syntax highlighting
# to be sourced from ocf:linbit:drbd and crm-fence-peer.sh, maybe others.

_sh_status_process() {
	# _volume not present should not happen,
	# but may help make this agent work even if it talks to drbd 8.3.
	: ${_volume:=0}
	# not-yet-created volumes are reported as -1
	(( _volume >= 0 )) || _volume=$[1 << 16]
	DRBD_ROLE_LOCAL[$_volume]=${_role:-Unconfigured}
	DRBD_ROLE_REMOTE[$_volume]=${_peer:-Unknown}
	DRBD_CSTATE[$_volume]=$_cstate
	DRBD_DSTATE_LOCAL[$_volume]=${_disk:-Unconfigured}
	DRBD_DSTATE_REMOTE[$_volume]=${_pdsk:-DUnknown}
}
_drbd_set_status_variables_from_sh_status() {
	DRBD_ROLE_LOCAL=()
	DRBD_ROLE_REMOTE=()
	DRBD_CSTATE=()
	DRBD_DSTATE_LOCAL=()
	DRBD_DSTATE_REMOTE=()

	# drbdsetup sh-status prints these values to stdout,
	# and then prints _sh_status_process.
	#
	# if we eval that, we do not need several drbdadm/drbdsetup commands
	# to figure out the various aspects of the state.
	local _minor _res_name _known _cstate _role _peer _disk _pdsk
	local _volume
	local _flags_susp _flags_aftr_isp _flags_peer_isp _flags_user_isp
	local _resynced_percent
	local out

	if $DRBD_HAS_MULTI_VOLUME ; then
		out="$($DRBDSETUP sh-status "$DRBD_RESOURCE")" && eval "$out"
	else
		# without "MULTI_VOLUME", the DRBD_DEVICES array
		# should contain exactly one value
		out="$($DRBDSETUP "$DRBD_DEVICES" sh-status)" && eval "$out"
	fi

	# if there was no output at all, or a weird output
	# make sure the status arrays won't be empty.
	[[ ${#DRBD_ROLE_LOCAL[@]}    != 0 ]] || DRBD_ROLE_LOCAL=(Unconfigured)
	[[ ${#DRBD_ROLE_REMOTE[@]}   != 0 ]] || DRBD_ROLE_REMOTE=(Unknown)
	[[ ${#DRBD_CSTATE[@]}        != 0 ]] || DRBD_CSTATE=(Unconfigured)
	[[ ${#DRBD_DSTATE_LOCAL[@]}  != 0 ]] || DRBD_DSTATE_LOCAL=(Unconfigured)
	[[ ${#DRBD_DSTATE_REMOTE[@]} != 0 ]] || DRBD_DSTATE_REMOTE=(DUnknown)

	: == DEBUG == DRBD_ROLE_LOCAL    == "${DRBD_ROLE_LOCAL[@]}" ==
	: == DEBUG == DRBD_ROLE_REMOTE   == "${DRBD_ROLE_REMOTE[@]}" ==
	: == DEBUG == DRBD_CSTATE        == "${DRBD_CSTATE[@]}" ==
	: == DEBUG == DRBD_DSTATE_LOCAL  == "${DRBD_DSTATE_LOCAL[@]}" ==
	: == DEBUG == DRBD_DSTATE_REMOTE == "${DRBD_DSTATE_REMOTE[@]}" ==

	status_primary=false
	status_unconfigured=false
	case ${DRBD_ROLE_LOCAL[*]} in
	*Primary*) status_primary=true ;;
	*Secondary*) : "at least it is configured" ;;
	*)	status_unconfigured=true ;;
	esac

	case ${DRBD_ROLE_REMOTE[*]} in
	*Primary*) status_some_peer_primary=true ;;
	esac

	local tmp
	status_pdsk_all_up_to_date=true
	status_pdsk_any_unknown=false
	for tmp in ${DRBD_DSTATE_REMOTE[*]}; do
		: $tmp
		case $tmp in
		UpToDate)	continue ;;
		DUnknown)	status_pdsk_all_up_to_date=false
				status_pdsk_any_unknown=true
				break
				;;
		*)		status_pdsk_all_up_to_date=false
		esac
	done
	status_disk_all_up_to_date=true
	status_disk_all_consistent=true
	status_disk_transitional_state=false
	for tmp in ${DRBD_DSTATE_LOCAL[*]}; do
		: $tmp
		case $tmp in
		UpToDate) continue ;;
		Diskless|Failed|Inconsistent|Outdated)
			status_disk_all_consistent=false ;;
		Attaching|Negotiating)
			status_disk_transitional_state=true ;;
		esac
		status_disk_all_up_to_date=false
	done
	status_some_peer_all_up_to_date=$status_pdsk_all_up_to_date
	status_some_peer_any_unknown=$status_pdsk_any_unknown
	$status_pdsk_all_up_to_date \
		&& status_some_peer_not_all_up_to_date=false \
		|| status_some_peer_not_all_up_to_date=true
}

# Note:
# using ":" builtin WITH SIDE EFFECTS of numerical expansion of arguments,
# because I find it convenient for debugging when running under set -x.
#
_drbd_set_status_variables_from_events2()
{
	local up_to_date_mask=0
	local line action object resname kv k v
	local OUT
	local IFS=$'\n'
	OUT=( $(drbdsetup events2 --now $DRBD_RESOURCE) ) || return
	IFS=$' \t\n'

	local _id _vol
	local _n_connections=0 _n_volumes=0 _n_diskless_client=0
	local _role=Unconfigured
	local _peer_ids=() _peer_role=() _conn_name=() _cstate=()

	local _disk_all_up_to_date=true
	local _disk_all_consistent=true
	local _disk_transitional_state=false

	local _pdsk_n_up_to_date=()
	local _pdsk_n_diskless_client=()
	local _pdsk_n_known=()

	DRBD_disk=()

	for line in "${OUT[@]}"; do
		: === DEBUG === line == $line ===
		set -- $line
		[[ $1 = exists ]] || continue
		[[ $2 = - ]] && break
		object=$2
		[[ $3 = name:$DRBD_RESOURCE ]] || continue
		shift 3

		case $object in
		resource)
			for kv ; do
				[[ $kv = role:* ]] && { _role=${kv#*:} ; break ; }
			done
			;;
		connection)
			: _n_connections = $(( _n_connections+=1 ))
			_id="-"
			for kv ; do
				v=${kv#*:}
				case $kv in
				peer-node-id:*)	_id=$v; _peer_ids[_id]=$v ;;
				# connection:*)	_cstate[_id]=$v ;;
				# conn-name:*)	_conn_name[_id]=$v ;;
				role:*)		_peer_role[_id]=$v ;;
				esac
			done
			_pdsk_n_diskless_client[_id]=0
			_pdsk_n_up_to_date[_id]=0
			_pdsk_n_known[_id]=0
			;;
		device)
			: _n_volumes = $(( _n_volumes+=1 ))
			# _vol="-"
			if [[ "$*" = *" disk:Diskless"*" client:yes"* ]]; then
				: _n_diskless_client = $(( _n_diskless_client+=1 ))
				DRBD_disk+=(DisklessClient) # used in informational log lines later
			else
				for kv ; do
					v=${kv#*:}
					case $kv in
					# volume:*)	_vol=$v; _volume_ids[_vol]=$v ;;
					# uninteresting, but if you want it,
					# remember to add it to "local" as well
					# minor:*)	_minor[_vol]=$v ;;
					disk:*)
						DRBD_disk+=($v)
						case $v in
						UpToDate) :;;
						Diskless|Failed|Inconsistent|Outdated|Detaching)
							_disk_all_consistent=false
							_disk_all_up_to_date=false
							;;
						Attaching|Negotiating)
							_disk_transitional_state=true ;;
						Consistent)
							_disk_all_up_to_date=false ;;
						# DUnknown: impossible for local disk.
						esac
						;;
					quorum:no)
						status_have_quorum=false
						;;
					esac
				done
			fi
			;;
		peer-device)
			_id="-" _vol="-"
			for kv ; do
				v=${kv#*:}
				case $kv in
				peer-node-id:*) _id=$v ;;
				volume:*)	_vol=$v ;;
				peer-client:yes)
						: _pdsk_n_diskless_client[$_id] = $(( _pdsk_n_diskless_client[_id] += 1 )) ;;
				peer-disk:*)
					case $v in
					UpToDate)
						: _pdsk_n_up_to_date[$_id] = $(( _pdsk_n_up_to_date[_id] += 1 ))
						: _pdsk_n_known[$_id] = $(( _pdsk_n_known[_id] += 1 )) ;;
					DUnknown) : ;;
					*)	: _pdsk_n_known[$_id] = $(( _pdsk_n_known[_id] += 1 )) ;;
					esac
				esac
			done
			;;
		esac
	done

	status_primary=false
	status_unconfigured=false
	case $_role in
	Primary) status_primary=true ;;
	Unconfigured) status_unconfigured=true ;;
	esac

	case "${_peer_role[*]}" in
	*Primary*) status_some_peer_primary=true ;;
	esac

	status_disk_all_up_to_date=$_disk_all_up_to_date
	status_disk_all_consistent=$_disk_all_consistent
	status_disk_transitional_state=$_disk_transitional_state

	status_some_peer_all_up_to_date=false
	status_some_peer_not_all_up_to_date=false
	status_some_peer_any_unknown=false

	$status_disk_all_up_to_date \
	&& : up_to_date_mask = $(( up_to_date_mask |= (1 << DRBD_MY_NODE_ID) ))

	: _n_volumes=$_n_volumes
	: _n_diskless_client=$_n_diskless_client
	(( _n_volumes == _n_diskless_client )) && status_diskless_client=true || status_diskless_client=false
	for _id in ${_peer_ids[*]}; do
		: "_pdsk_n_up_to_date[$_id]=${_pdsk_n_up_to_date[_id]}"
		: "_pdsk_n_known[$_id]=${_pdsk_n_known[_id]}"
		: "_pdsk_n_diskless_client[$_id]=${_pdsk_n_diskless_client[_id]}"

		# if this peer is a diskless client,
		# ignore it for determining status_some_peer*up_to_date
		if : "diskless client?"; (( _n_volumes == _pdsk_n_diskless_client[_id] )) ; then
			# TODO decide on bits of diskless clients
			if $status_disk_all_up_to_date && (( _n_volumes == _pdsk_n_known[_id] )) ; then
				: up_to_date_mask = $(( up_to_date_mask |= (1 << _id) ))
			fi
			# skip "status_pdsk_any_unknown" check for diskless clients.
			continue
		elif : "all up to date?"; (( _n_volumes == _pdsk_n_up_to_date[_id] )) ; then
			: up_to_date_mask = $(( up_to_date_mask |= (1 << _id) ))
			status_some_peer_all_up_to_date=true
		else
			status_some_peer_not_all_up_to_date=true
		fi
		: "any unknown?";
		(( _n_volumes != _pdsk_n_known[_id] )) && status_pdsk_any_unknown=true
	done

	: status_some_peer_all_up_to_date=$status_some_peer_all_up_to_date
	: status_some_peer_not_all_up_to_date=$status_some_peer_not_all_up_to_date
	: status_some_peer_any_unknown=$status_some_peer_any_unknown
	if $status_some_peer_all_up_to_date &&
	   ! $status_some_peer_not_all_up_to_date &&
	   ! $status_some_peer_any_unknown ; then
		status_pdsk_all_up_to_date=true
	else
		status_pdsk_all_up_to_date=false
	fi
	FAKE_UP_TO_DATE_NODE_MASK=$(printf "0x%x" $up_to_date_mask)
}

# What we really are interested in is
# Are we actively being used (aka Primary)?
# Do we have access to good _local_ data?
# What do we know about the remote data, is it as good as ours, better,
# worse, or could it potentially be better -- but we don't know?
#
drbd_set_status_variables()
{
	# "return" values:
	status_primary=false
	status_some_peer_primary=false
	status_diskless_client=false
	status_have_quorum=true

	# single peer, respectively aggregated
	status_pdsk_all_up_to_date=false
	status_pdsk_any_unknown=false

	# slightly more detail for multiple peers
	status_some_peer_any_unknown=false
	status_some_peer_all_up_to_date=false
	status_some_peer_not_all_up_to_date=false

	status_disk_all_up_to_date=false
	status_disk_all_consistent=false
	status_disk_transitional_state=false

	# maybe we can use it to "unfence"
	# a "stale" fencing constraint.
	FAKE_UP_TO_DATE_NODE_MASK=""

	if [[ $DRBD_HAS_EVENTS2 = true ]] ; then
		# role is per resource (scalar)
		_drbd_set_status_variables_from_events2
	else
		# role is still per volume (array)
		_drbd_set_status_variables_from_sh_status
	fi

	: == DEBUG == status_primary                      == $status_primary ==
	: == DEBUG == status_some_peer_primary            == $status_some_peer_primary ==
	: == DEBUG == status_diskless_client              == $status_diskless_client ==
	: == DEBUG == status_have_quorum                  == $status_have_quorum ==
	: == DEBUG == status_disk_all_up_to_date          == $status_disk_all_up_to_date ==
	: == DEBUG == status_disk_all_consistent          == $status_disk_all_consistent ==
	: == DEBUG == status_disk_transitional_state      == $status_disk_transitional_state ==
	: == DEBUG == status_pdsk_all_up_to_date          == $status_pdsk_all_up_to_date ==
	: == DEBUG == status_pdsk_any_unknown             == $status_pdsk_any_unknown ==
	: == DEBUG == status_some_peer_any_unknown        == $status_some_peer_any_unknown ==
	: == DEBUG == status_some_peer_all_up_to_date     == $status_some_peer_all_up_to_date ==
	: == DEBUG == status_some_peer_not_all_up_to_date == $status_some_peer_not_all_up_to_date ==
}

