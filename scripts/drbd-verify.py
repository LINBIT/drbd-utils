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

# Ceilings for the forward-mapping file-analysis fallback. Its cost
# scales with the number of files (inodes) walked, not raw capacity, so
# the used-inode count is the primary gate (cheap via statvfs). An
# optional partition-size ceiling is available as a secondary guard, off
# by default so capacity alone never vetoes a low-file-count volume.
# None means "no limit". Overridable with --forward-map-inode-limit /
# --forward-map-limit.
FORWARD_MAP_INODE_LIMIT_DEFAULT = 2_000_000
forward_map_inode_limit: Optional[int] = FORWARD_MAP_INODE_LIMIT_DEFAULT
forward_map_limit: Optional[int] = None

# Resolve affected inode numbers to file paths (default). Turning this
# off reports raw inode numbers and, in reverse-mapping mode, skips a
# full-tree walk+stat -- markedly cheaper on large/populated filesystems.
map_file_names = True

# Skip the per-partition affected-files mapping (mounting + reverse/
# forward mapping) entirely. OOS detection, entropy, fsck and resync
# suggestions still run; only the "which files are affected" step and its
# free/non-free/unknown breakdown are skipped. Set by --skip-file-analysis.
skip_file_analysis = False


def parse_size(s: str) -> Optional[int]:
    """Parse a size like '16G', '512M', '4096' (bytes) into bytes.

    'none'/'unlimited'/'off'/0 mean no limit and return None.
    """
    s = s.strip().lower()
    if s in ('none', 'unlimited', 'off'):
        return None
    m = re.fullmatch(r'(\d+(?:\.\d+)?)\s*([kmgt]?)(?:i?b)?', s)
    if not m:
        raise ValueError(f'invalid size: {s!r}')
    mult = {'': 1, 'k': 1024, 'm': 1024**2, 'g': 1024**3, 't': 1024**4}[m.group(2)]
    val = int(float(m.group(1)) * mult)
    return val if val > 0 else None

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


# Per-fsck-tool output-parsing spec: (error_patterns, warning_patterns,
# corrupt(returncode)). All four tools are parsed the same way -- count
# pattern hits across stdout+stderr -- so only the pattern lists and the
# "return code means corruption" predicate differ.
FSCK_PARSE_SPECS = {
    # xfs_repair -n: exit 0=clean, 1=corruption, 2=dirty log.
    'xfs_repair': (
        # error: actions that would be taken in repair mode
        [r'\bwould\s+(clear|correct|reset|rebuild|fix|remove|free)',
         r'\bwill\s+(clear|correct|reset|rebuild|fix|remove|free)',
         r'\bclearing\b',
         r'\bcorrecting\b',
         r'\bresetting\b',
         r'\bjunking\s+entry\b',
         r'\bbad\s+(extent|fork|attribute|inode)\b',
         r'\bdisconnected\s+(inode|dir)\b'],
        # Deliberately NOT an error pattern: "No modify flag set, skipping
        # ..." is printed twice by *every* clean -n run (once for phase 5,
        # once before exiting). It reports that -n suppressed a repair, not
        # that there is anything to repair. Matching it scored 2 errors on
        # every healthy XFS. A genuine finding either matches a pattern
        # above or shows up in the exit code below.
        # warning: informational but concerning
        [r'\bmissing\b',
         r'\bunexpected\b',
         r'\binconsistent\b'],
        lambda rc: rc == 1,
    ),
    # e2fsck -n -f: exit is a bitmask (1=corrected, 4=uncorrected).
    'e2fsck': (
        # error: things that would be fixed
        [r'\bFIXED\b',
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
         r'\?\s*\bno\b'],  # prompts answered 'no' in -n mode
        [r'\bwarning\b',
         r'\bnon-contiguous\b'],
        lambda rc: rc & 4,
    ),
    # fsck.fat -n: exit 0=clean, 1=errors found.
    'fsck.fat': (
        # error: actual filesystem problems
        [r'\bTruncating\s+file\b',
         r'\bcorrupt\b',
         r'\binvalid\b',
         r'\bcross-link\b',
         r'\borphan\b',
         r'\bcontains\s+a?\s*free\s+cluster\b',
         r'\bfirst\s+cluster\s+.*\s+out\s+of\b',
         r'\bBoth\s+FATs\s+.*\s+corrupt\b',
         r'\bshare\s+.*\s+cluster\b'],
        # warning: less severe issues
        [r'\bDirty\s+bit\s+is\s+set\b',
         r'\bFATs\s+differ\b',
         r'\breclaimed\b'],
        lambda rc: rc == 1,
    ),
    # ntfsfix -n: any non-zero exit means something went wrong.
    'ntfsfix': (
        [r'\bFAILED\b',
         r'\bError\b',
         r'\bcorrupt\b',
         r'\bmissing\b',
         r'\bInput/output\s+error\b',
         r'\bFailed\s+to\s+load\b',
         r'\bUnrecoverable\b'],
        [r'\bYou\s+should\s+run\s+chkdsk\b',
         r'\bscheduled\b.*\bcheck\b'],
        lambda rc: rc != 0,
    ),
}


def parse_fsck_output(tool: str, stdout: str, stderr: str, returncode: int) -> dict:
    """Count errors/warnings in an fsck tool's ``-n`` output.

    Patterns from FSCK_PARSE_SPECS[tool] are matched case-insensitively
    across stdout+stderr. If the return code signals corruption but no
    error pattern matched, one error is assumed so a non-zero exit is
    never reported as clean.

    Returns:
        Dict with 'errors' and 'warnings' counts.
    """
    error_patterns, warning_patterns, is_corrupt = FSCK_PARSE_SPECS[tool]
    combined = stdout + '\n' + stderr
    errors = sum(len(re.findall(p, combined, re.IGNORECASE))
                 for p in error_patterns)
    warnings = sum(len(re.findall(p, combined, re.IGNORECASE))
                   for p in warning_patterns)
    if is_corrupt(returncode) and errors == 0:
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
    elif tool == 'e2fsck':
        cmd = ['e2fsck', '-n', '-f', device_path]
    elif tool == 'fsck.fat':
        cmd = ['fsck.fat', '-n', device_path]
    elif tool == 'ntfsfix':
        cmd = ['ntfsfix', '-n', device_path]
    else:
        return None

    try:
        result = subprocess.run(cmd, capture_output=True, text=True)
        return parse_fsck_output(tool, result.stdout, result.stderr,
                                 result.returncode)
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


def drbdsetup_json(*args):
    """Run ``drbdsetup <args> --json`` and return the parsed result.

    run_silent drains both pipes, so this is safe against the PIPE-buffer
    deadlock and raises CalledProcessError on a non-zero drbdsetup exit.
    For streaming subcommands (events2) keep using Popen directly."""
    return json.loads(run_silent(['drbdsetup', *args, '--json']).stdout)


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


# Warnings accumulated for the current resource. main() snapshots this into
# the resource result and clears it around each process_res call.
_warnings: list = []


def warn(msg: str) -> None:
    """Log a warning and record it for the JSON result."""
    log(f' WARNING: {msg}')
    _warnings.append(msg.strip())


