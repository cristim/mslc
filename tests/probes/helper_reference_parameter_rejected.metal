// EXPECT: error parameter "x" of helper function "f" is a reference
#include <metal_stdlib>
using namespace metal;
float f(thread float& x) { return x; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ float v = 1.0f; out[i] = f(v); }
