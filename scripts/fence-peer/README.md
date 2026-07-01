# drbd-fence-peer.sh

A DRBD **fence-peer** handler that bridges DRBD's fencing interface to the
cluster **fence agents** (`fence_ipmilan`, `fence_redfish`, and other
`fence_*` agents) **without pacemaker**.

It targets two-node clusters driven by [drbd-reactor]'s promoter plugin. With
only two nodes DRBD quorum is not usable, so split-brain must be avoided by
fencing (STONITH) the peer at the DRBD level. This handler is the no-pacemaker
counterpart to `stonith_admin-fence-peer.sh` (`crm-fence-peer.sh` only sets
pacemaker location constraints, it does not STONITH): instead of asking
`stonith-ng` to fence the peer, it calls a fence agent directly.

[drbd-reactor]: https://github.com/LINBIT/drbd-reactor

## How it works

When DRBD loses contact with a peer and must guarantee that peer is no longer
touching the data, it runs the `fence-peer` handler with a set of `DRBD_*`
environment variables. This script:

1. Determines the peer node name from `DRBD_PEERS`, or (DRBD 9, per-connection)
   from `DRBD_PEER_NODE_ID` + the `DRBD_NODE_ID_<n>` map.
2. Reads that peer's parameters from `CONFDIR/peers/<nodename>.fence`.
3. Runs the configured fence agent, feeding all parameters on **stdin** (so
   credentials never appear in `argv`/`ps`).
4. Maps the agent result to the exit code DRBD expects.

### Exit codes returned to DRBD

| exit | meaning | effect |
|------|---------|--------|
| `7`  | peer has been STONITHed | DRBD/drbd-reactor may safely proceed |
| `1`  | fencing failed or peer unknown | DRBD keeps I/O **suspended** (safe) |

The handler never returns `5` ("unreachable, assume dead"): with real power
fencing it does not *assume* — either the agent confirms the peer is down
(exit 7) or it stays suspended (exit 1) until an operator intervenes. This
fail-safe means a misconfiguration or unreachable BMC freezes I/O rather than
risking split brain.

## Install layout

| what | path |
|------|------|
| handler script | `/usr/lib/drbd/drbd-fence-peer.sh` |
| config directory | `/etc/drbd.d/fence-peer/` |
| global settings | `/etc/drbd.d/fence-peer/fence.conf` |
| per-peer parameters | `/etc/drbd.d/fence-peer/peers/<nodename>.fence` |

## Configuration

### DRBD resource

```
resource r0 {
    net {
        fencing resource-and-stonith;
    }
    handlers {
        fence-peer "/usr/lib/drbd/drbd-fence-peer.sh";
        # No unfence-peer: STONITH power-cycles the peer; there is no
        # pacemaker location constraint to remove afterwards.
    }
    options {
        auto-promote no;   # drbd-reactor decides who is Primary
        quorum off;        # two nodes -> rely on fencing, not quorum
        on-suspended-primary-outdated force-secondary;
    }
    on node-a { node-id 0; ... }
    on node-b { node-id 1; ... }
}
```

### fence.conf

```
FENCE_ACTION="reboot"    # reboot (self-healing) | off (stays down)
FENCE_DELAY="5"          # priority-fencing-delay in seconds (0 disables)
LOG_TAG="drbd-fence-peer"
```

### peers/<nodename>.fence

One file per node, **named after the DRBD node name**, mode `0600` (holds
credentials). First line selects the agent; the rest are the agent's stdin
parameters. See `examples/ipmi/` and `examples/redfish/`.

```
AGENT=/usr/sbin/fence_ipmilan
ip=10.0.0.2
lanplus=1
login=admin
password=secret
method=onoff
```

Do **not** put a `delay=` in these files — the handler computes the fencing
delay itself (see below) and ignores a manual `delay=`.

### Priority fencing delay (automatic)

In a symmetric partition both nodes run `fence-peer` at the same instant and,
without coordination, would power-cycle each other. To avoid that, the handler
applies a **priority-fencing-delay** on its own (mirroring pacemaker's feature
of the same name), so you no longer have to hand-pick a preferred survivor.

The rule uses the only signal that is reliable during a partition — **our own
role** (the peer's role reads as `Unknown` once the connection drops):

- If we are **Primary** on the majority of the resources shared with the peer,
  we are the node actually serving data, so we **fence immediately**.
- If we are **Secondary** on the majority, we **wait `FENCE_DELAY` seconds**
  before fencing. If the peer Primary is alive it shoots us during that window;
  if it is truly dead, we simply fence it `FENCE_DELAY` seconds later and carry
  on.
- On an exact tie (e.g. both Secondary, or no shared resources) the race is
  broken by **node-id**: the lower node-id fences immediately, the higher one
  waits. This is deterministic, so exactly one node stands back — unlike a coin
  flip, which both nodes could lose the same way.

Set `FENCE_DELAY=0` in `fence.conf` to turn the feature off (both nodes then
fence immediately). Choose a value comfortably larger than the time one fence
action needs to confirm the power state, so the favoured node finishes first.

## Self-test

Runs the configured agent with `action=status` — queries the BMC, powers
nothing:

```
/usr/lib/drbd/drbd-fence-peer.sh --test <peer-node-name>
```

Beware that `action=status` can report success even when the peer→BMC mapping
is wrong: querying the *wrong* BMC still answers "on". To actually prove the
mapping, trigger a real fence and confirm the **right** node reboots:

```
DRBD_PEERS=<peer-node-name> /usr/lib/drbd/drbd-fence-peer.sh
```

If the peer survives while this node (or an innocent bystander) reboots
instead, the mapping is wrong — fix the `ip`/system parameters in that peer's
`.fence` file before relying on it.

## SELinux

The `fence-peer` handler is executed by DRBD via the kernel usermode-helper and
runs in the **`drbd_t`** domain. The drbd-utils SELinux policy
(`drbd-selinux`) already grants `drbd_t` outbound access to HTTP/HTTPS ports
(`corenet_tcp_connect_http_port`), so:

- **`fence_ipmilan`** (UDP/IPMI) works out of the box.
- **`fence_redfish`** to a BMC on a **standard** HTTP(S) port (80/443/8080/…)
  works out of the box.
- **`fence_redfish` to a BMC on a NON-standard port** is denied with
  `avc: denied { name_connect } ... tclass=tcp_socket`, because that port is
  not labeled as an HTTP port. Fix by labeling the port:

  ```
  semanage port -a -t http_port_t -p tcp <port>
  ```

  (for example, the `sushy-tools` Redfish emulator's default port 8000).

If you must permit `drbd_t` to reach an arbitrary port type instead of
relabeling it, add a small local policy module:

```
module drbd_fence_redfish 1.0;
require { type drbd_t; type <PORTTYPE>; class tcp_socket name_connect; }
allow drbd_t <PORTTYPE>:tcp_socket name_connect;
```

