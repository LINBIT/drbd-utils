#!/usr/bin/env python3

import subprocess
import argparse
import datetime
import json
import sys
import re
import os
import math
import mmap
import shutil
import tempfile
import fcntl
import struct
from collections import Counter
from typing import Optional

ssh_opts = ['-oStrictHostKeyChecking=no']
events_re = re.compile(
    r'change peer-device name:(\S+) peer-node-id:(\d+) conn-name:(\S+) volume:0 replication:(\w+)->(\w+)')
progress_re = re.compile(
    r'change peer-device name:(\S+) peer-node-id:(\d+) conn-name:(\S+) volume:0 done:(\d+\.\d+)')

host_name = os.uname()[1]
output_json = False
this_prog_path = os.path.realpath(__file__)
this_prog_name = os.path.basename(this_prog_path)

REMOTE_SCRIPT_DIR = '/run/drbd-verify'

REQUIRED_TOOLS = ['kpartx', 'blkid', 'drbdsetup', 'drbdmeta', 'lvcreate', 'lvremove',
                  'pvs', 'vgchange',
                  'mount', 'umount', 'ssh', 'scp']

# Mapping of filesystem type to fsck tool name
FSCK_TOOLS = {
    'xfs': 'xfs_repair',
    'ext2': 'e2fsck',
    'ext3': 'e2fsck',
    'ext4': 'e2fsck',
    'vfat': 'fsck.fat',
    'fat16': 'fsck.fat',
    'fat32': 'fsck.fat',
    'ntfs': 'ntfsfix',
}

# Set of available fsck tools (populated by check_fsck_tools)
available_fsck_tools = set()


def check_required_tools() -> None:
    """Check that all required external tools are available.

    Exits with error message if any tool is missing.
    """
    missing = []
    for tool in REQUIRED_TOOLS:
        if shutil.which(tool) is None:
            missing.append(tool)

    if missing:
        print(f'Error: Required tools not found: {", ".join(missing)}', file=sys.stderr)
        print('Please install the missing tools and try again.', file=sys.stderr)
        sys.exit(1)


def check_fsck_tools() -> None:
    """Check which optional fsck tools are available.

    Populates the global available_fsck_tools set with tool names that are found.
    """
    global available_fsck_tools
    unique_tools = set(FSCK_TOOLS.values())
    for tool in unique_tools:
        if shutil.which(tool) is not None:
            available_fsck_tools.add(tool)


def parse_xfs_repair_output(stdout: str, stderr: str, returncode: int) -> dict:
    """Parse xfs_repair -n output to count errors and warnings.

    Args:
        stdout: Standard output from xfs_repair
        stderr: Standard error from xfs_repair
        returncode: Exit code (0=clean, 1=corruption, 2=dirty log)

    Returns:
        Dict with 'errors' and 'warnings' counts
    """
    errors = 0
    warnings = 0
    combined = stdout + '\n' + stderr

    # Error patterns - actions that would be taken in repair mode
    error_patterns = [
        r'\bwould\s+(clear|correct|reset|rebuild|fix|remove|free)',
        r'\bwill\s+(clear|correct|reset|rebuild|fix|remove|free)',
        r'\bclearing\b',
        r'\bcorrecting\b',
        r'\bresetting\b',
        r'\bjunking\s+entry\b',
        r'\bbad\s+(extent|fork|attribute|inode)\b',
        r'\bdisconnected\s+(inode|dir)\b',
        r'\bNo\s+modify\s+flag\s+set,\s+skipping',
    ]

    # Warning patterns - informational but concerning
    warning_patterns = [
        r'\bmissing\b',
        r'\bunexpected\b',
        r'\binconsistent\b',
    ]

    for pattern in error_patterns:
        errors += len(re.findall(pattern, combined, re.IGNORECASE))

    for pattern in warning_patterns:
        warnings += len(re.findall(pattern, combined, re.IGNORECASE))

    # If return code indicates corruption but we found no specific errors, count as 1 error
    if returncode == 1 and errors == 0:
        errors = 1

    return {'errors': errors, 'warnings': warnings}


def parse_e2fsck_output(stdout: str, stderr: str, returncode: int) -> dict:
    """Parse e2fsck -n -f output to count errors and warnings.

    Args:
        stdout: Standard output from e2fsck
        stderr: Standard error from e2fsck
        returncode: Exit code (bitmask: 1=corrected, 4=uncorrected)

    Returns:
        Dict with 'errors' and 'warnings' counts
    """
    errors = 0
    warnings = 0
    combined = stdout + '\n' + stderr

    # Error patterns - things that would be fixed
    error_patterns = [
        r'\bFIXED\b',
        r'\bCLEARED\b',
        r'\bSALVAGED\b',
        r'\bTRUNCATED\b',
        r'\bRECOVERED\b',
        r'\bIllegal\b',
        r'\bInvalid\b',
        r'\bDuplicate\b',
        r'\bMissing\b',
        r'\bError\s+reading\b',
        r'\bCorrupt\b',
        r'\?\s*\bno\b',  # Prompts answered 'no' in -n mode
    ]

    # Warning patterns
    warning_patterns = [
        r'\bwarning\b',
        r'\bnon-contiguous\b',
    ]

    for pattern in error_patterns:
        errors += len(re.findall(pattern, combined, re.IGNORECASE))

    for pattern in warning_patterns:
        warnings += len(re.findall(pattern, combined, re.IGNORECASE))

    # Check return code for errors left uncorrected
    if returncode & 4 and errors == 0:
        errors = 1

    return {'errors': errors, 'warnings': warnings}


def parse_fsck_fat_output(stdout: str, stderr: str, returncode: int) -> dict:
    """Parse fsck.fat -n output to count errors and warnings.

    Args:
        stdout: Standard output from fsck.fat
        stderr: Standard error from fsck.fat
        returncode: Exit code (0=clean, 1=errors found)

    Returns:
        Dict with 'errors' and 'warnings' counts
    """
    errors = 0
    warnings = 0
    combined = stdout + '\n' + stderr

    # Error patterns - actual filesystem problems
    error_patterns = [
        r'\bTruncating\s+file\b',
        r'\bcorrupt\b',
        r'\binvalid\b',
        r'\bcross-link\b',
        r'\borphan\b',
        r'\bcontains\s+a?\s*free\s+cluster\b',
        r'\bfirst\s+cluster\s+.*\s+out\s+of\b',
        r'\bBoth\s+FATs\s+.*\s+corrupt\b',
        r'\bshare\s+.*\s+cluster\b',
    ]

    # Warning patterns - less severe issues
    warning_patterns = [
        r'\bDirty\s+bit\s+is\s+set\b',
        r'\bFATs\s+differ\b',
        r'\breclaimed\b',
    ]

    for pattern in error_patterns:
        errors += len(re.findall(pattern, combined, re.IGNORECASE))

    for pattern in warning_patterns:
        warnings += len(re.findall(pattern, combined, re.IGNORECASE))

    # If return code indicates errors but we found none, count as 1
    if returncode == 1 and errors == 0:
        errors = 1

    return {'errors': errors, 'warnings': warnings}


def parse_ntfsfix_output(stdout: str, stderr: str, returncode: int) -> dict:
    """Parse ntfsfix -n output to count errors and warnings.

    Args:
        stdout: Standard output from ntfsfix
        stderr: Standard error from ntfsfix
        returncode: Exit code

    Returns:
        Dict with 'errors' and 'warnings' counts
    """
    errors = 0
    warnings = 0
    combined = stdout + '\n' + stderr

    # Error patterns
    error_patterns = [
        r'\bFAILED\b',
        r'\bError\b',
        r'\bcorrupt\b',
        r'\bmissing\b',
        r'\bInput/output\s+error\b',
        r'\bFailed\s+to\s+load\b',
        r'\bUnrecoverable\b',
    ]

    # Warning patterns
    warning_patterns = [
        r'\bYou\s+should\s+run\s+chkdsk\b',
        r'\bscheduled\b.*\bcheck\b',
    ]

    for pattern in error_patterns:
        errors += len(re.findall(pattern, combined, re.IGNORECASE))

    for pattern in warning_patterns:
        warnings += len(re.findall(pattern, combined, re.IGNORECASE))

    # Non-zero return with no errors means something went wrong
    if returncode != 0 and errors == 0:
        errors = 1

    return {'errors': errors, 'warnings': warnings}


def replay_xfs_log(device_path: str) -> None:
    """Replay a dirty XFS log so that ``xfs_repair -n`` can run.

    xfs_repair refuses to operate on a filesystem with a dirty log
    (exit 2), and a snapshot of a live, mounted XFS almost always has
    one. Mounting it read-only *without* ``norecovery`` makes the kernel
    replay the log onto the device; after unmounting, the log is clean
    and ``xfs_repair -n`` produces a meaningful result.

    The device here is always a writable snapshot/clone (fsck only runs
    when a snapshot was taken), so replaying onto it is safe and is
    discarded together with the snapshot. Best effort: any failure is
    ignored, leaving xfs_repair to report the dirty log as before.
    """
    mountpoint = tempfile.mkdtemp(prefix='drbd-verify-logreplay-')
    try:
        # 'ro' without 'norecovery' triggers log recovery; 'nouuid'
        # because the snapshot's UUID still matches the mounted origin.
        if run_silent(['mount', '-t', 'xfs', '-o', 'ro,nouuid',
                       device_path, mountpoint], check=False).returncode == 0:
            run_silent(['umount', mountpoint], check=False)
    finally:
        try:
            os.rmdir(mountpoint)
        except OSError:
            pass


def run_fsck_check(device_path: str, fstype: str) -> Optional[dict]:
    """Run filesystem check on a device and return error/warning counts.

    Args:
        device_path: Path to the partition device (must be unmounted)
        fstype: Filesystem type (xfs, ext2, ext3, ext4, vfat, ntfs, etc.)

    Returns:
        Dict with 'errors' and 'warnings' counts, or None if tool unavailable
    """
    tool = FSCK_TOOLS.get(fstype)
    if tool is None:
        return None

    if tool not in available_fsck_tools:
        return None

    # Build command based on tool
    if tool == 'xfs_repair':
        # A snapshot of a live XFS almost always has a dirty log, on which
        # xfs_repair -n bails out (exit 2). Replay it first so the check
        # can actually inspect the metadata.
        replay_xfs_log(device_path)
        cmd = ['xfs_repair', '-n', device_path]
        parser = parse_xfs_repair_output
    elif tool == 'e2fsck':
        cmd = ['e2fsck', '-n', '-f', device_path]
        parser = parse_e2fsck_output
    elif tool == 'fsck.fat':
        cmd = ['fsck.fat', '-n', device_path]
        parser = parse_fsck_fat_output
    elif tool == 'ntfsfix':
        cmd = ['ntfsfix', '-n', device_path]
        parser = parse_ntfsfix_output
    else:
        return None

    try:
        result = subprocess.run(cmd, capture_output=True, text=True)
        return parser(result.stdout, result.stderr, result.returncode)
    except (subprocess.SubprocessError, OSError):
        return None


