// SPDX-License-Identifier: GPL-3.0-only
// Host harness for tests/appkit-view-rects-being-drawn.sh: runs the real text of
// -[NSView getRectsBeingDrawn:count:] (extracted from AppKit/NSView.m, message sends rewritten
// to C calls) against mock views. Messaging a nil opaqueAncestor writes nothing, as in Objective-C.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef long NSInteger;
typedef struct { double x, y, w, h; } NSRect;
typedef struct View View; typedef View NSView;
struct View {
    View *opaque, *super;
    NSRect *_rectsBeingRedrawn; NSInteger _rectsBeingRedrawnCount;
    NSRect _visibleRect; NSRect *_invalidRects; NSInteger _invalidRectCount;
    int _needsDisplay;
};
static int failures, callocs;
static void *mock_calloc(NSInteger n, size_t sz) {
    callocs++;
    if (n < 0 || n > 1000) { printf("  FAIL: calloc of %ld rects (garbage count)\n", n); failures++; return NULL; }
    return calloc(n ? n : 1, sz);
}
static int NSIntersectsRect(NSRect a, NSRect b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}
static void getRects(View *self, const NSRect **rects, NSInteger *count);
static void msg_getRects(View *v, const NSRect **r, NSInteger *c) { if (v) getRects(v, r, c); } /* nil: writes nothing */
static View *msg_opaqueAncestor(View *v) { return v->opaque; }
static View *msg_superview(View *v) { return v->super; }
static NSRect msg_convertRect(View *v, NSRect r, View *to) { (void)v; (void)to; return r; }
#include GETRECTS_INC

__attribute__((noinline)) static void poison_stack(void) {
    volatile unsigned char buf[16384];
    for (size_t i = 0; i < sizeof buf; i++) buf[i] = 0x7f;
}
static void run(const char *name, View *v, NSInteger expect, int expect_callocs) {
    printf("%s\n", name);
    callocs = 0;
    poison_stack();
    const NSRect *r = NULL; NSInteger c = 0;
    getRects(v, &r, &c);
    if (c != expect) { printf("  FAIL: count %ld, expected %ld\n", c, expect); failures++; }
    if (c && !r) { printf("  FAIL: count %ld with NULL rects\n", c); failures++; }
    free(v->_rectsBeingRedrawn); v->_rectsBeingRedrawn = NULL; v->_rectsBeingRedrawnCount = 0;
    if (callocs != expect_callocs) { printf("  FAIL: %d callocs, expected %d\n", callocs, expect_callocs); failures++; }
}
int main(void) {
    NSRect vis = {0, 0, 100, 100};
    /* 1: non-opaque view, no opaque ancestor (opaqueAncestor nil) */
    View a = {0}; a._visibleRect = vis;
    run("non-opaque view, nil opaqueAncestor", &a, 0, 0);
    /* 2: opaque view whose superview has no opaque ancestor */
    View top = {0}; top._visibleRect = vis;
    View b = {0}; b._visibleRect = vis; b.opaque = &b; b.super = &top;
    run("opaque view under non-opaque chain", &b, 0, 0);
    /* 3: positive control, non-opaque view with an opaque ancestor holding one invalid rect */
    NSRect inv = {10, 10, 20, 20};
    View o = {0}; o._visibleRect = vis; o.opaque = &o; o._invalidRects = &inv; o._invalidRectCount = 1;
    View c = {0}; c._visibleRect = vis; c.opaque = &o;
    run("non-opaque view with opaque ancestor", &c, 1, 1);
    printf(failures ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", failures);
    return failures != 0;
}
