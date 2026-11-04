#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Ancillary test tooling, GPL-3.0; the patched cocotron sources keep their own licence.
# Runs the real -[NSView getRectsBeingDrawn:count:] of AppKit/NSView.m against mock views whose
# opaqueAncestor is nil (no display, no network). The method text is extracted from the .m at test
# time; only the Objective-C message sends are rewritten to C calls. NSVIEW_SRC points at another
# copy of NSView.m (used to mutation-check the test). The uninitialised read is caught
# deterministically by MemorySanitizer (clang) and, on any compiler, by poisoning the stack first.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
src=${NSVIEW_SRC:-$root/AppKit/NSView.m}
temp=$(mktemp -d)
trap 'rm -rf "$temp"' EXIT

perl -0777 -e '
    my $src = <>;
    $src =~ /(- \(void\) getRectsBeingDrawn: \(const NSRect \*\*\) rects\s+count: \(NSInteger \*\) count\s*\{.*?\n\}\n)/s
        or die "no getRectsBeingDrawn:count: in the source; update tests/appkit-view-rects-being-drawn.sh\n";
    my $m = $1;
    $m =~ s/^- \(void\) getRectsBeingDrawn:.*?\n\{/static void getRects(View *self, const NSRect **rects, NSInteger *count)\n{/s;
    $m =~ s/\[(\w+)\s+getRectsBeingDrawn:\s*(&\w+)\s+count:\s*(&\w+)\]/msg_getRects($1, $2, $3)/gs;
    $m =~ s/\[(\w+)\s+opaqueAncestor\]/msg_opaqueAncestor($1)/g;
    $m =~ s/\[(\w+)\s+superview\]/msg_superview($1)/g;
    $m =~ s/\[(\w+)\s+convertRect:\s*(.*?)\s+toView:\s*(\w+)\]/msg_convertRect($1, $2, $3)/gs;
    $m =~ s/\b(_[a-zA-Z]\w*)\b/self->$1/g;
    $m =~ s/NSZoneCalloc\(\s*NULL,/mock_calloc(/g;
    print $m;
' "$src" >"$temp/getrects.inc"

# A '[' that does not follow an identifier or ']' opens a message send.
if grep -nE '(^|[^A-Za-z0-9_)\]])\[' "$temp/getrects.inc" >&2; then
    echo "FAIL: message sends the test does not model remain in the method (lines above); update tests/appkit-view-rects-being-drawn.sh" >&2
    exit 1
fi

flags=(-O0 -g -Wall -Wextra -Wno-unused-function -DGETRECTS_INC="\"$temp/getrects.inc\"")
cc "${flags[@]}" "$root/tests/appkit-view-rects-being-drawn.c" -o "$temp/plain"
"$temp/plain"
if command -v clang >/dev/null && echo 'int main(void){return 0;}' | clang -fsanitize=memory -x c - -o "$temp/msan-probe" 2>/dev/null; then
    clang "${flags[@]}" -fsanitize=memory -fsanitize-memory-track-origins \
        "$root/tests/appkit-view-rects-being-drawn.c" -o "$temp/msan"
    "$temp/msan"
else
    echo "note: MemorySanitizer unavailable; only the stack-poison check ran" >&2
fi
