#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Ancillary test tooling, GPL-3.0; the patched cocotron sources keep their own licence.
# Runs the real warp glue of CoreGraphics/CGDirectDisplay.m against mock screens (no display, no network).
# The function text is extracted from the .m at test time; only the Objective-C message sends are rewritten
# to struct accesses. CG_SRC_DIR points at another copy of CoreGraphics/ (used to mutation-check the test).
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
src=${CG_SRC_DIR:-$root/CoreGraphics}
temp=$(mktemp -d)
trap 'rm -rf "$temp"' EXIT

# Each function starts at the line matching its header and ends at the first line that is a lone "}".
headers=(
    '^static NSScreen \*screenForDisplayID\('
    '^static CGRect globalRectForFrame\('
    '^static CGDisplayGeometryRect desktopBounds\('
    '^static CGError warpToGlobalPoint\('
    '^CGError CGWarpMouseCursorPosition\('
    '^CGError CGDisplayMoveCursorToPoint\('
)
for header in "${headers[@]}"; do
    H=$header awk '$0 ~ ENVIRON["H"] {on=1; found=1} on {print} on && /^}$/ {on=0} END {exit !found}' \
        "$src/CGDirectDisplay.m" >>"$temp/raw.inc" ||
        { echo "FAIL: no function matching /$header/ in $src/CGDirectDisplay.m; update tests/cg-warp-mouse-glue.sh" >&2; exit 1; }
done

sed -E \
    -e 's/NSArray<NSScreen \*> \*/NSArray */g' \
    -e 's/for \(NSScreen \*screen in screens\) \{/for (int i_ = 0; i_ < screens->n; i_++) for (NSScreen *screen = screens->items[i_]; screen; screen = 0) {/' \
    -e 's/\[\[screens objectAtIndex: 0\] frame\]/(screens->items[0]->f)/g' \
    -e 's/\[screens objectAtIndex: 0\]/(screens->items[0])/g' \
    -e 's/\[screens count\]/(screens->n)/g' \
    -e 's/\[display screens\]/(display->scr)/g' \
    -e 's/\[(screen|main) frame\]/(\1->f)/g' \
    -e 's/\[main backingScaleFactor\]/(main->s)/g' \
    -e 's/\[screen cgDirectDisplayID\]/(screen->id_)/g' \
    -e 's/\[display warpMouse: (NSMakePoint\([^]]*\))\]/warpMouse(display, \1)/g' \
    "$temp/raw.inc" >"$temp/glue.inc"

# A '[' that does not follow an identifier or ']' opens a message send.
if grep -nE '(^|[^A-Za-z0-9_)\]])\[' "$temp/glue.inc" >&2; then
    echo "FAIL: message sends the test does not model remain in the glue (lines above); update tests/cg-warp-mouse-glue.sh" >&2
    exit 1
fi

cc -Wall -Wextra -Wno-unused-function -DGLUE_INC="\"$temp/glue.inc\"" -I"$src" \
    "$root/tests/cg-warp-mouse-glue.c" -lm -o "$temp/glue"
"$temp/glue"
