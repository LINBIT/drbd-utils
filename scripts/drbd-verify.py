#!/usr/bin/env python3

import subprocess
import argparse
import datetime
import json
import sys
import re
import os

events_re = re.compile(r'change peer-device name:(\S+) peer-node-id:(\d+) conn-name:(\S+) volume:0 replication:(\w+)->(\w+)')
progress_re = re.compile(r'change peer-device name:(\S+) peer-node-id:(\d+) conn-name:(\S+) volume:0 done:(\d+\.\d+)')

host_name = os.uname()[1]
output_json = False

def log(*args, **kwargs):
    if not output_json:
        print(*args, **kwargs)

def log_peer_result(peer_result):
    for key in peer_result:
        log('\r{} out-of-sync: {}'.format(key, peer_result[key]))

def get_oos(res_name: str, peer_node_id: int) -> int:
    with subprocess.Popen(['drbdsetup', 'status', res_name, '--json'], stdout=subprocess.PIPE) as p:
        res_status_json = json.load(p.stdout)
    [con] = [con for con in res_status_json[0]['connections'] if con['peer-node-id'] == peer_node_id]
    return con['peer_devices'][0]['out-of-sync']


def wait_verify(res_name: str, peer_node_id: int):
    # drbdsetup events2 <res_name> seems to have a bug. It often stops
    # reporting any events. Therefore, I am filtering for the right resource
    # in the Python code.
    with subprocess.Popen(['drbdsetup', 'events2', '--diff'], stdout=subprocess.PIPE) as p:
        for line in p.stdout:
            m = events_re.match(line.decode('utf-8'))
            if not m:
                m = progress_re.match(line.decode('utf-8'))
                if not m:
                    continue
                m_res_name, peer_id, conn_name, percent = m.groups()
                if m_res_name != res_name or int(peer_id) != peer_node_id:
                    continue
                log('\r{}-{} {}%'.format(host_name, conn_name, percent), end='', flush=True)
                continue
            m_res_name, peer_id, conn_name, from_state, to_state = m.groups()
            if m_res_name != res_name or int(peer_id) != peer_node_id:
                continue
            if from_state in ['VerifyS', 'VerifyT'] and to_state == 'Established':
                break


def verify_peer(res_name: str, peer_json) -> int:
    peer_node_id = peer_json['peer-node-id']

    log('{}-{} start'.format(host_name, peer_json['name']), end='', flush=True)
    subprocess.run(['drbdsetup', 'verify', res_name, str(peer_node_id), '0'])
    wait_verify(res_name, peer_node_id)
    oos = get_oos(res_name, peer_node_id)
    log('\r{}-{} out-of-sync: {}'.format(host_name, peer_json['name'], oos))
    return oos


