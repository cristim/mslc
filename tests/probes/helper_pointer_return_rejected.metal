// EXPECT: error helper function "f" returns a pointer
#include <metal_stdlib>
using namespace metal;
device float* f(device float* p) { return p; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = 1.0f; }
