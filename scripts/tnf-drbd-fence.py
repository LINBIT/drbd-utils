#!/usr/bin/env python3

# General:
#
# This is a user-mode-helper implementation for DRBD. DRBD invokes it
# in certain situations. The DRBD driver invokes the binary specified
# with the usermode_helper module parameter.
# (This has the default value of '/sbin/drbdadm').
#
# The first argument it receives is an event name (or command):
# fence-peer, pri-lost-after-sb, initial-split-brain, split-brain,
# pri-lost, disconnected, before-resync-target, before-resync-source,
# pri-on-incon-degr, local-io-error, quorum-lost, unfence-peer
#
# It may observe these environment variables:
# DRBD_MINOR, DRBD_VOLUME, DRBD_BACKING_DEV, DRBD_PEER_NODE_ID,
# DRBD_CSTATE, UP_TO_DATE_NODES
#
# This is different from the regular handler implementations. The
# handlers get invoked through drbdadm. This implementation is
# intended to be used instead of drbdadm.
#
# The DRBD driver expects one of these exit codes:
# 3 Peer’s disk state was already Inconsistent.
# 4 Peer’s disk state was successfully set to Outdated (or was
#   Outdated to begin with).
# 5 Connection to the peer node failed, peer could not be reached.
# 6 Peer refused to be outdated because the affected resource was in
#   the primary role.
# 7 Peer node was successfully fenced off the cluster. This should
#   never occur unless fencing is set to resource-and-stonith for the
#   affected resource.

# Specific to this implementation:
#
# It is to be used on Two-node OpenShift with Fencing (TNF)
# clusters. In this environment, the DRBD devices' resource files
# reside within a container. On the host level, where DRBD invokes
# this user-mode-helper, no resource files are available. It has
# access to the Pacemaker instance controlling the cluster.
#

import os
import sys
import time
import json
import fcntl
import syslog
import platform
import datetime
import subprocess
from lxml import objectify
from dateutil import parser


FENCING_WINDOW_MINUTES=5
DRBDSETUP='drbdsetup'
DEBUG=False
POLL_INTERVAL_SECONDS=0.5
DEFAULT_FILE='/etc/default/tnf-drbd-fence'
LOCK_FILE='/var/run/tnf-drbd-fence.lock'


def cib_peer_state():
    my_uname = platform.uname().node
    with subprocess.Popen(['cibadmin', '--query'], stdout=subprocess.PIPE) as p:
        xml_root = objectify.parse(p.stdout).getroot()
    ps = [ns for ns in xml_root.status.node_state if ns.attrib['uname'] != my_uname]
    return ps[0].attrib


def last_fencing_datetime(peer_name):
    with subprocess.Popen(['stonith_admin', '--last=' + peer_name, '--output-as=xml'], stdout=subprocess.PIPE) as p:
        xml_root = objectify.parse(p.stdout).getroot()
    when = parser.parse(getattr(xml_root, 'last-fenced').attrib['when'])
    status_code = xml_root.status.attrib['code']
    status_msg = xml_root.status.attrib['message']
    return (when, status_code, status_msg)


def drbdsetup_status_cstate(res_name, peer_node_id):
    with subprocess.Popen([DRBDSETUP, 'status', res_name, '--json'], stdout=subprocess.PIPE) as p:
        res_status_json = json.load(p.stdout)
    [con] = [con for con in res_status_json[0]['connections'] if con['peer-node-id'] == peer_node_id]
    return con['connection-state']


def log_event(event, res):
    known_events = ['fence-peer', 'pri-lost-after-sb', 'initial-split-brain', 'split-brain',
                    'pri-lost', 'disconnected', 'before-resync-target', 'before-resync-source',
                    'pri-on-incon-degr', 'local-io-error', 'quorum-lost', 'unfence-peer']

    if event in known_events:
        level = syslog.LOG_DEBUG
        msg = "event"
    else:
        level = syslog.LOG_NOTICE
        msg = "unknown event"

    syslog.syslog(level, '{}: {} {}'.format(res, msg, event))


def getenv_or_die(env_var_name):
    val = os.getenv(env_var_name)
    if val is None:
        msg = 'Environment Variable {} not set!'.format(env_var_name)
        syslog.syslog(syslog.LOG_WARNING, msg)
        sys.exit(msg)
    return val


