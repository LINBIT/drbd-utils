#!/bin/bash
#
# drbd-fence-peer.sh
#
# DRBD fence-peer handler that bridges DRBD's fencing script interface to the
# cluster fence agents (fence_ipmilan, fence_redfish, ...).
#
# It is meant for two-node clusters driven by drbd-reactor's promoter plugin,
# where DRBD quorum is not available and split-brain must be avoided by fencing
# (STONITH) the peer at the DRBD level.
#
# Wire it into the DRBD resource:
#
#     resource r0 {
#         net      { fencing resource-and-stonith; }
#         handlers { fence-peer "/usr/lib/drbd/drbd-fence-peer.sh"; }
#         ...
#     }
#
# When DRBD loses contact with a peer and needs to guarantee the peer is no
# longer accessing the data, it runs this handler with a set of DRBD_* env
# vars. We fence every node DRBD names by invoking its configured fence agent,
# then translate the agent's result into the exit code DRBD expects:
#
#     7  peer has been STONITHed   -> DRBD (and drbd-reactor) may safely proceed
#     1  fencing failed / unknown  -> DRBD keeps I/O SUSPENDED (safe default)
#
# We deliberately never return 5 ("unreachable, assume dead"): with real power
# fencing we do not *assume* -- either the agent confirms the peer is down
# (exit 7) or we stay suspended (exit 1) and wait for a human.
#
# Per-peer parameters live in $CONFDIR/peers/<nodename>.fence. The first
# "AGENT=/path" line selects the fence binary; every other key=value line is fed
# to that agent on STDIN, so credentials never appear in argv / ps output.
# Global settings (FENCE_ACTION, FENCE_DELAY, ...) live in $CONFDIR/fence.conf.
# See the examples/ directory shipped with this script.
#
# In a symmetric two-node partition both nodes fence at once and would reboot
# each other; we apply an automatic priority-fencing-delay so the data-serving
# (Primary) node wins the race. See priority_delay_for() below. A manual
# "delay=" in a peer file is ignored -- the delay is computed, not configured.
#
# Safe manual self-test (queries the BMC with action=status, powers nothing):
#
#     /usr/lib/drbd/drbd-fence-peer.sh --test <peer-node-name>
#
# Note that action=status can succeed even when the peer<->BMC mapping is wrong
# (querying the wrong BMC still answers "on"). To actually prove the mapping,
# trigger a real fence of the peer and confirm the *right* node reboots:
#
#     DRBD_PEERS=<peer-node-name> /usr/lib/drbd/drbd-fence-peer.sh
#
# If the peer survives while this node (or an innocent bystander) reboots, the
# mapping is wrong.
#
# This file is part of DRBD by LINBIT.  Licensed under the GNU GPL v2 or later.

set -u
PATH=/usr/sbin:/usr/bin:/sbin:/bin
export PATH

CONFDIR="${DRBD_FENCE_CONFDIR:-/etc/drbd.d/fence-peer}"
GLOBAL_CONF="$CONFDIR/fence.conf"

# Defaults -- override in $GLOBAL_CONF.
FENCE_ACTION="reboot"          # reboot | off
LOG_TAG="drbd-fence-peer"
FENCE_DELAY=5                  # priority-fencing-delay, seconds (0 disables)

[ -r "$GLOBAL_CONF" ] && . "$GLOBAL_CONF"

log() { logger -t "$LOG_TAG" -- "$*" 2>/dev/null; echo "$LOG_TAG: $*" >&2; }

