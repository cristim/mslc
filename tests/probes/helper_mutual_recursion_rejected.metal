// EXPECT: error recursive
#include <metal_stdlib>
using namespace metal;
float g(float x);
float f(float x) { return g(x); }
float g(float x) { return f(x); }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = f(2.0f); }