def run_fsck_on_partitions(kpartx: 'KpartxMappings') -> dict:
    """Run filesystem checks on all partitions.

    Args:
        kpartx: KpartxMappings context with partition information

    Returns:
        Dict mapping partition name to fsck results:
        {partition_name: {'fstype': str, 'errors': int, 'warnings': int}, ...}
    """
    results = {}
    for name, part_info in kpartx.partitions.items():
        fstype = part_info.get('fstype')
        if not fstype:
            continue

        fsck_result = run_fsck_check(part_info['dev_path'], fstype)
        if fsck_result is not None:
            results[name] = {
                'fstype': fstype,
                'errors': fsck_result['errors'],
                'warnings': fsck_result['warnings']
            }

    return results


def run_silent(cmd: list, check: bool = True) -> subprocess.CompletedProcess:
    """Run ``cmd`` discarding output on success. On non-zero exit, print
    captured stdout and stderr to fd 2; raise CalledProcessError if
    ``check`` is True. Safe against the PIPE-buffer deadlock because
    subprocess.run drains both streams via communicate()."""
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        if result.stdout:
            print(result.stdout, end='', file=sys.stderr)
        if result.stderr:
            print(result.stderr, end='', file=sys.stderr)
        if check:
            result.check_returncode()
    return result


class Snapshot:
    """Context manager exposing a block device of a DRBD backing
    volume at ``snapshot_path`` while inside ``with``. Subclasses
    provide the storage-specific create/destroy.

    If snapshot creation fails (e.g. VG/pool out of space), the
    subclass falls back to ``snapshot_path == backing_dev`` and
    sets ``snapshot_taken = False``. Callers check that flag to
    decide whether to attempt fsck/file-analysis (both require a
    stable view) or skip to entropy-only analysis."""

    snapshot_path: str
    snapshot_taken: bool

    def __init__(self, backing_dev: str):
        self.backing_dev = backing_dev
        self.snapshot_path = backing_dev
        self.snapshot_taken = False

    def __enter__(self) -> 'Snapshot':
        raise NotImplementedError

    def __exit__(self, _exc_type, _exc_val, _exc_tb) -> bool:
        raise NotImplementedError


class LVMSnapShot(Snapshot):
    """Context manager for creating and managing LVM snapshots."""

    def __init__(self, backing_dev: str, snapshot_size: str = '1G'):
        super().__init__(backing_dev)
        self.snapshot_size = snapshot_size

        lv_name = os.path.basename(backing_dev)
        vg_path = os.path.dirname(backing_dev)

        timestamp = datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
        self.snapshot_name = f'{lv_name}_snap_{timestamp}'
        # Path that the snapshot WILL have if creation succeeds. We
        # only commit it to ``self.snapshot_path`` in __enter__ after
        # a successful lvcreate, so the fallback default
        # (snapshot_path == backing_dev from the base class) holds on
        # failure.
        self._planned_snapshot_path = os.path.join(vg_path, self.snapshot_name)

        # First char of lv_attr is 'V' for thinly-provisioned volumes.
        # Thin snapshots share the pool, so they take no -L size and are
        # created inactive with the skip-activation flag set by default.
        result = run_silent(
            ['lvs', '--noheadings', '-o', 'lv_attr', backing_dev])
        self.is_thin = result.stdout.strip().startswith('V')

    def __enter__(self) -> 'LVMSnapShot':
        if self.is_thin:
            cmd = ['lvcreate', '-s', '-kn',
                   '-n', self.snapshot_name, self.backing_dev]
        else:
            cmd = ['lvcreate', '-s', '-L', self.snapshot_size,
                   '-n', self.snapshot_name, self.backing_dev]
        lvcreate_ok = False
        try:
            run_silent(cmd)
            lvcreate_ok = True
            if self.is_thin:
                run_silent(['lvchange', '-ay', self._planned_snapshot_path])
            self.snapshot_path = self._planned_snapshot_path
            self.snapshot_taken = True
        except subprocess.CalledProcessError:
            print(f'Warning: snapshot of {self.backing_dev} failed; '
                  f'falling back to entropy-only mode (no fsck/file '
                  f'analysis)', file=sys.stderr)
            if lvcreate_ok:
                # lvcreate produced an inactive snapshot but lvchange
                # -ay couldn't activate it. Tear it down.
                run_silent(['lvremove', '-f', self._planned_snapshot_path],
                           check=False)
        return self

    def __exit__(self, _exc_type, _exc_val, _exc_tb) -> bool:
        if self.snapshot_taken:
            run_silent(['lvremove', '-f', self.snapshot_path])
        return False


def zfs_dataset_for(backing_dev: str) -> Optional[str]:
    """Return the ZFS dataset name (``<pool>/<dataset>``) if
    ``backing_dev`` is a zvol, else ``None``. DRBD may report either
    the ``/dev/zvol/<pool>/<dataset>`` symlink or the resolved
    ``/dev/zdN`` path, so reverse-match by walking ``/dev/zvol/``."""
    zvol_root = '/dev/zvol'
    if not os.path.isdir(zvol_root):
        return None
    target = os.path.realpath(backing_dev)
    for dirpath, _, filenames in os.walk(zvol_root):
        for name in filenames:
            link = os.path.join(dirpath, name)
            if os.path.realpath(link) == target:
                return os.path.relpath(link, zvol_root)
    return None


class ZFSSnapshot(Snapshot):
    """Context manager backed by a ZFS clone of a fresh snapshot."""

    def __init__(self, backing_dev: str, dataset: str):
        super().__init__(backing_dev)
        self.dataset = dataset
        timestamp = datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
        self.snap_full = f'{dataset}@drbdverify_{timestamp}'
        self.clone_full = f'{dataset}_drbdverify_{timestamp}'
        # Path the clone WILL have if creation succeeds. Only
        # committed to ``self.snapshot_path`` after a successful
        # ``zfs clone`` so the fallback default (== backing_dev)
        # holds on failure.
        self._planned_snapshot_path = f'/dev/zvol/{self.clone_full}'

    def __enter__(self) -> 'ZFSSnapshot':
        snapshot_ok = False
        try:
            run_silent(['zfs', 'snapshot', self.snap_full])
            snapshot_ok = True
            # volmode=dev: force the clone to appear as a block
            # device regardless of the pool default. The clone is
            # read/write because drbdmeta opens the device O_RDWR
            # even for dump-md.
            run_silent(
                ['zfs', 'clone', '-o', 'volmode=dev',
                 self.snap_full, self.clone_full])
            # Wait for udev to create /dev/zvol/<pool>/<clone>.
            run_silent(['udevadm', 'settle'], check=False)
            self.snapshot_path = self._planned_snapshot_path
            self.snapshot_taken = True
        except subprocess.CalledProcessError:
            print(f'Warning: snapshot of {self.backing_dev} failed; '
                  f'falling back to entropy-only mode (no fsck/file '
                  f'analysis)', file=sys.stderr)
            if snapshot_ok:
                # ``zfs snapshot`` succeeded but ``zfs clone``
                # didn't. Drop the orphan snapshot.
                run_silent(['zfs', 'destroy', self.snap_full],
                           check=False)
        return self

    def __exit__(self, _exc_type, _exc_val, _exc_tb) -> bool:
        if not self.snapshot_taken:
            return False
        # Wait for queued udev events from device users (drbdmeta,
        # entropy byte reads, kpartx) to drain. Without this, the
        # kernel may still hold a brief ``bd_holder`` reference on
        # the zvol and ``zfs destroy`` fails with "dataset is busy".
        run_silent(['udevadm', 'settle'], check=False)
        # Destroy the clone first; the snapshot can't go while a
        # clone of it exists.
        run_silent(['zfs', 'destroy', self.clone_full])
        run_silent(['zfs', 'destroy', self.snap_full])
        return False


def make_backing_snapshot(backing_dev: str) -> Snapshot:
    """Pick the snapshot backend based on the backing device type."""
    dataset = zfs_dataset_for(backing_dev)
    if dataset:
        return ZFSSnapshot(backing_dev, dataset)
    return LVMSnapShot(backing_dev)


