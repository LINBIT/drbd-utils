#!/bin/bash
# This script exists to work around SELinux policies created for DRBD v8
# utilities. If drbdadm or drbdsetup would be called directly in a service
# we would be transitioned to the drbd_t context, which is targeted at
# DRBD v8 tools and disallow things like netlink sockets.
#
# By using this script, we first transition to a general unconfined context,
# which allows us calling drbdadm and drbdsetup without these restrictions.

case "$1" in
adjust)
  exec /usr/sbin/drbdadm adjust "$2"
  ;;
down)
  exec /usr/sbin/drbdsetup down "$2"
  ;;
primary)
  exec /usr/sbin/drbdsetup primary "$2"
  ;;
secondary)
  exec /usr/sbin/drbdsetup secondary "$2"
  ;;
*)
  echo "Unknown verb $1" >&2
  exit 1
  ;;
esac