def node_names_match(a: str, b: str) -> bool:
    """Whether two DRBD node names refer to the same node.

    Exact match wins. Only when one name is a bare short name (no dot) and
    the other is an FQDN (has a dot) do we fall back to comparing the
    leading label -- this reconciles a short os.uname() name with an FQDN
    drbdsetup connection name. Two distinct FQDNs, or two distinct short
    names, are never treated as equal.
    """
    if a == b:
        return True
    a_short = '.' not in a
    b_short = '.' not in b
    if a_short != b_short:
        return a.split('.', 1)[0] == b.split('.', 1)[0]
    return False


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
    res_status_json = drbdsetup_json('status', res_name)
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
    show_json = drbdsetup_json('show', res_name)
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
    [peer_node_id] = [conn['peer-node-id'] for conn in res_json['connections']
                      if node_names_match(conn['name'], peer)]

    with tempfile.TemporaryFile() as stderr_file:
        with subprocess.Popen(
                ['drbdmeta', '-', 'v09', snapshot_path, 'internal', 'dump-md', '--force'],
                stdout=subprocess.PIPE,
                stderr=stderr_file) as proc:
            try:
                bm_byte_per_bit, bitmap_data = parse_bitmap_for_peer(proc.stdout, peer_node_id)
            except RuntimeError:
                # A drbdmeta failure (empty/truncated dump) surfaces here as a parse error
                proc.wait()
                stderr_file.seek(0)
                for raw_line in stderr_file:
                    sys.stderr.buffer.write(raw_line)
                sys.stderr.flush()
                if proc.returncode != 0:
                    raise subprocess.CalledProcessError(proc.returncode, proc.args)
                raise
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
            if peers and not any(node_names_match(peer_name, p) for p in peers):
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
            # Propagate the forward-mapping ceiling so a remote peer that
            # runs the file analysis for a remote-remote connection uses
            # the same policy as the invoking host.
            script_args += ['--forward-map-limit',
                            'none' if forward_map_limit is None else str(forward_map_limit),
                            '--forward-map-inode-limit',
                            'none' if forward_map_inode_limit is None
                            else str(forward_map_inode_limit)]
            if not map_file_names:
                script_args.append('--no-file-names')
            if skip_file_analysis:
                script_args.append('--skip-file-analysis')
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


def count_entropy_higher(entropy_a: dict, entropy_b: dict) -> tuple:
    """Count blocks where a's entropy exceeds b's, and vice versa.

    Only blocks present in both maps are compared; ties count for neither.
    Returns (a_higher, b_higher)."""
    a_higher = 0
    b_higher = 0
    for bit_number, ea in entropy_a.items():
        eb = entropy_b.get(bit_number)
        if eb is not None:
            if ea > eb:
                a_higher += 1
            elif eb > ea:
                b_higher += 1
    return (a_higher, b_higher)


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

    peer1_higher, peer2_higher = count_entropy_higher(peer1_entropy, peer2_entropy)

    # block_size is normally populated by the level2 run on a diskful peer
    # and merged back in by verify_res. If it is somehow absent, fall back
    # to the usual 4 KiB bitmap granularity rather than raising a confusing
    # KeyError, and record it so downstream consumers stay consistent.
    conn = result_json['oos'][connection_hosts]
    if 'block_size' not in conn:
        warn(f'no block_size for connection {peer_name1}-{peer_name2}; '
             f'assuming 4096 bytes')
        conn['block_size'] = 4096
        conn['block_size_assumed'] = True
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

# Pseudo-owner values (uapi/linux/fsmap.h): FMR_OWNER(type, code) =
# ((__u64)type << 32) | code. FMR_OWN_FREE and FMR_OWN_UNKNOWN are
# generic and use type 0, so their values are just 1 and 2; the XFS
# metadata owners (FS/LOG/AG/...) use type 'X' (0x58...). Free space is
# the one special owner that lets us conclude "no file here" with
# certainty; UNKNOWN is used space the filesystem cannot attribute to an
# owner (e.g. XFS rmapbt=0), and the type-'X' values are real metadata.
FMR_OWN_FREE = (0 << 32) | 1     # FMR_OWNER(0, 1)
FMR_OWN_UNKNOWN = (0 << 32) | 2  # FMR_OWNER(0, 2)


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
    # Bound the query to the filesystem's own device. GETFSMAP orders
    # records by (device, physical); a key span of device 0..0xffffffff
    # leaves the physical range unenforced for the real device (whose id
    # sits strictly inside that span), so every query returns the whole
    # device. Pin both keys to fstat().st_dev so the physical range
    # actually applies.
    dev = os.fstat(fd).st_dev
    # keys: (device, block, owner, offset, flags)
    low_key = (dev, start_block, 0, 0, 0)
    keys_end = (dev, end_block, 0xffffffffffffffff, 0xffffffffffffffff, 0xffffffff)

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
            warn('GETFSMAP continuation failed; results may be truncated')
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
            warn('GETFSMAP did not advance; results may be truncated')
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


def oos_bit_ranges(oos_blocks: list, bm_byte_per_bit: int,
                   partition_start_byte: int) -> list:
    """Translate OOS bit numbers to partition-relative [start, end) byte
    ranges, clamped to the partition. Bits lying entirely before the
    partition are dropped; a bit straddling the start is clamped to 0.
    Returned unsorted, in oos_blocks order."""
    ranges = []
    for bit_number in oos_blocks:
        start = bit_number * bm_byte_per_bit - partition_start_byte
        end = start + bm_byte_per_bit
        if end <= 0:
            continue  # bit lies entirely before the partition
        if start < 0:
            start = 0  # bit straddles the partition start
        ranges.append((start, end))
    return ranges