class KpartxMappings:
    """Context manager for creating and managing kpartx partition mappings."""

    # Pattern to parse kpartx output lines like:
    # add map drbdpool-snap1p1 (253:10): 0 4096 linear 253:9 2048
    kpartx_re = re.compile(r'add map (\S+) \(\d+:\d+\): \d+ (\d+) linear \d+:\d+ (\d+)')

    def __init__(self, device_path: str):
        self.device_path = device_path
        self.partitions = {}  # name -> {start_sector, length_sectors, fstype, dev_path}
        self.has_partitions = False
        # VG UUIDs of nested LVM PVs found on our partitions. Tracked
        # so we can deactivate them and keep them down for the lifetime
        # of this context, otherwise udev's auto-activation holds the
        # partition device open and breaks ``kpartx -dv`` on exit.
        self.nested_vg_uuids: set = set()

    def __enter__(self) -> 'KpartxMappings':
        result = subprocess.run(
            ['kpartx', '-av', self.device_path],
            capture_output=True, text=True)
        # kpartx may output warnings to stderr but still succeed
        # Parse stdout for partition mappings
        for line in result.stdout.splitlines():
            match = self.kpartx_re.match(line)
            if match:
                name = match.group(1)
                length_sectors = int(match.group(2))
                start_sector = int(match.group(3))
                dev_path = f'/dev/mapper/{name}'

                self.partitions[name] = {
                    'start_sector': start_sector,
                    'length_sectors': length_sectors,
                    'dev_path': dev_path,
                    'fstype': None
                }
                self.has_partitions = True

        # Get filesystem types for each partition using blkid
        for name, part_info in self.partitions.items():
            try:
                blkid_result = subprocess.run(
                    ['blkid', part_info['dev_path']],
                    capture_output=True, text=True)
                if blkid_result.returncode == 0:
                    # Parse TYPE="..." from blkid output
                    type_match = re.search(r'TYPE="([^"]+)"', blkid_result.stdout)
                    if type_match:
                        part_info['fstype'] = type_match.group(1)
            except subprocess.CalledProcessError:
                pass  # fstype remains None

        # Discover nested LVM VGs sitting on these partitions and
        # deactivate them now; udev typically auto-activates them the
        # moment kpartx exposes the partition.
        for part_info in self.partitions.values():
            if part_info.get('fstype') == 'LVM2_member':
                uuid = self._pv_vg_uuid(part_info['dev_path'])
                if uuid:
                    self.nested_vg_uuids.add(uuid)
        self._deactivate_nested_vgs()

        return self

    def __exit__(self, _exc_type, _exc_val, _exc_tb) -> bool:
        if self.has_partitions:
            # udev may have re-activated nested VGs between __enter__
            # and now (each blkid/fsck open of a partition can retrigger
            # pvscan). Knock them down again before kpartx -dv, or the
            # partition device stays open and the snapshot can't be
            # removed.
            self._deactivate_nested_vgs()
            run_silent(['kpartx', '-dv', self.device_path], check=False)
        return False

    @staticmethod
    def _pv_vg_uuid(pv_path: str) -> Optional[str]:
        """Return the VG UUID for the PV at ``pv_path`` or None."""
        result = subprocess.run(
            ['pvs', '--noheadings', '-o', 'vg_uuid', pv_path],
            capture_output=True, text=True)
        if result.returncode != 0:
            return None
        uuid = result.stdout.strip()
        return uuid or None

    def _deactivate_nested_vgs(self) -> None:
        """Deactivate any nested guest VGs that landed on our
        partitions. Selected by VG UUID via ``--select`` to avoid
        collisions with a host-side VG that happens to share a name —
        ``vgchange`` positional arguments are VG names, not UUIDs."""
        for uuid in self.nested_vg_uuids:
            run_silent(
                ['vgchange', '-an', '--select', f'vg_uuid={uuid}'],
                check=False,
            )

    def get_partition_for_offset(self, byte_offset: int, block_size: int) -> Optional[str]:
        """Find which partition contains the given byte range.

        Args:
            byte_offset: Start offset in bytes
            block_size: Size of the block in bytes

        Returns:
            Partition name if the block overlaps with a partition, None otherwise.
        """
        # Convert byte offset to sectors (512 bytes per sector)
        block_start_sector = byte_offset // 512
        block_end_sector = (byte_offset + block_size - 1) // 512

        for name, part_info in self.partitions.items():
            part_start = part_info['start_sector']
            part_end = part_start + part_info['length_sectors'] - 1

            # Check for overlap
            if block_start_sector <= part_end and block_end_sector >= part_start:
                return name

        return None


class NoKpartxMappings:
    """Drop-in replacement for ``KpartxMappings`` used when no
    snapshot was taken. Exposes the same surface but with no
    partitions, so partition-keyed loops in the caller naturally
    degrade to no-op."""

    def __init__(self, _device_path: str):
        self.partitions = {}
        self.has_partitions = False

    def __enter__(self) -> 'NoKpartxMappings':
        return self

    def __exit__(self, _exc_type, _exc_val, _exc_tb) -> bool:
        return False

    def get_partition_for_offset(self, _byte_offset: int, _block_size: int) -> Optional[str]:
        return None


def log(*args, **kwargs):
    if not output_json:
        print(*args, **kwargs)


def log_peer_result(peer_result: dict):
    for key in peer_result:
        key_str = '-'.join(sorted(key))
        log(f'\r {key_str} out-of-sync: {peer_result[key]["value_KiB"]} KiB')


def is_resource_ready(res_json: dict) -> tuple:
    """Check if a resource is ready for verification.

    A resource is ready when all connections have replication-state "Established".
    This ensures no ongoing resync or verify operations.

    Args:
        res_json: Resource JSON from drbdsetup status

    Returns:
        Tuple of (is_ready: bool, reason: str or None)
    """
    for conn in res_json.get('connections', []):
        conn_name = conn.get('name', 'unknown')
        for peer_dev in conn.get('peer_devices', []):
            repl_state = peer_dev.get('replication-state', 'Unknown')
            if repl_state != 'Established':
                return (False, f'connection {conn_name} has replication-state {repl_state}')
    return (True, None)


def run_remote_script(peer_name: str, script_args: list, copy_script: bool = False,
                      json_object_hook=None) -> dict:
    """Run this script on a remote peer via SSH and return JSON result.

    Args:
        peer_name: The peer hostname to SSH into
        script_args: Arguments to pass to the remote script (--json is added automatically)
        copy_script: Whether to copy the script to the remote host first via SCP
        json_object_hook: Optional object_hook for json.load()

    Returns:
        Parsed JSON output from the remote script
    """
    def ssh_help_and_exit():
        print(f'\nError: Failed to connect to peer "{peer_name}"', file=sys.stderr)
        print('This tool requires passwordless SSH access to all peer nodes.', file=sys.stderr)
        print('Please set up SSH keys and use ssh-agent to enable passwordless login:', file=sys.stderr)
        print('  1. Generate SSH key: ssh-keygen', file=sys.stderr)
        print('  2. Copy public key to all peers', file=sys.stderr)
        sys.exit(10)

    try:
        if copy_script:
            # install(1) sets mode 0700 idempotently, and /run is not
            # world-writable, so a non-privileged attacker cannot plant a
            # symlink at the scp destination.
            run_silent(
                ['ssh'] + ssh_opts + [peer_name,
                 f'install -d -m 0700 {REMOTE_SCRIPT_DIR}'])
            run_silent(
                ['scp'] + ssh_opts + ['-q', this_prog_path,
                 f'{peer_name}:{REMOTE_SCRIPT_DIR}/'])

        # Invoke via python3 explicitly so the script also runs on hosts
        # that mount /run with noexec (e.g. Ubuntu). noexec blocks execve()
        # of the script's inode but not reading it as input to python3.
        args = ['ssh'] + ssh_opts + [peer_name,
                'python3', f'{REMOTE_SCRIPT_DIR}/{this_prog_name}',
                '--json'] + script_args

        with subprocess.Popen(args, stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE) as p:
            try:
                if json_object_hook:
                    result = json.load(p.stdout, object_hook=json_object_hook)
                else:
                    result = json.load(p.stdout)
            except json.JSONDecodeError:
                result = None
            remote_stderr = p.stderr.read().decode('utf-8', 'replace')
            rc = p.wait()

        if rc == 255:
            # ssh(1) reserves 255 for its own connection/auth failures.
            if remote_stderr.strip():
                print(remote_stderr, end='', file=sys.stderr)
            ssh_help_and_exit()

        if rc != 0 or result is None:
            print(f'\nError: remote drbd-verify.py on "{peer_name}" failed '
                  f'(exit {rc})', file=sys.stderr)
            if remote_stderr.strip():
                print(remote_stderr, end='', file=sys.stderr)
            sys.exit(10)

        return result
    except subprocess.CalledProcessError:
        # Reached only from the install/scp steps above.
        ssh_help_and_exit()


def get_oos(res_name: str, peer_node_id: int) -> int:
    with subprocess.Popen(['drbdsetup', 'status', res_name, '--json'], stdout=subprocess.PIPE) as p:
        res_status_json = json.load(p.stdout)
    [con] = [con for con in res_status_json[0]['connections'] if con['peer-node-id'] == peer_node_id]
    return con['peer_devices'][0]['out-of-sync']


def wait_verify(res_name: str, peer_node_id: int) -> None:
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
                log(f'\r {host_name}-{conn_name} {percent}%', end='', flush=True)
                continue
            m_res_name, peer_id, conn_name, from_state, to_state = m.groups()
            if m_res_name != res_name or int(peer_id) != peer_node_id:
                continue
            if from_state in ['VerifyS', 'VerifyT'] and to_state == 'Established':
                break


def verify_peer(res_name: str, peer_json: dict, skip_verify: bool = False) -> int:
    peer_node_id = peer_json['peer-node-id']

    if skip_verify:
        log(f' {host_name}-{peer_json["name"]} skip-verify', end='', flush=True)
    else:
        log(f' {host_name}-{peer_json["name"]} start', end='', flush=True)
        run_silent(['drbdsetup', 'verify', res_name, str(peer_node_id), '0'])
        wait_verify(res_name, peer_node_id)
    oos = get_oos(res_name, peer_node_id)
    log(f'\r {host_name}-{peer_json["name"]} out-of-sync: {oos} KiB')
    return oos


def backing_dev_res(res_name: str) -> str:
    with subprocess.Popen(['drbdsetup', 'show', res_name, '--json'], stdout=subprocess.PIPE) as p:
        show_json = json.load(p.stdout)
    return show_json[0]['_this_host']['volumes'][0]['backing-disk']


