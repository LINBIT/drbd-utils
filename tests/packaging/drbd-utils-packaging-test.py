#!/usr/bin/env python3
"""drbd-utils packaging test

Exercise install, upgrade, reinstall, downgrade and removal of the drbd-utils
packages with the distribution's package manager, and check the resulting
system state after every step.

Meant to run in a throwaway VM (see run.toml), but it can also be run by hand
in a fresh container or VM. Without a running systemd the service related
checks are skipped.

Required environment:
  DRBD_UTILS_VERSION      version-release of the build under test,
                          e.g. 9.99.0.<sha>-1
  REPOSITORY_URL          repository containing the build under test
  RELEASE_REPOSITORY_URL  repository containing released versions
apt only:
  REPOSITORY_DISTRIBUTION suite of the release repository, e.g. noble
  REPOSITORY_SUITE        suite of the CI repository
                          (default: REPOSITORY_DISTRIBUTION)
  REPOSITORY_COMPONENT    component of the CI repository (default: main)
Optional:
  RELEASE_VERSION         version-release of the release to start from
                          (default: newest in RELEASE_REPOSITORY_URL)

Exit status: 0 all checks passed, 1 some checks failed, 2 a package manager
operation failed and the run was aborted.

Must stay compatible with Python 3.6 (RHEL 7 software collections, RHEL 8).
Standard library only.
"""

import glob
import os
import re
import shutil
import subprocess
import sys
import time
from typing import Dict, Iterable, List, Pattern, Sequence, Tuple

MARKER = "# drbd-utils packaging test marker"
CONFIG_FILE = "/etc/drbd.d/global_common.conf"
MAN_DIR = "/usr/share/man"
MAN_LINKS = [
    "man8/drbd.8.gz",
    "man8/drbdadm.8.gz",
    "man8/drbdmeta.8.gz",
    "man8/drbdsetup.8.gz",
    "man5/drbd.conf.5.gz",
]
# Files that must be gone after the package was removed.
REMOVED_PATHS = [
    "/usr/sbin/drbdadm",
    "/usr/sbin/drbdsetup",
    "/usr/sbin/drbdmeta",
    "/usr/sbin/drbdmon",
    "/usr/sbin/drbd-events-log-supplier",
    "/sbin/drbdadm",
    "/sbin/drbdsetup",
    "/sbin/drbdmeta",
    "/usr/lib/drbd",
    "/usr/lib/tmpfiles.d/drbd.conf",
    "/usr/share/selinux/packages/drbd.pp.bz2",
]
UNIT_DIRS = ["/usr/lib/systemd/system", "/lib/systemd/system"]

Snapshot = Dict[str, Dict[str, str]]


class Abort(Exception):
    """A package manager operation failed; the run cannot continue."""


class Report:
    def __init__(self) -> None:
        self.failures = 0

    @staticmethod
    def log(msg: str) -> None:
        print("\n### " + msg, flush=True)

    @staticmethod
    def note(msg: str) -> None:
        print("note: " + msg, flush=True)

    def fail(self, msg: str) -> None:
        print("FAIL: " + msg, flush=True)
        self.failures += 1

    def summary(self) -> None:
        print("\n=== %d check(s) failed" % self.failures, flush=True)


# ---------------------------------------------------------------------------
# running commands
# ---------------------------------------------------------------------------


def run(argv: Sequence[str]) -> Tuple[int, str]:
    """Run a command, echo and stream its output, return (status, output)."""
    print("+ " + " ".join(argv), flush=True)
    proc = subprocess.Popen(
        list(argv),
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        universal_newlines=True,
    )
    lines = []  # type: List[str]
    assert proc.stdout is not None
    for line in proc.stdout:
        sys.stdout.write(line)
        lines.append(line)
    sys.stdout.flush()
    return proc.wait(), "".join(lines)


