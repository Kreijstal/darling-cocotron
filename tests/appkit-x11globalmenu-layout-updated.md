# X11GlobalMenu LayoutUpdated guest test

`appkit-x11globalmenu-layout-updated.m` calls `NSApplication`'s `setMainMenu:`
twice and listens for the `LayoutUpdated` signal that `X11GlobalMenu` emits on
`/MenuBar` (`com.canonical.dbusmenu`). It passes only if both signals have
signature `ui`, arrive in order with revision 2 then 3, and have parent 0. It
then prints `PASS: LayoutUpdated ui 2/0 then ui 3/0 from cached emitter` and
exits 0. Any other outcome prints `FAIL: ...` and exits 1.

Coverage: this exercises 1 of the 5 `dbus_message_append_args` call sites moved
to non-variadic iterator appends. The other four (Introspect `s`, AboutToShow
`b`, RegisterWindow `uo`, UnregisterWindow `u`) were compared byte for byte
against the old variadic form with libdbus marshalling during review. That
comparison is not committed.

## Running it by hand

There is no runner. Build the `.m` as a Darwin guest executable linked against
AppKit, Foundation, libSystem and the Mach-O libdbus wrapper
(`/usr/lib/native/libdbus.dylib` in the guest), not host `-ldbus-1`. Use a
private `DPREFIX`, a private session bus and display, and set
`DARLING_APPKIT_BACKEND=x11`. The test needs a nonempty
`DBUS_SESSION_BUS_ADDRESS`. Then run:

```text
DPREFIX=<fresh-prefix> DARLING_INSTALL_PREFIX=<image>/usr/local \
  <build>/src/startup/darling shell <guest-test-path>
```

## Status

Not run in CI: no runner is registered. This is a manual guest fixture.