# --- automatic priority fencing delay ------------------------------------
# In a symmetric two-node partition both nodes run fence-peer at the same time
# and would reboot each other. Like pacemaker's priority-fencing-delay, we let
# the node that is *serving* the data win the race: the Primary-majority node
# fences immediately, the Secondary-majority node waits FENCE_DELAY seconds --
# long enough to be shot first if the Primary is actually alive. If the Primary
# is dead, the delay merely postpones the (successful) fence by FENCE_DELAY.
#
# The decision is made from our OWN role only: during the partition the peer's
# role reads as Unknown, so our role is the sole reliable signal. Exact ties are
# broken by node-id (lower proceeds) -- deterministic, so exactly one node
# waits. (A coin flip would not: two independent flips can pick the same side.)
#
# Sets $delay for fencing peer $1. Never delays if FENCE_DELAY is 0 / non-numeric
# or if we cannot even determine node-ids for the tiebreak.
priority_delay_for() {
    local peer="$1" counts strong weak me them
    delay=0
    [ "${FENCE_DELAY:-0}" -gt 0 ] 2>/dev/null || return

    # Count, over resources shared with this peer, where WE are Primary vs
    # Secondary. events2 --now is the machine-readable current-state snapshot.
    counts=$(timeout 5 drbdsetup events2 --now 2>/dev/null | awk -v peer="$peer" '
        $1=="exists" && $2=="resource" {
            n=""; r=""
            for (i=3; i<=NF; i++) {
                if      ($i ~ /^name:/) n=substr($i,6)
                else if ($i ~ /^role:/) { r=substr($i,6); sub(/[^A-Za-z].*/,"",r) }
            }
            if (n != "") rrole[n]=r
        }
        $1=="exists" && $2=="connection" {
            n=""; cn=""
            for (i=3; i<=NF; i++) {
                if      ($i ~ /^name:/)      n=substr($i,6)
                else if ($i ~ /^conn-name:/) cn=substr($i,11)
            }
            if (cn == peer) shared[n]=1
        }
        END {
            s=0; w=0
            for (r in shared) {
                if      (rrole[r]=="Primary")   s++
                else if (rrole[r]=="Secondary") w++
            }
            print s" "w
        }')
    strong=${counts%% *}; weak=${counts##* }
    # If the snapshot was unreadable, fall through as a 0/0 tie -> node-id.
    [ -n "${strong:-}" ] || strong=0
    [ -n "${weak:-}" ]   || weak=0

    if [ "$strong" -gt "$weak" ]; then
        log "peer=$peer: we are Primary on $strong/$((strong+weak)) shared resource(s) -> fence immediately"
        delay=0
    elif [ "$weak" -gt "$strong" ]; then
        log "peer=$peer: we are Secondary on $weak/$((strong+weak)) shared resource(s) -> priority-fencing-delay ${FENCE_DELAY}s"
        delay=$FENCE_DELAY
    else
        # Balanced (or no shared resources): deterministic node-id tiebreak.
        me="${DRBD_MY_NODE_ID:-}"; them="${DRBD_PEER_NODE_ID:-}"
        if [ -n "$me" ] && [ -n "$them" ] && [ "$me" -lt "$them" ] 2>/dev/null; then
            log "peer=$peer: role tie, our node-id $me < $them -> fence immediately"
            delay=0
        elif [ -n "$me" ] && [ -n "$them" ]; then
            log "peer=$peer: role tie, our node-id $me > $them -> priority-fencing-delay ${FENCE_DELAY}s"
            delay=$FENCE_DELAY
        else
            log "peer=$peer: role tie and no node-id for tiebreak -> fence immediately"
            delay=0
        fi
    fi
}

# --- optional safe self-test mode ----------------------------------------
TEST_MODE=0
if [ "${1:-}" = "--test" ]; then
    TEST_MODE=1
    shift
    DRBD_PEERS="${1:-${DRBD_PEERS:-}}"
fi

: "${DRBD_RESOURCE:=<unknown>}"

# Determine which peer(s) to fence.
#   DRBD_PEERS is the simple case (space-separated peer hostnames), but DRBD 9
#   invokes fence-peer per connection and often sets only DRBD_PEER_NODE_ID plus
#   a DRBD_NODE_ID_<n>=<hostname> map. Derive the hostname from that.
peers="${DRBD_PEERS:-}"
if [ -z "$peers" ] && [ -n "${DRBD_PEER_NODE_ID:-}" ]; then
    eval "peers=\${DRBD_NODE_ID_${DRBD_PEER_NODE_ID}:-}"
fi

if [ -z "$peers" ]; then
    log "ERROR: cannot determine peer (DRBD_PEERS='${DRBD_PEERS:-}' DRBD_PEER_NODE_ID='${DRBD_PEER_NODE_ID:-}') resource=$DRBD_RESOURCE -> exit 1 (I/O stays suspended)"
    exit 1
fi

action="$FENCE_ACTION"
[ "$TEST_MODE" = 1 ] && action="status"

all_ok=1
for peer in $peers; do
    pconf="$CONFDIR/peers/$peer.fence"
    if [ ! -r "$pconf" ]; then
        log "ERROR: no fence config for peer '$peer' ($pconf) -> cannot fence"
        all_ok=0
        continue
    fi

    agent=$(sed -n 's/^AGENT=//p' "$pconf" | head -n1)
    if [ -z "$agent" ] || [ ! -x "$agent" ]; then
        log "ERROR: fence agent for peer '$peer' missing/not executable (AGENT='$agent')"
        all_ok=0
        continue
    fi

    if grep -qiE '^[[:space:]]*delay=' "$pconf"; then
        log "NOTE: peer=$peer: ignoring manual 'delay=' in $pconf; the delay is computed automatically (see FENCE_DELAY)"
    fi

    # Priority-fencing-delay: wait here (never in --test) if the peer looks
    # stronger, so a live Primary peer can shoot us before we shoot it.
    if [ "$TEST_MODE" != 1 ]; then
        priority_delay_for "$peer"
        [ "$delay" -gt 0 ] && sleep "$delay"
    fi

    log "resource=$DRBD_RESOURCE peer=$peer: running ${agent##*/} action=$action (ctx=$(id -Z 2>/dev/null))"

    # key=value params (minus comments / blank / the AGENT= and delay= lines) +
    # action, on stdin. delay is handled by us, not passed to the agent.
    out=$({ grep -viE '^[[:space:]]*(#|agent=|delay=|$)' "$pconf"; echo "action=$action"; } | "$agent" 2>&1)
    rc=$?

    if [ "$rc" -eq 0 ]; then
        log "peer=$peer: ${agent##*/} action=$action SUCCEEDED"
    else
        log "peer=$peer: ${agent##*/} action=$action FAILED (rc=$rc): $(printf '%s' "$out" | tr '\n' '|' | tail -c 300)"
        all_ok=0
    fi
done

if [ "$TEST_MODE" = 1 ]; then
    [ "$all_ok" = 1 ] && { log "self-test OK (no power action taken)"; exit 0; }
    log "self-test FAILED"; exit 1
fi

if [ "$all_ok" = 1 ]; then
    log "all peers fenced -> exit 7 (peer STONITHed)"
    exit 7
fi
log "fencing incomplete -> exit 1 (DRBD keeps I/O suspended)"
exit 1
