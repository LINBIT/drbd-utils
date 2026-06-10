#!/usr/bin/env python3

import json
import sys


def load_input(argv):
    if len(argv) == 1 or argv[1] == '-':
        return sys.stdin.read()
    with open(argv[1]) as f:
        return f.read()


def main(argv):
    data = json.loads(load_input(argv))
    records = data[0]

    diskful = {}
    for rec in records:
        if 'DISKLESS' in rec.get('flags', []):
            continue
        diskful.setdefault(rec['name'], []).append(rec['node_name'])

    hosts = sorted({h for hs in diskful.values() for h in hs})

    printed = set()
    for host in hosts:
        print(f'#host {host}:')
        assigned = sorted(
            name for name, hs in diskful.items()
            if host in hs and name not in printed
        )
        if not assigned:
            continue
        for name in assigned:
            print(f'./drbd-verify.py -r {name}')
            printed.add(name)


if __name__ == '__main__':
    main(sys.argv)
