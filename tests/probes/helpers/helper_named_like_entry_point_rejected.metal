// EXPECT: error "k" names both an entry point and a helper function
#include <metal_stdlib>
using namespace metal;
float k(float x) { return x; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = 1.0f; }
