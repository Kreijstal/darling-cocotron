// SPDX-License-Identifier: GPL-3.0-only
// Ancillary test tooling, GPL-3.0; the patched cocotron sources keep their own licence.
/* Runs the real warp glue of CoreGraphics/CGDirectDisplay.m against mock screens.
 *
 * tests/cg-warp-mouse-glue.sh extracts the text of the functions below from CGDirectDisplay.m and swaps only the
 * Objective-C message sends for struct accesses, so the logic under test is the shipped one. The mock models what
 * the glue reads from the backend: the screens (frame, backing scale, display ID) and -warpMouse:'s BOOL. */
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef double CGFloat;
typedef struct { CGFloat x, y; } CGPoint;
typedef struct { CGFloat width, height; } CGSize;
typedef struct { CGPoint origin; CGSize size; } CGRect;
typedef CGPoint NSPoint;
typedef CGRect NSRect;
typedef int CGError;
typedef unsigned int CGDirectDisplayID;
typedef int BOOL;
enum { kCGErrorSuccess = 0, kCGErrorIllegalArgument = 1001, kCGErrorInvalidConnection = 1002, kCGErrorCannotComplete = 1004 };
#define nil 0

static CGPoint CGPointMake(double x, double y) { CGPoint p = {x, y}; return p; }
static CGRect CGRectMake(double x, double y, double w, double h) { CGRect r = {{x, y}, {w, h}}; return r; }
static NSPoint NSMakePoint(double x, double y) { return CGPointMake(x, y); }
static NSRect NSMakeRect(double x, double y, double w, double h) { return CGRectMake(x, y, w, h); }
#define NSHeight(r) ((r).size.height)
#define NSIsEmptyRect(r) ((r).size.width <= 0 || (r).size.height <= 0)

typedef struct { NSRect f; CGFloat s; unsigned id_; } NSScreen;
typedef struct { NSScreen *items[8]; int n; } NSArray;
typedef struct { NSArray *scr; BOOL ret; int calls; NSPoint last; } NSDisplay;

static NSDisplay *theDisplay;
static NSDisplay *currentDisplay(void) { return theDisplay; }
static BOOL warpMouse(NSDisplay *d, NSPoint p) { d->calls++; d->last = p; return d->ret; }

#include "CGDisplayGeometry.h"
#include GLUE_INC

static int fails, checks;
#define CK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static NSScreen *mk(double x, double y, double w, double h, double s, unsigned id) {
    NSScreen *o = calloc(1, sizeof *o);
    o->f = NSMakeRect(x, y, w, h); o->s = s; o->id_ = id;
    return o;
}
static NSDisplay *disp(int n, NSScreen **s, BOOL ret) {
    NSArray *a = calloc(1, sizeof *a);
    a->n = n;
    for (int i = 0; i < n; i++) a->items[i] = s[i];
    NSDisplay *d = calloc(1, sizeof *d);
    d->scr = a; d->ret = ret;
    return d;
}

