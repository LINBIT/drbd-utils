#!/bin/bash
# DRBD utils regression tests
#
# License: GPL-2.0
# Copyright (C) 2018 LINBIT HA-Solutions GmbH
# Roland Kammerer <roland.kammerer@linbit.com>
###

die() {
	>&2 echo "$1"
	exit 1
}

[ $# = 0 ] && die "Specifiy at least 1 test"

cd "$(cd "$(dirname "$0")"; pwd -P)"

# be very careful with that, a build might fail in a container because the host has a loaded drbd module
# for now we only have "up -d" tests.
# if [ -z "${FORCE+x}" ]; then
# 	lsmod | grep -q drbd && die "You have a loaded drbd module, please unload, or set FORCE=1"
# fi

: "${__DRBD_NODE__:=undertest}"
export __DRBD_NODE__

testdir="$(dirname "$1")"
case $testdir in
	*v84) testdir=v84; DRBD_DRIVER_VERSION_OVERRIDE=8.4.0;;
	*v9) testdir=v9; DRBD_DRIVER_VERSION_OVERRIDE=9.0.0;;
	*) die "Could not determine test directory/test directory invalid ('$testdir')";;
esac

[ -d "$testdir" ] || die "Test directory $testdir does not exist"
export DRBD_DRIVER_VERSION_OVERRIDE

cd "$testdir" || exit 1
[ -L drbdadm ] || die "Test directory does not contain a link named 'drbdadm'"

tests=()
for t in "$@"; do
	[ "$testdir" != "$(basename "$(dirname "$t")")" ] && die "You are not allowed to mix different module test classes"
	tests+=("$(basename "$t")")
done
readarray -t sortedtests < <(printf '%s\0' "${tests[@]}" | sort -z | xargs -0n1)

export PATH=.:$PATH
echo
echo "Test setup:"
echo "cd $PWD"
echo "PATH=$PATH \\"
echo "__DRBD_NODE__=$__DRBD_NODE__  DRBD_DRIVER_VERSION_OVERRIDE=$DRBD_DRIVER_VERSION_OVERRIDE \\"
echo "bash clitest --first" "${sortedtests[@]}"
echo
bash clitest --first "${sortedtests[@]}"
