// EXPECT: valid
#include <metal_stdlib>
using namespace metal;
constant float kScale = 3.0f;
float f(float x) { return x * kScale; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = f(2.0f); }