def find_affected_files_forward(mountpoint: str, oos_blocks: list,
                                bm_byte_per_bit: int,
                                partition_start_byte: int,
                                want_names: bool = True) -> set:
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
        want_names: return relative paths (True) or inode numbers (False).

    Returns:
        Set of affected file paths (relative to mountpoint), or inode
        numbers when ``want_names`` is False.
    """
    affected_files = set()

    # Convert OOS blocks to partition-relative byte ranges so they line
    # up with FIEMAP physical offsets.
    oos_ranges = oos_bit_ranges(oos_blocks, bm_byte_per_bit, partition_start_byte)

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
                            if want_names:
                                affected_files.add(
                                    os.path.relpath(filepath, mountpoint))
                            else:
                                affected_files.add(os.stat(filepath).st_ino)
                            break
                    else:
                        continue
                    break
            except (OSError, IOError):
                continue

    return affected_files


def find_affected_files_reverse(mountpoint: str, oos_blocks: list,
                                bm_byte_per_bit: int,
                                partition_start_byte: int,
                                block_stats: Optional[dict] = None) -> Optional[set]:
    """Find files affected by OOS blocks using reverse mapping (FS_IOC_GETFSMAP).

    Args:
        mountpoint: Mount point of the filesystem
        oos_blocks: List of bit numbers for OOS blocks
        bm_byte_per_bit: Bytes per bitmap bit

    Returns:
        Set of affected inode numbers (empty when every OOS block maps to
        free space), or None if reverse mapping is unusable and the caller
        should fall back to forward mapping. The caller resolves inodes to
        paths only if requested.

    If ``block_stats`` is passed (a dict), it is filled in place with the
    per-OOS-block classification counts 'free'/'filedata'/'metadata'/
    'unknown' (mutually exclusive, so they sum to the block count), plus a
    'metadata_seen' bool. The bool is set whenever any OOS extent belonged
    to filesystem metadata, even if the block also carried file data (in
    which case it is counted under 'filedata'), so metadata involvement is
    visible whether or not file data is affected as well.
    """
    if block_stats is not None:
        block_stats.update(free=0, filedata=0, metadata=0, unknown=0,
                           metadata_seen=False)

    # Translate each OOS bit to a partition-relative [start, end) byte
    # range (clamped to the partition) and coalesce contiguous ranges into
    # runs. GETFSMAP is queried once per run, not once per block: a bounded
    # query seeks straight to the run and returns only the extents
    # overlapping it, so cost scales with the OOS footprint rather than the
    # whole-device extent count, and a file extent spanning many OOS blocks
    # is fetched once rather than per block.
    ranges = oos_bit_ranges(oos_blocks, bm_byte_per_bit, partition_start_byte)
    ranges.sort()

    runs = []  # [run_start, run_end, [member (start, end), ...]]
    for start, end in ranges:
        if runs and start <= runs[-1][1]:
            runs[-1][1] = max(runs[-1][1], end)
            runs[-1][2].append((start, end))
        else:
            runs.append([start, end, [(start, end)]])

    try:
        fd = os.open(mountpoint, os.O_RDONLY | os.O_DIRECTORY)
    except OSError:
        return None

    affected_inodes = set()
    ioctl_supported = None  # Track if ioctl works
    saw_unknown = False     # FMR_OWN_UNKNOWN seen -> reverse can't resolve
    saw_metadata = False    # any filesystem-metadata extent seen
    try:
        for run_start, run_end, members in runs:
            records = get_fsmap_for_range(fd, run_start, run_end)
            if records is None:
                return None  # ioctl not supported
            ioctl_supported = True

            # Classify the run's extents and collect any inode owners
            # (files with data in the OOS run).
            extents = []  # (ext_start, ext_end, kind)
            for record in records:
                phys = record['physical']
                if record['flags'] & FMR_OF_SPECIAL_OWNER:
                    # owner is one of the FMR_OWN_* constants (free space,
                    # unknown, AG metadata, log, ...), not an inode
                    if record['owner'] == FMR_OWN_FREE:
                        kind = 'free'
                    elif record['owner'] == FMR_OWN_UNKNOWN:
                        kind = 'unknown'
                        saw_unknown = True
                    else:
                        kind = 'metadata'  # static fs metadata (FS/LOG/AG/...)
                        saw_metadata = True
                else:
                    kind = 'filedata'  # inode-owned: real file data
                    if record['owner'] > 0:
                        affected_inodes.add(record['owner'])
                extents.append((phys, phys + record['length'], kind))

            if block_stats is None:
                continue

            # Classify each member block by the extents overlapping it.
            # Both members and extents are in physical order, so sweep them
            # together (O(members + extents) per run). A block wins the
            # strongest class it touches: filedata > metadata >
            # unknown/uncovered > free.
            extents.sort()
            j0 = 0
            for bstart, bend in members:
                while j0 < len(extents) and extents[j0][1] <= bstart:
                    j0 += 1
                kinds = set()
                j = j0
                while j < len(extents) and extents[j][0] < bend:
                    if extents[j][1] > bstart:
                        kinds.add(extents[j][2])
                    j += 1
                if 'filedata' in kinds:
                    block_stats['filedata'] += 1
                elif 'metadata' in kinds:
                    block_stats['metadata'] += 1
                elif kinds == {'free'}:
                    block_stats['free'] += 1
                else:  # 'unknown', or no covering extent
                    block_stats['unknown'] += 1
    finally:
        os.close(fd)
        if block_stats is not None:
            block_stats['metadata_seen'] = saw_metadata

    if ioctl_supported is None:
        # No blocks to check
        return set()

    # Files resolved: report their inode numbers (the caller maps them to
    # paths only if names were requested).
    if affected_inodes:
        return affected_inodes

    # No inode owners. If any OOS extent was unattributable
    # (FMR_OWN_UNKNOWN, e.g. XFS rmapbt=0), reverse mapping cannot decide
    # here -- fall back to forward mapping. Otherwise every OOS extent was
    # free space or filesystem metadata, so no file is affected: report an
    # empty set with confidence rather than walking the tree.
    if saw_unknown:
        return None
    return set()


def resolve_inodes_to_paths(mountpoint: str, inodes: set) -> dict:
    """Walk ``mountpoint`` and map each inode in ``inodes`` to the relative
    path(s) carrying it. This is the expensive step (lstat per entry) that
    reporting inode numbers alone avoids.

    Directories are walked too, not just files: directory data blocks are
    inode-owned just like file data, so a directory is a perfectly ordinary
    owner of an out-of-sync extent. lstat() is used so that a symlink is
    identified by its own inode rather than its target's.

    Inodes absent from the returned dict could not be resolved to a path.
    The caller must still treat them as affected -- reverse mapping already
    proved the block belongs to them."""
    found: dict = {}
    if not inodes:
        return found
    try:
        root_ino = os.stat(mountpoint).st_ino
        if root_ino in inodes:
            found.setdefault(root_ino, set()).add('.')
    except OSError:
        pass
    for root, dirs, files in os.walk(mountpoint):
        for name in dirs + files:
            path = os.path.join(root, name)
            try:
                ino = os.lstat(path).st_ino
            except OSError:
                continue
            if ino in inodes:
                found.setdefault(ino, set()).add(os.path.relpath(path, mountpoint))
    return found


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
        - 'affected_files': Set of affected file paths (or inode numbers when
          name resolution is disabled), or None if couldn't determine
        - 'affected_count': How many owners were found, whether or not a path
          could be put to them. This, not len(affected_files), decides
          "are files affected": an owner that resolves to no path is still an
          owner, and dropping it would turn a positive into a false negative.
        - 'unresolved_inodes': Sorted inode numbers that were found to own an
          OOS block but could not be resolved to a path.
        - 'method': 'reverse', 'forward', or 'entropy_only'
        - 'block_stats': {'free', 'filedata', 'metadata', 'unknown'}
          OOS-block counts plus a 'metadata_seen' bool. Only the 'reverse'
          method classifies blocks; the others cannot cheaply do so and
          report every block as 'unknown'.
    """
    # Fallback classification for paths that cannot split ownership:
    # everything is of unattributable ownership, metadata undetermined.
    unknown_stats = {'free': 0, 'filedata': 0, 'metadata': 0,
                     'unknown': len(oos_blocks), 'metadata_seen': False}

    if not fstype:
        return {'affected_files': None, 'affected_count': 0,
                'unresolved_inodes': [], 'method': 'no_filesystem',
                'block_stats': unknown_stats}

    try:
        with PartitionMount(device_path, fstype) as mount:
            # Try reverse mapping first. It yields inode numbers; resolve
            # them to paths (a full-tree walk) only if names were asked for.
            block_stats = {'free': 0, 'filedata': 0, 'metadata': 0,
                           'unknown': 0, 'metadata_seen': False}
            affected = find_affected_files_reverse(
                mount.mountpoint, oos_blocks, bm_byte_per_bit, partition_start_byte,
                block_stats=block_stats)
            if affected is not None:
                # Reverse mapping yields inode numbers, and that set is the
                # authoritative answer to "is anything affected". Path
                # resolution is only cosmetics on top of it and may come up
                # empty (an inode whose entry is gone, an unreadable
                # subtree); those inodes are reported as numbers so the
                # verdict never silently degrades to "no files affected".
                inodes = set(affected)
                unresolved = []
                if map_file_names and inodes:
                    found = resolve_inodes_to_paths(mount.mountpoint, inodes)
                    affected = set()
                    for paths in found.values():
                        affected |= paths
                    unresolved = sorted(inodes - set(found))
                return {'affected_files': affected, 'affected_count': len(inodes),
                        'unresolved_inodes': unresolved, 'method': 'reverse',
                        'block_stats': block_stats}

            # Fallback to forward mapping: walk the filesystem and check
            # each file's FIEMAP against the OOS ranges. The walk cost
            # scales with the number of files visited, so gate on the
            # used-inode count (cheap, one statvfs). The optional size
            # ceiling is a secondary guard. If the inode count cannot be
            # determined, do not let it veto the walk; the size ceiling
            # (when set) still applies.
            try:
                vfs = os.statvfs(mount.mountpoint)
                used_inodes = vfs.f_files - vfs.f_ffree if vfs.f_files else None
            except OSError:
                used_inodes = None

            inode_ok = (forward_map_inode_limit is None or used_inodes is None
                        or used_inodes <= forward_map_inode_limit)
            size_ok = (forward_map_limit is None
                       or partition_size_bytes <= forward_map_limit)
            if inode_ok and size_ok:
                affected = find_affected_files_forward(
                    mount.mountpoint, oos_blocks, bm_byte_per_bit, partition_start_byte,
                    want_names=map_file_names)
                return {'affected_files': affected,
                        'affected_count': len(affected),
                        'unresolved_inodes': [], 'method': 'forward',
                        'block_stats': unknown_stats}

            # Large partition without reverse mapping support
            return {'affected_files': None, 'affected_count': 0,
                    'unresolved_inodes': [], 'method': 'entropy_only',
                    'block_stats': unknown_stats}

    except subprocess.CalledProcessError:
        return {'affected_files': None, 'affected_count': 0,
                'unresolved_inodes': [], 'method': 'mount_failed',
                'block_stats': unknown_stats}


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

                        local_higher, peer_higher = count_entropy_higher(
                            oos_blocks_entropy, peer_oos_blocks_entropy)

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
                                warn(f'{unpart["oos_kib"]} KiB of OOS data is outside any '
                                     f'partition (GPT/MBR header, gaps between partitions, '
                                     f'or space after the last partition)')

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
                            # Per-partition detail for the v2 result (node
                            # partition names differ, so keep them per node).
                            oos_dict['_fsck_detail'] = {
                                host_name: local_fsck_results,
                                peer_name: peer_fsck,
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
                    # Owners that reverse mapping proved to be affected but
                    # whose path could not be determined. Reported as inode
                    # numbers rather than dropped.
                    all_unresolved_inodes = set()
                    # A "no files affected" verdict is authoritative only
                    # when every OOS-carrying partition was resolved by an
                    # ownership-aware method (reverse/forward). Record each
                    # partition's method and track this so a false in the
                    # JSON is self-describing rather than ambiguous.
                    pub_partitions = oos_dict.get('partitions', {})
                    analysis_conclusive = True
                    # Metadata conclusiveness is tracked separately and is
                    # strictly stronger: only reverse mapping ever looks at
                    # filesystem metadata. Forward mapping walks the tree and
                    # tests file extents, which settles the file question but
                    # says nothing at all about metadata, so it must not make
                    # a metadata_affected == False authoritative.
                    metadata_conclusive = True

                    # Roll up how much of this connection's OOS is
                    # definitively free space, file data, filesystem
                    # metadata, or of unattributable ownership (KiB). Only
                    # reverse mapping can split these; other paths count the
                    # whole partition as unknown.
                    bm_kib = bm_byte_per_bit // 1024
                    metadata_affected = False
                    oos_kib_roll = {'free': 0, 'filedata': 0, 'metadata': 0,
                                    'unknown': 0}

                    def record_kib(pname, free_kib, filedata_kib, metadata_kib,
                                   unknown_kib):
                        if pname in pub_partitions:
                            pub_partitions[pname]['oos_free_kib'] = free_kib
                            pub_partitions[pname]['oos_filedata_kib'] = filedata_kib
                            pub_partitions[pname]['oos_metadata_kib'] = metadata_kib
                            pub_partitions[pname]['oos_unknown_kib'] = unknown_kib
                        oos_kib_roll['free'] += free_kib
                        oos_kib_roll['filedata'] += filedata_kib
                        oos_kib_roll['metadata'] += metadata_kib
                        oos_kib_roll['unknown'] += unknown_kib

                    for part_name, part_info in partitions.items():
                        if part_name == 'unpartitioned':
                            # OOS outside any partition (GPT/MBR header,
                            # gaps). Not inspected as a filesystem and could
                            # hide an undetected one, so it does not make a
                            # "no files" verdict authoritative.
                            if part_name in pub_partitions:
                                pub_partitions[part_name]['method'] = 'unpartitioned'
                            record_kib(part_name, 0, 0, 0, part_info.get('oos_kib', 0))
                            analysis_conclusive = metadata_conclusive = False
                            continue

                        fstype = part_info.get('fstype')
                        if not fstype:
                            log(f' Partition {part_name}: no filesystem detected')
                            if part_name in pub_partitions:
                                pub_partitions[part_name]['method'] = 'no_filesystem'
                            record_kib(part_name, 0, 0, 0, part_info.get('oos_kib', 0))
                            analysis_conclusive = metadata_conclusive = False
                            continue

                        oos_blocks = part_info.get('blocks', [])
                        if not oos_blocks:
                            continue

                        if skip_file_analysis:
                            log(f' Partition {part_name} ({fstype}): '
                                f'{part_info.get("oos_kib", 0)} KiB OOS, '
                                f'skipping file analysis (--skip-file-analysis)')
                            if part_name in pub_partitions:
                                pub_partitions[part_name]['method'] = 'skipped'
                            record_kib(part_name, 0, 0, 0, part_info.get('oos_kib', 0))
                            analysis_conclusive = metadata_conclusive = False
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
                        # Owners found, whether or not a path was put to
                        # them; see analyze_partition_files().
                        affected_count = analysis.get('affected_count', 0)
                        unresolved = analysis.get('unresolved_inodes') or []

                        stats = analysis.get('block_stats',
                                             {'free': 0, 'filedata': 0,
                                              'metadata': 0,
                                              'unknown': len(oos_blocks),
                                              'metadata_seen': False})
                        record_kib(part_name, stats['free'] * bm_kib,
                                   stats['filedata'] * bm_kib,
                                   stats['metadata'] * bm_kib,
                                   stats['unknown'] * bm_kib)
                        if stats.get('metadata_seen'):
                            metadata_affected = True

                        if part_name in pub_partitions:
                            pub_partitions[part_name]['method'] = method
                            pub_partitions[part_name]['metadata_affected'] = \
                                bool(stats.get('metadata_seen'))
                        if method not in ('reverse', 'forward'):
                            # entropy_only, mount_failed, unknown: file
                            # ownership was not actually determined.
                            analysis_conclusive = False
                        if method != 'reverse':
                            # Only GETFSMAP reports metadata ownership. A
                            # forward FIEMAP walk settles the file question
                            # but never saw a metadata extent, so it cannot
                            # make "no metadata affected" authoritative.
                            metadata_conclusive = False

                        meta_note = ' (+ fs metadata)' if stats.get('metadata_seen') else ''
                        if affected is not None:
                            if affected_count:
                                files_affected = True
                                all_affected_files.update(affected)
                                all_unresolved_inodes.update(unresolved)
                                log(f'\r Partition {part_name}: {affected_count} file(s) affected{meta_note} (via {method})\x1b[K')
                                for f in sorted(affected)[:10]:  # Show first 10
                                    log(f'   - {f}')
                                if len(affected) > 10:
                                    log(f'   ... and {len(affected) - 10} more')
                                for ino in unresolved[:10]:
                                    log(f'   - inode {ino} (no path found; '
                                        f'directory or unlinked entry)')
                                if len(unresolved) > 10:
                                    log(f'   ... and {len(unresolved) - 10} '
                                        f'more unresolved inode(s)')
                            elif stats.get('metadata_seen'):
                                log(f'\r Partition {part_name}: no file data affected, but fs metadata is out of sync (via {method})\x1b[K')
                            else:
                                log(f'\r Partition {part_name}: no files affected (via {method})\x1b[K')
                        else:
                            log(f'\r Partition {part_name}: could not determine affected files{meta_note} ({method})\x1b[K')

                    # Store file analysis results (only meaningful
                    # if we had a snapshot to mount and inspect)
                    if snapshot.snapshot_taken:
                        oos_dict['files_affected'] = files_affected
                        # files_affected == False is authoritative only when
                        # the analysis was conclusive; otherwise it means
                        # "could not determine". files_affected == True is
                        # always authoritative (the files were found).
                        oos_dict['files_affected_conclusive'] = (
                            files_affected or analysis_conclusive)
                        # metadata_affected mirrors files_affected: True is
                        # always authoritative (metadata extents were seen);
                        # False is authoritative only when every OOS-carrying
                        # partition was reverse-mapped, else it means "could
                        # not determine". Note this is a stricter bar than
                        # files_affected_conclusive, which forward mapping
                        # also satisfies.
                        oos_dict['metadata_affected'] = metadata_affected
                        oos_dict['metadata_affected_conclusive'] = (
                            metadata_affected or metadata_conclusive)
                        oos_dict['oos_free_kib'] = oos_kib_roll['free']
                        oos_dict['oos_filedata_kib'] = oos_kib_roll['filedata']
                        oos_dict['oos_metadata_kib'] = oos_kib_roll['metadata']
                        oos_dict['oos_unknown_kib'] = oos_kib_roll['unknown']
                        if all_affected_files:
                            key = ('affected_files' if map_file_names
                                   else 'affected_inodes')
                            oos_dict[key] = sorted(all_affected_files)
                        if all_unresolved_inodes:
                            oos_dict['unresolved_inodes'] = sorted(all_unresolved_inodes)

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
                    if snapshot.snapshot_taken and not skip_file_analysis:
                        suggestion['files_affected'] = files_affected
                        suggestion['files_affected_conclusive'] = (
                            files_affected or analysis_conclusive)
                        suggestion['metadata_affected'] = metadata_affected
                    if skip_file_analysis:
                        suggestion['file_analysis_skipped'] = True
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


def _short_node(name: str) -> str:
    """Canonical (short) node name for the v2 output: the leading label.

    os.uname() may be short while drbdsetup names connections by FQDN, so
    the raw names in a run can be mixed; normalising every identifier to the
    short form gives a consumer one spelling per node. Executable resync
    commands keep the drbdsetup-native name (they are left verbatim)."""
    return name.split('.', 1)[0]


def _initial_oos_kib(initial_status: Optional[list], res_name: str,
                     nodes: list, invoked_on: str) -> Optional[int]:
    """Out-of-sync KiB for this connection as reported by the pre-verify
    drbdsetup status. Only available for connections that involve the
    invoking host (its status has no peer-to-peer connections)."""
    if not initial_status or invoked_on not in nodes:
        return None
    peer = next(n for n in nodes if n != invoked_on)
    for r in initial_status:
        if r.get('name') != res_name:
            continue
        for c in r.get('connections', []):
            if node_names_match(c.get('name', ''), peer):
                pds = c.get('peer_devices', [])
                if pds:
                    return pds[0].get('out-of-sync')
    return None


def _connection_v2(nodes: list, v: dict, initial_oos: Optional[int]) -> dict:
    conn = {'nodes': sorted(_short_node(n) for n in nodes),
            'out_of_sync_kib': v.get('value_KiB', 0)}
    if initial_oos is not None:
        conn['out_of_sync_kib_initial'] = initial_oos
        conn['oos_changed_by_verify'] = initial_oos != conn['out_of_sync_kib']
    if 'block_size' in v:
        conn['block_size_bytes'] = v['block_size']
    if v.get('block_size_assumed'):
        conn['block_size_assumed'] = True
    higher = {_short_node(n): v[f'{n} higher'] for n in nodes if f'{n} higher' in v}
    if higher:
        conn['entropy_higher_blocks'] = higher
    if any(k in v for k in ('oos_free_kib', 'oos_filedata_kib',
                            'oos_metadata_kib', 'oos_unknown_kib')):
        conn['oos_kib_by_class'] = {'free': v.get('oos_free_kib', 0),
                                    'filedata': v.get('oos_filedata_kib', 0),
                                    'metadata': v.get('oos_metadata_kib', 0),
                                    'unknown': v.get('oos_unknown_kib', 0)}
    if 'files_affected' in v:
        files = {'affected': v['files_affected'],
                 'conclusive': v.get('files_affected_conclusive', False)}
        if 'affected_files' in v:
            files['paths'] = v['affected_files']
        if 'affected_inodes' in v:
            files['inodes'] = v['affected_inodes']
        if 'unresolved_inodes' in v:
            # Owners proven affected by reverse mapping for which no path
            # exists in the tree (a directory whose entry is gone, an
            # unreadable subtree). Part of the affected set regardless.
            files['unresolved_inodes'] = v['unresolved_inodes']
        conn['files'] = files
    if 'metadata_affected' in v:
        conn['metadata_affected'] = {
            'affected': v['metadata_affected'],
            'conclusive': v.get('metadata_affected_conclusive', False)}
    detail = v.get('_fsck_detail')
    if detail:
        conn['fsck'] = [
            {'node': _short_node(node),
             'errors': sum(p.get('errors', 0) for p in parts.values()),
             'warnings': sum(p.get('warnings', 0) for p in parts.values()),
             'partitions': [{'name': pn, 'fstype': pv.get('fstype'),
                             'errors': pv.get('errors', 0),
                             'warnings': pv.get('warnings', 0)}
                            for pn, pv in sorted(parts.items())]}
            for node, parts in sorted(detail.items())]
    elif 'fsck' in v:
        conn['fsck'] = [{'node': _short_node(n), 'errors': d.get('errors', 0),
                         'warnings': d.get('warnings', 0)}
                        for n, d in sorted(v['fsck'].items())]
    if 'partitions' in v:
        parts = []
        for pn, pv in sorted(v['partitions'].items()):
            p = {'name': pn, 'fstype': pv.get('fstype'),
                 'oos_kib': pv.get('oos_kib'), 'method': pv.get('method')}
            if 'oos_free_kib' in pv:
                p['oos_kib_by_class'] = {'free': pv['oos_free_kib'],
                                         'filedata': pv['oos_filedata_kib'],
                                         'metadata': pv['oos_metadata_kib'],
                                         'unknown': pv['oos_unknown_kib']}
            if 'metadata_affected' in pv:
                p['metadata_affected'] = pv['metadata_affected']
            parts.append(p)
        conn['partitions'] = parts
    return conn


def _node_role(status: Optional[list], res_name: str, node: str,
               invoked_on: str) -> tuple:
    """(role, disk_state) for node in res_name from the invoking host's
    drbdsetup status (own fields if node is the invoker, else the peer
    fields of the connection to it). (None, None) if not determinable."""
    if not status:
        return (None, None)
    for r in status:
        if r.get('name') != res_name:
            continue
        if node_names_match(node, invoked_on):
            dev = (r.get('devices') or [{}])[0]
            return (r.get('role'), dev.get('disk-state'))
        for c in r.get('connections', []):
            if node_names_match(c.get('name', ''), node):
                pd = (c.get('peer_devices') or [{}])[0]
                return (c.get('peer-role'), pd.get('peer-disk-state'))
    return (None, None)


def _suggestion_v2(s: dict, role_status: Optional[list], res_name: str,
                   invoked_on: str) -> dict:
    # Node identifiers are normalised to the short form; commands keep the
    # drbdsetup-native name (source/target derive the pair -- the 'connection'
    # string uses '-', ambiguous since node names contain '-').
    src, tgt = _short_node(s['source']), _short_node(s['target'])
    out = {'nodes': sorted([src, tgt]), 'source': src, 'target': tgt,
           'commands': s.get('commands', [])}
    for k in ('files_affected', 'files_affected_conclusive', 'metadata_affected',
              'file_analysis_skipped'):
        if k in s:
            out[k] = s[k]
    # The target is the node whose data gets discarded (made SyncTarget).
    # If it is currently Primary, overwriting it can violate cache coherency
    # for applications holding the device open -- flag that prominently.
    role, disk = _node_role(role_status, res_name, s['target'], invoked_on)
    if role is not None:
        out['target_role'] = role
    if disk is not None:
        out['target_disk_state'] = disk
    if role == 'Primary':
        out['role_conflict'] = True
        out['warning'] = (
            f"target {tgt} is currently Primary"
            f"{f' ({disk})' if disk else ''}; resyncing onto it discards data "
            f"in use and can violate cache coherency for current users of the "
            f"device -- take it out of service (Secondary, stop users) first")
    return out


def _resource_v2(name: str, res_data: dict, initial_status: Optional[list],
                 role_status: Optional[list], invoked_on: str) -> dict:
    oos = res_data.get('oos', {})
    connections = []
    for key, v in oos.items():
        nodes = sorted(key)
        connections.append(_connection_v2(
            nodes, v, _initial_oos_kib(initial_status, name, nodes, invoked_on)))
    connections.sort(key=lambda c: c['nodes'])

    datasets = res_data.get('datasets')
    if datasets is not None:
        datasets = [sorted({_short_node(n) for n in ds}) for ds in datasets]
    total = sum(c['out_of_sync_kib'] for c in connections)
    max_pair = max((c['out_of_sync_kib'] for c in connections), default=0)
    if res_data.get('dataset_error'):
        status = 'transitivity_error'
    elif datasets and len(datasets) > 1:
        status = 'diverged'
    elif total > 0:
        status = 'out_of_sync'
    else:
        status = 'in_sync'

    files_affected = any(c.get('files', {}).get('affected') for c in connections)
    metadata_affected = any(c.get('metadata_affected', {}).get('affected')
                            for c in connections)
    fsck_errors = any(f.get('errors') for c in connections for f in c.get('fsck', []))
    res = {
        'name': name,
        'status': status,
        'summary': {
            'total_oos_kib': total,
            'max_pair_oos_kib': max_pair,
            'dataset_count': len(datasets) if datasets is not None else None,
            'connection_count': len(connections),
            'files_affected': files_affected,
            'metadata_affected': metadata_affected,
            'fsck_errors': bool(fsck_errors),
        },
        'snapshot_used': any(v.get('snapshot') for v in oos.values()),
        'datasets': datasets,
        'connections': connections,
        'resync_suggestions': [_suggestion_v2(s, role_status, name, invoked_on)
                               for s in res_data.get('resync_suggestions', [])],
        'warnings': res_data.get('warnings', []),
    }
    if res_data.get('dataset_error'):
        res['dataset_error'] = res_data['dataset_error']
    return res


def build_result_v2(result_json: dict, *, invoked_on: str,
                    started_at: str, generated_at: Optional[str],
                    initial_status: Optional[list], final_status: Optional[list],
                    skipped: list) -> dict:
    """Build the schema-2 result: an array of resource objects (each with an
    embedded name, status and summary) plus run provenance and the initial
    and final drbdsetup status snapshots."""
    role_status = final_status or initial_status
    resources = [_resource_v2(name, rd, initial_status, role_status, invoked_on)
                 for name, rd in result_json.items()]
    for sk in skipped:
        resources.append({'name': sk['name'], 'status': sk['status'],
                          'skipped_reason': sk['reason'], 'connections': [],
                          'resync_suggestions': [], 'warnings': []})
    resources.sort(key=lambda r: r['name'])

    by_status = Counter(r['status'] for r in resources)
    return {
        'schema_version': 2,
        'started_at': started_at,
        'generated_at': generated_at,
        'invoked_on': invoked_on,
        'summary': {
            'resources': len(resources),
            'by_status': dict(sorted(by_status.items())),
            'total_oos_kib': sum(r.get('summary', {}).get('total_oos_kib', 0)
                                 for r in resources),
        },
        'drbdsetup_status': {
            'initial': {'captured_at': started_at, 'status': initial_status},
            'final': {'captured_at': generated_at, 'status': final_status},
        },
        'resources': resources,
    }


# Classification thresholds (KiB) for the "actions" report.
REPORT_BIG_KIB = 100 * 1024   # a single contradictory pair this large is suspect
REPORT_SMALL_KIB = 512        # below this, treat as benign/transient

CLASS_ORDER = {'A': 0, 'M': 0, 'C': 1, 'B': 2, 'D': 3, 'E': 4}
CLASS_ACTION = {
    'A': 'files affected -- identify the authoritative copy, then resync the stale side',
    'M': 'filesystem metadata affected (no file data) -- identify the authoritative copy, then resync the stale side',
    'C': 'verify is internally inconsistent (transitivity/contradiction) -- re-verify; do NOT resync blindly',
    'B': 'genuinely diverged (multiple datasets) -- re-verify; if it persists, resync from the authoritative copy',
    'D': 'sizable single-pair OOS, likely transient -- re-verify during a quiet window',
    'E': 'small OOS, likely free-space/transient -- monitor; re-verify if it persists',
}


def _classify(res: dict):
    """Return (code, label) severity class, or None for in_sync/not-analyzed."""
    status = res.get('status')
    if status in ('skipped', 'diskless', 'in_sync'):
        return None
    conns = res.get('connections', [])
    max_pair = res.get('summary', {}).get('max_pair_oos_kib', 0)
    if any(c.get('files', {}).get('affected') for c in conns):
        return ('A', 'files-impacted')
    if any(c.get('metadata_affected', {}).get('affected') for c in conns):
        return ('M', 'metadata-impacted')
    if status == 'transitivity_error':
        return ('C', 'inconsistent')
    if status == 'out_of_sync' and max_pair >= REPORT_BIG_KIB:
        return ('C', 'inconsistent')
    if status == 'diverged':
        return ('B', 'diverged')
    if status == 'out_of_sync' and max_pair >= REPORT_SMALL_KIB:
        return ('D', 'suspect')
    if status == 'out_of_sync':
        return ('E', 'benign')
    return None


def _entropy_tie(res: dict, sugg: dict) -> bool:
    for c in res.get('connections', []):
        if c.get('nodes') == sugg.get('nodes'):
            e = c.get('entropy_higher_blocks')
            if not e:
                return True
            return len(set(e.values())) <= 1
    return False


def _report_overview(data: dict) -> None:
    summ = data.get('summary', {})
    print(f'DRBD verify report  (started {data.get("started_at")}, finished '
          f'{data.get("generated_at")}, on {data.get("invoked_on")})')
    by_status = summ.get('by_status', {})
    print('resources: %d  [%s]' % (
        summ.get('resources', 0),
        '  '.join(f'{k}={v}' for k, v in sorted(by_status.items()))))
    print(f'total out-of-sync: {summ.get("total_oos_kib", 0)} KiB')
    res = data.get('resources', [])
    role_conflicts = sum(1 for r in res for s in r.get('resync_suggestions', [])
                         if s.get('role_conflict'))
    warns = sum(1 for r in res if r.get('warnings'))
    meta_affected = sum(1 for r in res
                        if r.get('summary', {}).get('metadata_affected'))
    if role_conflicts:
        print(f'!!! {role_conflicts} resync suggestion(s) target a Primary '
              f'(role conflict -- see the actions report)')
    if meta_affected:
        print(f'{meta_affected} resource(s) have filesystem metadata out of sync')
    if warns:
        print(f'{warns} resource(s) have analysis warnings')
    affected = [r for r in res if r.get('summary', {}).get('total_oos_kib', 0) > 0]
    affected.sort(key=lambda r: -r['summary']['total_oos_kib'])
    if affected:
        print('\nlargest out-of-sync:')
        for r in affected[:8]:
            print(f'  {r["summary"]["total_oos_kib"]:>13} KiB  '
                  f'{r["status"]:17} {r["name"]}')


def _report_actions(data: dict) -> None:
    rows = []
    for res in data.get('resources', []):
        c = _classify(res)
        if c:
            rows.append((c, res))
    rows.sort(key=lambda t: (CLASS_ORDER[t[0][0]], t[1]['name']))
    if not rows:
        print('No affected resources needing action.')
    for (code, label), res in rows:
        summ = res.get('summary', {})
        print(f'\n[{code}] {label:14} {res["name"]}  '
              f'status={res["status"]}  oos={summ.get("total_oos_kib", 0)} KiB  '
              f'datasets={summ.get("dataset_count")}')
        print(f'      action: {CLASS_ACTION[code]}')
        for s in res.get('resync_suggestions', []):
            if s.get('role_conflict'):
                print(f'      !!! ROLE CONFLICT: {s.get("warning", "")}')
            if code != 'M' and s.get('metadata_affected'):
                print('      note: filesystem metadata is out of sync '
                      '(whether or not file data is)')
            flags = []
            if s.get('file_analysis_skipped'):
                flags.append('file-analysis-skipped')
            if _entropy_tie(res, s):
                flags.append('entropy TIED -- direction is a guess, decide manually')
            suffix = f'   [{"; ".join(flags)}]' if flags else ''
            for cmd in s.get('commands', []):
                print(f'      $ {cmd}{suffix}')
    not_analyzed = [r for r in data.get('resources', [])
                    if r.get('status') in ('skipped', 'diskless')]
    if not_analyzed:
        print('\nNot analyzed:')
        for r in sorted(not_analyzed, key=lambda r: r['name']):
            print(f'  {r["name"]}: {r["status"]} ({r.get("skipped_reason", "")})')


def run_report(level: str, path: Optional[str]) -> int:
    """Post-process a schema-2 result. Fails fast with a short message if the
    input is unreadable, not JSON, or not a schema-2 result."""
    src = path or 'stdin'
    try:
        text = open(path).read() if path else sys.stdin.read()
    except OSError as e:
        print(f'error: cannot read {src}: {e.strerror}', file=sys.stderr)
        return 2
    try:
        data = json.loads(text)
    except json.JSONDecodeError as e:
        print(f'error: {src}: invalid JSON ({e.msg}, line {e.lineno})', file=sys.stderr)
        return 2
    if not isinstance(data, dict) or data.get('schema_version') != 2:
        print(f'error: {src}: not a drbd-verify schema_version 2 result '
              f'(feed the JSON this tool produces)', file=sys.stderr)
        return 2
    if not isinstance(data.get('resources'), list):
        print(f'error: {src}: malformed result, no "resources" array',
              file=sys.stderr)
        return 2
    if level == 'overview':
        _report_overview(data)
    else:
        _report_actions(data)
    return 0


_DESCRIPTION = """\
Run DRBD online verifies across all resources of this host (or only those
given with -r/--resource) and analyse any out-of-sync (OOS) blocks.

Unlike a plain `drbdadm verify` (this node against its peers), this runs
every pairwise verify needed to compare all nodes (1 for 2 nodes, 3 for 3,
6 for 4, ...). For each OOS block it reports the affected files where it
can, uses the Shannon entropy of the differing blocks and filesystem
checks to gauge impact, and recommends a resync direction (use --do-it to
execute it). Results are written to a schema_version 2 JSON file,
drbd-verify-result_YYYY-MM-DD_HHMM.json, in the current directory.
"""

_EPILOG = """\
report modes (post-process a result JSON; no verify is run):
  --report overview [FILE]   cluster summary: status counts, total OOS,
                             largest out-of-sync resources
  --report actions  [FILE]   resources ranked by suspected severity, each
                             with a recommended corrective/investigative
                             action; a resync onto a node that is currently
                             Primary is flagged as a ROLE CONFLICT
  FILE defaults to stdin, so:  drbd-verify.py ... | drbd-verify.py --report actions

examples:
  drbd-verify.py                         verify every local resource
  drbd-verify.py -r res0 res1            verify only res0 and res1
  drbd-verify.py -r res0 --skip-verify   analyse the CURRENT OOS of res0
  drbd-verify.py --report actions drbd-verify-result_2026-07-11_0021.json

requirements:
  Passwordless SSH (keys + ssh-agent) to every peer: for the pairwise
  verifies the tool copies itself to /run/drbd-verify/ on each diskful peer
  and runs there. The progress display updates about every 3 seconds.

related:
  drbd-resource-host-map.py is a companion planner. It reads a LINSTOR
  machine-readable resource list (e.g. `linstor -m resource list`) and
  prints, grouped per host, the `drbd-verify.py -r <resource>` commands to
  run so that every diskful resource is verified exactly once, from a host
  that holds it. Use it to drive a cluster-wide verification: run the block
  under each `#host <name>:` on that host.
"""


class _ShortHelp(argparse.Action):
    """-h : concise usage summary."""
    def __init__(self, option_strings, dest=argparse.SUPPRESS, **kw):
        super().__init__(option_strings, dest, nargs=0,
                         default=argparse.SUPPRESS, **kw)

    def __call__(self, parser, namespace, values, option_string=None):
        parser.print_usage()
        print('\nVerify DRBD resources and analyse out-of-sync blocks.\n'
              'Use --help for full documentation.')
        parser.exit()


class _LongHelp(argparse.Action):
    """--help : full, man-page-like documentation."""
    def __init__(self, option_strings, dest=argparse.SUPPRESS, **kw):
        super().__init__(option_strings, dest, nargs=0,
                         default=argparse.SUPPRESS, **kw)

    def __call__(self, parser, namespace, values, option_string=None):
        parser.print_help()
        parser.exit()


def main() -> int:
    global output_json, forward_map_limit, forward_map_inode_limit, map_file_names
    global skip_file_analysis
    result_json = {}

    arg_parser = argparse.ArgumentParser(
        add_help=False, description=_DESCRIPTION, epilog=_EPILOG,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    arg_parser.add_argument('-h', action=_ShortHelp,
                            help='short usage summary')
    arg_parser.add_argument('--help', action=_LongHelp,
                            help='full documentation (this text)')
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
    arg_parser.add_argument('--forward-map-inode-limit', dest='forward_map_inode_limit',
                            type=str, default=None, metavar='N',
                            help='Used-inode ceiling for the forward-mapping '
                                 'file-analysis fallback; its cost scales with the '
                                 "number of files walked. 'none' or 0 disables it. "
                                 'Default 2000000.')
    arg_parser.add_argument('--forward-map-limit', dest='forward_map_limit',
                            type=str, default=None, metavar='SIZE',
                            help='Optional partition-size ceiling for the '
                                 'forward-mapping fallback (e.g. 16G, 512M), a '
                                 'secondary guard on top of --forward-map-inode-limit. '
                                 "'none' or 0 disables it. Default: disabled.")
    arg_parser.add_argument('--no-file-names', dest='map_file_names',
                            action='store_false',
                            help='Report affected inode numbers instead of file '
                                 'paths. Skips a full-tree walk in reverse-mapping '
                                 'mode; markedly faster on large filesystems.')
    arg_parser.add_argument('--skip-file-analysis', dest='skip_file_analysis',
                            action='store_true',
                            help='Skip the affected-files analysis on partitions '
                                 "(no mounting or block-to-file mapping); faster, "
                                 'but does not identify affected files or the '
                                 'free/non-free split. OOS, entropy, fsck and resync '
                                 'suggestions are still produced.')
    arg_parser.add_argument('--report', dest='report',
                            choices=['overview', 'actions'],
                            help='Post-process a result JSON (from FILE or stdin) into '
                                 'a human report and exit, without running any verify. '
                                 "'overview' is a cluster summary; 'actions' is a "
                                 'severity-ranked list of recommended corrective or '
                                 'investigative actions.')
    arg_parser.add_argument('report_file', nargs='?',
                            help='Result JSON to post-process with --report '
                                 '(default: stdin).')
    args = arg_parser.parse_args()
    output_json = args.json
    if args.report:
        return run_report(args.report, args.report_file)
    map_file_names = args.map_file_names
    skip_file_analysis = args.skip_file_analysis
    if args.forward_map_limit is not None:
        try:
            forward_map_limit = parse_size(args.forward_map_limit)
        except ValueError as e:
            print(f'Error: {e}', file=sys.stderr)
            return 1
    if args.forward_map_inode_limit is not None:
        v = args.forward_map_inode_limit.strip().lower()
        if v in ('none', 'unlimited', 'off'):
            forward_map_inode_limit = None
        else:
            try:
                forward_map_inode_limit = int(v) or None
            except ValueError:
                print(f'Error: invalid inode count: {v!r}', file=sys.stderr)
                return 1

    check_required_tools()
    check_fsck_tools()

    drbd_status_json = drbdsetup_json('status')
    initial_status = drbd_status_json
    started_at = datetime.datetime.now().isoformat(timespec='seconds')

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
    skipped_resources = []

    def render(final_status, generated_at):
        return build_result_v2(result_json, invoked_on=host_name,
                               started_at=started_at, generated_at=generated_at,
                               initial_status=initial_status, final_status=final_status,
                               skipped=skipped_resources)

    for res_json in work:
        res_name = res_json['name']

        # Skip resources with ongoing resync or verify operations
        ready, reason = is_resource_ready(res_json)
        if not ready:
            log(f'Skipping {res_name}: {reason}')
            skipped_resources.append({'name': res_name, 'status': 'skipped',
                                      'reason': reason})
            continue
        if res_json['devices'][0]['disk-state'] == 'Diskless':
            log(f'Ignoring {res_name}, because it is Diskless')
            skipped_resources.append({'name': res_name, 'status': 'diskless',
                                      'reason': 'local disk-state Diskless'})
            continue

        _warnings.clear()
        res_json = process_res(res_json, args.peers, args.level2, args.skip_verify)
        res_json['warnings'] = list(_warnings)
        if res_json['oos'] or args.level2:
            result_json[res_name] = res_json
        if not args.level2:
            # Incremental (crash-safe) write; final status not yet known.
            with open(result_file_name + '.tmp', 'w') as f:
                f.write(json.dumps(render(None, None), indent=4))
            os.rename(result_file_name + '.tmp', result_file_name)

    if args.level2:
        # Internal wire format consumed by the parent's json_key_to_frozenset.
        print(json.dumps(frozenset_to_json_key(result_json), sort_keys=True, indent=4))
        return 0

    # A second drbdsetup status: captures state drift over the (possibly
    # long) run, and lets a consumer see whether this verify changed the
    # out-of-sync counts (e.g. stale bitmap bits cleared).
    final_status = drbdsetup_json('status')
    out = render(final_status, datetime.datetime.now().isoformat(timespec='seconds'))
    with open(result_file_name + '.tmp', 'w') as f:
        f.write(json.dumps(out, indent=4))
    os.rename(result_file_name + '.tmp', result_file_name)
    log(f'all results as JSON (also in file {result_file_name}):')
    print(json.dumps(out, indent=4))

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
