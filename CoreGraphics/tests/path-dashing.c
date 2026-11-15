/* SPDX-License-Identifier: MIT
 * Public API geometry controls; SVG2 edge policy, independent numerical curve reference. */
#include "../CGPathDash.h"
#include <math.h>
#include <float.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) return __LINE__; } while (0)
static int near(double a,double b) { return fabs(a-b)<1e-7; }
static CGDashCommand move(double x,double y) { CGDashCommand c={0};c.p[0]=(CGDashPoint){x,y};return c; }
static CGDashCommand line(double x,double y) { CGDashCommand c=move(x,y);c.op=1;return c; }
static int intervals(const CGDashResult *r,const double *expected,size_t n) {
 if (r->count!=2*n) return 0;
 for (size_t i=0;i<n;i++) if (r->commands[2*i].op!=0||r->commands[2*i+1].op!=1||!near(r->commands[2*i].p[0].x,expected[2*i])||!near(r->commands[2*i+1].p[0].x,expected[2*i+1])) return 0;
 return 1;
}
static int straights(void) {
 CGDashCommand p[]={move(0,0),line(12,0)},saved[2];memcpy(saved,p,sizeof p);CGDashResult r;double dash[]={2,3};
 CHECK(CGDashCopy(p,2,NULL,0,dash,2,&r,NULL));double expected[]={0,2,5,7,10,12};CHECK(intervals(&r,expected,3));CGDashDestroy(&r,NULL);
 CHECK(CGDashCopy(p,2,NULL,1,dash,2,&r,NULL));double shifted[]={0,1,4,6,9,11};CHECK(intervals(&r,shifted,3));CGDashDestroy(&r,NULL);
 CHECK(CGDashCopy(p,2,NULL,-1,dash,2,&r,NULL));double neg[]={1,3,6,8,11,12};CHECK(intervals(&r,neg,3));CGDashDestroy(&r,NULL);
 double odd[]={2,3,1},even[]={2,3,1,2,3,1};CGDashResult other;
 CHECK(CGDashCopy(p,2,NULL,-7,odd,3,&r,NULL));CHECK(CGDashCopy(p,2,NULL,-7,even,6,&other,NULL));CHECK(r.count==other.count&&memcmp(r.commands,other.commands,r.count*sizeof(*r.commands))==0);CGDashDestroy(&r,NULL);CGDashDestroy(&other,NULL);
 CGDashCommand onlyMove[]={move(0,0)};double dotPattern[]={0,3};CHECK(CGDashCopy(onlyMove,1,NULL,0,dotPattern,2,&r,NULL));CHECK(r.count==0);CGDashDestroy(&r,NULL);
 CGDashCommand pointLine[]={move(2,4),line(2,4)};CHECK(CGDashCopy(pointLine,2,NULL,0,dash,2,&r,NULL));CHECK(r.count==2&&r.commands[1].op==1&&near(r.commands[1].p[0].x,2)&&near(r.commands[1].p[0].y,4));CGDashDestroy(&r,NULL);
 CGDashCommand pointCurve[]={move(2,4),{.op=3,.p={{2,4},{2,4},{2,4}}}};CHECK(CGDashCopy(pointCurve,2,NULL,0,dash,2,&r,NULL));CHECK(r.count==2&&r.commands[1].op==1);CGDashDestroy(&r,NULL);
 CGDashCommand pointClose[]={move(2,4),{.op=4}};CHECK(CGDashCopy(pointClose,2,NULL,0,dash,2,&r,NULL));CHECK(r.count>=2&&r.commands[1].op==1);CGDashDestroy(&r,NULL);
 double dots[]={0,3};CHECK(CGDashCopy(p,2,NULL,0,dots,2,&r,NULL));double dotted[]={0,0,3,3,6,6,9,9,12,12};CHECK(intervals(&r,dotted,5));CGDashDestroy(&r,NULL);
 double continuous[]={2,0};CHECK(CGDashCopy(p,2,NULL,0,continuous,2,&r,NULL));size_t moves=0;for(size_t i=0;i<r.count;i++)moves+=r.commands[i].op==0;CHECK(moves==1);CGDashDestroy(&r,NULL);
 double zero[]={0,0};CHECK(CGDashCopy(p,2,NULL,0,zero,2,&r,NULL));CHECK(r.count==2&&r.commands[0].op==0&&r.commands[1].op==1&&near(r.commands[1].p[0].x,12));CGDashDestroy(&r,NULL);
 double matrix[]={2,0,0,3,7,-5};CHECK(CGDashCopy(p,2,matrix,0,dash,2,&r,NULL));CHECK(near(r.commands[0].p[0].x,7)&&near(r.commands[1].p[0].x,9)&&near(r.commands[0].p[0].y,-5));CGDashDestroy(&r,NULL);
 CHECK(memcmp(saved,p,sizeof p)==0);CHECK(CGDashCopy(NULL,0,NULL,0,NULL,0,&r,NULL));CHECK(r.count==0);CGDashDestroy(&r,NULL);return 0;
}
static int joins(void) {
 CGDashResult r;double dash[]={3,2};CGDashCommand corner[]={move(0,0),line(2,0),line(2,3)};
 CHECK(CGDashCopy(corner,3,NULL,0,dash,2,&r,NULL));CHECK(r.count==3&&near(r.commands[2].p[0].x,2)&&near(r.commands[2].p[0].y,1));CGDashDestroy(&r,NULL);
 CGDashCommand separate[]={move(0,0),line(2,0),move(0,5),line(2,5)};CHECK(CGDashCopy(separate,4,NULL,2,dash,2,&r,NULL));CHECK(r.count==4&&near(r.commands[1].p[0].x,1)&&near(r.commands[3].p[0].x,1));CGDashDestroy(&r,NULL);
 CGDashCommand square[]={move(0,0),line(2,0),line(2,2),line(0,2),{.op=4}};double seam[]={3,2};CHECK(CGDashCopy(square,5,NULL,0,seam,2,&r,NULL));
 /* Last painted edge arrives at the initial point, then continues along its first edge. */
 CHECK(r.count>=4);CHECK(near(r.commands[0].p[0].x,1)&&near(r.commands[0].p[0].y,2));size_t moves=0;for(size_t i=0;i<r.count;i++){moves+=r.commands[i].op==0;CHECK(r.commands[i].op!=4);}CHECK(moves==1);CGDashDestroy(&r,NULL);
 CGDashCommand implicit[]={move(0,0),line(2,0),line(2,2),line(0,2),{.op=4},line(9,0)};
 CGDashCommand explicitMove[]={move(0,0),line(2,0),line(2,2),line(0,2),{.op=4},move(0,0),line(9,0)};
 CGDashResult reference;CHECK(CGDashCopy(implicit,6,NULL,0,seam,2,&r,NULL));CHECK(CGDashCopy(explicitMove,7,NULL,0,seam,2,&reference,NULL));CHECK(r.count==reference.count&&memcmp(r.commands,reference.commands,r.count*sizeof(*r.commands))==0);CGDashDestroy(&r,NULL);CGDashDestroy(&reference,NULL);
 CGDashCommand repeated[]={move(0,0),line(2,0),line(2,2),line(0,2),{.op=4},{.op=4},line(9,0)};
 CHECK(CGDashCopy(repeated,7,NULL,0,seam,2,&r,NULL));CHECK(CGDashCopy(explicitMove,7,NULL,0,seam,2,&reference,NULL));CHECK(r.count==reference.count&&memcmp(r.commands,reference.commands,r.count*sizeof(*r.commands))==0);CGDashDestroy(&r,NULL);CGDashDestroy(&reference,NULL);
 double solid[]={20,2};CHECK(CGDashCopy(square,5,NULL,0,solid,2,&r,NULL));CHECK(r.commands[r.count-1].op==4);CGDashDestroy(&r,NULL);
 CHECK(CGDashCopy(square,5,NULL,0,NULL,0,&r,NULL));CHECK(r.count==5&&r.commands[4].op==4);CGDashDestroy(&r,NULL);return 0;
}
static CGDashPoint evaluate(const CGDashPoint *p,unsigned degree,double t) {
 double u=1-t;CGDashPoint v;
 if(degree==2){v.x=u*u*p[0].x+2*u*t*p[1].x+t*t*p[2].x;v.y=u*u*p[0].y+2*u*t*p[1].y+t*t*p[2].y;}
 else {v.x=u*u*u*p[0].x+3*u*u*t*p[1].x+3*u*t*t*p[2].x+t*t*t*p[3].x;v.y=u*u*u*p[0].y+3*u*u*t*p[1].y+3*u*t*t*p[2].y+t*t*t*p[3].y;}return v;
}
/* Independent composite Simpson integration of the analytic derivative. */
static double speed(const CGDashPoint *p,unsigned degree,double t) {
 double u=1-t,x,y;
 if(degree==2){x=2*(u*(p[1].x-p[0].x)+t*(p[2].x-p[1].x));y=2*(u*(p[1].y-p[0].y)+t*(p[2].y-p[1].y));}
 else{x=3*(u*u*(p[1].x-p[0].x)+2*u*t*(p[2].x-p[1].x)+t*t*(p[3].x-p[2].x));y=3*(u*u*(p[1].y-p[0].y)+2*u*t*(p[2].y-p[1].y)+t*t*(p[3].y-p[2].y));}return hypot(x,y);
}
static double integral(const CGDashPoint *p,unsigned d,double t) { const unsigned n=4096;double h=t/n,sum=speed(p,d,0)+speed(p,d,t);for(unsigned i=1;i<n;i++)sum+=(i%2?4:2)*speed(p,d,i*h);return sum*h/3; }
static double separation(CGDashPoint a,CGDashPoint b){return hypot(a.x-b.x,a.y-b.y);}
static double curveDistance(CGDashPoint point,const CGDashPoint *p,unsigned degree){
 unsigned best=0;double distance=INFINITY;
 for(unsigned i=0;i<=256;i++){double value=separation(point,evaluate(p,degree,(double)i/256));if(value<distance){distance=value;best=i;}}
 double lo=(double)(best?best-1:0)/256,hi=(double)(best<256?best+1:256)/256;
 for(unsigned i=0;i<40;i++){double a=(2*lo+hi)/3,b=(lo+2*hi)/3;if(separation(point,evaluate(p,degree,a))<separation(point,evaluate(p,degree,b)))hi=b;else lo=a;}
 return fmin(distance,separation(point,evaluate(p,degree,(lo+hi)/2)));
}
static int curveCase(CGDashPoint *p,unsigned degree) {
 CGDashCommand input[2]={move(p[0].x,p[0].y),{0}};input[1].op=degree==2?2:3;for(unsigned i=0;i<degree;i++)input[1].p[i]=p[i+1];CGDashCommand saved[2];memcpy(saved,input,sizeof input);
 double dash[]={2,3};CGDashResult r;CHECK(CGDashCopy(input,2,NULL,0,dash,2,&r,NULL));size_t firstEnd=1;while(firstEnd+1<r.count&&r.commands[firstEnd+1].op!=0)firstEnd++;
 double lo=0,hi=1;for(unsigned i=0;i<45;i++){double t=(lo+hi)/2;if(integral(p,degree,t)<2)lo=t;else hi=t;}CGDashPoint expected=evaluate(p,degree,(lo+hi)/2),actual=r.commands[firstEnd].p[0];CHECK(hypot(actual.x-expected.x,actual.y-expected.y)<2e-4);
 double painted=0;CGDashPoint last={0,0};for(size_t i=0;i<r.count;i++){CGDashCommand c=r.commands[i];if(c.op==1)painted+=hypot(c.p[0].x-last.x,c.p[0].y-last.y);CHECK(curveDistance(c.p[0],p,degree)<1.01e-4);last=c.p[0];}
 double length=integral(p,degree,1),cycles=floor(length/5),expectedPaint=cycles*2+fmin(2,length-cycles*5);CHECK(fabs(painted-expectedPaint)<2e-4);CHECK(memcmp(saved,input,sizeof input)==0);CGDashDestroy(&r,NULL);return 0;
}
static int analyticCurve(unsigned degree,double length,CGDashPoint expected){
 CGDashCommand p[2]={move(0,0),{0}};p[1].op=degree==2?2:3;
 if(degree==2){p[1].p[0]=(CGDashPoint){1,1};p[1].p[1]=(CGDashPoint){2,0};}
 else{p[1].p[0]=(CGDashPoint){0,1};p[1].p[1]=(CGDashPoint){1,1};p[1].p[2]=(CGDashPoint){1,0};}
 double pattern[]={length/2,length};CGDashResult r;CHECK(CGDashCopy(p,2,NULL,0,pattern,2,&r,NULL));size_t end=1;while(end+1<r.count&&r.commands[end+1].op!=0)end++;CHECK(separation(r.commands[end].p[0],expected)<2e-4);CGDashDestroy(&r,NULL);
 double solid[]={length+1,0};CHECK(CGDashCopy(p,2,NULL,0,solid,2,&r,NULL));double actual=0;CGDashPoint last={0,0};for(size_t i=0;i<r.count;i++){if(r.commands[i].op==1)actual+=separation(last,r.commands[i].p[0]);last=r.commands[i].p[0];}CHECK(fabs(actual-length)<1.01e-4);CGDashDestroy(&r,NULL);return 0;
}
static int curves(void) {
 CGDashPoint q[]={{0,0},{0,4},{6,0}},c[]={{0,0},{0,5},{6,-4},{8,0}},linear[]={{0,0},{0,0},{0,0},{12,0}};
 int n=analyticCurve(2,sqrt(2)+asinh(1),(CGDashPoint){1,.5});if(n)return n;n=analyticCurve(3,2,(CGDashPoint){.5,.75});if(n)return n;n=curveCase(q,2);if(n)return n;n=curveCase(c,3);if(n)return n;return curveCase(linear,3);
}
typedef struct {size_t calls,fail,live;} Allocation;
static void *alloc(void *p,size_t n,void *ctx){Allocation *a=ctx;if(++a->calls==a->fail)return NULL;void *q=realloc(p,n);if(q&&!p)a->live++;return q;}
static void dealloc(void *p,void *ctx){Allocation *a=ctx;if(p)a->live--;free(p);}
static int failures(void) {
 CGDashCommand input[]={move(0,0),line(12,0)};CGDashResult r;double bad[]={-1,2},nan[]={NAN,2};
 CHECK(!CGDashCopy(input,2,NULL,0,bad,2,&r,NULL)&&r.commands==NULL);CHECK(!CGDashCopy(input,2,NULL,0,nan,2,&r,NULL));CHECK(!CGDashCopy(input,2,NULL,INFINITY,NULL,0,&r,NULL));CHECK(!CGDashCopy(input,2,NULL,0,NULL,1,&r,NULL));CHECK(!CGDashCopy(input,SIZE_MAX,NULL,0,NULL,0,&r,NULL));CHECK(!CGDashCopy(input,2,NULL,0,bad,SIZE_MAX,&r,NULL));
 CGDashCommand wrong[]={line(1,2)};CHECK(!CGDashCopy(wrong,1,NULL,0,NULL,0,&r,NULL));wrong[0].op=99;CHECK(!CGDashCopy(wrong,1,NULL,0,NULL,0,&r,NULL));input[0].p[0].x=NAN;CHECK(!CGDashCopy(input,2,NULL,0,NULL,0,&r,NULL));input[0].p[0].x=0;
 double overflow[]={DBL_MAX,DBL_MAX};CHECK(!CGDashCopy(input,2,NULL,0,overflow,2,&r,NULL));
 double huge[]={DBL_MAX,0,0,1,0,0};CHECK(!CGDashCopy(input,2,huge,0,NULL,0,&r,NULL));
 double matrix[]={INFINITY,0,0,1,0,0};CHECK(!CGDashCopy(input,2,matrix,0,NULL,0,&r,NULL));
 CGDashCommand curved[]={move(0,0),{.op=3,.p={{0,10},{10,10},{10,0}}}};double tiny[]={1e-20,1e-20};CHECK(!CGDashCopy(curved,2,NULL,0,tiny,2,&r,NULL));CHECK(r.commands==NULL&&r.count==0);
 CGDashCommand translated[]={move(1e15,0),line(1e15+1,0)};double fine[]={.01,.01};CHECK(!CGDashCopy(translated,2,NULL,0,fine,2,&r,NULL));CHECK(r.commands==NULL&&r.count==0);
 double pattern[]={.1,.1};for(size_t fail=1;fail<10;fail++){Allocation a={0,fail,0};CGDashAllocator allocator={alloc,dealloc,&a};int success=CGDashCopy(input,2,NULL,0,pattern,2,&r,&allocator);if(success)CGDashDestroy(&r,&allocator);else CHECK(r.commands==NULL&&r.count==0);CHECK(a.live==0);}
 CGDashCommand closed[]={move(0,0),line(2,0),line(2,2),line(0,2),{.op=4}};double seam[]={3,2};
 for(size_t fail=1;fail<=3;fail++){Allocation a={0,fail,0};CGDashAllocator allocator={alloc,dealloc,&a};int success=CGDashCopy(closed,5,NULL,0,seam,2,&r,&allocator);if(success)CGDashDestroy(&r,&allocator);else CHECK(r.commands==NULL&&r.count==0);CHECK(a.live==0);}
 return 0;
}
int main(void){int r=straights();if(r)return r;r=joins();if(r)return r;r=curves();if(r)return r;return failures();}
