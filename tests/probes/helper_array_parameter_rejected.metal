// EXPECT: error parameter "x" of helper function "f" is an array
#include <metal_stdlib>
using namespace metal;
float f(float x[2]) { return x[0]; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = 1.0f; }