def local_mbr_md5(res_name: str) -> str:
    # Reading from the backing device, since the DRBD device refuses to
    # open when it is primary on another node.
    with subprocess.Popen(['drbdsetup', 'show', res_name, '--json'], stdout=subprocess.PIPE) as p:
        show_json = json.load(p.stdout)
    backing_disk = show_json[0]['_this_host']['volumes'][0]['backing-disk']
    p = subprocess.run(['dd if={} bs=512 count=1 iflag=direct | md5sum'.format(backing_disk)],
                       shell=True, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    return p.stdout.decode('utf-8').split()[0]


def verify_res(res_json, peers, mbr_only: bool, level2:bool):
    this_prog_path = os.path.realpath(__file__)
    this_prog_name = os.path.basename(this_prog_path)
    res_name = res_json['name']
    diskful_peers = []
    result_json = { 'oos': {}, 'mbr_md5sums': {} }

    if res_json['devices'][0]['disk-state'] != 'Diskless':
        result_json['mbr_md5sums'][host_name] = local_mbr_md5(res_name)
        if mbr_only:
            return result_json
        # local backing disk exists. Run verify.
        log('Running verify operations for {}, from this host first, then between the remotes'.format(res_name))
        for peer_json in res_json['connections']:
            peer_name = peer_json['name']
            if peers and peer_name not in peers:
                continue
            peer_disk_state = peer_json['peer_devices'][0]['peer-disk-state']
            if peer_disk_state not in ['Diskless', 'DUnknown']:
                oos = verify_peer(res_name, peer_json)
                result_json['oos'][host_name + '-' + peer_name] = oos
                diskful_peers.append(peer_json)
    else:
        print('Ignoring {}, because it is Diskless'.format(res_name), file=sys.stderr)
        return result_json

    if len(diskful_peers) >= 1:
        for i, peer_json in enumerate(diskful_peers):
            peer_name = peer_json['name']
            args = ['ssh', peer_name, '/tmp/' + this_prog_name, '--json']
            args += ['--resource', res_name, '--level2']
            peers = [p['name'] for j, p in enumerate(diskful_peers) if j > i]
            if peers:
                args.append('--peers')
                args += peers
                log('{} [remote]'.format(peer_name), end='', flush=True)
            else:
                if level2:
                    continue
                args.append('--mbr-only')
            subprocess.run(['scp', '-q', this_prog_path, '{}:/tmp/'.format(peer_name)])
            with subprocess.Popen(args, stdout=subprocess.PIPE) as p:
                peer_result = json.load(p.stdout)
                result_json['oos'].update(peer_result[res_name]['oos'])
                result_json['mbr_md5sums'].update(peer_result[res_name]['mbr_md5sums'])
                log_peer_result(peer_result[res_name]['oos'])

    return result_json


def main() -> int:
    global output_json
    result_json = {}

    desc = """This program automates running DRBD's verification
operation on all the resources of this host."""
    epilog = """It is intended for
interactive use, as it updates its progress display every 3 seconds.
It starts verifying operations between diskfull peers by copying
itself to those machines (/tmp/ folder) and running itself there. For
that, it requires logging into all peers by ssh without providing a
password. Please use ssh keys and the ssh-agent to enable passwordless
login.  It creates a file in the current working directory with the
name drbd-verify-result_YYYY-MM-DD_HHMM.json that contains the results
in JSON format."""

    arg_parser = argparse.ArgumentParser(description=desc, epilog=epilog)
    arg_parser.add_argument('-j', '--json', dest='json', action='store_true',
                            help='only output json, suspress progress output')
    arg_parser.add_argument('-r', '--resource', dest='res_names', type=str, nargs='*',
                            help='Opereate only on the specified resource')
    arg_parser.add_argument('-b', '--mbr-only', dest='mbr_only', action='store_true',
                            help='Do not start verify operations, only MBR MD5')
    arg_parser.add_argument('-2', '--level2', dest='level2', action='store_true',
                            help='For internal use, no recursion')
    arg_parser.add_argument('-p', '--peers', dest='peers', type=str, nargs='*',
                            help='run verify only to these peers')
    args = arg_parser.parse_args()
    output_json = args.json

    result_file_name = datetime.datetime.now().strftime('drbd-verify-result_%Y-%m-%d_%H%M.json')
    with subprocess.Popen(['drbdsetup', 'status', '--json'], stdout=subprocess.PIPE) as p:
        drbd_status_json = json.load(p.stdout)

    if args.res_names:
        work = [res for res in drbd_status_json if res['name'] in args.res_names]
        if not work:
            print('resource(s) does not exist.', file=sys.stderr)
            return 1
    else:
        work = drbd_status_json

    for res_json in work:
        res_name = res_json['name']
        res_json = verify_res(res_json, args.peers, args.mbr_only, args.level2)
        if res_json['oos'] or args.level2:
            result_json[res_name] = res_json
        if not args.level2:
            with open(result_file_name + '.tmp', 'w') as f:
                f.write(json.dumps(result_json, sort_keys=True, indent=4))
            os.rename(result_file_name + '.tmp', result_file_name)

    log('all results as JSON (also in file {}):'.format(result_file_name))
    print(json.dumps(result_json, sort_keys=True, indent=4))
    return 0


if __name__ == '__main__':
    sys.exit(main())
