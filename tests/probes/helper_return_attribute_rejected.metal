// EXPECT: error attributes on helper function "f"
#include <metal_stdlib>
using namespace metal;
float f(float x) [[unavailable]] { return x; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = 1.0f; }
