#!/bin/bash
#

export LANG=C LC_ALL=C TZ=":/etc/localtime"

grep_rsc_location()
{
	# expected input: exactly one tag per line: "^[[:space:]]*<.*/?>$"
	sed -ne '
	# within the rsc_location constraint with that id,
	/<rsc_location .*\bid="'"$1"'"/, /<\/rsc_location>/ {
		# make sure expressions have their attributes ordered
		# as we expect them later
		s/\(<expression\)\( .*\)\( attribute="[^"]*"\)/\1\3\2/
		s/\(<expression attribute="[^"]*"\)\( .*\)\( operation="[^"]*"\)/\1\3\2/
		s/\(<expression attribute="[^"]*" operation="[^"]*"\)\( .*\)\( value="[^"]*"\)/\1\3\2/
		p;
		/<\/rsc_location>/q # done, if closing tag is found
	}'
}

sed_rsc_location_suitable_for_string_compare()
{
	# expected input: exactly one tag per line: "^[[:space:]]*<.*/?>$"
	sed -ne '
	# within the rsc_location constraint with that id,
	/<rsc_location .*\bid="'"$1"'"/, /<\/rsc_location>/ {
		/<\/rsc_location>/q # done, if closing tag is found
		s/^[[:space:]]*//   # trim spaces
		s/ *\bid="[^"]*"//  # remove id tag
		/^<!--/d # remove comments
		# print each attribute on its own line, by
		: attr
		h # remember the current (tail of the) line
		# remove all but the first attribute, and print,
		s/^\([^[:space:]]*[[:space:]][^= ]*="[^"]*"\).*$/\1/p
		g # then restore the remembered line,
		# and remove the first attribute.
		s/^\([^[:space:]]*\)[[:space:]][^= ]*="[^"]*"\(.*\)$/\1\2/
		# then repeat, until no more attributes are left
		t attr
	}' | sort
}

cibadmin_invocations=0
remove_constraint()
{
	cibadmin_invocations=$(( $cibadmin_invocations + 1 ))
	cibadmin -D -X "<rsc_location rsc=\"$master_id\" id=\"$id_prefix-$master_id\"/>"
}

