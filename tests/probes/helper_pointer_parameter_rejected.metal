// EXPECT: error parameter "p" of helper function "f" is a pointer
#include <metal_stdlib>
using namespace metal;
float f(device float* p) { return p[0]; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = f(out); }
