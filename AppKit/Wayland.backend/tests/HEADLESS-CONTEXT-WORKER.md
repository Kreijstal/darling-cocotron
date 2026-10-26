# Viewless context on a worker

Build `headless-context-worker.m` as an arm64 Mach-O against the build's AppKit,
Foundation, OpenGL, Objective-C runtime and libSystem, with its normal SDK and
reexport mappings. Run in a private Darling prefix with the Wayland backend.

The main thread initializes the display and a context with no view, then waits
up to five seconds without pumping its run loop. The worker makes the context
current, checks both AppKit and CGL thread-local state and a live GL version,
and clears it. The main thread checks its context state remained clear.

Before the guard in `makeCurrentContext`, the worker waits for main to execute
`updateViewParameters`, which has nothing to do for a nil view; exit 5 reports
that stall. Afterward the worker completes without main-thread participation.
The timeout exits the process intentionally because the blocked worker cannot
be joined without running the very main loop this test excludes.

Provenance: existing Cocotron source and authored API-boundary test (rung 4).
This does not relax Wayland's main-thread restriction for window drawables,
validate worker presentation, or establish that a guest application's UI works.
