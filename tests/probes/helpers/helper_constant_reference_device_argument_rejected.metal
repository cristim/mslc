// EXPECT: error not in the constant address space
#include <metal_stdlib>
using namespace metal;
float plus(const constant float &c) { return c + 1.0f; }
kernel void k(device float *o [[buffer(0)]]) { o[0] = plus(o[1]); }