def output(argv: Sequence[str]) -> str:
    """Run a command quietly and return its stdout ('' on failure)."""
    try:
        proc = subprocess.run(
            list(argv),
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            universal_newlines=True,
        )
    except OSError:
        return ""
    return proc.stdout if proc.returncode == 0 else ""


def succeeds(argv: Sequence[str]) -> bool:
    try:
        return (
            subprocess.run(
                list(argv), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
            ).returncode
            == 0
        )
    except OSError:
        return False


def drop_config_lines(paths: Iterable[str], pattern: Pattern, report: Report) -> None:
    for path in paths:
        try:
            with open(path) as f:
                lines = f.readlines()
        except OSError:
            continue
        keep = [l for l in lines if not pattern.search(l)]
        if len(keep) == len(lines):
            continue
        with open(path, "w") as f:
            f.writelines(keep)
        report.note("dropped a documentation exclusion from " + path)


def exists(path: str) -> bool:
    """True for files, directories and also dangling symlinks."""
    return os.path.lexists(path)


# ---------------------------------------------------------------------------
# package managers
# ---------------------------------------------------------------------------


class Config:
    def __init__(self, env: Dict[str, str]) -> None:
        try:
            self.version = env["DRBD_UTILS_VERSION"]
            self.repo_url = env["REPOSITORY_URL"]
            self.release_repo_url = env["RELEASE_REPOSITORY_URL"]
        except KeyError as e:
            sys.exit("%s must be set" % e.args[0])
        self.distribution = env.get("REPOSITORY_DISTRIBUTION", "")
        self.suite = env.get("REPOSITORY_SUITE") or self.distribution
        self.component = env.get("REPOSITORY_COMPONENT") or "main"
        self.release_version = env.get("RELEASE_VERSION", "")


class PackageManager:
    kind = ""  # "rpm" or "deb"
    name = ""

    def __init__(self, cfg: Config, report: Report) -> None:
        self.cfg = cfg
        self.report = report

    def operation(self, argv: Sequence[str]) -> None:
        """Run a package manager command; abort the test if it fails."""
        rc, out = run(argv)
        self.check_output(out, argv)
        if rc != 0:
            raise Abort("'%s' failed with status %d" % (" ".join(argv), rc))

    def check_output(self, out: str, argv: Sequence[str]) -> None:
        pass

    # to be implemented by the subclasses
    def include_docs(self) -> None:
        """Make sure the package manager unpacks documentation.

        Slim images exclude it, and then the package's man pages are never
        written, which would silently hollow out the man page checks.
        """
        raise NotImplementedError

    def setup_repos(self) -> None:
        raise NotImplementedError

    def release_version(self) -> str:
        raise NotImplementedError

    def install(self, version: str, ci: bool) -> None:
        raise NotImplementedError

    def upgrade(self, version: str, ci: bool) -> None:
        raise NotImplementedError

    def reinstall(self, version: str) -> None:
        raise NotImplementedError

    def downgrade(self, version: str) -> None:
        raise NotImplementedError

    def remove(self) -> None:
        raise NotImplementedError

    def purge(self) -> None:
        """Debian only: remove the configuration files as well."""

    def installed_version(self) -> str:
        raise NotImplementedError

    def leftover_packages(self, purged: bool) -> List[str]:
        raise NotImplementedError

    def prerm_stops(self) -> List[str]:
        """Units the installed package stops on upgrade (Debian only)."""
        return []


