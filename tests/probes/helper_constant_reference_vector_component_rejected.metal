// EXPECT: error is a component of a vector
#include <metal_stdlib>
using namespace metal;
float plus(const constant float &c) { return c + 1.0f; }
kernel void k(device float *o [[buffer(0)]], constant float2 &v [[buffer(1)]]) { o[0] = plus(v.y); }