restrict_existing_constraint_further()
{
	[[ ${#EXCLUDE_NODES[@]} != 0 ]] || return

	new_constraint=$have_constraint

	# compare with setup_new_constraint()
	local i n a v
	for i in "${!EXCLUDE_NODES[@]}"; do
		n=${EXCLUDE_NODES[i]}
		a=${ATTRIBUTES[i]:-}
		if [[ -z "$a" ]] ; then
			a=$fencing_attribute
			if [[ $a = "#uname" ]]; then
				v=$n
			elif ! v=$(crm_attribute -Q -t nodes -N $n -n $a 2>/dev/null); then
				# FALLBACK.
				a="#uname"
				v=$n
			fi
			ATTRIBUTES[i]=$a
			VALUES[i]=$v
		else
			v=${VALUES[i]}
		fi
		# see grep_rsc_location(), which is supposed to fix the order of xml attributes
		new_constraint=$(set +x; echo "$new_constraint" |
			grep -v "<expression attribute=\"$a\" operation=\"ne\" value=\"$v\"")
	done
}

# set CIB_file= in environment, unless you want to grab the "live" one.
#
# I'd like to use the optional prefix filter on the constraint id,
# but pacemaker upstream broke that functionality (and later accidentally fixed it again)
# :-( so I "grep"
# Since "human readable" output is not stable enough for parsing, use
# the "--as-xml" switch (in more recent pacemaker, that would be the
# "--output-as=xml", but the --as-xml is still supported for the time being).
crm_mon_xml_sed_for_quorum_and_ban()
{
	# Yes, modern bash has lowercase substitution,
	# and gnu sed has case insensitive match,
	# but people use DRBD in weird deployments sometimes,
	# while insiting on having "camel case" hostnames...
	local node_name=$( echo $1 | tr '[[:upper:]]' '[[:lower:]]' )
	crm_mon --as-xml |
	sed -n \
		-e 's/^.*<current_dc .*with_quorum="true".*$/have_quorum/p' \
		-e '/<ban /!d' \
		-e '/ id="'$id_prefix-'\(rule-\)\?\<'$master_id'"/!d' \
		-e '/ resource="'$master_id'"/!d' \
		-e 'y/ABCDEFGHIJKLMNOPQRSTUVWXYZ/abcdefghijklmnopqrstuvwxyz/' \
		-e '/ node="'$node_name'"/ { s/.*/already_rejected/p }'
}

create_or_modify_constraint()
{
	local DIR
	local ex=1

	DIR=$(mktemp -d)
	cd "$DIR" || exit 1

	cleanup()
	{
		trap - EXIT HUP INT QUIT TERM
		local ex=$? sig=${1:-}
		cd - && rm -rf "$DIR"
		[[ $sig = EXIT ]] && exit $ex
		[[ $sig ]] && kill -$sig $$
	}

	trap "cleanup EXIT" EXIT
	trap "cleanup HUP" HUP
	trap "cleanup INT" INT
	trap "cleanup QUIT" QUIT
	trap "cleanup TERM" TERM

	local create_modify_replace="--modify --allow-create"
	while :; do
# ==================================================================
		cibadmin -Q | tee cib.xml.orig > cib.xml
		export CIB_file=cib.xml

		set -- $( crm_mon_xml_sed_for_quorum_and_ban $HOSTNAME )
		if [[ $# != 1 || $1 != "have_quorum" ]] ; then
			: "$*"
			: "sorry, want a quorate partition, and not be rejected by constraint already"
			unset CIB_file
			break
		fi

		if [[ $ACTION = fence ]]; then
			# fence should only restrict further, not lift restrictions.
			# There may have been a race between multiple instances of this script.
			have_constraint=$(grep_rsc_location "$id_prefix-$master_id" < $CIB_file)
			if [[ -n "$have_constraint" ]] ; then
				create_modify_replace="--replace"
				restrict_existing_constraint_further
			fi
		fi

		# comments seem to not work in any sane way yet :-(
		# new_constraint=${new_constraint/>/$'>\n'"<!-- $ACTION at $start_time_utc on $HOSTNAME mask $UP_TO_DATE_NODES -->"}
		cibadmin $create_modify_replace -o constraints -X "$new_constraint"

		crm_diff=$(crm_diff -o $CIB_file.orig -n $CIB_file)
		unset CIB_file
# ==================================================================

		cibadmin_invocations=$(( $cibadmin_invocations + 1 ))
		echo "$crm_diff" | cibadmin --patch --xml-pipe
		ex=$?
		case $ex in
		0)
			: "0 ==> cib successfully changed"
			break
			;;

			# see pacemaker commit f7e5558d6 Refactor: all: use consistent exit status codes
			# Relevant for Pacemaker 2.0+
			# 103/CRM_EX_OLD is Pacemaker 2, 205 aka pcmk_err_old_data is Pacemaker 1.1
		103|205)	: "103 aka CRM_EX_OLD, or 205 aka pcmk_err_old_data ==> going to retry in a bit"
			(( $SECONDS >= $timeout )) && break
			sleep 1
			continue
			;;

		*)	: "$ex ==> cib modify failed, giving up"
			break
			;;
		esac
	done
	cleanup
	unset -f cleanup
	return $ex
}

cib_xml=""
cib_xml_first_line=""
crm_feature_set=""
admin_epoch=""
epoch=""
num_updates=""
have_quorum=""
get_cib_xml() {
	cibadmin_invocations=$(( $cibadmin_invocations + 1 ))
	cib_xml=$( set +x; cibadmin "$@" )
	cib_xml_first_line=${cib_xml%%>*}

	set -- ${cib_xml_first_line}
	local x
	for x ; do
	case $x in
	crm_feature_set=*)	x=${x#*'="'}; x=${x%'"'}; crm_feature_set=$x ;;
	admin_epoch=*)		x=${x#*'="'}; x=${x%'"'}; admin_epoch=$x ;;
	epoch=*)		x=${x#*'="'}; x=${x%'"'}; epoch=$x ;;
	num_updates=*)		x=${x#*'="'}; x=${x%'"'}; num_updates=$x ;;
	have-quorum=*)		x=${x#*'="'}; x=${x%'"'}; have_quorum=$x ;;
	esac
	done
}


# if not passed in, try to "guess" it from the cib
# we only know the DRBD_RESOURCE.
fence_peer_init()
{
	# we know which instance we are: $OCF_RESOURCE_INSTANCE.
	# but we do not know the xml ID of the <master/> :(

	# with Pacemaker 2, its "promotable clones" instead of the
	# "master" (which was deemed a bad naming choice).
	# newer pacemaker does stil understand and may still use the old xml, though,
	# and it is rather cumbersome to detect which is in use.
	# just try both

	# cibadmin -Ql --xpath \
	# '//master[primitive[@type="drbd" and instance_attributes/nvpair[@name = "drbd_resource" and @value="r0"]]]/@id'
	# but I'd have to pipe that through sed anyways, because @attribute
	# xpath queries are not supported.
	# and I'd be incompatible with older cibadmin not supporting --xpath.
	# be cool, sed it out.
	# I could be more strict about primitive class:provider:type,
	# or double check that it is in fact a promotable="true" clone...
	# But in the real world, this is good enough.
	: ${master_id=$(set +x; echo "$cib_xml" |
		sed -ne '/<\(clone\|master\) /,/<\/\(clone\|master\)>/ {
			   /<\(clone\|master\) / h;
			     /<primitive/,/<\/primitive/ {
			       /<instance_attributes/,/<\/instance_attributes/ {
				 /<nvpair .*\bname="drbd_resource"/ {
				   /.*\bvalue="'"$DRBD_RESOURCE"'"/! d
				   x
				   s/^.*\bid="\([^"]*\)".*/\1/p
				   q
				 };};};}')}
	if [[ -z $master_id ]] ; then
		echo WARNING "drbd-fencing could not determine the master id of drbd resource $DRBD_RESOURCE"
		return 1;
	fi
	return 0
}

# drbd_fence_peer_exit_code is per the exit code
# convention of the DRBD "fence-peer" handler,
# obviously.
# 3: peer is already outdated or worse (e.g. inconsistent)
# 4: peer has been successfully fenced
# 5: peer not reachable, assumed to be dead
# 6: please outdate yourself, peer is known (or likely)
#    to have better data, or is even currently primary.
#    (actually, currently it is "peer is active primary now", but I'd like to
#    change that meaning slightly towards the above meaning)
# 7: peer has been STONITHed, thus assumed to be properly fenced
#    XXX IMO, this should rather be handled like 5, not 4.

# NOTE:
#    On loss of all cluster comm (cluster split-brain),
#    without STONITH configured, you always still risk data divergence.
#
# There are different timeouts:
#
# --timeout is how long we poll the DC for a definite "unreachable" node state,
# before we give up and say "unknown".
# This should be longer than "dead time" or "stonith timeout",
# the time it takes the cluster manager to declare the other node dead and
# shoot it, just to be sure.
#
# --dc-timeout is how long we try to contact a DC before we give up.
# This is necessary, because placing the constraint will fail (with some
# internal timeout) if no DC was available when we request the constraint.
# Which is likely if the DC crashed. Then the surviving DRBD Primary needs
# to wait for a new DC to be elected. Usually such election takes only
# fractions of a second, but it can take much longer (the default election
# timeout in pacemaker is ~2 minutes!).
#
# --network-hickup is how long we wait for the replication link to recover,
# if crmadmin confirms that the peer is in fact still alive.
# It may have been just a network hickup. If so, no need to potentially trigger
# node level fencing.
#
# a) Small-ish (1s) timeout, medium (10..20s) dc-timeout:
#    Intended use case: fencing resource-only, no STONITH configured.
#
#    Even with STONITH properly configured, on cluster-split-brain this method
#    risks to complete transactions to user space which can be lost due to
#    STONITH later.
#
#    With dual-primary setup (cluster file system),
#    you should use method b).
#
# b) timeout >= deadtime, dc-timeout > timeout
#    Intended use case: fencing resource-and-stonith, STONITH configured.
#
#    Difference to a)
#
#       If peer is still reachable according to the cib,
#	we first poll the cib/try to confirm with crmadmin,
#	until either crmadmin confirms reachability, timeout has elapsed,
#	or the peer becomes definitely unreachable.
#
#	This gives STONITH the chance to kill us.
#	With "fencing resource-and-stontith;" this protects us against
#	completing transactions to userland which might otherwise be lost.
#
#	We then place the constraint (if we are UpToDate), as explained below,
#	and return reachable/unreachable according to our last cib status poll
#	or crmadmin -S result.
#

#
#    replication link loss, current Primary calls this handler:
#	We are UpToDate, but we potentially need to wait for a DC election.
#	Once we have contacted the DC, we poll the cib until the peer is
#	confirmed unreachable, or crmadmin -S confirms it as reachable,
#	or timeout expired.
#	Then we place the constraint, and are done.
#
#	If it is complete communications loss, one will stonith the other.
#	For two-node clusters with no-quorum-policy=ignore, we will have a
#	deathmatch shoot-out, which the former DC is likely to win.
#
#	In dual-primary setups, if it is only replication link loss, both nodes
#	will call this handler, but only one will succeed to place the
#	constraint. The other will then typically need to "commit suicide".
#	With stonith enabled, and --suicide-on-failure-if-primary,
#	we will trigger a node level fencing, telling
#	pacemaker to "terminate" that node,
#	and scheduling a reboot -f just in case.
#
#    Primary crash, promotion of former Secondary:
#	DC-election, if any, will have taken place already.
#	We are UpToDate, we place the constraint, done.
#
#    node or cluster crash, promotion of Secondary with replication link down:
#	We are "Only" Consistent.  Usually any "init-dead-time" or similar has
#	expired already, and the cib node states are already authoritative
#	without doing additional waiting.  If the peer is still reachable, we
#	place the constraint - if the peer had better data, it should have a
#	higher master score, and we should not have been asked to become
#	primary.  If the peer is not reachable, we don't do anything, and DRBD
#	will refuse to be promoted. This is necessary to avoid problems
#	With data diversion, in case this "crash" was due to a STONITH operation,
#	maybe the reboot did not fix our cluster communications!
#
#	Note that typically, if STONITH is in use, it has been done on any
#	unreachable node _before_ we are promoted, so the cib should already
#	know that the peer is dead - if it is.
#

# slightly different logic than crm_is_true
crm_is_not_false()
{
	case ${1:-} in
	no|n|false|0|off)
		false ;;
	*)
		true ;;
	esac
}

