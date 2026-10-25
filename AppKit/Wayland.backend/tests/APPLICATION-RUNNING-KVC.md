# NSApplication running-state KVC

Compile `application-running-kvc.m` using the configured AppKit Objective-C
compiler flags, then link it with the configured AppKit/Foundation/libSystem
libraries and dylib mappings. Run the Mach-O in a private Darling prefix with the
Wayland backend available.

Baseline: setting the `_running` key throws `NSUnknownKeyException` and the test
exits 1. With the accessors, the test prints `NSApplication running KVC passed`
and exits 0. It verifies true/false KVC round trips, consistency with public
`isRunning`, and continued rejection of an unrelated unknown key.

The name comes from an unchanged guest application's own KVC error at startup
(rung 4). Standard accessor lookup comes from the vendored Foundation
`NSKeyValueAccessor.m`; the backing state and public getter already exist in
`NSApplication.m`. Treating this private key as the existing running flag is a
conservative compatibility inference (GUESS/rung 6); it does not claim Apple's
private implementation. The setter only updates state; it does not independently
start an event loop or synthesize launch notifications.
