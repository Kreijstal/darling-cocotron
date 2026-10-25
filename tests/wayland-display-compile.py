#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compile this checkout's complete WaylandDisplay.m with a configured Darling build.

Usage: python3 tests/wayland-display-compile.py /path/to/darling/build
The build supplies SDK/Wayland headers and flags; its outputs are not changed.
"""
import pathlib
import shlex
import subprocess
import sys
import tempfile

build = pathlib.Path(sys.argv[1]).resolve()
source = pathlib.Path(__file__).resolve().parents[1] / "AppKit/Wayland.backend/WaylandDisplay.m"
target = "src/external/cocotron/AppKit/CMakeFiles/Wayland_backend.dir/Wayland.backend/WaylandDisplay.m.o"
commands = subprocess.check_output(["ninja", "-C", str(build), "-t", "commands", target], text=True)
command = shlex.split(commands.splitlines()[-1])
if any(token in command for token in ("&&", ";", "|")):
    raise SystemExit("Expected one compiler command, not a shell pipeline")
with tempfile.TemporaryDirectory(prefix="wayland-display-compile-") as directory:
    command[command.index("-c") + 1] = str(source)
    command[command.index("-o") + 1] = str(pathlib.Path(directory) / "display.o")
    if "-MF" in command:
        command[command.index("-MF") + 1] = str(pathlib.Path(directory) / "display.d")
    result = subprocess.run(command, cwd=build)
    if result.returncode:
        raise SystemExit(result.returncode)
print("PASS: the complete WaylandDisplay translation unit compiles")