check_cluster_properties()
{
	local x properties=$(set +x; echo "$cib_xml" |
		sed -n -e '/<crm_config/,/<\/crm_config/ !d;' \
			-e '/<cluster_property_set/,/<\/cluster_property_set/ !d;' \
			-e '/<nvpair / !d' \
			-e 's/^.* name="\([^"]*\)".* value="\([^"]*\)".*$/\1=\2/p' \
			-e 's/^.* value="\([^"]*\)".* name="\([^"]*\)".*$/\2=\1/p')

	for x in $properties ; do
		case $x in
		startup[-_]fencing=*)	startup_fencing=${x#*=} ;;
		stonith[-_]enabled=*)	stonith_enabled=${x#*=} ;;
		esac
	done

	crm_is_not_false ${startup_fencing:-} && startup_fencing=true || startup_fencing=false
	crm_is_not_false ${stonith_enabled:-} && stonith_enabled=true || stonith_enabled=false
}


#
# In case this is a two-node cluster (still common with
# DRBD clusters) it does not have real quorum.
# If it is configured to do STONITH, and reboot,
# and after reboot that STONITHed node cluster comm is
# still broken, it will shoot the still online node,
# and try to go online with stale data.
# Exactly what this "fence" handler should prevent.
# But setting constraints in a cluster partition with
# "no-quorum-policy=ignore" will usually succeed.
#
# So we need to differentiate between node reachable or
# not, and DRBD "Consistent" or "UpToDate".
#
try_place_constraint()
{
	local peer_state

	rc=1

	while :; do
		check_peer_node_reachable
		! $all_excluded_peers_reachable && break
		# if it really is still reachable, maybe the replication link
		# recovers by itself, and we can get away without taking action?
		(( $net_hickup_time > $SECONDS )) || break
		sleep $(( net_hickup_time - SECONDS ))
	done

	if $fail_if_no_quorum ; then
		if [[ $have_quorum = 1 ]] ; then
			# double check
			have_quorum=$(crm_node --quorum -VVV)
			[[ $have_quorum = 0 ]] && echo WARNING "Cib still had quorum, but no quorum according to crm_node --quorum"
		fi
		if [[ $have_quorum != 1 ]] ; then
			echo WARNING "Found $cib_xml_first_line"
			echo WARNING "I don't have quorum; did not place the constraint!"
			rc=0
			return
		fi
	fi

	set_states_from_proc_drbd_or_events2
	if : "all peer disks UpToDate?"; $status_pdsk_all_up_to_date ; then
		echo WARNING "All peer disks are UpToDate! Did not place the constraint."
		rc=0
		return
	fi

	: == DEBUG == CTS_mode=$CTS_mode ==
	: == DEBUG == status_disk_all_consistent=$status_disk_all_consistent ==
	: == DEBUG == status_disk_all_up_to_date=$status_disk_all_up_to_date ==
	: == DEBUG == all_excluded_peers_reachable=$all_excluded_peers_reachable ==
	: == DEBUG == all_excluded_peers_fenced=$all_excluded_peers_fenced ==

	if : "Unconfigured?" ; $status_unconfigured; then
		# Someone called this script, without the corresponding drbd
		# resource being configured. That's not very useful.
		echo WARNING "could not determine my disk state: did not place the constraint!"
		rc=0
		# keep drbd_fence_peer_exit_code at "generic error",
		# which will cause a "script is broken" message in case it was
		# indeed called as handler from within drbd

	# No, NOT fenced/Consistent:
	# just because we have been able to shoot him
	# does not make our data any better.
	elif : "all peers reachable and all local disks consistent?";
		$all_excluded_peers_reachable && $status_disk_all_consistent; then
		#                reachable && $status_disk_all_up_to_date
		#	is implicitly handled here as well.
		create_or_modify_constraint &&
		drbd_fence_peer_exit_code=4 rc=0 &&
		echo INFO "peers are reachable, my disk is ${DRBD_disk[*]}: placed constraint '$id_prefix-$master_id'"

	elif : "all peers fenced (clean offline) and all local disks UpToDate?";
		$all_excluded_peers_fenced && $status_disk_all_up_to_date ; then
		create_or_modify_constraint &&
		drbd_fence_peer_exit_code=7 rc=0 &&
		echo INFO "peers are (node-level) fenced, my disk is ${DRBD_disk[*]}: placed constraint '$id_prefix-$master_id'"

	# Peer is neither "reachable" nor "fenced" (above would have matched)
	# So we just hit some timeout.
	# As long as we are UpToDate, place the constraint and continue.
	# If you don't like that, use a ridiculously high timeout,
	# or patch this script.
	elif : "some peer UNCLEAN, but all local disks UpToDate?"; $status_disk_all_up_to_date ; then
		# We could differentiate between unreachable,
		# and DC-unreachable.  In the latter case, placing the
		# constraint will fail anyways, and  drbd_fence_peer_exit_code
		# will stay at "generic error".
		create_or_modify_constraint &&
		drbd_fence_peer_exit_code=5 rc=0 &&
		echo INFO "some peer is still UNCLEAN, my disk is UpToDate: placed constraint '$id_prefix-$master_id' anyways"

	# This block is reachable by operator intervention only
	# (unless you are hacking this script and know what you are doing)
	elif : "not all peers reachable, --unreachable-peer-is-outdated, all local disks consistent?";
		! $all_excluded_peers_reachable && [[ $unreachable_peer_is = outdated ]] && $status_disk_all_consistent; then
		# If the peer is not reachable, but we are only Consistent, we
		# may need some way to still allow promotion.
		# Easy way out: --force primary with drbdsetup.
		# But that would not place the constraint, nor outdate the
		# peer.  With this --unreachable-peer-is-outdated, we still try
		# to set the constraint.  Next promotion attempt will find the
		# "correct" constraint, consider the peer as successfully
		# fenced, and continue.
		create_or_modify_constraint &&
		drbd_fence_peer_exit_code=5 rc=0 &&
		echo WARNING "peer is unreachable, my disk is only Consistent: --unreachable-peer-is-outdated FORCED constraint '$id_prefix-$master_id'" &&
		echo WARNING "This MAY RISK DATA INTEGRITY"

	# So I'm not UpToDate, and (some) peer is not reachable.
	# Tell the module about "not reachable", and don't do anything else.
	else
		echo WARNING "some peer is UNCLEAN, my disk is not UpToDate, did not place the constraint!"
		drbd_fence_peer_exit_code=5 rc=0
		# I'd like to return 6 here, otherwise pacemaker will retry
		# forever to promote, even though 6 is not strictly correct.
	fi
	return $rc
}

