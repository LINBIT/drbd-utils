#!/bin/bash
# Find DRBD volumes whose metadata is in "day0" UUID state:
# current_uuid is a real value but no rotation has ever occurred
# (all bitmap_uuid and history_uuid entries are zero).
#
# Such volumes are potentially vulnerable to silent data corruption if a
# peer is re-provisioned with a fresh day0 UUID: DRBD would see matching
# UUIDs and declare the peer UpToDate without resyncing actual data.
#
# Mitigation:
# -----------
# We strongly recommend an upgrade to DRBD 9.2.18 / 9.3.2,
# and to LINSTOR v1.33.2 (or v1.34+) for LINSTOR users.
#
# If you can not do that (yet):
# For existing volumes, you need to provoke a "UUID bump".  In most
# deployments (two or more diskful nodes), this is rather easy: gracefully
# disconnect one of the diskful nodes, and wait for a write to happen on
# the primary, then reconnect.
#
# For "only one diskful node": the easiest way is to stop using the
# resource, isolate the single diskful node and bump the uuid once:
# (`drbdadm disconnect <res> && drbdadm new-current-uuid <res>`),
# then start services again.
#
# For new resources: upgrade LINSTOR to v1.33.2, 1.34+ initializes new
# volumes differently, new volumes will never enter the problematic state.
# -----------
#
# Note: this reads on-disk metadata via drbdmeta(8) without locking the DRBD
# device, which is safe for running resources but may lag a UUID rotation
# that has occurred in memory but not yet been flushed to disk (the window
# is the duration of one metadata disk write -- milliseconds at most).
# Worst case, it would miss a UUID bump that was happening _right now_,
# and spuriously flag a resource that just became unaffected.
# For a definitive check of running volumes use  drbdsetup get-gi  instead.

# Verify required tools are present and sufficiently recent.
for tool in drbdadm drbdmeta awk jq; do
	command -v "$tool" >/dev/null 2>&1 ||
		{ echo "ERROR: required tool not found: $tool" >&2; exit 1; }
done

# drbdmeta is co-packaged with drbdadm; check drbd-utils version via drbdadm.
# Filtering on ."#meta".valid in jq requires drbd-utils >= 9.33.0.
DRBD_UTILS_MIN="9.33.0"
drbd_utils_ver=$(drbdadm --version | sed -ne 's/^DRBDADM_VERSION=//p')
if [ -z "$drbd_utils_ver" ]; then
	echo "ERROR: could not determine drbd-utils version from drbdadm --version" >&2
	exit 1
fi
if ! printf '%s\n%s\n' "$DRBD_UTILS_MIN" "$drbd_utils_ver" | sort -V -C; then
	echo "ERROR: drbd-utils $drbd_utils_ver is too old; need >= $DRBD_UTILS_MIN" >&2
	exit 1
fi

if [[ "$(id -u)" != 0 ]]; then
	echo "ERROR: this needs to be run as root." >&2
	exit 1
fi

OUT=$( mktemp unrotated_day0_resources_$(uname -n)_$(date +%F_%s).XXXXXX.json )

drbdadm -d up all |
	awk '/^drbdsetup new-minor/ { res[$4]=$3; vol[$4]=$5; }
		/^drbdsetup attach/ { print res[$3],vol[$3],$3,$4,$5,$6 }' |
while read r v m d md idx; do
	drbdmeta - v09 $md $idx dump-superblock --output json 2>/dev/null |
		jq -c \
			   --arg hostname "$HOSTNAME" \
			   --arg resource "$r" \
			   --arg volume "$v" \
			   --arg minor "$m" \
			   --arg disk "$d" \
			   --arg "meta-disk" "$md" \
			   --arg "meta-idx" "$idx" \
			'select(
				(."#meta".valid | IN("VALID_MD_FOUND", "VALID_MD_FOUND_AT_LAST_KNOWN_LOCATION")) and
				# Exclude uninitialised (0) and fresh create-md
				# (UUID_JUST_CREATED = 4, or 5 with UUID_PRIMARY bit
				# set) -- not a real day0 UUID.
				(."current_uuid" | IN(
					"0x0000000000000000",
					"0x0000000000000004",
					"0x0000000000000005") | not) and
				([.peers[].bitmap_uuid == "0x0000000000000000"] | all) and
				([.history_uuids[] == "0x0000000000000000"] | all) and
				# this is (.flags & MDF_WAS_UP_TO_DATE);
				# but jq does not have convenient decode_hex or bitwise_and
				(.flags[-2:-1]|ascii_downcase|((explode[]-48)%39%2!=0)))
			| { current_uuid: .current_uuid, superblock: ., where: $ARGS.named }'
done > "$OUT"

if [ -s "$OUT" ]; then
	echo "=== Found suspect resources:"
	jq -r '.where|[.hostname, "\(.resource)/\(.volume)", "/dev/drbd\(.minor)"]|@tsv' "$OUT"
	echo "=== $(wc -l < "$OUT") suspect resources; superblock details in ${OUT}"
else
	echo "No resources in day0 UUID state found." >&2
	rm -f "$OUT"
fi
