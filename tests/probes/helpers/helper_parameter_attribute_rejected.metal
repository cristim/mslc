// EXPECT: error parameter "x" of helper function "f" has an attribute
#include <metal_stdlib>
using namespace metal;
float f(float x [[buffer(0)]]) { return x; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = f(1.0f); }
