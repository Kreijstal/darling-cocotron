/* SPDX-License-Identifier: MIT */
#ifndef CG_PATH_DASH_INTERNAL_H
#define CG_PATH_DASH_INTERNAL_H
#include <stddef.h>
typedef struct { double x, y; } CGDashPoint;
/* Internal commands use the existing move/line/quadratic/cubic/close order. */
typedef struct { unsigned char op; CGDashPoint p[3]; } CGDashCommand;
typedef struct { CGDashCommand *commands; size_t count; } CGDashResult;
typedef struct {
    void *(*resize)(void *, size_t, void *);
    void (*release)(void *, void *);
    void *context;
} CGDashAllocator;
__attribute__((visibility("hidden")))
int CGDashCopy(const CGDashCommand *, size_t, const double *, double,
               const double *, size_t, CGDashResult *, const CGDashAllocator *);
__attribute__((visibility("hidden"))) void CGDashDestroy(CGDashResult *, const CGDashAllocator *);
#endif
