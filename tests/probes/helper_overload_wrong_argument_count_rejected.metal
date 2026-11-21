// EXPECT: error no matching function for call to "f"
// No overload takes three arguments.
#include <metal_stdlib>
using namespace metal;
float f(float x) { return x; }
float f(float x, float y) { return x + y; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = f(1.0f, 2.0f, 3.0f); }
