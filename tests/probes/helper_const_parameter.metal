// EXPECT: valid
#include <metal_stdlib>
using namespace metal;
float f(const float x) { return x + 1.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = f(1.0f); }
