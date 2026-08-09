#!/bin/bash
# Check out the ddnet-libs revision that the submodule points at.
#
# While bundled library changes are still being reviewed the revision is not on
# ddnet/ddnet-libs yet, so look for it in the forks belonging to the repository owner and
# to the pull request author as well. Pass additional owners as arguments, or set
# DDNET_LIBS_URL to a repository that should be searched first.
#
# This script is temporary: once https://github.com/ddnet/ddnet-libs/pull/51 is merged,
# delete it and check the submodule out with "submodules: recursive" again in every
# workflow that calls it.
set -euo pipefail

revision="$(git rev-parse HEAD:ddnet-libs)"

if [ -e ddnet-libs/.git ] && [ "$(git -C ddnet-libs rev-parse HEAD 2> /dev/null || true)" = "${revision}" ]; then
	echo "ddnet-libs is already at ${revision}"
	exit 0
fi

echo "Looking for ddnet-libs revision ${revision}"

urls=()
if [ -n "${DDNET_LIBS_URL:-}" ]; then
	urls+=("${DDNET_LIBS_URL}")
fi
urls+=("https://github.com/ddnet/ddnet-libs")
for owner in "$@"; do
	if [ -n "${owner}" ]; then
		urls+=("https://github.com/${owner}/ddnet-libs")
	fi
done

if [ ! -e ddnet-libs/.git ]; then
	mkdir -p ddnet-libs
	git -C ddnet-libs init -q
fi

tried=()
for url in "${urls[@]}"; do
	case " ${tried[*]-} " in
	*" ${url} "*) continue ;;
	esac
	tried+=("${url}")

	echo "Trying ${url}"
	if git -C ddnet-libs fetch -q --depth 1 "${url}" "${revision}" 2> /dev/null; then
		git -C ddnet-libs checkout -q --force FETCH_HEAD
		echo "Checked out ddnet-libs ${revision} from ${url}"
		exit 0
	fi
done

echo "Could not find ddnet-libs revision ${revision} in: ${tried[*]}" >&2
exit 1