def interpret_pcmk_node_status(in_ccm, crmd, join, expected, **unused):
    if DEBUG:
        syslog.syslog(syslog.LOG_DEBUG, 'in_ccm={} crmd={} join={} expected={}'.
                      format(in_ccm, crmd, join, expected))
    will_fence = join == 'down' and expected == 'member'
    clean_down = join == 'down' and expected == 'down'

    #logic from crm-fence-peer-9.sh:
    #will_fence = join == 'banned'
    #clean_down = expected == 'down' and int(in_ccm) == 0 and int(crmd) > 0
    #if not clean_down and int(in_ccm) == 0 and int(crmd) == 0:
    #    will_fence = True

    return (will_fence, clean_down)


def fence_peer(res):
    drbd_peer_node_id = int(getenv_or_die('DRBD_PEER_NODE_ID'))

    while True:
        drbd_cstate = drbdsetup_status_cstate(res, drbd_peer_node_id)
        if drbd_cstate == 'Connected':
            return ('DRBD connection reestablished', 0)

        pcmk_peer_state = cib_peer_state()
        pcmk_will_fence, pcmk_clean_down = interpret_pcmk_node_status(**pcmk_peer_state)
        if pcmk_clean_down:
            fencing_time, fencing_code, fencing_msg = last_fencing_datetime(pcmk_peer_state['uname'])
            fencing_window = datetime.timedelta(minutes=FENCING_WINDOW_MINUTES)
            recently_fenced = \
                fencing_time > datetime.datetime.now(datetime.timezone.utc) - fencing_window \
                and int(fencing_code) == 0
            if recently_fenced:
                return ('Pacemaker fenced peer at {}'.format(fencing_time), 7)
            return ('Pacemaker considers peer down', 7)

        if not pcmk_will_fence:
            return ('Pacemaker does not intend to fence', 1)

        time.sleep(POLL_INTERVAL_SECONDS)


def show_events(res_name):
    drbd_peer_node_id = int(getenv_or_die('DRBD_PEER_NODE_ID'))
    prev_join = ''
    prev_fencing_time = datetime.datetime.fromisoformat('2000-01-01T00:00:00')

    while True:
        pcmk_peer_state = cib_peer_state()
        fencing_time, fencing_code, fencing_msg = last_fencing_datetime(pcmk_peer_state['uname'])
        join = pcmk_peer_state['join']
        if join == prev_join and prev_fencing_time == fencing_time:
            time.sleep(0.5)
            continue

        cstate = drbdsetup_status_cstate(res_name, drbd_peer_node_id)
        message = '{} f: {} {} {} d: {}'.format(pcmk_peer_state, fencing_time, fencing_code,
                                                fencing_msg, cstate)

        now = datetime.datetime.now()
        print(now, message)
        syslog.syslog(syslog.LOG_NOTICE, message)

        prev_join = join
        prev_fencing_time = fencing_time


def fence_peer_exclusive(res):
    while True:
        try:
            with open(LOCK_FILE, 'x') as fw:
                fcntl.flock(fw.fileno(), fcntl.LOCK_EX)
                msg, code = fence_peer(res)
                print('{}§{}'.format(code, msg), file=fw)
                os.remove(LOCK_FILE)
                fcntl.flock(fw.fileno(), fcntl.LOCK_UN)
            break
        except FileExistsError:
            try:
                with open(LOCK_FILE, 'r') as fr:
                    time.sleep(0.2)
                    fcntl.flock(fr.fileno(), fcntl.LOCK_SH)
                    code_str, msg = fr.readline().strip().split('§')
                    code = int(code_str)
                    fcntl.flock(fr.fileno(), fcntl.LOCK_UN)
                break
            except FileNotFoundError:
                pass

    message = '{}: {}, exiting {}'.format(res, msg, code)
    syslog.syslog(syslog.LOG_NOTICE, message)
    sys.exit(code)


def main():
    if len(sys.argv) != 3:
        sys.exit('Usage: {} event resource'.format(sys.argv[0]))

    if os.path.isfile(DEFAULT_FILE):
        with open(DEFAULT_FILE) as f:
            exec(f.read(), globals())

    log_event(*sys.argv[1:3])

    if sys.argv[1] == 'fence-peer':
        fence_peer_exclusive(sys.argv[2])
    elif sys.argv[1] == 'show-events':
        show_events(sys.argv[2])


if __name__ == "__main__":
    main()

