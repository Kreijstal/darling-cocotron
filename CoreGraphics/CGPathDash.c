/* SPDX-License-Identifier: MIT
 * Apple public API: https://developer.apple.com/documentation/coregraphics/cgpathcreatecopybydashingpath
 * Explicit engineering edge policy: https://www.w3.org/TR/SVG2/painting.html#StrokeDashing
 * Odd parity, phase wrapping and zero entries follow that policy, without an
 * Apple runtime-equivalence claim. Curves become bounded-error polylines;
 * depth/work/allocation/finite-arithmetic failures reject the whole result.
 * No renderer, stroke width, cap or stroke outline is introduced here. */
#include "CGPathDash.h"
#include <math.h>
#include <float.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define DASH_LIMIT ((size_t)1 << 20)
typedef struct {
    CGDashResult result;
    size_t capacity, work, index, patternCount;
    const double *pattern;
    double remaining, period, phase, tolerance, curveBudget, roundoff;
    CGDashPoint last;
    int pen, gap;
    const CGDashAllocator *allocator;
} Dash;
static void *resize(Dash *d, void *p, size_t n) {
    return d->allocator ? d->allocator->resize(p,n,d->allocator->context) : realloc(p,n);
}
static void release(Dash *d, void *p) {
    if (d->allocator) d->allocator->release(p,d->allocator->context); else free(p);
}
void CGDashDestroy(CGDashResult *r, const CGDashAllocator *a) {
    if (a) a->release(r->commands,a->context); else free(r->commands);
    r->commands=NULL; r->count=0;
}
static int finitePoint(CGDashPoint p) { return isfinite(p.x)&&isfinite(p.y); }
static int equal(CGDashPoint a, CGDashPoint b) { return a.x==b.x&&a.y==b.y; }
static double distance(CGDashPoint a, CGDashPoint b) { return hypot(a.x-b.x,a.y-b.y); }
static CGDashPoint mid(CGDashPoint a, CGDashPoint b) {
    CGDashPoint p={a.x*.5+b.x*.5,a.y*.5+b.y*.5}; return p;
}
static int append(Dash *d, unsigned char op, const CGDashPoint *p) {
    if (d->result.count>=DASH_LIMIT) return 0;
    if (d->result.count==d->capacity) {
        size_t n=d->capacity ? d->capacity*2 : 32;
        if (n>DASH_LIMIT) n=DASH_LIMIT;
        if (n>SIZE_MAX/sizeof(CGDashCommand)) return 0;
        void *q=resize(d,d->result.commands,n*sizeof(CGDashCommand));
        if (!q) return 0;
        d->result.commands=q; d->capacity=n;
    }
    CGDashCommand *c=&d->result.commands[d->result.count++];
    memset(c,0,sizeof(*c)); c->op=op;
    size_t n=op==2?2:op==3?3:op<2?1:0;
    if (n) memcpy(c->p,p,n*sizeof(*p));
    return 1;
}
static int paint(Dash *d, CGDashPoint a, CGDashPoint b) {
    if ((!d->pen||!equal(d->last,a))&&!append(d,0,&a)) return 0;
    if (!append(d,1,&b)) return 0;
    d->pen=1; d->last=b; return 1;
}
static double entry(Dash *d) { return d->pattern[d->index%d->patternCount]; }
static void next(Dash *d) {
    d->index=(d->index+1)%(d->patternCount*(d->patternCount%2?2:1));
    d->remaining=entry(d);
}
static int zeros(Dash *d, CGDashPoint at) {
    size_t limit=d->patternCount*(d->patternCount%2?2:1);
    for (size_t n=0; d->remaining==0; ++n) {
        if (n>=limit||++d->work>DASH_LIMIT) return 0;
        if (!(d->index%2)&&!paint(d,at,at)) return 0;
        next(d);
    }
    return 1;
}
static int restart(Dash *d, CGDashPoint at) {
    d->index=0; d->remaining=entry(d); d->pen=0; d->gap=0;
    double phase=d->phase;
    size_t limit=d->patternCount*(d->patternCount%2?2:1);
    for (size_t n=0; phase>0; ++n) {
        if (n>limit) return 0;
        if (phase<d->remaining) { d->remaining-=phase; phase=0; }
        else { phase-=d->remaining; next(d); }
    }
    (void)at;
    return 1; /* A move alone has no drawable segment, including dot patterns. */
}
static int line(Dash *d, CGDashPoint a, CGDashPoint b) {
    double length=distance(a,b), consumed=0;
    if (!isfinite(length)) return 0;
    if (length==0) {
        if (!zeros(d,a)) return 0;
        return d->index%2 ? 1 : paint(d,a,a);
    }
    while (consumed<length) {
        if (++d->work>DASH_LIMIT||!zeros(d,a)) return 0;
        double left=length-consumed, step=fmin(left,d->remaining);
        double end=consumed+step;
        if (!(step>0)||!(end>consumed)) return 0;
        CGDashPoint q=b;
        if (end<length) {
            double t=end/length;
            q.x=a.x+(b.x-a.x)*t; q.y=a.y+(b.y-a.y)*t;
        }
        /* a stays the source segment start; last/current interpolation is separate. */
        CGDashPoint p=a;
        if (consumed>0) {
            double t=consumed/length;
            p.x=a.x+(b.x-a.x)*t; p.y=a.y+(b.y-a.y)*t;
        }
        if (!finitePoint(p)||!finitePoint(q)) return 0;
        /* Conservative double-precision error accounting. Large translations
         * or tiny intervals must fail instead of collapsing real dash spans.
         * A quarter of the tolerance is reserved for boundary arithmetic. */
        double scale=fmax(fmax(fabs(a.x),fabs(a.y)),fmax(fabs(b.x),fabs(b.y)));
        double margin=64*DBL_EPSILON*(scale+length+fabs(d->remaining));
        d->roundoff+=margin;
        if (!isfinite(d->roundoff)||d->roundoff>d->tolerance*.25||equal(p,q)) return 0;
        if (!(d->index%2)) { if (!paint(d,p,q)) return 0; }
        else { d->pen=0; d->gap=1; }
        consumed=end;
        if (step==d->remaining) { next(d); if (!zeros(d,q)) return 0; }
        else {
            double remaining=d->remaining-step;
            if (remaining==d->remaining) return 0;
            d->remaining=remaining;
        }
    }
    return 1;
}
static double deviation(CGDashPoint p, CGDashPoint a, CGDashPoint b) {
    double length=distance(a,b);
    if (!length) return distance(p,a);
    double ux=(b.x-a.x)/length, uy=(b.y-a.y)/length;
    double projection=(p.x-a.x)*ux+(p.y-a.y)*uy;
    if (projection<=0) return distance(p,a);
    if (projection>=length) return distance(p,b);
    return fabs((p.x-a.x)*uy-(p.y-a.y)*ux);
}
static int curve(Dash *d, CGDashPoint *p, unsigned n, double budget, unsigned depth) {
    if (++d->work>DASH_LIMIT) return 0;
    double upper=0, lower=distance(p[0],p[n-1]), dev=0;
    double scale=0;
    for (unsigned i=0;i<n;i++) scale=fmax(scale,fmax(fabs(p[i].x),fabs(p[i].y)));
    for (unsigned i=1;i<n;i++) upper+=distance(p[i-1],p[i]);
    for (unsigned i=1;i+1<n;i++) dev=fmax(dev,deviation(p[i],p[0],p[n-1]));
    if (!isfinite(upper)||!isfinite(lower)||!isfinite(dev)) return 0;
    if (upper==0) return line(d,p[0],p[n-1]);
    double margin=64*DBL_EPSILON*(scale+upper+lower);
    if (!isfinite(margin)) return 0;
    if (upper-lower+margin<=budget&&dev+margin<=d->tolerance*.75) return line(d,p[0],p[n-1]);
    if (depth>=32||budget*.5==0) return 0;
    CGDashPoint left[4],right[4],q[4]; memcpy(q,p,n*sizeof(*p));
    left[0]=q[0]; right[n-1]=q[n-1];
    for (unsigned level=1;level<n;level++) {
        for (unsigned i=0;i<n-level;i++) q[i]=mid(q[i],q[i+1]);
        left[level]=q[0]; right[n-level-1]=q[n-level-1];
    }
    return curve(d,left,n,budget*.5,depth+1)&&curve(d,right,n,budget*.5,depth+1);
}
static int seam(Dash *d, size_t base, CGDashPoint start) {
    if (base==d->result.count) return 1;
    CGDashCommand *c=d->result.commands;
    if (!equal(c[base].p[0],start)||!d->pen||!equal(d->last,start)) return 1;
    size_t firstEnd=base+1,last=base;
    while (firstEnd<d->result.count&&c[firstEnd].op!=0) firstEnd++;
    for (size_t i=firstEnd;i<d->result.count;i++) if (c[i].op==0) last=i;
    if (last==base) return d->gap ? 1 : append(d,4,NULL);
    size_t n=d->result.count-base,tail=d->result.count-last,head=firstEnd-base;
    if (n>SIZE_MAX/sizeof(*c)) return 0;
    CGDashCommand *tmp=resize(d,NULL,n*sizeof(*c)); if (!tmp) return 0;
    memcpy(tmp,c+last,tail*sizeof(*c));
    memcpy(tmp+tail,c+base+1,(head-1)*sizeof(*c));
    memcpy(tmp+tail+head-1,c+firstEnd,(last-firstEnd)*sizeof(*c));
    memcpy(c+base,tmp,(n-1)*sizeof(*c)); d->result.count--; release(d,tmp); return 1;
}
int CGDashCopy(const CGDashCommand *input,size_t count,const double *transform,
               double phase,const double *pattern,size_t patternCount,
               CGDashResult *out,const CGDashAllocator *allocator) {
    if (!out) return 0;
    out->commands=NULL; out->count=0;
    if ((count&&!input)||count>DASH_LIMIT||patternCount>DASH_LIMIT||
        (patternCount&&!pattern)||!isfinite(phase)||
        (allocator&&(!allocator->resize||!allocator->release))) return 0;
    Dash d={0}; d.allocator=allocator; d.pattern=pattern; d.patternCount=patternCount;
    double minimum=INFINITY,total=0,identity[6]={1,0,0,1,0,0};
    if (!transform) transform=identity;
    for (unsigned i=0;i<6;i++) if (!isfinite(transform[i])) return 0;
    for (size_t i=0;i<patternCount;i++) {
        if (!isfinite(pattern[i])||pattern[i]<0) return 0;
        total+=pattern[i]; if (!isfinite(total)) return 0;
        if (pattern[i]>0) minimum=fmin(minimum,pattern[i]);
    }
    d.period=total*(patternCount%2?2:1); if (!isfinite(d.period)) return 0;
    if (d.period) { d.phase=fmod(phase,d.period); if (d.phase<0) d.phase+=d.period; }
    size_t curves=0;
    for (size_t i=0;i<count;i++) {
        if (input[i].op>4) return 0;
        curves+=input[i].op==2||input[i].op==3;
    }
    /* Half the tolerance covers summed curve length deficits and their
     * arithmetic margins; child budgets halve. A quarter covers boundary
     * arithmetic; the geometric chord test leaves that same safety margin. */
    d.tolerance=fmin(1e-4,minimum/256); d.curveBudget=(d.tolerance*.5)/(curves?curves:1);
    if (d.period&&(!(d.tolerance>0)||!(d.curveBudget>0))) return 0;
    CGDashPoint current={0,0},start={0,0}; size_t base=0; int active=0,closed=0;
    for (size_t i=0;i<count;i++) {
        const CGDashCommand *c=&input[i]; unsigned n=c->op==2?2:c->op==3?3:c->op<2?1:0;
        CGDashPoint p[4]; p[0]=current;
        for (unsigned j=0;j<n;j++) {
            CGDashPoint v=c->p[j]; if (!finitePoint(v)) goto fail;
            p[j+1]=(CGDashPoint){transform[0]*v.x+transform[2]*v.y+transform[4],transform[1]*v.x+transform[3]*v.y+transform[5]};
            if (!finitePoint(p[j+1])) goto fail;
        }
        if (c->op!=0&&!active) goto fail;
        if (closed&&c->op==4) continue;
        if (closed&&c->op!=0) {
            /* Close establishes an implicit new subpath at the old start. */
            base=d.result.count;
            if (d.period&&!restart(&d,current)) goto fail;
            closed=0;
        }
        if (!d.period) { if (!append(&d,c->op,p+1)) goto fail; }
        if (c->op==0) {
            start=current=p[1]; active=1; closed=0; base=d.result.count;
            if (d.period&&!restart(&d,start)) goto fail;
        } else if (c->op==4) {
            if (d.period&&(!line(&d,current,start)||!seam(&d,base,start))) goto fail;
            current=start; closed=1;
        } else {
            if (d.period&&!(n==1?line(&d,current,p[1]):curve(&d,p,n+1,d.curveBudget,0))) goto fail;
            current=p[n];
        }
    }
    *out=d.result; return 1;
fail:
    CGDashDestroy(&d.result,allocator); return 0;
}
