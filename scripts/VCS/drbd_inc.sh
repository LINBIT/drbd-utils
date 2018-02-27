########
# to be sourced by LINBIT vcs resource agents DRBDConfigure and DRBDPrimary
#
# Expects DRBD Resource Name in ResName attribute
#
# License: GPL-2.0
# Copyright (C) 2017 LINBIT HA-Solutions GmbH
# Roland Kammerer <roland.kammerer@linbit.com>
# Lars Ellenberg <lars.ellenberg@linbit.com>
###

DRBDADM="/sbin/drbdadm"
DRBDSETUP="/sbin/drbdsetup"

VCS_HOME=${VCS_HOME:-/opt/VRTSvcs}
ag_inc=${VCS_HOME}/bin/ag_i18n_inc.sh
. $ag_inc || { >&2 echo "$0: Failed to source $ag_inc"; exit 1; }

RESNAME=$1; shift;
VCSAG_SET_ENVS "$RESNAME"

VCSAG_GET_ATTR_VALUE "ResName" -1 1 "$@"
if [ $? != $VCSAG_SUCCESS ]; then
   case $entry_point in
	monitor)	exit $VCSAG_RES_UNKNOWN ;;
	*)		exit $VCSAG_EP_DONE ;;
   esac
fi
DRBDRESNAME=${VCSAG_ATTR_VALUE}

# does it need "--stacked"?
$DRBDADM --stacked sh-dev $DRBDRESNAME >/dev/null 2>&1 && DRBDADM="$DRBDADM --stacked"

confidence_from_drbd_state()
{
	local drbd_resource_state=$1
	if [ -z "$drbd_resource_state" ]; then
		VCSAG_LOGDBG_MSG 1 "usage: confidence_from_drbd_state \"output-of-drbdsetup events2 --now '\$DRBDRESNAME'\""
		return 99 # $VCSAG_RES_UNKNOWN
	fi

	peer_disks=$( echo "$drbd_resource_state" | grep " peer-device " )
	pd_uptodate=$( echo "$peer_disks" | grep peer-disk:UpToDate | wc -l)
	pd_not_uptodate=$( echo "$peer_disks" | grep " peer-disk:" | grep -v " peer-disk:UpToDate" | wc -l)

	disks=$(echo "$drbd_resource_state" | grep " device ")
	d_uptodate=$(echo "$disks" | grep " disk:UpToDate" | wc -l)
	d_not_uptodate=$( echo "$disks" | grep " disk:" | grep -v " disk:UpToDate" | wc -l)

	VCSAG_LOGDBG_MSG 2 "local/peer bad::good $d_not_uptodate/$pd_not_uptodate::$d_uptodate/$pd_uptodate"

	case $d_not_uptodate/$pd_not_uptodate::$d_uptodate/$pd_uptodate in
		0/0::0/0)
			# should not happen
			return 100 ;; # $VCSAG_RES_OFFLINE
		0/0::*)
			# perfect, all local and remote are uptodate
			return 110 ;; # $VCSAG_RES_ONLINE
		0/*::*/0)
			# all local are good, no remote are good
			return 106 ;;
		0/*::*)
			# all local are good, some remote are good
			return 107 ;;
		*::0/0)
			# no good data anywhere :-(
			# online, but barely, waiting for the good peer to connect?
			return 101 ;;
		*/0::0/*)
			# no local good, all remote good
			return 105 ;;
		*::0/*)
			# no local good, some remote good
			return 103 ;;
		*)
			# some local good, some remote good
			return 102 ;;
	esac
}

DRBD_ROLE_PRI=$VCSAG_RES_ONLINE
DRBD_ROLE_SEC=$VCSAG_RES_OFFLINE
DRBD_ROLE_UNNKOWN=$VCSAG_RES_UNKNOWN
role_from_drbd_state()
{
	if [ -z "$drbd_resource_state" ]; then
		VCSAG_LOGDBG_MSG 1 "usage: role_from_drbd_state \"output-of-drbdsetup events2 --now '\$DRBDRESNAME'\""
		return $DRBD_ROLE_UNNKOWN
	fi
	echo "$drbd_resource_state" | grep ' resource ' | grep ' role:Primary' && return $DRBD_ROLE_PRI
	echo "$drbd_resource_state" | grep ' resource ' | grep ' role:Secondary' && return $DRBD_ROLE_SEC
	return $DRBD_ROLE_UNNKOWN
}
