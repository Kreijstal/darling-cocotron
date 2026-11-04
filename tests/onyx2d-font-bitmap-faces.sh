#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Ancillary test tooling, GPL-3.0; the patched cocotron sources keep their own licence.
# Runs the real filenameOfFirstScalableFace() of Onyx2D/O2Font_freetype.m against fontconfig font
# sets that rank a bitmap (.pcf) face first (host fontconfig headers, no fonts, no display). The
# function text is extracted from the .m at test time; only the NSString calls are rewritten to
# C strings. O2_SRC_DIR points at another copy of Onyx2D/ (used to mutation-check the test).
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
src=${O2_SRC_DIR:-$root/Onyx2D}
temp=$(mktemp -d)
trap 'rm -rf "$temp"' EXIT

awk '/^static NSString \*filenameOfFirstScalableFace\(/ {on=1; found=1} on {print} on && /^}$/ {on=0} END {exit !found}' \
    "$src/O2Font_freetype.m" >"$temp/raw.inc" ||
    { echo "FAIL: filenameOfFirstScalableFace not found in $src/O2Font_freetype.m; update tests/onyx2d-font-bitmap-faces.sh" >&2; exit 1; }

sed -E \
    -e 's/NSString \*/const char */g' \
    -e 's/\[NSString stringWithUTF8String: \(char \*\) filename\]/((const char *) filename)/' \
    -e 's/\bnil\b/NULL/g' \
    "$temp/raw.inc" >"$temp/select.inc"

if grep -nE '(^|[^A-Za-z0-9_)\]])\[' "$temp/select.inc" >&2; then
    echo "FAIL: message sends the test does not model remain in the function (lines above); update tests/onyx2d-font-bitmap-faces.sh" >&2
    exit 1
fi

cc -Wall -Wextra -DSELECT_INC="\"$temp/select.inc\"" "$root/tests/onyx2d-font-bitmap-faces.c" \
    $(pkg-config --cflags --libs fontconfig) -o "$temp/select"
"$temp/select"
