#!/usr/bin/env bash
# Runs tests/x11-remove-decorations.c on its own Xvfb or Xephyr (never the user's session).
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
temp=$(mktemp -d)
xpid=""

cleanup() {
    if [ -n "$xpid" ]; then
        kill "$xpid" 2>/dev/null || true
    fi
    rm -rf "$temp"
}
trap cleanup EXIT

CC="${CC:-gcc}"

sanitizer_flags=()
if echo 'int main(void){return 0;}' | "$CC" -fsanitize=address,undefined -x c - -o "$temp/asan-probe" 2>/dev/null; then
    sanitizer_flags=(-fsanitize=address,undefined)
else
    echo "note: AddressSanitizer/UndefinedBehaviorSanitizer unavailable; compiling without sanitizers" >&2
fi

"$CC" -Wall -Wextra "${sanitizer_flags[@]}" -g "$root/tests/x11-remove-decorations.c" -lX11 -o "$temp/test_decorations"

if command -v xvfb-run >/dev/null 2>&1; then
    xvfb-run -a -s '-screen 0 1024x768x24' "$temp/test_decorations"
elif command -v Xephyr >/dev/null 2>&1; then
    disp_num=""
    for i in $(seq 99 120); do
        if [ ! -e "/tmp/.X11-unix/X$i" ]; then
            disp_num="$i"
            break
        fi
    done
    if [ -z "$disp_num" ]; then
        echo "FAIL: could not find a free display number for Xephyr" >&2
        exit 1
    fi
    disp=":$disp_num"
    Xephyr "$disp" -screen 1024x768 -ac -br -noreset >/dev/null 2>&1 &
    xpid=$!

    ready=0
    for _ in $(seq 1 30); do
        if DISPLAY="$disp" xset q >/dev/null 2>&1 || DISPLAY="$disp" xprop -root >/dev/null 2>&1; then
            ready=1
            break
        fi
        sleep 0.1
    done
    if [ "$ready" -ne 1 ]; then
        echo "FAIL: Xephyr failed to become ready on $disp" >&2
        exit 1
    fi

    DISPLAY="$disp" "$temp/test_decorations"
else
    # Fallback to in-process memory and ASan bounds check when no X server is available
    echo "note: neither xvfb-run nor Xephyr found; running in-process bounds checks only" >&2
    "$temp/test_decorations"
fi
