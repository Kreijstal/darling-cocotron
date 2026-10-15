# NSApplication terminate after last window

Build `application-terminate-after-last-window.m` as a standalone Objective-C executable linked with
AppKit and Foundation using the Darling SDK, and run it in a Darling guest. It needs no display
connection. `DARLING_QUIT_ON_LAST_WINDOW` and the `NSQuitOnLastWindowClosed` default must be unset;
the test refuses to run otherwise.

The test calls `-_checkForTerminate` directly. With no visible window it asks the delegate's
`-applicationShouldTerminateAfterLastWindowClosed:` and calls `-terminate:` only on YES when both
`DARLING_QUIT_ON_LAST_WINDOW` and the `NSQuitOnLastWindowClosed` default are unset. A delegate that
returns NO, lacks the method, or is nil leaves the app running. A visible window, or one hidden only
because the app was deactivated, counts as open.

The test uses an `NSApplication` subclass that counts `-terminate:` calls instead of exiting, and
`NSWindow` stubs with fixed `-isVisible` and `-_isHiddenForDeactivate`, so the real
`-_checkForTerminate` runs without a window server. Nine cases cover delegate YES, NO, no method and
nil with no or closed windows, then YES with a visible, a deactivation-hidden and a mixed window set.
A build whose `_checkForTerminate` unconditionally terminates fails on: delegate NO with no windows,
delegate NO with a closed window, delegate without the method, and nil delegate. The expected fixed
result is `terminate after last window checks=9 failures=0`.

## Limitations

The test covers the helper's termination decision, including delegate YES with no windows. It does not
close a real `NSWindow`, post a close notification, or exercise the delayed check scheduled by
`-_windowWillClose:`.
The NSPanel skip at NSApplication.m:772 is not covered.