int main(void) {
    /* Main 1920x1080 at 1x; a second display to its left, 1280x1024, bottom aligned: CG y = 1080 - 0 - 1024 = 56. */
    NSScreen *m = mk(0, 0, 1920, 1080, 1, 1), *l = mk(-1280, 0, 1280, 1024, 1, 2), *dead = mk(0, 0, 0, 0, 1, 3);
    NSScreen *two[2] = {m, l};
    theDisplay = disp(2, two, 1);
    CGError e;

    e = CGWarpMouseCursorPosition(CGPointMake(100, 200));
    CK(e == 0 && theDisplay->last.x == 100 && theDisplay->last.y == 880, "basic: %d %g,%g", e, theDisplay->last.x, theDisplay->last.y);
    e = CGWarpMouseCursorPosition(CGPointMake(-1e9, -1e9));
    CK(e == 0 && theDisplay->last.x == -1280 && theDisplay->last.y == 1080, "clip to top-left: %g,%g", theDisplay->last.x, theDisplay->last.y);
    e = CGWarpMouseCursorPosition(CGPointMake(1e9, 1e9));
    CK(e == 0 && theDisplay->last.x == 1919 && theDisplay->last.y == 1, "clip to bottom-right: %g,%g", theDisplay->last.x, theDisplay->last.y);

    /* The point is relative to the display's own upper-left corner: CG (-1270,76) is Cocoa y 1004. */
    e = CGDisplayMoveCursorToPoint(2, CGPointMake(10, 20));
    CK(e == 0 && theDisplay->last.x == -1270 && theDisplay->last.y == 1004, "move on display 2: %d %g,%g", e, theDisplay->last.x, theDisplay->last.y);
    e = CGDisplayMoveCursorToPoint(99, CGPointMake(10, 20));
    CK(e == kCGErrorIllegalArgument, "unknown display id: %d", e);

    int calls = theDisplay->calls;
    e = CGWarpMouseCursorPosition(CGPointMake(NAN, 1));
    CK(e == kCGErrorIllegalArgument && theDisplay->calls == calls, "NaN rejected before warpMouse: %d", e);
    e = CGWarpMouseCursorPosition(CGPointMake(1, INFINITY));
    CK(e == kCGErrorIllegalArgument && theDisplay->calls == calls, "Inf rejected before warpMouse: %d", e);

    /* A backend that cannot move the pointer is reported, not hidden. */
    theDisplay = disp(2, two, 0);
    e = CGWarpMouseCursorPosition(CGPointMake(5, 5));
    CK(e == kCGErrorCannotComplete && theDisplay->calls == 1, "warpMouse: NO from CGWarpMouseCursorPosition: %d", e);
    e = CGDisplayMoveCursorToPoint(1, CGPointMake(5, 5));
    CK(e == kCGErrorCannotComplete, "warpMouse: NO from CGDisplayMoveCursorToPoint: %d", e);

    /* An inactive output (empty frame) is no display and does not widen the desktop. */
    NSScreen *three[3] = {m, l, dead};
    theDisplay = disp(3, three, 1);
    e = CGDisplayMoveCursorToPoint(3, CGPointMake(5, 5));
    CK(e == kCGErrorIllegalArgument, "inactive display id: %d", e);
    e = CGWarpMouseCursorPosition(CGPointMake(-1e9, 1e9));
    CK(e == 0 && theDisplay->last.x == -1280 && theDisplay->last.y == 1, "inactive output ignored by the desktop: %g,%g", theDisplay->last.x, theDisplay->last.y);

    NSScreen *none[1] = {m};
    theDisplay = disp(0, none, 1);
    e = CGWarpMouseCursorPosition(CGPointMake(1, 1));
    CK(e == kCGErrorCannotComplete && theDisplay->calls == 0, "no screens: %d", e);
    theDisplay = nil;
    e = CGWarpMouseCursorPosition(CGPointMake(1, 1));
    CK(e == kCGErrorInvalidConnection, "no display, warp: %d", e);
    e = CGDisplayMoveCursorToPoint(1, CGPointMake(1, 1));
    CK(e == kCGErrorInvalidConnection, "no display, move: %d", e);

    /* Every output inactive: the main screen is a zero rect, so there is no desktop to warp inside. */
    NSScreen *allDead[1] = {dead};
    theDisplay = disp(1, allDead, 1);
    e = CGWarpMouseCursorPosition(CGPointMake(10, 10));
    CK(e == kCGErrorCannotComplete && theDisplay->calls == 0, "empty main frame: %d, %d warps", e, theDisplay->calls);

    /* 2x: the far edges are one device pixel (half a point) inside. */
    NSScreen *m2 = mk(0, 0, 960, 540, 2, 1), *one[1] = {m2};
    theDisplay = disp(1, one, 1);
    e = CGWarpMouseCursorPosition(CGPointMake(1e9, 1e9));
    CK(e == 0 && theDisplay->last.x == 959.5 && theDisplay->last.y == 0.5, "2x far edge: %g,%g", theDisplay->last.x, theDisplay->last.y);

    printf("%s: %d checks, %d failures\n", fails ? "FAIL" : "PASS", checks, fails);
    return fails != 0;
}
