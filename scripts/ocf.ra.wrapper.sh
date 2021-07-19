#!/bin/bash

set -o nounset
set -o pipefail
: "${AGENT}"
: "${OCF_ROOT}"
: "${OCF_RESOURCE_INSTANCE}"

die() { echo >&2 "<3>$*"; exit 255; }
test -x "${AGENT}" || die "not executable: ${AGENT}"

ACTION=$1
case $ACTION in
stop)
	# don't do double stop if we don't need to
	[[ ${EXIT_CODE-} = exited ]] && [[ ${EXIT_STATUS-} = 0 ]] && exit
	exec "${AGENT}" stop
	;;
start|start-and-monitor)
	: "implemented below"
	;;
*)
	die "not implemented: $*"
esac

# start[-and-monitor]
# start, notify READY, then monitor every monitor_interval

# echo >&2 "<7>$0 $*"
# cmd_output_to_debug_prio env
# instead, use:
#  systemctl show -p Names,Environment,ExecStart,ActiveState,SubState,StatusText,MainPID 'ocf.ra@*.service'
#  systemctl list-dependencies "drbd-services@$drbd_resource.target"
#  systemctl list-dependencies --reverse "drbd-promote@$drbd_resource.service"
#  journalctl -u ocf.ra@\* -p debug -n 200 --output with-units
# recent systemd has also "--with-dependencies"
#  systemctl status --with-dependencies drbd-services@\*.target
# old systemd needs something like
#  systemctl show --value -p Names drbd-services@\*.target |
#	xargs -rn 1 systemctl list-dependencies )
#  systemctl show --value -p Names drbd-promote@\*.service |
#	xargs -rn 1 systemctl list-dependencies --reverse
#  systemctl show --value -p Names drbd-services@\*.target |
#	xargs -rn 1 systemctl list-dependencies --plain |
#	xargs -r    systemctl status
#

_N=${NOTIFY_SOCKET:-}; unset NOTIFY_SOCKET
sd_notify() {
	local level="<7>"
	[[ $* = *STATUS=* ]] && level="<5>"
	echo >&2 "${level}${AGENT##*/}: ${OCF_RESOURCE_INSTANCE}: NOTIFY $*"
	NOTIFY_SOCKET=$_N systemd-notify "$@"
}

# nonsense to avoid to fork/exec sleep
exec {sleep_pipe}<> <(:)
mysleep() {
	local i=$1
	# in one second steps; read restarts after signal!
	while ! $should_stop && (( --i >= 0 )); do
		read -u $sleep_pipe -rt 1;
		[[ $? = 142 ]] || $should_stop || sleep 1 
	done
}

should_stop=false
trap "should_stop=true" TERM

sleep=${monitor_interval:=30}
if [[ $sleep != [1-9]* ]] || [[ $sleep = *[!0-9]* ]] ; then
	echo >&2 "<3>invalid monitor_interval '$monitor_interval', set to default"
	sleep=30
fi

"${AGENT}" start || exit $?
sd_notify READY=1 STATUS="calling monitor every $monitor_interval seconds"

agent_monitor() {
	local out_and_err ex
	out_and_err=$("${AGENT}" monitor 2>&1)
	ex=$?
	if [[ $ex = 0 ]]; then
		# discard output
		return 0
	else
		echo "$out_and_err" | sed -e "s/^/<7>monitor: /" >&2
		return $ex
	fi
}

# spread monitoring actions
mysleep $(( monitor_interval + RANDOM % monitor_interval / 2 ))
while ! $should_stop; do
	if agent_monitor {sleep_pipe}<&-; then
		# Possibly insert a notify "WATCHDOG=1" in the monitor loop,
		# and a WatchdogSec=... in the unit file template
		# sd_notify WATCHDOG=1
		mysleep $sleep
	else
		ex=$?
		break
	fi
done
exec {sleep_pipe}<&- 

if $should_stop ; then
	# ExecStopPost will call this anyways.
	# But I'm unsure about the ordering, and whether systemd will
	# wait for all ExecStopPost before trying to stop the next unit
	# down the chain
	sd_notify STATUS="about to exec stop"
	exec "${AGENT}" stop
else
	sd_notify STATUS="monitor FAILED with $ex"
	exit $ex
fi
