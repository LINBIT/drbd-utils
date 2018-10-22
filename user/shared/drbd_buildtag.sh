#!/bin/bash

drbd_buildtag_h() {
	local out=$1
	set -e; exec > ${out}.new
	echo -e "/* automatically generated. DO NOT EDIT. */"
	if test -e ../../.git && GITHEAD=$(git rev-parse HEAD); then
		GITDIFF=$(cd .. && git diff --name-only HEAD |
			tr -s '\t\n' '  ' |
			sed -e 's/ *$//')
		echo -e "#define GITHASH \"${GITHEAD}\""
		echo -e "#define GITDIFF \"${GITDIFF}\""
	elif ! test -e ${out} ; then
		echo >&2 "${out} not found."
		test -e ../../.git &&
		>&2 printf "%s\n" \
			"git did not work, but this looks like a git checkout?"
			"Install git and try again." ||
		echo >&2 "Your DRBD source tree is broken. Unpack again.";
		exit 1;
	else
		grep GITHASH "${out}"
		grep GITDIFF "${out}"
	fi
	mv -f "${out}.new" "${out}"

	exit 0
}

drbd_buildtag_c() {
	local out=$1
	set -e; exec > ${out}.new
	echo -e "/* automatically generated. DO NOT EDIT. */"
	echo -e "#include \"drbd_buildtag.h\""
	echo -e "const char *drbd_buildtag(void)\n{"
	echo -e "\treturn \"GIT-hash: \" GITHASH GITDIFF"
	if [ -z "${WANT_DRBD_REPRODUCIBLE_BUILD}" ] || [ -z "${SOURCE_DATE_EPOCH}" ] ; then
		buildinfo="build by ${USER}@${HOSTNAME}, $(date "+%F %T")"
	else											\
		buildinfo="reproducible build, $(date -u -d@${SOURCE_DATE_EPOCH} "+%F %T")"
	fi
	echo -e "\t\t\" $buildinfo\";\n}"
	mv -f "${out}.new" "${out}"

	exit 0
}

calldir=$PWD
cd "$(cd "$(dirname "$0")"; pwd -P)"

[[ $1 =~ drbd_buildtag\.h$ ]] && drbd_buildtag_h "${calldir}/$1"
[[ $1 =~ drbd_buildtag\.c$ ]] && drbd_buildtag_c "${calldir}/$1"

exit 1
