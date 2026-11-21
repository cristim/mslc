// EXPECT: error cannot store through
// A reference to constant memory cannot be assigned through, with or without a written const.
#include <metal_stdlib>
using namespace metal;
float bump(constant float &c) { c = 1.0f; return c; }
kernel void k(device float *o [[buffer(0)]], constant float &c [[buffer(1)]]) { o[0] = bump(c); }
