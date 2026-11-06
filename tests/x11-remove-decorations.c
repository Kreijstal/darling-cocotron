#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Host simulation of the format-32 wire packing performed by Xlib's XChangeProperty
// (libX11 ChProp.c). In Xlib, format 32 casts data to (long *) and reads
// nelements * sizeof(long) bytes from memory regardless of host pointer width.
static void xlib_format32_read(const void *data, int nelements, uint32_t *wire_out) {
    const unsigned long *values = (const unsigned long *) data;
    for (int i = 0; i < nelements; i++) {
        wire_out[i] = (uint32_t) values[i];
    }
}

static void test_format32_asan_bounds(void) {
    // Exactly matches the definition in +[X11Window removeDecorationForWindow:onDisplay:]
    unsigned long hints[5] = { 2, 0, 0, 0, 0 };
    uint32_t wire[5] = { 0 };

    xlib_format32_read(hints, 5, wire);

    assert(wire[0] == 2);
    assert(wire[1] == 0);
    assert(wire[2] == 0);
    assert(wire[3] == 0);
    assert(wire[4] == 0);
}

static void test_x11_server_property(Display *dpy) {
    Window root = DefaultRootWindow(dpy);
    Window w = XCreateSimpleWindow(dpy, root, 10, 10, 100, 100, 0, 0, 0);

    // Call XChangeProperty matching X11Window.m
    unsigned long hints[5] = { 2, 0, 0, 0, 0 };
    Atom motif_atom = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);

    XChangeProperty(dpy, w, motif_atom, motif_atom, 32,
                    PropModeReplace, (const unsigned char *) hints, 5);

    Atom actual_type = None;
    int actual_format = 0;
    unsigned long nitems = 0, bytes_after = 0;
    unsigned char *prop = NULL;

    int status = XGetWindowProperty(dpy, w, motif_atom, 0, 5, False, motif_atom,
                                    &actual_type, &actual_format, &nitems, &bytes_after, &prop);

    assert(status == Success);
    assert(actual_type == motif_atom);
    assert(actual_format == 32);
    assert(nitems == 5);
    assert(prop != NULL);

    const unsigned long *vals = (const unsigned long *) prop;
    assert(vals[0] == 2); // flags: MWM_HINTS_DECORATIONS
    assert(vals[1] == 0); // functions: 0
    assert(vals[2] == 0); // decorations: 0
    assert(vals[3] == 0); // input_mode: 0 (must not contain stack garbage)
    assert(vals[4] == 0); // status: 0 (must not contain stack garbage)

    XFree(prop);
    XDestroyWindow(dpy, w);
}

int main(void) {
    // 1. In-process bounds check with ASan
    test_format32_asan_bounds();

    // 2. Integration check with X11 display if available
    const char *disp_name = getenv("DISPLAY");
    if (disp_name && *disp_name) {
        Display *dpy = XOpenDisplay(disp_name);
        if (dpy) {
            test_x11_server_property(dpy);
            XCloseDisplay(dpy);
        }
    }

    printf("PASS: x11 remove decoration format 32 property test\n");
    return 0;
}
