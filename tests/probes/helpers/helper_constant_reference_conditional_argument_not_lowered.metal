// EXPECT: error has to be a variable, an element or a member of constant memory
// Apple accepts a conditional of two constant lvalues (it is an lvalue); mslc does not lower it yet.
#include <metal_stdlib>
using namespace metal;
constant float A = 1.0f;
constant float B = 2.0f;
float plus(const constant float &c) { return c + 1.0f; }
kernel void k(device float *o [[buffer(0)]]) { o[0] = plus(o[1] > 0.0f ? A : B); }
