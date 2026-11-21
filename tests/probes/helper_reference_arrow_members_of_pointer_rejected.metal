// EXPECT: error may overlap
// *p and p[1] (or p->a and p->b) are not proved apart by mslc; Apple accepts, so this is a loud refusal.
#include <metal_stdlib>
using namespace metal;
struct S { float a; float b; };
void f(thread float &x, thread float &y) { x = 1.0f; y = 2.0f; }
void h(thread S *p) { f(p->a, p->b); }
kernel void k(device float *o [[buffer(0)]]) { S s[2]; s[0].a = 0; s[0].b = 0; h(s); o[0] = s[0].a + s[0].b*10.0f; }
