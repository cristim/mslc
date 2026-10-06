// EXPECT: error a default argument is not lowered
#include <metal_stdlib>
using namespace metal;
float f(float x = 2.0f) { return x; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = f(1.0f); }
