#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Prints the CHANGELOG.md section of a version, without its heading, as release notes.
#   release-notes.sh <x.y.z> [CHANGELOG.md]
set -euo pipefail

version=${1:?usage: release-notes.sh <x.y.z> [CHANGELOG.md]}
changelog=${2:-$(dirname "$0")/../../CHANGELOG.md}

notes=$(awk -v heading="## [$version]" '
    index($0, heading) == 1 { found = 1; next }
    found && (/^## \[/ || /^\[[^]]+\]: /) { exit }
    found { print }
' "$changelog" | sed -e '/./,$!d')

if [[ -z $notes ]]; then
    echo "release-notes.sh: no '## [$version]' section in $changelog" >&2
    exit 1
fi
printf '%s\n' "$notes"
