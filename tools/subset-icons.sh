#!/bin/sh
# SPDX-License-Identifier: MIT
# Regenerates assets/fonts/FuinIcons.ttf and assets/fonts/icons.txt from the full Material Symbols Rounded font.
# Usage: tools/subset-icons.sh [path/to/MaterialSymbolsRounded[FILL,GRAD,opsz,wght].ttf]
# The default is the font of the Arch package ttf-material-symbols-variable. fontTools is installed into a
# throwaway virtual environment, nothing is installed system-wide.
set -eu
root=$(cd "$(dirname "$0")/.." && pwd)
font=${1:-/usr/share/fonts/TTF/MaterialSymbolsRounded[FILL,GRAD,opsz,wght].ttf}
[ -f "$font" ] || { echo "font not found: $font" >&2; exit 1; }
venv=${FUIN_ICON_VENV:-${TMPDIR:-/tmp}/fuin-icons-venv}
[ -x "$venv/bin/python" ] || python3 -m venv "$venv"
"$venv/bin/python" -c 'import fontTools' 2>/dev/null || "$venv/bin/pip" install --quiet fonttools
"$venv/bin/python" "$root/tools/subset_icons.py" "$font"