class Rpm(PackageManager):
    kind = "rpm"

    # rpm only prints a warning when a scriptlet fails
    SCRIPTLET_FAILURE = re.compile(
        r"scriptlet failed|scriptlet failure|Error in (PRE|POST)(IN|UN) scriptlet",
        re.IGNORECASE,
    )
    # documentation exclusions of slim images
    NODOCS = re.compile(r"^\s*tsflags\s*=.*\bnodocs\b")
    EXCLUDEDOCS = re.compile(r"^\s*%_excludedocs\s")

    def __init__(self, cfg: Config, report: Report) -> None:
        super().__init__(cfg, report)
        self.name = "dnf" if shutil.which("dnf") else "yum"
        # drbd-utils only recommends drbd-udev, but drbd-udev requires exactly
        # the installed drbd-utils version. The CI build only publishes
        # drbd-utils and drbd-selinux, so never pull in weak dependencies, or
        # the next upgrade to the CI build would be blocked by drbd-udev.
        # yum on RHEL 7 knows no weak dependencies.
        self.weak_deps = (
            ["--setopt=install_weak_deps=False"] if self.name == "dnf" else []
        )

    def check_output(self, out: str, argv: Sequence[str]) -> None:
        if self.SCRIPTLET_FAILURE.search(out):
            self.report.fail("package scriptlet failed during: " + " ".join(argv))

    @staticmethod
    def write_repo(path: str, repo_id: str, name: str, url: str, enabled: bool) -> None:
        with open(path, "w") as f:
            f.write(
                "[%s]\nname=%s\nbaseurl=%s\ngpgcheck=0\nenabled=%d\n"
                % (repo_id, name, url, 1 if enabled else 0)
            )

    def include_docs(self) -> None:
        drop_config_lines(
            ["/etc/yum.conf", "/etc/dnf/dnf.conf", "/etc/dnf/libdnf5.conf"],
            self.NODOCS,
            self.report,
        )
        # container images carry %_excludedocs in /etc/rpm/macros.imgcreate
        drop_config_lines(
            glob.glob("/etc/rpm/macros*"), self.EXCLUDEDOCS, self.report
        )

    def setup_repos(self) -> None:
        self.write_repo(
            "/etc/yum.repos.d/drbd-utils-release.repo",
            "drbd-utils-release",
            "DRBD released packages",
            self.cfg.release_repo_url,
            True,
        )
        self.write_repo(
            "/etc/yum.repos.d/drbd-utils-ci.repo",
            "drbd-utils-ci",
            "DRBD CI packages",
            self.cfg.repo_url,
            False,
        )
        subprocess.run(
            [self.name, "-q", "makecache"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )

    def release_version(self) -> str:
        out = output(
            [
                self.name,
                "-q",
                "--disablerepo=*",
                "--enablerepo=drbd-utils-release",
                "list",
                "available",
                "drbd-utils",
            ]
        )
        return parse_yum_list(out)

    def packages(self, version: str) -> List[str]:
        # drbd-utils requires drbd-selinux on systems with the targeted
        # policy; pin it as well or the package manager may pick another
        # version from the CI repository.
        pkgs = ["drbd-utils-" + version]
        if succeeds(["rpm", "-q", "selinux-policy-targeted"]):
            pkgs.append("drbd-selinux-" + version)
        return pkgs

    def command(self, verb: str, version: str, ci: bool) -> List[str]:
        argv = [self.name, "-y"] + self.weak_deps
        if ci:
            argv.append("--enablerepo=drbd-utils-ci")
        return argv + [verb] + self.packages(version)

    def install(self, version: str, ci: bool) -> None:
        self.operation(self.command("install", version, ci))

    def upgrade(self, version: str, ci: bool) -> None:
        self.operation(self.command("upgrade", version, ci))

    def reinstall(self, version: str) -> None:
        self.operation(self.command("reinstall", version, True))

    def downgrade(self, version: str) -> None:
        self.operation(self.command("downgrade", version, False))

    def remove(self) -> None:
        names = output(["rpm", "-qa", "--qf", "%{NAME}\n", "drbd*"]).split()
        self.operation([self.name, "-y", "remove"] + names)

    def installed_version(self) -> str:
        return output(
            ["rpm", "-q", "--qf", "%{VERSION}-%{RELEASE}\n", "drbd-utils"]
        ).strip()

    def leftover_packages(self, purged: bool) -> List[str]:
        return output(["rpm", "-qa", "drbd*"]).split()


def parse_yum_list(out: str) -> str:
    """Version of the newest 'drbd-utils.<arch>' line of 'yum list'."""
    version = ""
    for line in out.splitlines():
        fields = line.split()
        if len(fields) >= 2 and fields[0].startswith("drbd-utils."):
            version = fields[1]
    return version


class Apt(PackageManager):
    kind = "deb"
    name = "apt"

    # keep a locally modified config file instead of prompting
    OPTS = [
        "-y",
        "-q",
        "--no-install-recommends",
        "-o",
        "Dpkg::Options::=--force-confdef",
        "-o",
        "Dpkg::Options::=--force-confold",
    ]
    # documentation exclusion of slim images
    PATH_EXCLUDE = re.compile(r"^\s*path-exclude\s*=\s*/usr/share/man")

    def __init__(self, cfg: Config, report: Report) -> None:
        super().__init__(cfg, report)
        os.environ["DEBIAN_FRONTEND"] = "noninteractive"

    @staticmethod
    def source_configured(url: str, suite: str) -> bool:
        pattern = re.compile(
            r"^deb .*\s%s\s+%s(\s|$)" % (re.escape(url), re.escape(suite))
        )
        files = ["/etc/apt/sources.list"]
        try:
            files += [
                os.path.join("/etc/apt/sources.list.d", f)
                for f in os.listdir("/etc/apt/sources.list.d")
            ]
        except OSError:
            pass
        for path in files:
            try:
                with open(path) as f:
                    if any(pattern.search(line) for line in f):
                        return True
            except OSError:
                continue
        return False

    def add_source(self, path: str, url: str, suite: str, component: str) -> None:
        # The test images already carry the release repository (with a
        # signing key); adding it a second time with different options makes
        # apt refuse the whole source list.
        if self.source_configured(url, suite):
            self.report.note(
                "apt source %s %s already configured, not adding it again" % (url, suite)
            )
            return
        with open(path, "w") as f:
            f.write("deb [trusted=yes] %s %s %s\n" % (url, suite, component))

    def include_docs(self) -> None:
        drop_config_lines(
            ["/etc/dpkg/dpkg.cfg"] + glob.glob("/etc/dpkg/dpkg.cfg.d/*"),
            self.PATH_EXCLUDE,
            self.report,
        )

    def setup_repos(self) -> None:
        if not self.cfg.distribution:
            raise Abort("REPOSITORY_DISTRIBUTION must be set for apt")
        self.add_source(
            "/etc/apt/sources.list.d/drbd-utils-release.list",
            self.cfg.release_repo_url,
            self.cfg.distribution,
            "drbd-9",
        )
        self.add_source(
            "/etc/apt/sources.list.d/drbd-utils-ci.list",
            self.cfg.repo_url,
            self.cfg.suite,
            self.cfg.component,
        )
        # The package asks which man page version to link; answer up front so
        # that any other, unexpected prompt fails the run instead of hanging.
        subprocess.run(
            ["debconf-set-selections"],
            input="drbd-utils drbd-utils/manpages select 9.0\n",
            universal_newlines=True,
            check=True,
        )
        self.operation(["apt-get", "-q", "update"])

    def release_version(self) -> str:
        out = output(["apt-cache", "madison", "drbd-utils"])
        best = ""
        for version in parse_madison(out, self.cfg.release_repo_url):
            if not best or succeeds(["dpkg", "--compare-versions", version, "gt", best]):
                best = version
        return best

    def install(self, version: str, ci: bool) -> None:
        self.operation(["apt-get"] + self.OPTS + ["install", "drbd-utils=" + version])

    def upgrade(self, version: str, ci: bool) -> None:
        self.install(version, ci)

    def reinstall(self, version: str) -> None:
        self.operation(
            ["apt-get"] + self.OPTS + ["install", "--reinstall", "drbd-utils=" + version]
        )

    def downgrade(self, version: str) -> None:
        self.operation(
            ["apt-get"]
            + self.OPTS
            + ["install", "--allow-downgrades", "drbd-utils=" + version]
        )

    def remove(self) -> None:
        self.operation(["apt-get", "-y", "-q", "remove", "drbd-utils"])

    def purge(self) -> None:
        self.operation(["apt-get", "-y", "-q", "purge", "drbd-utils"])

    def installed_version(self) -> str:
        return output(["dpkg-query", "-W", "-f", "${Version}\n", "drbd-utils"]).strip()

    def leftover_packages(self, purged: bool) -> List[str]:
        # after "remove" the package may linger as "rc" (config files remain)
        out = output(
            ["dpkg-query", "-W", "-f", "${Package} ${db:Status-Abbrev}\n", "drbd*"]
        )
        left = []
        for line in out.splitlines():
            fields = line.split()
            if len(fields) < 2:
                continue
            status = fields[1]
            if status.startswith("i") or (purged and status.startswith("r")):
                left.append("%s (%s)" % (fields[0], status))
        return left

    def prerm_stops(self) -> List[str]:
        # dpkg runs the old package's prerm when upgrading, so this is the old
        # package's behaviour, which the build under test cannot change.
        try:
            with open("/var/lib/dpkg/info/drbd-utils.prerm") as f:
                return parse_prerm_stops(f.read())
        except OSError:
            return []


def parse_madison(out: str, repo_url: str) -> List[str]:
    """Versions listed by 'apt-cache madison' for the given repository."""
    versions = []
    for line in out.splitlines():
        fields = [f.strip() for f in line.split("|")]
        if len(fields) >= 3 and fields[0] == "drbd-utils" and repo_url.rstrip("/") in fields[2]:
            versions.append(fields[1])
    return versions


def parse_prerm_stops(script: str) -> List[str]:
    """Units named in "deb-systemd-invoke stop '...' '...'" lines."""
    units = set()
    for match in re.finditer(r"deb-systemd-invoke stop((?: '[^']+')+)", script):
        units.update(re.findall(r"'([^']+)'", match.group(1)))
    return sorted(units)


def detect_package_manager(cfg: Config, report: Report) -> PackageManager:
    if shutil.which("dnf") or shutil.which("yum"):
        return Rpm(cfg, report)
    if shutil.which("apt-get"):
        return Apt(cfg, report)
    raise Abort("unknown package manager")


# ---------------------------------------------------------------------------
# systemd
# ---------------------------------------------------------------------------


class Systemd:
    PROPERTIES = [
        "ActiveState",
        "ActiveEnterTimestampMonotonic",
        "InactiveEnterTimestampMonotonic",
    ]

    def __init__(self) -> None:
        self.available = os.path.isdir("/run/systemd/system") and bool(
            shutil.which("systemctl")
        )

    def units(self) -> List[str]:
        """All drbd units known to systemd, excluding templates."""
        names = set()
        for line in output(
            ["systemctl", "list-unit-files", "--no-legend", "drbd*"]
        ).splitlines():
            fields = line.split()
            if fields:
                names.add(fields[0])
        for line in output(
            ["systemctl", "list-units", "--all", "--no-legend", "drbd*"]
        ).splitlines():
            # not-found units are prefixed with a bullet
            fields = re.sub(r"^[^A-Za-z0-9]*", "", line).split()
            if fields:
                names.add(fields[0])
        return sorted(u for u in names if "@." not in u)

    def wait_idle(self) -> None:
        # Package operations may queue unit jobs that are still running when
        # the package manager returns. Let the job queue drain first.
        for _ in range(30):
            if not output(["systemctl", "list-jobs", "--no-legend"]).strip():
                return
            time.sleep(1)

    def snapshot(self) -> Snapshot:
        if not self.available:
            return {}
        self.wait_idle()
        snap = {}  # type: Snapshot
        for unit in self.units():
            argv = ["systemctl", "show"]
            for prop in self.PROPERTIES:
                argv += ["-p", prop]
            out = output(argv + [unit])
            props = dict(
                line.split("=", 1) for line in out.splitlines() if "=" in line
            )
            # some systemd versions print nothing for units whose file is gone
            if props:
                snap[unit] = props
        return snap

    def activate(self) -> None:
        """Start every drbd unit that can be started.

        On a real system the graceful shutdown/disconnect units are active
        whenever DRBD devices exist; stopping them runs "drbdadm down all" or
        "disconnect all", which is exactly what a package upgrade must never
        do. Units that cannot start here (no kernel module) stay inactive or
        failed.
        """
        if not self.available:
            return
        for unit in self.units():
            succeeds(["systemctl", "start", unit])

    def deactivate(self) -> None:
        """Stop and forget all drbd units.

        Used before the package is removed: the package managers leave units
        they did not stop running as "not-found", some systemd versions then
        refuse to stop or even show them, and their state would be
        attributed to the next installation.
        """
        if not self.available:
            return
        for unit in self.units():
            succeeds(["systemctl", "stop", unit])
            succeeds(["systemctl", "reset-failed", unit])


def format_unit(unit: str, props: Dict[str, str]) -> str:
    return unit + " " + " ".join("%s=%s" % (k, props[k]) for k in sorted(props))


def log_units(what: str, snap: Snapshot) -> None:
    print("drbd units before %s:" % what, flush=True)
    for unit in sorted(snap):
        print("  " + format_unit(unit, snap[unit]), flush=True)


def compare_snapshots(
    what: str,
    before: Snapshot,
    after: Snapshot,
    expected_stops: Iterable[str] = (),
    ignore_starts: bool = False,
) -> Tuple[List[str], List[str]]:
    """Compare two unit snapshots, return (failures, notes).

    Units present before must be unchanged, except for units in
    expected_stops and, with ignore_starts, units that merely became active
    (used for the downgrade, where the released package's postinst starts
    units; that package is not under test). Units that appeared must be
    inactive. A unit that was active and disappeared counts as stopped.
    """
    failures = []  # type: List[str]
    notes = []  # type: List[str]
    expected = set(expected_stops)
    for unit in sorted(before):
        b = before[unit]
        a = after.get(unit)
        if a == b:
            continue
        if unit in expected:
            notes.append(
                "%s changed unit %s, expected: the previously installed package "
                "stops it in its prerm" % (what, unit)
            )
            continue
        if a is None:
            if b.get("ActiveState") == "active":
                failures.append(
                    "%s stopped unit %s, whose unit file it removed or replaced: "
                    "before [%s]" % (what, unit, format_unit(unit, b))
                )
            continue
        if (
            ignore_starts
            and b.get("ActiveState") == "inactive"
            and a.get("ActiveState") == "active"
        ):
            notes.append(
                "%s started unit %s (done by the package we downgraded to, "
                "not under test)" % (what, unit)
            )
            continue
        failures.append(
            "%s changed unit %s: before [%s], after [%s]"
            % (what, unit, format_unit(unit, b), format_unit(unit, a))
        )
    for unit in sorted(after):
        if unit in before:
            continue
        # "failed" is left over from an earlier start attempt of a unit that
        # needs the kernel module; it was not started by the package.
        if after[unit].get("ActiveState") in ("inactive", "failed"):
            continue
        if ignore_starts:
            notes.append(
                "%s activated new unit %s (done by the package we downgraded to, "
                "not under test)" % (what, unit)
            )
        else:
            failures.append(
                "%s activated new unit: %s" % (what, format_unit(unit, after[unit]))
            )
    return failures, notes


# ---------------------------------------------------------------------------
# state checks
# ---------------------------------------------------------------------------


def find_man_pages(dangling_only: bool) -> List[str]:
    found = []
    for root, _dirs, files in os.walk(MAN_DIR):
        for name in files:
            if not (name.startswith("drbd") or name.startswith("drbd.conf")):
                continue
            path = os.path.join(root, name)
            if dangling_only and not (os.path.islink(path) and not os.path.exists(path)):
                continue
            found.append(path)
    return sorted(found)


def find_files(dirs: Iterable[str], prefix: str, recursive: bool) -> List[str]:
    found = set()
    for d in dirs:
        if not os.path.isdir(d):
            continue
        if recursive:
            for root, _dirs, files in os.walk(d):
                found.update(
                    os.path.join(root, n) for n in files if n.startswith(prefix)
                )
        else:
            found.update(
                os.path.join(d, n) for n in os.listdir(d) if n.startswith(prefix)
            )
    return sorted(found)


def selinux_module_installed() -> bool:
    # The distribution policy ships a drbd module of its own (priority 100),
    # so look specifically for the one drbd-selinux installs at priority 200.
    if not shutil.which("semodule"):
        return False
    for line in output(["semodule", "-lfull"]).splitlines():
        fields = line.split()
        if len(fields) >= 2 and fields[0] == "200" and fields[1] == "drbd":
            return True
    return False


class Checks:
    def __init__(self, pm: PackageManager, report: Report) -> None:
        self.pm = pm
        self.report = report

    def installed(self, want: str) -> None:
        fail = self.report.fail
        have = self.pm.installed_version()
        if have != want:
            fail("drbd-utils version: want %s, have '%s'" % (want, have))

        upstream = want.split("-", 1)[0]
        adm = ""
        for line in output(["drbdadm", "--version"]).splitlines():
            if line.startswith("DRBDADM_VERSION="):
                adm = line.split("=", 1)[1]
        if adm != upstream:
            fail("drbdadm --version reports '%s', package is %s" % (adm, upstream))

        # include_docs() made sure the man pages are unpacked, so a link
        # without a target is a packaging bug, not an image that skips docs.
        for link in MAN_LINKS:
            path = os.path.join(MAN_DIR, link)
            if not os.path.islink(path):
                fail("man page link %s missing" % path)
            elif not os.path.exists(path):
                fail("man page link %s is dangling" % path)
        dangling = find_man_pages(dangling_only=True)
        if dangling:
            fail("dangling man page links: " + " ".join(dangling))

        for path in ("/etc/drbd.conf", CONFIG_FILE):
            if not os.path.isfile(path):
                fail("config file %s missing" % path)

        if self.pm.kind == "deb" and not os.path.samefile("/sbin", "/usr/sbin"):
            for name in ("drbdadm", "drbdsetup", "drbdmeta"):
                if not os.access("/sbin/" + name, os.X_OK):
                    fail("/sbin/%s compat link missing" % name)

        if self.pm.kind == "rpm" and succeeds(["rpm", "-q", "drbd-selinux"]):
            if not selinux_module_installed():
                fail("drbd-selinux is installed but the drbd module (priority 200) is not")

    def marker_preserved(self, what: str) -> None:
        try:
            with open(CONFIG_FILE) as f:
                if MARKER in f.read():
                    return
        except OSError:
            pass
        self.report.fail("local change to %s was lost (%s)" % (CONFIG_FILE, what))

    def removed(self, purged: bool) -> None:
        fail = self.report.fail
        left = self.pm.leftover_packages(purged)
        if left:
            fail("packages still installed: " + " ".join(left))

        for path in REMOVED_PATHS:
            if exists(path):
                fail("leftover after removal: " + path)

        left = find_files(UNIT_DIRS, "drbd", recursive=False)
        if left:
            fail("leftover systemd units after removal: " + " ".join(left))

        left = find_man_pages(dangling_only=False)
        if left:
            fail("leftover man pages or links after removal: " + " ".join(left))

        if selinux_module_installed():
            fail("SELinux module drbd (priority 200) still installed after removal")

        if self.pm.kind == "rpm":
            # unmodified config files must be gone; the modified one may be
            # kept as .rpmsave
            for path in ("/etc/drbd.conf", CONFIG_FILE):
                if exists(path):
                    fail("leftover after removal: " + path)
        elif purged:
            for path in ("/etc/drbd.conf", "/etc/drbd.d"):
                if exists(path):
                    fail("leftover after purge: " + path)
            if shutil.which("debconf-show"):
                left = output(["debconf-show", "drbd-utils"]).strip()
                if left:
                    fail("debconf entries left after purge: " + left.replace("\n", " "))

        # dpkg keeps conffiles and unit enablement links until the package is
        # purged
        if self.pm.kind == "rpm" or purged:
            if exists("/etc/init.d/drbd"):
                fail("leftover after removal: /etc/init.d/drbd")
            left = find_files(["/etc/systemd/system"], "drbd", recursive=True)
            if left:
                fail("leftover systemd enablement links after removal: " + " ".join(left))


# ---------------------------------------------------------------------------
# the test
# ---------------------------------------------------------------------------


def pretty_name() -> str:
    try:
        with open("/etc/os-release") as f:
            for line in f:
                if line.startswith("PRETTY_NAME="):
                    return line.split("=", 1)[1].strip().strip('"')
    except OSError:
        pass
    return "unknown"


def main() -> int:
    report = Report()
    cfg = Config(dict(os.environ))
    systemd = Systemd()
    try:
        pm = detect_package_manager(cfg, report)
        checks = Checks(pm, report)
        report.log(
            "drbd-utils packaging test on %s (%s)" % (pretty_name(), pm.name)
        )
        if not systemd.available:
            report.note("systemd not running, skipping service checks")

        pm.include_docs()
        pm.setup_repos()

        release = cfg.release_version or pm.release_version()
        if not release:
            raise Abort(
                "could not determine the released drbd-utils version from "
                + cfg.release_repo_url
            )
        version = cfg.version
        print("release version: " + release)
        print("version under test: " + version, flush=True)
        if release == version:
            raise Abort("release version and version under test are identical")

        def compare(what: str, before: Snapshot, expected: Iterable[str] = (),
                    ignore_starts: bool = False) -> None:
            failures, notes = compare_snapshots(
                what, before, systemd.snapshot(), expected, ignore_starts
            )
            for msg in notes:
                report.note(msg)
            for msg in failures:
                report.fail(msg)

        report.log("1. install release " + release)
        pm.install(release, ci=False)
        checks.installed(release)

        report.log("2. modify " + CONFIG_FILE)
        with open(CONFIG_FILE, "a") as f:
            f.write(MARKER + "\n")

        report.log("3. upgrade to " + version)
        systemd.activate()
        before = systemd.snapshot()
        log_units("upgrade", before)
        expected = pm.prerm_stops()
        if expected:
            print("the installed package's prerm stops on upgrade: " + " ".join(expected))
        pm.upgrade(version, ci=True)
        checks.installed(version)
        checks.marker_preserved("upgrade")
        compare("upgrade", before, expected)

        report.log("4. reinstall %s over itself" % version)
        systemd.activate()
        before = systemd.snapshot()
        log_units("reinstall", before)
        pm.reinstall(version)
        checks.installed(version)
        checks.marker_preserved("reinstall")
        compare("reinstall", before)

        report.log("5. downgrade to " + release)
        systemd.activate()
        before = systemd.snapshot()
        log_units("downgrade", before)
        pm.downgrade(release)
        checks.installed(release)
        checks.marker_preserved("downgrade")
        compare("downgrade", before, ignore_starts=True)

        report.log("6. upgrade to %s again, then remove" % version)
        pm.upgrade(version, ci=True)
        checks.installed(version)
        systemd.deactivate()
        pm.remove()
        checks.removed(purged=False)
        if pm.kind == "deb":
            pm.purge()
            checks.removed(purged=True)

        report.log("7. fresh install of %s, then remove" % version)
        before = systemd.snapshot()
        log_units("fresh install", before)
        pm.install(version, ci=True)
        checks.installed(version)
        compare("fresh install", before)
        pm.remove()
        pm.purge()
        checks.removed(purged=True)
    except Abort as e:
        print("ABORT: %s" % e, flush=True)
        report.summary()
        return 2

    report.summary()
    return 1 if report.failures else 0


if __name__ == "__main__":
    sys.exit(main())
