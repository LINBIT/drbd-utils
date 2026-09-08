#!/bin/bash
#
# Generate the vmshed VM specification for the packaging tests from the
# drbd9-tests VM specification, so that the list of distributions stays in
# one place (the build job derives its distribution list from the same file).
#
# Usage: gen-vms.sh <drbd9-tests/virter/vms.toml> > vms.toml
#
# Drops the drbd9-tests provisioning (we want the plain distribution image
# and provision it with provision.toml instead) and every VM that is not
# tagged "distro" (mainline kernel, Windows).
#
# Requires rq (TOML -> JSON) and jq. The TOML is written by hand because rq
# refuses to serialize the nested tables.

set -e -u -o pipefail

[ $# -eq 1 ] || { echo "Usage: $0 <vms.toml>" >&2; exit 1; }

rq -t < "$1" | jq -r '
	"name = \"pkgtest\"",
	"provision_file = \"provision.toml\"",
	"provision_timeout = \"10m\"",
	( .vms[] | select(.vm_tags | index("distro")) |
		"\n[[vms]]",
		"base_image = \(.base_image | tojson)",
		"vcpus = \(.vcpus)",
		"memory = \(.memory | tojson)",
		"vm_tags = \(.vm_tags | tojson)",
		"[vms.values]",
		( .values | to_entries[] | "\(.key) = \(.value | tojson)" )
	)'