commit_suicide()
{
	local reboot_timeout=20
	local extra_msg

	if $stonith_enabled ; then
		# avoid double fence, tell pacemaker to kill me
		echo WARNING "trying to have pacemaker kill me now!"
		crm_attribute -t status -N $HOSTNAME -n terminate -v 1
		echo WARNING "told pacemaker to kill me, but scheduling reboot -f in 300 seconds just in case"

		# -------------------------
		echo WARNING $'\n'"    told pacemaker to kill me,"\
			     $'\n'"    but scheduling reboot -f in 300 seconds just in case."\
			     $'\n'"    kill $$ # to cancel" | wall
		# -------------------------

		reboot_timeout=300
		extra_msg="Pacemaker terminate pending. If that fails, I'm "

	else
		# -------------------------
		echo WARNING $'\n'"    going to reboot -f in $reboot_timeout seconds"\
			     $'\n'"    kill $$ # to cancel!" | wall
		# -------------------------
	fi

	reboot_timeout=$(( reboot_timeout + SECONDS ))
	# pacemaker apparently cannot kill me.
	while (( $SECONDS < $reboot_timeout )); do
		echo WARNING "${extra_msg}going to reboot -f in $(( reboot_timeout - SECONDS )) seconds! To cancel: kill $$"
		sleep 2
	done
	echo WARNING "going to reboot -f now!"
	reboot -f
	sleep 864000
}

setup_node_lists()
{
	EXCLUDE_NODES=()
	INCLUDE_NODES=()
	SKIP_NODES=()
	ATTRIBUTES=()
	VALUES=()

	if [[ -z $UP_TO_DATE_NODES ]] ; then
		setup_node_lists_8 || return
	else
		setup_node_lists_9 || return
	fi
}

setup_node_lists_8()
{
	INCLUDE_NODES=( $HOSTNAME )
	EXCLUDE_NODES=( $DRBD_PEER ) # not quoted, so may be empty array.
}

is_up_to_date_node() { (( (UP_TO_DATE_NODES & (1<<$1)) != 0 )); }

