// EXPECT: error may overlap
// *p and p[1] (or p->a and p->b) are not proved apart by mslc; Apple accepts, so this is a loud refusal.
#include <metal_stdlib>
using namespace metal;
void f(thread float &x, thread float &y) { x = 1.0f; y = 2.0f; }
void h(thread float *p) { f(*p, p[1]); }
kernel void k(device float *o [[buffer(0)]]) { float a[2] = {0.0f, 0.0f}; h(a); o[0] = a[0] + a[1]*10.0f; }