def parse_bitmap_for_peer(metadata_stream, peer_node_id: int) -> tuple:
    """Parse DRBD metadata dump to extract bitmap for a specific peer."""
    # First, find the peer section and get the bitmap-index
    bitmap_index = None
    bm_byte_per_bit = None
    in_peer_section = False
    peer_pattern = re.compile(rf'peer\[{peer_node_id}\]\s*{{')
    bitmap_index_pattern = re.compile(r'bitmap-index\s+(-?\d+);')
    bm_byte_per_bit_pattern = re.compile(r'bm-byte-per-bit\s+(\d+);')

    for raw_line in metadata_stream:
        line = raw_line.decode('utf-8').rstrip('\n')
        if peer_pattern.search(line):
            in_peer_section = True
        elif in_peer_section:
            match = bitmap_index_pattern.search(line)
            if match:
                bitmap_index = int(match.group(1))
                if bitmap_index == -1:
                    return (bm_byte_per_bit, bytes())  # No bitmap for this peer
                break
            elif line.strip() == '}':
                in_peer_section = False

    if bitmap_index is None:
        raise RuntimeError(f'Peer with node-id {peer_node_id} not found in metadata')

    for raw_line in metadata_stream:
        line = raw_line.decode('utf-8').rstrip('\n')
        if bm_byte_per_bit is None:
            match = bm_byte_per_bit_pattern.search(line)
            if match:
                bm_byte_per_bit = int(match.group(1))
                break

    if bm_byte_per_bit is None:
        raise RuntimeError('bm-byte-per-bit field not found in metadata')

    # Now find and parse the bitmap[bitmap_index] section
    bitmap_pattern = re.compile(rf'bitmap\[{bitmap_index}\]\s*{{')
    in_bitmap_section = False
    result = bytearray()

    for raw_line in metadata_stream:
        line = raw_line.decode('utf-8').rstrip('\n')
        if bitmap_pattern.search(line):
            in_bitmap_section = True
            continue
        elif in_bitmap_section:
            if line.strip() == '}':
                break
            if line.strip().startswith('#'):
                continue

            # Handle "X times 0xVALUE;" format and individual "0xVALUE;" values
            times_matches = list(re.finditer(r'(\d+)\s+times\s+(0x[0-9A-Fa-f]+);', line))
            if times_matches:
                for times_match in times_matches:
                    count = int(times_match.group(1))
                    value = int(times_match.group(2), 16)
                    value_bytes = value.to_bytes(8, byteorder='little')
                    for _ in range(count):
                        result.extend(value_bytes)
                continue

            # Handle standalone "0xVALUE;" values
            hex_matches = list(re.finditer(r'(0x[0-9A-Fa-f]+);', line))
            if hex_matches:
                for hex_match in hex_matches:
                    value = int(hex_match.group(1), 16)
                    result.extend(value.to_bytes(8, byteorder='little'))
                continue

            raise RuntimeError(f'Unexpected characters {line} found in metadata')

    # Drain any remaining bytes. dump-md continues to emit other
    # peers' bitmap[] sections plus trailing history after the one
    # we parsed; without consuming them, drbdmeta blocks on a full
    # stdout pipe (~64 KiB) and the caller's proc.wait() deadlocks.
    while metadata_stream.read(65536):
        pass

    return (bm_byte_per_bit, bytes(result))


def get_oos_bitmap(res_json: dict, peer: str, snapshot_path: str) -> tuple:
    """Get the out-of-sync bitmap for a specific peer.

    Dumps DRBD metadata from the given snapshot and extracts the bitmap
    for the specified peer.

    Args:
        res_json: Resource JSON from drbdsetup status
        peer: Peer name to get bitmap for
        snapshot_path: Path to the LVM snapshot device

    Returns:
        Tuple of (bm_byte_per_bit, bitmap_data)
    """
    [peer_node_id] = [conn['peer-node-id'] for conn in res_json['connections'] if conn['name'] == peer]

    with tempfile.TemporaryFile() as stderr_file:
        with subprocess.Popen(
                ['drbdmeta', '0', 'v09', snapshot_path, 'internal', 'dump-md', '--force'],
                stdout=subprocess.PIPE,
                stderr=stderr_file) as proc:
            bm_byte_per_bit, bitmap_data = parse_bitmap_for_peer(proc.stdout, peer_node_id)
            proc.wait()

        stderr_file.seek(0)
        for raw_line in stderr_file:
            if b'Found meta data is "unclean"' in raw_line:
                continue
            sys.stderr.buffer.write(raw_line)
        sys.stderr.flush()

        if proc.returncode != 0:
            raise subprocess.CalledProcessError(proc.returncode, proc.args)

    return (bm_byte_per_bit, bitmap_data)


def verify_res(res_json: dict, peers, level2: bool, skip_verify: bool = False) -> dict:
    res_name = res_json['name']
    diskful_peers = []
    result_json = {'oos': {}}

    if res_json['devices'][0]['disk-state'] != 'Diskless':
        # local backing disk exists. Run verify.
        log(f'Running verify operations for {res_name}, from this host first, then between the remotes')
        for peer_json in res_json['connections']:
            peer_name = peer_json['name']
            if peers and peer_name not in peers:
                continue
            peer_disk_state = peer_json['peer_devices'][0]['peer-disk-state']
            if peer_disk_state not in ['Diskless', 'DUnknown']:
                oos = verify_peer(res_name, peer_json, skip_verify)
                result_json['oos'][frozenset([host_name, peer_name])] = {'value_KiB': oos}
                diskful_peers.append(peer_json)
    else:
        print(f'Ignoring {res_name}, because it is Diskless', file=sys.stderr)
        return result_json

    if len(diskful_peers) >= 1:
        for i, peer_json in enumerate(diskful_peers):
            peer_name = peer_json['name']
            peers = [p['name'] for j, p in enumerate(diskful_peers) if j > i]
            if not peers:
                continue

            script_args = ['--resource', res_name, '--level2', '--peers'] + peers
            if skip_verify:
                script_args.append('--skip-verify')
            log(f' {peer_name} [remote]', end='', flush=True)

            peer_result = run_remote_script(peer_name, script_args, copy_script=True)
            oos_update = json_key_to_frozenset(peer_result[res_name]['oos'])
            result_json['oos'].update(oos_update)
            log_peer_result(oos_update)

    return result_json