setup_node_lists_9()
{
	local i k v

	: === UP_TO_DATE_NODES = $UP_TO_DATE_NODES ===
	if [[ $UP_TO_DATE_NODES != 0x[0-9a-fA-F]* ]] || [[ ${UP_TO_DATE_NODES#0x} == *[!0-9a-fA-F]* ]] ; then
		echo WARNING "Unexpected input UP_TO_DATE_NODES=$UP_TO_DATE_NODES, expected 0x... hex mask"
		return 1
	fi

	: === DRBD_MY_NODE_ID = $DRBD_MY_NODE_ID ===
	case $DRBD_MY_NODE_ID in
	[0-9]|[1-9][0-9]) : "looks OK" ;;
	*)
		echo WARNING "Unexpected input, DRBD_MY_NODE_ID=$DRBD_MY_NODE_ID should be a decimal number"
		return 1
	esac
	if ! is_up_to_date_node $DRBD_MY_NODE_ID ; then
		echo WARNING "I ($DRBD_MY_NODE_ID) am not a member of the UP_TO_DATE_NODES=$UP_TO_DATE_NODES set myself."
		return 1
	fi

	k=DRBD_NODE_ID_$DRBD_MY_NODE_ID ; v=${!k}
	[[ $HOSTNAME = $v ]] || echo WARNING "My node id ($DRBD_MY_NODE_ID) does not resolve to my hostname ($HOSTNAME) but to $v"

	for i in {0..31}; do
		k=DRBD_NODE_ID_$i ; v=${!k:-}
		[[ $v ]] || continue

		if is_up_to_date_node $i; then
			INCLUDE_NODES[i]=$v
		else
			EXCLUDE_NODES[i]=$v
		fi
	done
	return 0
}

have_expected_contraint()
{
	# do we have the exactly matching constraint already?
	[[ "$have_constraint" = "$new_constraint" ]] && return 0

	new_constraint_for_compare=$(set +x; echo "$new_constraint" |
		sed_rsc_location_suitable_for_string_compare "$id_prefix-$master_id")
	have_constraint_for_compare=$(set +x; echo "$have_constraint" |
		sed_rsc_location_suitable_for_string_compare "$id_prefix-$master_id")

	# do we have a semantically equivalent constraint?
	[[ "$have_constraint_for_compare" = "$new_constraint_for_compare" ]] && return 0

	return 1
}

_node_already_rejected()
{
	local node_name=$1

	# Not so easy. May be a second link failure and fence action,
	# and resulting update to that constraint, but may also be
	# the result of a fencing shoot-out race.
	# Are we still part of the allowed crowd?

        # if we differ by more than number and content of expressions,
        # that's not the "expected" constraint.
        #[[ "$(set +x; echo "$have_constraint_for_compare" | grep -v '<expression')" != \
        #   "$(set +x; echo "$new_constraint_for_compare" | grep -v '<expression')" ]] &&
        #        return (what exactly?)
	#
	# allow for permutation in attribute order,
	# but require "$my_attribute ne $my_value" to be present.
	# ! echo "$have_constraint" |
	# 	grep -Ee '<expression .*\<operation="ne"' |
	# 	grep -Fe " attribute=\"$my_attribute\"" |
	# 	grep -qFe " value=\"$my_value\"" \
	#

	# Maybe we better just ask crm_mon instead:
	set -- $( set +x; echo "$cib_xml" | CIB_file=/proc/self/fd/0 crm_mon_xml_sed_for_quorum_and_ban $node_name )
	[[ $* = *already_rejected ]]
}

# you should call have_expected_contraint() first.
existing_constraint_rejects_me()
{
	_node_already_rejected $HOSTNAME
}

setup_new_constraint()
{
	new_constraint="<rsc_location rsc=\"$master_id\" id=\"$id_prefix-$master_id\">"$'\n'
	# double negation: do not run but with my data.
	new_constraint+=" <rule role=\"$role\" score=\"-INFINITY\" id=\"$id_prefix-rule-$master_id\">"$'\n'

	local i n a v
	for i in "${!INCLUDE_NODES[@]}"; do
		n=${INCLUDE_NODES[i]}
		a=${ATTRIBUTES[i]:-}
		if [[ -z "$a" ]] ; then
			a=$fencing_attribute
			if [[ $a = "#uname" ]]; then
				v=$n
			elif ! v=$(crm_attribute -Q -t nodes -N $n -n $a 2>/dev/null); then
				# FALLBACK.
				a="#uname"
				v=$n
			fi
			ATTRIBUTES[i]=$a
			VALUES[i]=$v
		else
			v=${VALUES[i]}
		fi
		[[ $i = $DRBD_MY_NODE_ID ]] && my_attribute=$a my_value=$v
		# double negation: do not run but with my data.
		new_constraint+="  <expression attribute=\"$a\" operation=\"ne\" value=\"$v\" id=\"$id_prefix-expr-$i-$master_id\"/>"$'\n'
	done
	new_constraint+=$' </rule>\n</rsc_location>\n'
}

# drbd_peer_fencing fence|unfence
drbd_peer_fencing()
{
	# We are going to increase the cib timeout with every timeout,
	# see get_cib_xml_from_dc().
	# For the actual invocation, we use int(cibtimeout/10).
	# scaled by 5 / 4 with each iteration,
	# this results in a timeout sequence of 1 2 2 3 4 5 6 7 9 ... seconds 
	local cibtimeout=18

	local rc

	local have_constraint
	local have_constraint_for_compare
	local had_constraint_on_entry
	local new_constraint
	local new_constraint_for_compare
	local my_attribute my_value

	# if I cannot query the local cib, give up
	get_cib_xml -Ql || return

	# input to fence_peer_init:
	# $DRBD_RESOURCE is set by command line or from environment.
	# $id_prefix is set by command line or default.
	# $master_id is set by command line or will be parsed from the cib.
	fence_peer_init || return

	if [[ $1 = fence ]] || [[ -n $UP_TO_DATE_NODES ]] || $unfence_only_if_owner_match ; then
		setup_node_lists || return 1
		setup_new_constraint
	fi
	have_constraint=$(set +x; echo "$cib_xml" | grep_rsc_location "$id_prefix-$master_id")
	[[ -z $have_constraint ]] && had_constraint_on_entry=false || had_constraint_on_entry=true

	case $1 in
	fence)

		local startup_fencing stonith_enabled
		check_cluster_properties

		if ! $had_constraint_on_entry ; then

			# try to place it.
			try_place_constraint && return

			# maybe callback and operator raced for the same constraint?
			# before we potentially trigger node level fencing
			# or keep IO frozen, double check.

			get_cib_xml_from_dc
			have_constraint=$(set +x; echo "$cib_xml" | grep_rsc_location "$id_prefix-$master_id")
		fi

		if [[ -n "$have_constraint" ]] ; then
			if have_expected_contraint ; then
				echo INFO "suitable constraint already placed: '$id_prefix-$master_id'"
				drbd_fence_peer_exit_code=4
				rc=0
				return
			fi

			if existing_constraint_rejects_me; then
				echo WARNING "constraint already exists, and rejects me: $have_constraint"
			else
				try_place_constraint && return
				echo WARNING "constraint already exists, could not modify it: $have_constraint"
				# TODO
				# what about the exit status?
				# am I allowed to continue, or not?
			fi

			# anything != 0 will do;
			# 21 happend to be "The object already exists" with my cibadmin
			rc=21

			# maybe: drbd_fence_peer_exit_code=6
			# as this is not the constraint we'd like to set,
			# it is likely the inverse, so we probably can assume
			# that the peer is active primary, or at least has
			# better data than us, and wants us outdated.
		fi

		if [[ $rc != 0 ]]; then
			# at least we tried.
			# maybe it was already in place?
			echo WARNING "DATA INTEGRITY at RISK: could not place the fencing constraint!"

			# actually, at risk only if we did not freeze IO locally, or allow to resume.
			# which depends on the policies you set.
		fi

		# XXX policy decision:
		if $suicide_on_failure_if_primary && [[ $drbd_fence_peer_exit_code != [3457] ]]; then
			set_states_from_proc_drbd_or_events2
			$status_primary && commit_suicide
		fi

		return $rc
		;;
	unfence)
		if [[ -n $have_constraint ]]; then
			set_states_from_proc_drbd_or_events2
			# if $unfence_only_if_owner_match && ! have_expected_contraint ; then
			if $unfence_only_if_owner_match && existing_constraint_rejects_me ; then
				echo WARNING "Constraint owner does not match, leaving constraint in place."
			else
				if $status_disk_all_up_to_date && $status_pdsk_all_up_to_date; then
					# try to remove it based on that xml-id
					remove_constraint && echo INFO "Removed constraint '$id_prefix-$master_id'"
				else
					if have_expected_contraint; then
						$quiet || echo "expected constraint still in place, nothing to do"
					else
						# only one of several possible peers was sync'ed up.
						# allow that one, but not all, yet.
						create_or_modify_constraint || echo WARNING "could not modify, leaving constraint in place."
					fi
				fi
			fi
		else
			if [[ ${#EXCLUDE_NODES[@]} != 0 ]] ; then
				echo WARNING "No constraint in place, called for unfence, but (${EXCLUDE_NODES[*]}) still supposed to be excluded. Weird."
			else
				$quiet || echo "No constraint in place, nothing to do."
			fi
			return 0
		fi
	esac
}

double_check_after_fencing()
{
	set_states_from_proc_drbd_or_events2
	if $status_pdsk_all_up_to_date ; then
		echo WARNING "All peer disks are UpToDate (again), trying to remove the constraint again."
		remove_constraint && drbd_fence_peer_exit_code=1 rc=0
		return
	fi
}

guess_if_pacemaker_will_fence()
{
	# try to guess whether it is useful to wait and poll again,
	# (node fencing in progress...),
	# or if pacemaker thinks the node is "clean" dead.
	local x

	# "return values:"
	crmd='' in_ccm='' expected='' join='' will_fence=false

	# Older pacemaker has an "ha" attribute, too.
	# For stonith-enabled=false, the "crmd" attribute may stay "online",
	# but once ha="dead", we can stop waiting for changes.
	ha_dead=false

	node_state=${node_state%>}
	node_state=${node_state%/}
	for x in ${node_state} ; do
		case $x in
		in_ccm=\"*\")	x=${x#*=\"}; x=${x%\"}; in_ccm=$x ;;
		crmd=\"*\")	x=${x#*=\"}; x=${x%\"}; crmd=$x ;;
		expected=\"*\")	x=${x#*=\"}; x=${x%\"}; expected=$x ;;
		join=\"*\")	x=${x#*=\"}; x=${x%\"}; join=$x ;;
		ha=\"dead\")	ha_dead=true ;;
		esac
	done

	# if it is not enabled, no point in waiting for it.
	if ! $stonith_enabled ; then
		# "normalize" the rest of the logic
		# where this is called.
		# for stonith-enabled=false, and ha="dead",
		# reset crmd="offline".
		# Then we stop polling the cib for changes.

		$ha_dead && crmd="offline"
		return
	fi

	if [[ -z $node_state ]] ; then
		# if we don't know nothing about the peer,
		# and startup_fencing is explicitly disabled,
		# no fencing will take place.
		$startup_fencing || return
	fi

	# for further inspiration, see pacemaker:lib/pengine/unpack.c, determine_online_status_fencing()
	[[ -z $in_ccm ]] && will_fence=true
	[[ $crmd = "banned" ]] && will_fence=true
	if [[ ${expected-down} = "down" && $in_ccm = "false"  && $crmd != "online" ]]; then
		: "pacemaker considers this as clean down"
	elif [[ $in_ccm = false ]] || [[ $crmd != "online" ]]; then
		will_fence=true
	fi
}

# return values in
#	$all_excluded_peers_reachable
#	$all_excluded_peers_fenced
check_peer_node_reachable()
{
	local full_timeout
	local nr_other_nodes
	local other_node_uname_attrs

	# we have a cibadmin -Ql in cib_xml already
	# filter out <node uname, but ignore type="ping" nodes,
	# they don't run resources
	other_node_uname_attrs=$(set +x; echo "$cib_xml" |
		sed -e '/<node /!d; / type="ping"/d;s/^.* \(uname="[^"]*"\).*>$/\1/' |
		grep -v -F uname=\"$HOSTNAME\")
	set -- $other_node_uname_attrs
	nr_other_nodes=$#

	if [[ -z $UP_TO_DATE_NODES ]]; then
		if [[ -z $DRBD_PEER ]] && [[ $nr_other_nodes = 1 ]]; then
			# very unlikely: old DRBD, no DRBD_PEER passed in,
			# but in fact only one other cluster node.
			# Use that one as DRBD_PEER.
			DRBD_PEER=${other_node_uname_attrs#uname=\"}
			DRBD_PEER=${DRBD_PEER%\"}
		fi
		# This time, quoted.
		# Yes, it may be empty, resulting in [0]="".
		EXCLUDE_NODES=( "$DRBD_PEER" )
	fi

	get_cib_xml_from_dc || {
		all_excluded_peers_reachable=false
		all_excluded_peers_fenced=false
		return
	}

	all_excluded_peers_reachable=true
	all_excluded_peers_fenced=true

	if [[ ${#EXCLUDE_NODES[@]} != 0 ]] ; then
		for DRBD_PEER in "${EXCLUDE_NODES[@]}"; do
			# If it is already rejected,
			# we do not really care if it is currently reachable,
			# or currently UNCLEAN or what not.
			# What's done, is done.
			_node_already_rejected $DRBD_PEER && continue

			_check_peer_node_reachable $DRBD_PEER
			[[ $peer_state != reachable ]] && all_excluded_peers_reachable=false
			[[ $peer_state != fenced ]] && all_excluded_peers_fenced=false
		done
	fi
}

get_cib_xml_from_dc()
{
	while :; do
		local t=$SECONDS
		#
		# Update our view of the cib, ask the DC this time.
		# Timeout, in case no DC is available.
		# Caution, some cibadmin (pacemaker 0.6 and earlier)
		# apparently use -t use milliseconds, so will timeout
		# many times until a suitably long timeout is reached
		# by increasing below.
		#
		# Why not use the default timeout?
		# Because that would unecessarily wait for 30 seconds
		# or longer, even if the DC is re-elected right now,
		# and available within the next second.
		#
		get_cib_xml -Q -t $(( cibtimeout/10 )) && return 0

		# bash magic $SECONDS is seconds since shell invocation.
		(( $SECONDS > $dc_timeout )) && return 1

		# avoid busy loop
		[[ $t = $SECONDS ]] && sleep 1

		# try again, longer timeout.
		let "cibtimeout = cibtimeout * 5 / 4"
	done
}

# return value in $peer_state
# DC-unreachable
#	We have not been able to contact the DC.
# fenced
#	According to the node_state recorded in the cib,
#	the peer is offline and expected down
#	(which means successfully fenced, if stonith is enabled)
# reachable
#	cib says it's online, and crmadmin -S says peer state is "ok"
# unreachable
#	cib says it's offline (but does not yet say "expected" down)
#	and we reached the timeout
# unknown
#	cib does not say it was offline (or we don't know who the peer is)
#	and we reached the timeout
#
_check_peer_node_reachable()
{
	DRBD_PEER=$1
	while :; do
		local state_lines='' node_state='' crmd='' in_ccm=''
		local expected='' join='' will_fence='' ha_dead=''

		state_lines=$( set +x; echo "$cib_xml" | grep '<node_state ' |
			grep -F -e "$other_node_uname_attrs" )

		if $CTS_mode; then
			# CTS requires startup-fencing=false.
			# For PartialStart, NearQuorumPoint and similar tests,
			# we would likely stay Consistent, and refuse to Promote.
			# And CTS would be very unhappy.
			# Pretend that the peer was reachable if we are missing a node_state entry for it.
			if [[ $DRBD_PEER ]] && ! echo "$state_lines" | grep -q -F uname=\"$DRBD_PEER\" ; then
				peer_state="reachable"
				echo WARNING "CTS-mode: pretending that unseen node $DRBD_PEER was reachable"
				return
			fi
		fi

		if [[ -z $DRBD_PEER ]]; then
			# Multi node cluster, but unknown DRBD Peer.
			# This should not be a problem, unless you have
			# no_quorum_policy=ignore in an N > 2 cluster.
			# (yes, I've seen such beasts in the wild!)
			# As we don't know the peer,
			# we could only safely return here if *all*
			# potential peers are confirmed down.
			# Don't try to be smart, just wait for the full
			# timeout, which should allow STONITH to
			# complete.
			full_timeout=$(( $timeout - $SECONDS ))
			if (( $full_timeout > 0 )) ; then
				echo WARNING "don't know who my peer is; sleep $full_timeout seconds just in case"
				sleep $full_timeout
			fi

			# In the unlikely case that we don't know our DRBD peer,
			#	there is no point in polling the cib again,
			#	that won't teach us who our DRBD peer is.
			#
			#	We waited $full_timeout seconds already,
			#	to allow for node level fencing to shoot us.
			#
			#	So if we are still alive, then obviously no-one has shot us.
			#

			peer_state="unknown"
			return
		fi

		#
		# we know the peer or/and are a two node cluster
		#

		node_state=$(set +x; echo "$state_lines" | grep -F uname=\"$DRBD_PEER\")

		# populates in_ccm, crmd, exxpected, join, will_fence=[false|true]
		guess_if_pacemaker_will_fence

		if ! $will_fence && [[ $crmd != "online" ]] ; then

			# "legacy" cman + pacemaker clusters older than 1.1.10
			# may "forget" about startup fencing.
			# We can detect this because the "expected" attribute is missing.
			# Does not make much difference for our logic, though.
			[[ $expected/$in_ccm = "down/false" ]] && peer_state="fenced" || peer_state="unreachable"

			return
		fi

		# So the cib does still indicate the peer was reachable.
		#
		# try crmadmin; if we can successfully query the state of the remote crmd,
		# it is obviously reachable.
		#
		# Do this only after we have been able to reach a DC above.
		# Note: crmadmin timeout is in milli-seconds, and defaults to 30000 (30 seconds).
		# Our variable $cibtimeout should be in deci-seconds (see above)
		# (unless you use a very old version of pacemaker, so don't do that).
		# Convert deci-seconds to milli-seconds, and double it.
		if [[ $crmd = "online" ]] ; then
			local out
			if out=$( crmadmin -t $(( cibtimeout * 200 )) -S $DRBD_PEER ) \
			&& [[ $out = *"(ok)" ]]; then
				peer_state="reachable"
				return
			fi
		fi

		# We know our DRBD peer.
		# We are still not sure about its status, though.
		#
		# It is not (yet) "expected down" per the cib, but it is not
		# reliably reachable via crmadmin -S either.
		#
		# If we already polled for longer than timeout, give up.
		#
		# For a resource-and-stonith setup, or dual-primaries (which
		# you should only use with resource-and-stonith, anyways),
		# the recommended timeout is larger than the deadtime or
		# stonith timeout, and according to beekhof maybe should be
		# tuned up to the election-timeout (which, btw, defaults to 2
		# minutes!).
		#
		if (( $SECONDS >= $timeout )) ; then
			[[ $crmd = offline ]] && peer_state="unreachable" || peer_state="unknown"
			return
		fi

		# wait a bit before we poll the DC again
		sleep 2
		get_cib_xml_from_dc || {
			# unreachable: cannot even reach the DC
			peer_state="DC-unreachable"
			return
		}
	done
	# NOT REACHED
}

source_drbd_shellfuncs()
{
        local dir=.
        [[ $0 = */* ]] && dir=${0%/*}
        for dir in $dir /usr/lib/ocf/resource.d/linbit ; do
		test -r "$dir/drbd.shellfuncs.sh" || continue
                source "$dir/drbd.shellfuncs.sh" && return
        done
        echo WARNING "unable to source drbd.shellfuncs.sh"
}
source_drbd_shellfuncs

set_states_from_proc_drbd_or_events2()
{
	if test -n $UP_TO_DATE_NODES ; then
		_drbd_set_status_variables_from_events2
	else
		# fallback, in case someone tries to use this
		# with older DRBD
		set_states_from_proc_drbd
	fi
}
set_states_from_proc_drbd()
{
	local IFS line lines i disk pdsk
	# DRBD_MINOR exported by drbdadm since 8.3.3
	[[ $DRBD_MINOR ]] || DRBD_MINOR=$(drbdadm ${DRBD_CONF:+ -c "$DRBD_CONF"} sh-minor $DRBD_RESOURCE) || return

	# if we have more than one minor, do a word split, ...
	set -- $DRBD_MINOR
	# ... and convert into regex:
	IFS="|$IFS"; DRBD_MINOR="($*)"; IFS=${IFS#?}

	# in a fence-peer handler, at least on certain older DRBD versions,
	# we must not recurse into netlink, this may be a synchronous callback
	# triggered by "drbdsetup primary", while holding the genl_lock.
	# grep /proc/drbd instead

	local DRBD_peer=()
	local DRBD_role=()
	DRBD_disk=()	# used in informational log lines later
	local DRBD_pdsk=()
	status_disk_all_up_to_date=true
	status_disk_all_consistent=true
	status_pdsk_all_up_to_date=true

	IFS=$'\n'
	lines=($(sed -nre "/^ *$DRBD_MINOR: cs:/ { s/:/ /g; p; }" /proc/drbd))
	IFS=$' \t\n'

	i=0
	for line in "${lines[@]}"; do
		set -- $line
		DRBD_peer[i]=${5#*/}
		DRBD_role[i]=${5%/*}
		pdsk=${7#*/}
		disk=${7%/*}
		DRBD_disk[i]=${disk:-Unconfigured}
		DRBD_pdsk[i]=${pdsk:-DUnknown}
		case $disk in
		UpToDate) ;;
		Consistent)
			status_disk_all_up_to_date=false ;;
		*)
			status_disk_all_up_to_date=false
			status_disk_all_consistent=false ;;
		esac
		[[ $pdsk != UpToDate ]] && status_pdsk_all_up_to_date=false
		let i++
	done
	if (( i == 0 )) ; then
		status_pdsk_all_up_to_date=false
		status_disk_all_up_to_date=false
		status_disk_all_consistent=false
	fi
	: == DEBUG == DRBD_role=${DRBD_role[*]} ===
	: == DEBUG == DRBD_peer=${DRBD_peer[*]} ===
	: == DEBUG == DRBD_pdsk=${DRBD_pdsk[*]} ===

	status_primary=false
	status_unconfigured=false
	case ${DRBD_role[*]} in
	*Primary*) status_primary=true ;;
	*Secondary*) : "at least it is configured" ;;
	*)	status_unconfigured=true ;;
	esac
}

############################################################

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

# clean environment just in case.
unset fencing_attribute id_prefix timeout dc_timeout unreachable_peer_is
unset flock_timeout flock_required lock_dir lock_file
quiet=false
unfence_only_if_owner_match=false
CTS_mode=false
suicide_on_failure_if_primary=false
fail_if_no_quorum=true

# poor mans command line argument parsing,
# allow for command line overrides
set -- "$@" ${OCF_RESKEY_unfence_extra_args:-}
while [[ $# != 0 ]]; do
	case $1 in
	--logfacility=*)
		redirect_to_logger ${1#*=}
		;;
	--logfacility)
		redirect_to_logger $2
		shift
		;;
	--resource=*)
		DRBD_RESOURCE=${1#*=}
		;;
	-r|--resource)
		DRBD_RESOURCE=$2
		shift
		;;
	--master-id=*)
		master_id=${1#*=}
		;;
	-i|--master-id)
		master_id=$2
		shift
		;;
	--role=*)
		role=${1#*=}
		;;
	-l|--role)
		role=${2}
		shift
		;;
	--fencing-attribute=*)
		fencing_attribute=${1#*=}
		;;
	-a|--fencing-attribute)
		fencing_attribute=$2
		shift
		;;
	--id-prefix=*)
		id_prefix=${1#*=}
		;;
	-p|--id-prefix)
		id_prefix=$2
		shift
		;;
	--timeout=*)
		timeout=${1#*=}
		;;
	-t|--timeout)
		timeout=$2
		shift
		;;
	--dc-timeout=*)
		dc_timeout=${1#*=}
		;;
	-d|--dc-timeout)
		dc_timeout=$2
		shift
		;;
	--quiet)
		quiet=true
		;;
	--unfence-only-if-owner-match)
		unfence_only_if_owner_match=true
		;;
	--flock-required)
		flock_required=true
		;;
	--flock-timeout=*)
		flock_timeout=${1#*=}
		;;
	--flock-timeout)
		flock_timeout=$2
		shift
		;;
	--lock-dir=*)
		lock_dir=${1#*=}
		;;
	--lock-dir)
		lock_dir=$2
		shift
		;;
	--lock-file=*)
		lock_file=${1#*=}
		;;
	--lock-file)
		lock_file=$2
		shift
		;;
	--net-hickup=*|--network-hickup=*)
		net_hickup_time=${1#*=}
		;;
	--net-hickup|--network-hickup)
		net_hickup_time=$2
		shift
		;;
	--CTS-mode)
		CTS_mode=true
		;;
	--unreachable-peer-is-outdated)
		# This is NOT to be scripted.
		# Or people will put this into the handler definition in
		# drbd.conf, and all this nice work was useless.
		test -t 0 &&
		unreachable_peer_is=outdated
		;;
	--suicide-on-failure-if-primary)
		suicide_on_failure_if_primary=true
		;;
	-*)
		echo >&2 "ignoring unknown option $1"
		;;
	*)
		echo >&2 "ignoring unexpected argument $1"
		;;
	esac
	shift
done

#
# Sanitize lock_file and lock_dir
#
if [[ ${lock_dir:=/var/lock/drbd} != /* ]] ; then
	echo WARNING "lock_dir needs to be an absolute path, not [$lock_dir]; using default."
	lock_dir=/var/lock/drbd
fi
case ${lock_file:-""} in
"")	lock_file=$lock_dir/fence.${DRBD_RESOURCE//\//_} ;;
NONE)	: ;;
/*)	: ;;
*)	lock_file=$lock_dir/$lock_file ;;
esac
if [[ $lock_file != NONE && $lock_file != $lock_dir/* ]]; then
	lock_dir=${lock_file%/*}; : ${lock_dir:=/}
	: == DEBUG == "override: lock_dir=$lock_dir to match lock_file=$lock_file"
fi

# DRBD_RESOURCE: from environment
# master_id: parsed from cib

: "== unreachable_peer_is == ${unreachable_peer_is:=unknown}"
# apply defaults:
: "== fencing_attribute   == ${fencing_attribute:="#uname"}"
: "== id_prefix           == ${id_prefix:="drbd-fence-by-handler"}"
: "== role                == ${role:="Master"}"

# defaults suitable for most cases
: "== net_hickup_time     == ${net_hickup_time:=0}"
: "== timeout             == ${timeout:=90}"
: "== dc_timeout          == ${dc_timeout:=20}"
: "== flock_timeout       == ${flock_timeout:=120}"
: "== flock_required      == ${flock_required:=false}"
: "== lock_file           == ${lock_file}"
: "== lock_dir            == ${lock_dir}"


# check envars normally passed in by drbdadm
# TODO DRBD_CONF is also passed in.  we may need to use it in the
# xpath query, in case someone is crazy enough to use different
# conf files with the _same_ resource name.
# for now: do not do that, or hardcode the cib id of the master
# in the handler section of your drbd conf file.
for var in DRBD_RESOURCE; do
	if [ -z "${!var}" ]; then
		echo "Environment variable \$$var not found (this is normally passed in by drbdadm)." >&2
		exit 1
	fi
done

# Fixup id-prefix to include the resource name
# There may be multiple drbd instances part of the same M/S Group, pointing to
# the same master-id. Still they need to all have their own constraint, to be
# able to unfence independently when they finish their resync independently.
# Be nice to people who already explicitly configure an id prefix containing
# the resource name.
if [[ $id_prefix != *"-$DRBD_RESOURCE" ]] ; then
	id_prefix="$id_prefix-$DRBD_RESOURCE"
	: "== id_prefix           == ${id_prefix}"
fi

# make sure it contains what we expect
HOSTNAME=$(uname -n)
start_time_utc=$(date --utc +%s_%F_%T)

$quiet || {
	for k in ${!DRBD_*} UP_TO_DATE_NODES; do printf "%s=%q " "$k" "${!k}"; done
	printf '%q' "$0"
	[[ $# != 0 ]] && printf ' %q' "$@"
	printf '\n'
}

# to be set by drbd_peer_fencing()
drbd_fence_peer_exit_code=1

got_flock=false
if [[ $lock_file != NONE ]] ; then
	test -d "$lock_dir"			||
		mkdir -p -m 0700 "$lock_dir"	||
		echo WARNING "mkdir -p $lock_dir failed"

	if exec 9>"$lock_file" && flock --exclusive --timeout $flock_timeout 9
	then
		got_flock=true
	else
		echo WARNING "Could not get flock on $lock_file"
		$flock_required && exit 1

		# If I cannot get the lock file, I can at least still try to place the constraint
	fi
	: == DEBUG == $SECONDS seconds, got_flock=$got_flock ==
fi

case $PROG in
    crm-fence-peer.*)
	ACTION=fence
	if drbd_peer_fencing fence; then
		: == DEBUG == $cibadmin_invocations cibadmin calls ==
		: == DEBUG == $SECONDS seconds ==
		[[ $drbd_fence_peer_exit_code = [347] ]] && double_check_after_fencing
		exit $drbd_fence_peer_exit_code
	fi
	;;
    crm-unfence-peer.*)
	ACTION=unfence
	if drbd_peer_fencing unfence; then
		: == DEBUG == $cibadmin_invocations cibadmin calls ==
		: == DEBUG == $SECONDS seconds ==
		exit 0
	fi
esac 9>&- # Don't want to "leak" the lock fd to child processes.

# 1: unexpected error
exit 1
