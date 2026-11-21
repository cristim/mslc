// EXPECT: error no matching function for call to "f"
// Apple has no implicit conversion from one vector type to another: an int3 is neither a
// float3 nor a float.
#include <metal_stdlib>
using namespace metal;
float f(float3 x) { return 1.0f; }
float f(float x) { return 2.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = f(int3(1)); }