def iterate_oos_offsets(bm_byte_per_bit: int, bitmap_data: bytes):
    """Yield disk offsets (in bytes) for each set bit in the bitmap."""
    for byte_offset in range(0, len(bitmap_data), 8):
        # Read 8 bytes as a 64-bit little-endian integer
        value = int.from_bytes(bitmap_data[byte_offset:byte_offset+8], byteorder='little')

        if value == 0:
            continue

        for bit_pos in range(64):
            if value & (1 << bit_pos):
                bit_number = (byte_offset // 8) * 64 + bit_pos
                yield bit_number


def shannon_entropy(data: bytes) -> float:
    """Calculate Shannon entropy of binary data.

    Returns entropy in bits per byte (0.0 to 8.0).
    Higher values indicate more randomness/information.
    """
    if not data:
        return 0.0

    # Count frequency of each byte value (0-255)
    byte_counts = Counter(data)
    data_len = len(data)

    # Calculate Shannon entropy
    entropy = 0.0
    for count in byte_counts.values():
        if count > 0:
            probability = count / data_len
            entropy -= probability * math.log2(probability)

    return entropy


def get_oos_blocks_entropy(backing_dev: str, bm_byte_per_bit: int, bitmap_data: bytes) -> dict:
    """Calculate Shannon entropy for each out-of-sync block.

    Reads blocks from the backing device without placing them in the
    OS's buffer cache or page cache.

    Returns a dict mapping bit_number to entropy.
    """
    entropy_dict = {}

    fd = os.open(backing_dev, os.O_RDONLY | os.O_DIRECT)
    try:
        # Create page-aligned buffer using anonymous mmap
        buffer = mmap.mmap(-1, bm_byte_per_bit)
        try:
            for bit_number in iterate_oos_offsets(bm_byte_per_bit, bitmap_data):
                disk_offset = bit_number * bm_byte_per_bit
                bytes_read = os.preadv(fd, [buffer], disk_offset)
                entropy = shannon_entropy(buffer[:bytes_read])
                entropy_dict[bit_number] = entropy
        finally:
            buffer.close()
    finally:
        os.close(fd)

    return entropy_dict


def fetch_peer_entropy(peer_name: str, res_name: str, target_peer: str) -> dict:
    """Fetch OOS block entropy from a remote peer via SSH.

    Args:
        peer_name: The peer to SSH into
        res_name: The resource name
        target_peer: The peer to calculate entropy for

    Returns:
        Dictionary mapping bit_number to entropy value
    """
    script_args = ['--resource', res_name, '--entropy-only', '--peers', target_peer]
    return run_remote_script(
        peer_name, script_args, copy_script=True,
        json_object_hook=lambda d: {int(k): v for k, v in d.items()})


def compare_peer_peer_entropy(res_name: str, connection_hosts: frozenset,
                              result_json: dict) -> None:
    """Fetch entropy from both endpoints of a remote-only connection and
    record per-peer "higher entropy" block counts into result_json.

    The connection's ``block_size`` is expected to have been populated by
    a level2 run on one of the peers; if absent it falls back to 4096 with
    a warning instead of raising KeyError."""
    [peer_name1, peer_name2] = sorted(connection_hosts)
    log(f' Calculating entropy for remote connection {peer_name1}-{peer_name2}',
        end='', flush=True)

    peer1_entropy = fetch_peer_entropy(peer_name1, res_name, peer_name2)
    peer2_entropy = fetch_peer_entropy(peer_name2, res_name, peer_name1)

    peer1_higher = 0
    peer2_higher = 0
    for bit_number, e1 in peer1_entropy.items():
        e2 = peer2_entropy.get(bit_number)
        if e2 is not None:
            if e1 > e2:
                peer1_higher += 1
            elif e2 > e1:
                peer2_higher += 1

    # block_size is normally populated by the level2 run on a diskful peer
    # and merged back in by verify_res. If it is somehow absent, fall back
    # to the usual 4 KiB bitmap granularity rather than raising a confusing
    # KeyError, and record it so downstream consumers stay consistent.
    conn = result_json['oos'][connection_hosts]
    if 'block_size' not in conn:
        log(f' WARNING: no block_size for connection {peer_name1}-{peer_name2}; '
            f'assuming 4096 bytes')
        conn['block_size'] = 4096
    bm_byte_per_bit = conn['block_size']
    bm_kbyte_per_bit = bm_byte_per_bit // 1024
    log(f'\r {peer_name1} has higher entropy for {peer1_higher*bm_kbyte_per_bit} KiB\x1b[K')
    log(f' {peer_name2} has higher entropy for {peer2_higher*bm_kbyte_per_bit} KiB')
    conn[f'{peer_name1} higher'] = peer1_higher
    conn[f'{peer_name2} higher'] = peer2_higher


def fetch_peer_fsck(peer_name: str, res_name: str) -> dict:
    """Fetch filesystem check results from a remote peer via SSH.

    Args:
        peer_name: The peer to SSH into
        res_name: The resource name

    Returns:
        Dict mapping partition name to fsck results:
        {partition_name: {'fstype': str, 'errors': int, 'warnings': int}, ...}
    """
    script_args = ['--resource', res_name, '--fsck-only', '--peers', peer_name]
    return run_remote_script(peer_name, script_args, copy_script=True)


class DatasetTransitivityError(Exception):
    """Raised when OOS values violate transitivity requirements."""
    pass


class PartitionMount:
    """Context manager for mounting a partition read-only."""

    def __init__(self, device_path: str, fstype: str):
        self.device_path = device_path
        self.fstype = fstype
        self.mountpoint = None

    def __enter__(self) -> 'PartitionMount':
        self.mountpoint = tempfile.mkdtemp(prefix='drbd-verify-mount-')

        mount_opts = ['ro']
        if self.fstype == 'xfs':
            mount_opts += ['nouuid', 'norecovery']
        elif self.fstype in ('ext3', 'ext4'):
            mount_opts.append('noload')

        try:
            run_silent(
                ['mount', '-t', self.fstype, '-o', ','.join(mount_opts),
                 self.device_path, self.mountpoint])
        except subprocess.CalledProcessError:
            os.rmdir(self.mountpoint)
            self.mountpoint = None
            raise

        return self

    def __exit__(self, _exc_type, _exc_val, _exc_tb) -> bool:
        if self.mountpoint:
            run_silent(['umount', self.mountpoint], check=False)
            try:
                os.rmdir(self.mountpoint)
            except OSError:
                pass
        return False


# FS_IOC_GETFSMAP ioctl: _IOWR(0x58, 59, struct fsmap_head). The size
# baked into the ioctl encoding is sizeof(struct fsmap_head) *with*
# fmh_keys[2] (64 + 2*64 = 192), distinct from the in-buffer offset
# constants below which exclude the keys.
FS_IOC_GETFSMAP = 0xc0c0583b

# FSMAP structure sizes (excluding flex-array members)
# struct fsmap_head (without fmh_keys/fmh_recs): 4*__u32 + 6*__u64 = 64.
# struct fsmap: 2*__u32 + 4*__u64 + 3*__u64 reserved = 64.
FSMAP_HEAD_SIZE = 64  # sizeof(struct fsmap_head), excluding fmh_keys/fmh_recs
FSMAP_SIZE = 64       # sizeof(struct fsmap)

# fmr_flags bits from uapi/linux/fsmap.h
FMR_OF_SPECIAL_OWNER = 0x10  # owner is FMR_OWN_FREE/FS/AG/... (1..10), not an inode
# Set on the very last record of a GETFSMAP query. Until it appears,
# more records remain and the query must be resubmitted with the low
# key advanced to the last record returned.
FMR_OF_LAST = 0x20


def pack_fsmap_head(keys_start: tuple, keys_end: tuple, count: int) -> bytes:
    """Pack fsmap_head structure for ioctl call.

    Args:
        keys_start: (device, block, owner, offset, flags) tuple for range start
        keys_end: (device, block, owner, offset, flags) tuple for range end
        count: Maximum number of entries to return

    Returns:
        Packed bytes for fsmap_head structure
    """
    # struct fsmap_head {
    #     __u32 fmh_iflags;       // input flags
    #     __u32 fmh_oflags;       // output flags
    #     __u32 fmh_count;        // # of entries in array
    #     __u32 fmh_entries;      // # of entries filled in
    #     __u64 fmh_reserved[6];  // reserved for future use
    #     struct fsmap fmh_keys[2]; // low and high keys for query
    #     struct fsmap fmh_recs[]; // output records
    # }
    # struct fsmap {
    #     __u32 fmr_device;       // device id
    #     __u32 fmr_flags;        // mapping flags
    #     __u64 fmr_physical;     // device offset
    #     __u64 fmr_owner;        // owner id (inode)
    #     __u64 fmr_offset;       // file offset
    #     __u64 fmr_length;       // length of mapping
    #     __u64 fmr_reserved[3];  // reserved for future use
    # }

    header = struct.pack(
        '<IIII6Q',
        0,       # fmh_iflags
        0,       # fmh_oflags
        count,   # fmh_count
        0,       # fmh_entries
        0, 0, 0, 0, 0, 0)  # fmh_reserved

    # Pack the two key structures
    key_low = struct.pack(
        '<II4Q3Q',
        keys_start[0],  # fmr_device
        keys_start[4],  # fmr_flags
        keys_start[1],  # fmr_physical
        keys_start[2],  # fmr_owner
        keys_start[3],  # fmr_offset
        0,              # fmr_length
        0, 0, 0)        # fmr_reserved

    key_high = struct.pack(
        '<II4Q3Q',
        keys_end[0],   # fmr_device
        keys_end[4],   # fmr_flags
        keys_end[1],   # fmr_physical
        keys_end[2],   # fmr_owner
        keys_end[3],   # fmr_offset
        0,             # fmr_length
        0, 0, 0)       # fmr_reserved

    return header + key_low + key_high


def unpack_fsmap_head(data: bytes) -> tuple:
    """Unpack fsmap_head structure from ioctl result.

    Returns:
        Tuple of (oflags, entries, records_list)
    """
    iflags, oflags, count, entries = struct.unpack_from('<IIII', data, 0)
    # Skip reserved and keys, go to records
    records_offset = FSMAP_HEAD_SIZE + 2 * FSMAP_SIZE
    records = []

    for i in range(entries):
        offset = records_offset + i * FSMAP_SIZE
        device, flags, physical, owner, file_offset, length = struct.unpack_from(
            '<II4Q', data, offset)
        records.append({
            'device': device,
            'flags': flags,
            'physical': physical,
            'owner': owner,
            'offset': file_offset,
            'length': length
        })

    return (oflags, entries, records)


def get_fsmap_for_range(fd: int, start_block: int, end_block: int) -> Optional[list]:
    """Get filesystem mapping for a physical block range using FS_IOC_GETFSMAP.

    Args:
        fd: File descriptor for the mounted filesystem
        start_block: Start physical block offset
        end_block: End physical block offset

    Returns:
        List of mapping records, or None if the ioctl is not supported.
        The list is complete across the requested range: GETFSMAP returns
        at most max_entries records per call, so we resubmit (advancing the
        low key past the last record) until the kernel marks the final
        record with FMR_OF_LAST.
    """
    max_entries = 1024
    # keys: (device, block, owner, offset, flags)
    low_key = (0, start_block, 0, 0, 0)
    keys_end = (0xffffffff, end_block, 0xffffffffffffffff, 0xffffffffffffffff, 0xffffffff)

    buffer_size = FSMAP_HEAD_SIZE + 2 * FSMAP_SIZE + max_entries * FSMAP_SIZE
    buffer = bytearray(buffer_size)

    all_records = []
    first = True
    while True:
        head = pack_fsmap_head(low_key, keys_end, max_entries)
        buffer[:len(head)] = head

        try:
            fcntl.ioctl(fd, FS_IOC_GETFSMAP, buffer)
        except OSError:
            if first:
                return None  # ioctl not supported
            # A continuation call failed unexpectedly; return what we have
            # rather than losing the records already gathered.
            log(' WARNING: GETFSMAP continuation failed; results may be truncated')
            break
        first = False

        _, entries, records = unpack_fsmap_head(bytes(buffer))
        if entries == 0:
            break
        all_records.extend(records)

        last = records[-1]
        if last['flags'] & FMR_OF_LAST:
            break

        # Advance the low key past the last record returned. The kernel
        # resumes strictly after it; length is not part of the key.
        next_low = (last['device'], last['physical'], last['owner'],
                    last['offset'], last['flags'])
        if next_low == low_key:
            # No forward progress (would otherwise loop forever).
            log(' WARNING: GETFSMAP did not advance; results may be truncated')
            break
        low_key = next_low

    return all_records


# FIEMAP ioctl constants
FS_IOC_FIEMAP = 0xc020660b  # Linux ioctl for FIEMAP
FIEMAP_FLAG_SYNC = 0x00000001
FIEMAP_EXTENT_LAST = 0x00000001


def get_file_extents(filepath: str) -> list:
    """Get physical extents for a file using FIEMAP ioctl.

    Args:
        filepath: Path to the file

    Returns:
        List of (physical_start, physical_end, logical_start, length) tuples
    """
    # struct fiemap {
    #     __u64 fm_start;         // logical offset to start
    #     __u64 fm_length;        // logical length of mapping
    #     __u32 fm_flags;         // flags
    #     __u32 fm_mapped_extents; // number of extents
    #     __u32 fm_extent_count;  // size of extent array
    #     __u32 fm_reserved;
    #     struct fiemap_extent fm_extents[0];
    # }
    # struct fiemap_extent {
    #     __u64 fe_logical;       // logical offset
    #     __u64 fe_physical;      // physical offset
    #     __u64 fe_length;        // length in bytes
    #     __u64 fe_reserved64[2];
    #     __u32 fe_flags;
    #     __u32 fe_reserved[3];
    # }

    extents = []
    max_extents = 256
    extent_size = 56  # sizeof(struct fiemap_extent)
    header_size = 32  # sizeof(struct fiemap) without extents

    try:
        fd = os.open(filepath, os.O_RDONLY)
        try:
            fm_start = 0
            while True:
                buffer_size = header_size + max_extents * extent_size
                buffer = bytearray(buffer_size)

                # Pack fiemap header
                struct.pack_into(
                    '<QQIIII', buffer, 0,
                    fm_start,            # fm_start
                    0xffffffffffffffff,  # fm_length (max)
                    FIEMAP_FLAG_SYNC,    # fm_flags
                    0,                   # fm_mapped_extents
                    max_extents,         # fm_extent_count
                    0)                   # fm_reserved

                fcntl.ioctl(fd, FS_IOC_FIEMAP, buffer)

                # Unpack results
                _, _, _, mapped_extents, _, _ = struct.unpack_from('<QQIIII', buffer, 0)

                if mapped_extents == 0:
                    break

                last_extent = False
                for i in range(mapped_extents):
                    offset = header_size + i * extent_size
                    logical, physical, length = struct.unpack_from('<QQQ', buffer, offset)
                    flags = struct.unpack_from('<I', buffer, offset + 40)[0]

                    extents.append((physical, physical + length, logical, length))

                    if flags & FIEMAP_EXTENT_LAST:
                        last_extent = True
                        break

                    fm_start = logical + length

                if last_extent:
                    break

        finally:
            os.close(fd)
    except (OSError, IOError):
        pass

    return extents


def find_affected_files_forward(mountpoint: str, oos_blocks: list,
                                bm_byte_per_bit: int,
                                partition_start_byte: int) -> set:
    """Find files affected by OOS blocks using forward mapping (FIEMAP).

    Walks the filesystem and checks each file's extents against OOS blocks.

    Args:
        mountpoint: Mount point of the filesystem
        oos_blocks: List of bit numbers for OOS blocks (device-wide indexes)
        bm_byte_per_bit: Bytes per bitmap bit
        partition_start_byte: Byte offset where this partition starts on
            the backing device. Subtracted from device-wide OOS offsets so
            they line up with FIEMAP physical offsets, which are relative
            to the partition's block device.

    Returns:
        Set of affected file paths (relative to mountpoint)
    """
    affected_files = set()

    # Convert OOS blocks to partition-relative byte ranges so they line
    # up with FIEMAP physical offsets.
    oos_ranges = []
    for bit_number in oos_blocks:
        start = bit_number * bm_byte_per_bit - partition_start_byte
        end = start + bm_byte_per_bit
        if end <= 0:
            continue  # bit lies entirely before the partition
        if start < 0:
            start = 0  # bit straddles the partition start
        oos_ranges.append((start, end))

    # Walk the filesystem
    for root, dirs, files in os.walk(mountpoint):
        for filename in files:
            filepath = os.path.join(root, filename)
            try:
                extents = get_file_extents(filepath)
                for phys_start, phys_end, _, _ in extents:
                    for oos_start, oos_end in oos_ranges:
                        # Check for overlap
                        if phys_start < oos_end and phys_end > oos_start:
                            rel_path = os.path.relpath(filepath, mountpoint)
                            affected_files.add(rel_path)
                            break
                    else:
                        continue
                    break
            except (OSError, IOError):
                continue

    return affected_files


def find_affected_files_reverse(mountpoint: str, oos_blocks: list,
                                bm_byte_per_bit: int,
                                partition_start_byte: int) -> Optional[set]:
    """Find files affected by OOS blocks using reverse mapping (FS_IOC_GETFSMAP).

    Args:
        mountpoint: Mount point of the filesystem
        oos_blocks: List of bit numbers for OOS blocks
        bm_byte_per_bit: Bytes per bitmap bit

    Returns:
        Set of affected file paths, or None if reverse mapping not supported
    """
    try:
        fd = os.open(mountpoint, os.O_RDONLY | os.O_DIRECTORY)
    except OSError:
        return None

    affected_inodes = set()
    ioctl_supported = None  # Track if ioctl works
    total_records = 0
    special_records = 0
    try:
        for bit_number in oos_blocks:
            # GETFSMAP physical offsets are relative to the partition's
            # block device, so translate from device-wide OOS coords.
            start_offset = bit_number * bm_byte_per_bit - partition_start_byte
            end_offset = start_offset + bm_byte_per_bit
            if end_offset <= 0:
                continue  # bit lies entirely before the partition
            if start_offset < 0:
                start_offset = 0  # bit straddles the partition start

            records = get_fsmap_for_range(fd, start_offset, end_offset)
            if records is None:
                # ioctl not supported
                return None
            ioctl_supported = True

            for record in records:
                total_records += 1
                if record['flags'] & FMR_OF_SPECIAL_OWNER:
                    # owner is one of the FMR_OWN_* constants (free
                    # space, AG metadata, log, ...), not an inode
                    special_records += 1
                    continue
                if record['owner'] > 0:
                    affected_inodes.add(record['owner'])
    finally:
        os.close(fd)

    if ioctl_supported is None:
        # No blocks to check
        return set()

    # If the kernel returned records for every query but every single
    # one was a special-owner record, the filesystem cannot map back
    # from physical extents to inodes (XFS without rmapbt=1, ext4
    # without metadata_csum_seed/...). Treat this as "reverse not
    # usable" so the caller falls back to forward mapping rather than
    # silently reporting "no files affected".
    if total_records > 0 and total_records == special_records:
        return None

    if not affected_inodes:
        return set()

    # Map inodes to file paths
    affected_files = set()
    for root, dirs, files in os.walk(mountpoint):
        for filename in files:
            filepath = os.path.join(root, filename)
            try:
                stat = os.stat(filepath)
                if stat.st_ino in affected_inodes:
                    rel_path = os.path.relpath(filepath, mountpoint)
                    affected_files.add(rel_path)
            except OSError:
                continue

    return affected_files


def analyze_partition_files(device_path: str, fstype: str, oos_blocks: list,
                            bm_byte_per_bit: int, partition_size_bytes: int,
                            partition_start_byte: int) -> dict:
    """Analyze which files are affected by OOS blocks on a partition.

    Args:
        device_path: Path to the partition device
        fstype: Filesystem type
        oos_blocks: List of bit numbers for OOS blocks within this partition
            (device-wide indexes; the helpers translate to partition-relative).
        bm_byte_per_bit: Bytes per bitmap bit
        partition_size_bytes: Size of the partition in bytes
        partition_start_byte: Byte offset where the partition starts on the
            backing device, used to translate OOS coordinates.

    Returns:
        Dict with:
        - 'affected_files': Set of affected file paths, or None if couldn't determine
        - 'method': 'reverse', 'forward', or 'entropy_only'
    """
    if not fstype:
        return {'affected_files': None, 'method': 'no_filesystem'}

    try:
        with PartitionMount(device_path, fstype) as mount:
            # Try reverse mapping first
            affected = find_affected_files_reverse(
                mount.mountpoint, oos_blocks, bm_byte_per_bit, partition_start_byte)
            if affected is not None:
                return {'affected_files': affected, 'method': 'reverse'}

            # Fallback to forward mapping: walks the filesystem and
            # checks each file's FIEMAP against OOS ranges. Cost grows
            # with file count, not partition size, but partition size
            # is the only cheap proxy we have. 16 GiB is generous
            # enough for typical /boot, /var and small data partitions
            # without risking minute-long walks on populated PB-scale
            # volumes.
            if partition_size_bytes <= 16 * 1024 * 1024 * 1024:
                affected = find_affected_files_forward(
                    mount.mountpoint, oos_blocks, bm_byte_per_bit, partition_start_byte)
                return {'affected_files': affected, 'method': 'forward'}

            # Large partition without reverse mapping support
            return {'affected_files': None, 'method': 'entropy_only'}

    except subprocess.CalledProcessError:
        return {'affected_files': None, 'method': 'mount_failed'}


def generate_resync_commands(res_name: str, source_node: str, target_node: str,
                             invoking_host: str) -> list:
    """Generate resync commands to sync from source to target node.

    Args:
        res_name: DRBD resource name
        source_node: Node with authoritative data (the blocks with higher entropy;
                    we always sync from higher-entropy blocks onto lower-entropy ones)
        target_node: Node to be overwritten
        invoking_host: The host where drbd-verify.py was invoked

    Returns:
        List of command strings to execute
    """
    commands = []

    if invoking_host == target_node:
        # Local node is target - use invalidate
        cmd = f"drbdadm invalidate --reset-bitmap=no {res_name}:{source_node}"
        commands.append(cmd)
    elif invoking_host == source_node:
        # Local node is source - use invalidate-remote
        cmd = f"drbdadm invalidate-remote --reset-bitmap=no {res_name}:{target_node}"
        commands.append(cmd)
    else:
        # Both nodes are remote - need to SSH
        cmd = f"ssh {target_node} drbdadm invalidate --reset-bitmap=no {res_name}:{source_node}"
        commands.append(cmd)

    return commands


def determine_resync_direction(oos_info: dict, node1: str, node2: str) -> tuple:
    """Pick the resync source as the node with the higher per-block
    Shannon entropy. Returns (source, target). Ties go to node1.

    fsck error/warning counts are reported on each suggestion for
    operator context but do not influence the choice."""
    key1 = f'{node1} higher'
    key2 = f'{node2} higher'
    if oos_info.get(key1, 0) >= oos_info.get(key2, 0):
        return (node1, node2)
    return (node2, node1)


def determine_datasets(oos_results: dict, all_nodes: set) -> list:
    """Determine distinct datasets from OOS relationships.

    Nodes with OOS=0 between them share the same dataset.
    For nodes in different datasets, OOS values must be equal (transitivity).

    Args:
        oos_results: Dict mapping frozenset pairs to OOS info with 'value_KiB'
        all_nodes: Set of all node names involved

    Returns:
        List of sets, each set contains node names sharing the same dataset.

    Raises:
        DatasetTransitivityError: If OOS values violate transitivity requirements.
    """
    # Build adjacency for nodes with OOS=0 (same dataset)
    same_dataset = {node: {node} for node in all_nodes}

    for pair, oos_info in oos_results.items():
        if oos_info['value_KiB'] == 0:
            nodes = list(pair)
            if len(nodes) == 2:
                node1, node2 = nodes
                # Union the sets
                combined = same_dataset[node1] | same_dataset[node2]
                for n in combined:
                    same_dataset[n] = combined

    # Get unique datasets
    datasets = []
    seen = set()
    for node in all_nodes:
        dataset_key = frozenset(same_dataset[node])
        if dataset_key not in seen:
            seen.add(dataset_key)
            datasets.append(same_dataset[node])

    # Verify transitivity: OOS between any node in dataset A and any node in dataset B
    # must be the same value
    for i, dataset_a in enumerate(datasets):
        for dataset_b in datasets[i+1:]:
            expected_oos = None
            for node_a in dataset_a:
                for node_b in dataset_b:
                    pair = frozenset([node_a, node_b])
                    if pair in oos_results:
                        oos_value = oos_results[pair]['value_KiB']
                        if expected_oos is None:
                            expected_oos = oos_value
                        elif oos_value != expected_oos:
                            raise DatasetTransitivityError(
                                f'OOS transitivity violation: {node_a}-{node_b} has {oos_value} KiB, '
                                f'but expected {expected_oos} KiB based on other pairs between '
                                f'datasets {dataset_a} and {dataset_b}'
                            )

    return datasets


def map_oos_to_partitions(oos_bitmap: tuple, kpartx: KpartxMappings) -> dict:
    """Map out-of-sync blocks to partitions.

    Args:
        oos_bitmap: Tuple of (bm_byte_per_bit, bitmap_data)
        kpartx: KpartxMappings context with partition information

    Returns:
        Dict with partition names as keys, each containing:
        - 'oos_kib': Amount of OOS data in KiB
        - 'fstype': Filesystem type or None
        Also includes 'unpartitioned' for OOS blocks outside any partition.
    """
    bm_byte_per_bit, bitmap_data = oos_bitmap
    result = {'unpartitioned': {'oos_kib': 0, 'fstype': None, 'blocks': []}}

    # Initialize result for each partition
    for name, part_info in kpartx.partitions.items():
        result[name] = {
            'oos_kib': 0,
            'fstype': part_info['fstype'],
            'blocks': []
        }

    # Map each OOS block to a partition
    for bit_number in iterate_oos_offsets(bm_byte_per_bit, bitmap_data):
        byte_offset = bit_number * bm_byte_per_bit
        partition = kpartx.get_partition_for_offset(byte_offset, bm_byte_per_bit)

        if partition:
            result[partition]['oos_kib'] += bm_byte_per_bit // 1024
            result[partition]['blocks'].append(bit_number)
        else:
            result['unpartitioned']['oos_kib'] += bm_byte_per_bit // 1024
            result['unpartitioned']['blocks'].append(bit_number)

    # Remove empty entries but keep blocks list for file analysis
    filtered_result = {}
    for name, info in result.items():
        if info['oos_kib'] > 0:
            filtered_result[name] = {
                'oos_kib': info['oos_kib'],
                'fstype': info['fstype'],
                'blocks': info['blocks']
            }

    return filtered_result


def append_dataset_analysis(result_json: dict) -> bool:
    """Run dataset analysis over all connections and record it in
    result_json. Returns False (and sets 'dataset_error') on a
    transitivity violation, True otherwise."""
    all_nodes = set()
    for connection_hosts in result_json['oos'].keys():
        all_nodes.update(connection_hosts)
    if not all_nodes:
        return True

    try:
        datasets = determine_datasets(result_json['oos'], all_nodes)
    except DatasetTransitivityError as e:
        log(f'\nERROR: {e}')
        result_json['dataset_error'] = str(e)
        return False

    result_json['datasets'] = [sorted(list(ds)) for ds in datasets]
    log('\nDataset analysis:')
    log(f'  Number of distinct datasets: {len(datasets)}')
    for i, ds in enumerate(datasets, 1):
        log(f'  Dataset {i}: {", ".join(sorted(ds))}')
    return True


def append_remote_resync_suggestion(res_name: str, connection_hosts: frozenset,
                                     oos_dict: dict, result_json: dict) -> None:
    """Append a resync suggestion for a connection whose endpoints are
    both remote to the invoking host. The direction comes from the
    peer-peer entropy comparison recorded by compare_peer_peer_entropy.
    generate_resync_commands emits an ``ssh <target> drbdadm invalidate
    ...`` for the both-remote case."""
    node1, node2 = sorted(connection_hosts)
    source, target = determine_resync_direction(oos_dict, node1, node2)
    commands = generate_resync_commands(res_name, source, target, host_name)
    result_json.setdefault('resync_suggestions', []).append({
        'connection': f'{node1}-{node2}',
        'source': source,
        'target': target,
        'commands': commands,
    })
    log(f'\nResync suggestion for {node1}-{node2}:')
    log(f'  Sync from higher entropy ({source}) to lower ({target})')
    log(f'  Command: {commands[0]}')


def process_res(res_json: dict, peers, level2: bool, skip_verify: bool = False) -> dict:
    res_name = res_json['name']
    entropy_map = {}

    result_json = verify_res(res_json, peers, level2, skip_verify)

    # Check if we have any OOS that needs local analysis
    has_local_oos = any(
        oos_dict['value_KiB'] > 0 and host_name in connection_hosts
        for connection_hosts, oos_dict in result_json['oos'].items()
    )

    if not has_local_oos:
        # Only remote connections have OOS. Handle them without a local
        # snapshot, but still produce dataset analysis and resync
        # suggestions so invoking the tool on a node without local OOS
        # (e.g. the "good" node) is just as useful as on a diskful peer.
        result_json['resync_suggestions'] = []
        for connection_hosts, oos_dict in result_json['oos'].items():
            if oos_dict['value_KiB'] > 0:
                compare_peer_peer_entropy(res_name, connection_hosts, result_json)
        if not append_dataset_analysis(result_json):
            return result_json
        for connection_hosts, oos_dict in result_json['oos'].items():
            if oos_dict['value_KiB'] > 0:
                append_remote_resync_suggestion(
                    res_name, connection_hosts, oos_dict, result_json)
        return result_json

    # We have local OOS - create a single snapshot for all analysis
    backing_dev = backing_dev_res(res_name)

    with make_backing_snapshot(backing_dev) as snapshot:
        local_bitmaps = {}  # peer_name -> (bm_byte_per_bit, bitmap_data)
        for connection_hosts, oos_dict in result_json['oos'].items():
            if oos_dict['value_KiB'] > 0 and host_name in connection_hosts:
                [peer_name] = connection_hosts - {host_name}
                local_bitmaps[peer_name] = get_oos_bitmap(res_json, peer_name, snapshot.snapshot_path)

        # Without a snapshot, kpartx-on-a-live-backing-device is
        # unsafe (concurrent writes during fsck), so substitute a
        # stub that reports no partitions and skip fsck/file
        # analysis entirely.
        kpartx_cls = (KpartxMappings if snapshot.snapshot_taken
                      else NoKpartxMappings)
        with kpartx_cls(snapshot.snapshot_path) as kpartx:
            # Run filesystem checks on partitions (before mounting for file analysis)
            if snapshot.snapshot_taken:
                log(' Running filesystem checks on local partitions...', end='', flush=True)
                local_fsck_results = run_fsck_on_partitions(kpartx)
                if local_fsck_results:
                    total_errors = sum(r['errors'] for r in local_fsck_results.values())
                    total_warnings = sum(r['warnings'] for r in local_fsck_results.values())
                    log(f'\r Filesystem check: {total_errors} errors, {total_warnings} warnings\x1b[K')
                    for part_name, fsck_info in local_fsck_results.items():
                        if fsck_info['errors'] > 0 or fsck_info['warnings'] > 0:
                            log(f'   {part_name} ({fsck_info["fstype"]}): '
                                f'{fsck_info["errors"]} errors, {fsck_info["warnings"]} warnings')
                else:
                    log(f'\r Filesystem check: no supported filesystems found\x1b[K')
            else:
                local_fsck_results = {}
                log(' Filesystem check skipped: no snapshot available')

            # Fetch remote fsck results for each peer with OOS. Skip
            # entirely without a local snapshot: we can't compare
            # without local fsck, and the peer might also be unable
            # to snapshot.
            peer_fsck_results = {}  # peer_name -> {partition_name: fsck_result}
            if snapshot.snapshot_taken:
                for connection_hosts, oos_dict in result_json['oos'].items():
                    if oos_dict['value_KiB'] > 0 and host_name in connection_hosts:
                        [peer_name] = connection_hosts - {host_name}
                        log(f' Fetching filesystem check from {peer_name}...', end='', flush=True)
                        peer_fsck = fetch_peer_fsck(peer_name, res_name)
                        peer_fsck_results[peer_name] = peer_fsck
                        if peer_fsck:
                            total_errors = sum(r['errors'] for r in peer_fsck.values())
                            total_warnings = sum(r['warnings'] for r in peer_fsck.values())
                            log(f'\r {peer_name} filesystem check: {total_errors} errors, {total_warnings} warnings\x1b[K')
                        else:
                            log(f'\r {peer_name} filesystem check: no supported filesystems found\x1b[K')

            # Calculate entropy for local OOS blocks (using snapshot)
            for connection_hosts, oos_dict in result_json['oos'].items():
                if oos_dict['value_KiB'] > 0:
                    if host_name in connection_hosts:
                        [peer_name] = connection_hosts - {host_name}
                        log(f' Calculating entropy for all out-of-sync blocks '
                            f'{host_name}-{peer_name}', end='', flush=True)
                        oos_bitmap = local_bitmaps[peer_name]
                        oos_blocks_entropy = get_oos_blocks_entropy(snapshot.snapshot_path, *oos_bitmap)
                        entropy_map[connection_hosts] = {host_name: oos_blocks_entropy}

                        peer_oos_blocks_entropy = fetch_peer_entropy(peer_name, res_name, host_name)
                        entropy_map[connection_hosts][peer_name] = peer_oos_blocks_entropy

                        local_higher = 0
                        peer_higher = 0

                        for bit_number, entropy in oos_blocks_entropy.items():
                            peer_entropy = peer_oos_blocks_entropy.get(bit_number)
                            if peer_entropy is not None:
                                if entropy > peer_entropy:
                                    local_higher += 1
                                elif peer_entropy > entropy:
                                    peer_higher += 1

                        bm_byte_per_bit = oos_bitmap[0]
                        bm_kbyte_per_bit = bm_byte_per_bit // 1024
                        log(f'\r {host_name} has higher entropy for {local_higher*bm_kbyte_per_bit} KiB\x1b[K')
                        log(f' {peer_name} has higher entropy for {peer_higher*bm_kbyte_per_bit} KiB')
                        result_json['oos'][connection_hosts][f'{host_name} higher'] = local_higher
                        result_json['oos'][connection_hosts][f'{peer_name} higher'] = peer_higher
                        result_json['oos'][connection_hosts]['block_size'] = bm_byte_per_bit
                        result_json['oos'][connection_hosts]['snapshot'] = snapshot.snapshot_taken

                        if snapshot.snapshot_taken:
                            # Map OOS blocks to partitions
                            oos_by_partition = map_oos_to_partitions(oos_bitmap, kpartx)
                            # Store full info including blocks for later file analysis
                            oos_dict['_partitions_with_blocks'] = oos_by_partition
                            # Store cleaned version (without blocks) for JSON output
                            result_json['oos'][connection_hosts]['partitions'] = {
                                name: {'oos_kib': info['oos_kib'], 'fstype': info['fstype']}
                                for name, info in oos_by_partition.items()
                            }

                            # Report unpartitioned OOS blocks
                            if 'unpartitioned' in oos_by_partition:
                                unpart = oos_by_partition['unpartitioned']
                                log(f' WARNING: {unpart["oos_kib"]} KiB of OOS data is outside any partition')
                                log('   (GPT/MBR header, gaps between partitions, or space after last partition)')

                            # Store fsck results for this connection
                            local_errors = sum(r['errors'] for r in local_fsck_results.values())
                            local_warnings = sum(r['warnings'] for r in local_fsck_results.values())
                            peer_fsck = peer_fsck_results.get(peer_name, {})
                            peer_errors = sum(r['errors'] for r in peer_fsck.values())
                            peer_warnings = sum(r['warnings'] for r in peer_fsck.values())

                            oos_dict['_fsck'] = {
                                host_name: {'errors': local_errors, 'warnings': local_warnings},
                                peer_name: {'errors': peer_errors, 'warnings': peer_warnings}
                            }
                            result_json['oos'][connection_hosts]['fsck'] = {
                                host_name: {'errors': local_errors, 'warnings': local_warnings},
                                peer_name: {'errors': peer_errors, 'warnings': peer_warnings}
                            }

                    else:
                        compare_peer_peer_entropy(res_name, connection_hosts, result_json)

            # Determine datasets
            if not append_dataset_analysis(result_json):
                return result_json

            # Analyze files on partitions and generate resync suggestions
            result_json['resync_suggestions'] = []

            for connection_hosts, oos_dict in result_json['oos'].items():
                if oos_dict['value_KiB'] > 0 and host_name in connection_hosts:
                    [peer_name] = connection_hosts - {host_name}
                    # Use internal partitions data with blocks for file analysis
                    partitions = oos_dict.get('_partitions_with_blocks', {})
                    bm_byte_per_bit = oos_dict.get('block_size', 4096)

                    files_affected = False
                    all_affected_files = set()

                    for part_name, part_info in partitions.items():
                        if part_name == 'unpartitioned':
                            continue

                        fstype = part_info.get('fstype')
                        if not fstype:
                            log(f' Partition {part_name}: no filesystem detected')
                            continue

                        oos_blocks = part_info.get('blocks', [])
                        if not oos_blocks:
                            continue

                        # Get partition size and start from kpartx
                        kpart_info = kpartx.partitions.get(part_name, {})
                        part_size_bytes = kpart_info.get('length_sectors', 0) * 512
                        part_start_byte = kpart_info.get('start_sector', 0) * 512

                        log(f' Analyzing files on partition {part_name} ({fstype})...', end='', flush=True)

                        analysis = analyze_partition_files(
                            kpart_info.get('dev_path', f'/dev/mapper/{part_name}'),
                            fstype, oos_blocks, bm_byte_per_bit, part_size_bytes,
                            part_start_byte,
                        )

                        method = analysis.get('method', 'unknown')
                        affected = analysis.get('affected_files')

                        if affected is not None:
                            if affected:
                                files_affected = True
                                all_affected_files.update(affected)
                                log(f'\r Partition {part_name}: {len(affected)} file(s) affected (via {method})\x1b[K')
                                for f in sorted(affected)[:10]:  # Show first 10
                                    log(f'   - {f}')
                                if len(affected) > 10:
                                    log(f'   ... and {len(affected) - 10} more')
                            else:
                                log(f'\r Partition {part_name}: no files affected (via {method})\x1b[K')
                        else:
                            log(f'\r Partition {part_name}: could not determine affected files ({method})\x1b[K')

                    # Store file analysis results (only meaningful
                    # if we had a snapshot to mount and inspect)
                    if snapshot.snapshot_taken:
                        oos_dict['files_affected'] = files_affected
                        if all_affected_files:
                            oos_dict['affected_files'] = sorted(list(all_affected_files))

                    # Generate resync suggestion (direction = higher entropy → lower)
                    source, target = determine_resync_direction(
                        oos_dict, host_name, peer_name
                    )
                    commands = generate_resync_commands(res_name, source, target, host_name)

                    suggestion = {
                        'connection': f'{host_name}-{peer_name}',
                        'source': source,
                        'target': target,
                        'commands': commands,
                    }
                    if snapshot.snapshot_taken:
                        suggestion['files_affected'] = files_affected
                    result_json['resync_suggestions'].append(suggestion)

                    log(f'\nResync suggestion for {host_name}-{peer_name}:')
                    log(f'  Sync from higher entropy ({source}) to lower ({target})')
                    log(f'  Command: {commands[0]}')

            # Resync suggestions for remote-remote connections (entropy
            # comparison for these was done above via compare_peer_peer_entropy).
            for connection_hosts, oos_dict in result_json['oos'].items():
                if oos_dict['value_KiB'] > 0 and host_name not in connection_hosts:
                    append_remote_resync_suggestion(
                        res_name, connection_hosts, oos_dict, result_json)

    return result_json


def json_key_to_frozenset(oos_dict: dict) -> dict:
    """Convert OOS dictionary with comma-separated string keys to frozenset keys."""
    result = {}
    for key_str, value in oos_dict.items():
        hosts = key_str.split(',')
        result[frozenset(hosts)] = value
    return result


def frozenset_to_json_key(result_json: dict) -> dict:
    """Convert frozenset keys in OOS dictionary to JSON-serializable format."""
    json_result = {}
    for res_name, res_data in result_json.items():
        json_res_data = res_data.copy()
        # Convert frozenset keys to comma-separated strings and remove internal fields
        json_res_data['oos'] = {}
        for k, v in res_data['oos'].items():
            # Remove internal fields from OOS dict
            cleaned_v = {key: val for key, val in v.items() if not key.startswith('_')}
            json_res_data['oos'][','.join(sorted(k))] = cleaned_v
        json_result[res_name] = json_res_data
    return json_result


def main() -> int:
    global output_json
    result_json = {}

    desc = """This program automates running DRBD's verification
           operation on all the resources of this host."""
    epilog = """It is intended for
             interactive use, as it updates its progress display every 3 seconds.
             It starts verifying operations between diskfull peers by copying
             itself to those machines (/run/drbd-verify/) and running itself
             there. For
             that, it requires logging into all peers by ssh without providing a
             password. Please use ssh keys and the ssh-agent to enable passwordless
             login.  It creates a file in the current working directory with the
             name drbd-verify-result_YYYY-MM-DD_HHMM.json that contains the results
             in JSON format."""

    arg_parser = argparse.ArgumentParser(description=desc, epilog=epilog)
    arg_parser.add_argument('-j', '--json', dest='json', action='store_true',
                            help='only output json, suppress progress output')
    arg_parser.add_argument('-r', '--resource', dest='res_names', type=str, nargs='*',
                            help='Operate only on the specified resource(s). '
                                 '--entropy-only and --fsck-only require exactly one.')
    arg_parser.add_argument('--skip-verify', dest='skip_verify', action='store_true',
                            help='Skip the drbdsetup verify step and analyze the '
                                 'currently out-of-sync blocks as reported by DRBD')
    arg_parser.add_argument('--level2', dest='level2', action='store_true',
                            help=argparse.SUPPRESS)
    arg_parser.add_argument('--entropy-only', dest='entropy_only', action='store_true',
                            help=argparse.SUPPRESS)
    arg_parser.add_argument('--fsck-only', dest='fsck_only', action='store_true',
                            help=argparse.SUPPRESS)
    arg_parser.add_argument('--peers', dest='peers', type=str, nargs='*',
                            help=argparse.SUPPRESS)
    arg_parser.add_argument('--do-it', dest='do_it', action='store_true',
                            help='Execute the suggested resync commands')
    args = arg_parser.parse_args()
    output_json = args.json

    check_required_tools()
    check_fsck_tools()

    with subprocess.Popen(['drbdsetup', 'status', '--json'], stdout=subprocess.PIPE) as p:
        drbd_status_json = json.load(p.stdout)

    if args.res_names:
        work = [res for res in drbd_status_json if res['name'] in args.res_names]
        if not work:
            print('resource(s) does not exist.', file=sys.stderr)
            return 1
    else:
        work = drbd_status_json

    if args.entropy_only:
        if args.res_names is None or len(args.res_names) > 1:
            print('Naming a resource is mandatory in --entropy-only mode', file=sys.stderr)
            sys.exit(10)
        if args.peers is None or len(args.peers) > 1:
            print('Exactly one peer is necessary in --entropy-only mode', file=sys.stderr)
            sys.exit(10)

        res_json = work[0]
        peer_name = args.peers[0]
        backing_dev = backing_dev_res(res_json['name'])

        # Create snapshot for consistent reads
        with make_backing_snapshot(backing_dev) as snapshot:
            oos_bitmap = get_oos_bitmap(res_json, peer_name, snapshot.snapshot_path)
            oos_blocks_entropy = get_oos_blocks_entropy(snapshot.snapshot_path, *oos_bitmap)

        print(json.dumps(oos_blocks_entropy))
        return 0

    if args.fsck_only:
        if args.res_names is None or len(args.res_names) > 1:
            print('Naming a resource is mandatory in --fsck-only mode', file=sys.stderr)
            sys.exit(10)
        if args.peers is None or len(args.peers) > 1:
            print('Exactly one peer is necessary in --fsck-only mode', file=sys.stderr)
            sys.exit(10)

        res_json = work[0]
        backing_dev = backing_dev_res(res_json['name'])

        # Create snapshot and run fsck on partitions. If the
        # snapshot can't be taken (e.g. VG/pool full on this
        # remote), return an empty dict: fsck on the live backing
        # device is not safe.
        with make_backing_snapshot(backing_dev) as snapshot:
            if snapshot.snapshot_taken:
                with KpartxMappings(snapshot.snapshot_path) as kpartx:
                    fsck_results = run_fsck_on_partitions(kpartx)
            else:
                fsck_results = {}

        print(json.dumps(fsck_results))
        return 0

    result_file_name = datetime.datetime.now().strftime('drbd-verify-result_%Y-%m-%d_%H%M.json')
    for res_json in work:
        res_name = res_json['name']

        # Skip resources with ongoing resync or verify operations
        ready, reason = is_resource_ready(res_json)
        if not ready:
            log(f'Skipping {res_name}: {reason}')
            continue

        res_json = process_res(res_json, args.peers, args.level2, args.skip_verify)
        if res_json['oos'] or args.level2:
            result_json[res_name] = res_json
        if not args.level2:
            with open(result_file_name + '.tmp', 'w') as f:
                f.write(json.dumps(frozenset_to_json_key(result_json), sort_keys=True, indent=4))
            os.rename(result_file_name + '.tmp', result_file_name)

    log(f'all results as JSON (also in file {result_file_name}):')
    print(json.dumps(frozenset_to_json_key(result_json), sort_keys=True, indent=4))

    # Execute resync commands if --do-it was specified
    if args.do_it:
        for res_name, res_data in result_json.items():
            suggestions = res_data.get('resync_suggestions', [])
            for suggestion in suggestions:
                for cmd in suggestion.get('commands', []):
                    log(f'\nExecuting: {cmd}')
                    # Commands may be local drbdadm or ssh commands
                    result = subprocess.run(cmd, shell=True)
                    if result.returncode != 0:
                        log(f'  Command failed with exit code {result.returncode}')
                    else:
                        log(f'  Command completed successfully')

    return 0


if __name__ == '__main__':
    sys.exit(main())
