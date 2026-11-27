// EXPECT: error argument 1 of the call to "g" cannot be converted to the parameter type float3
// Apple: "no matching function for call to 'g'".
#include <metal_stdlib>
using namespace metal;
float3 g(float3 x) { return x; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = g(float2(1.0f)).x; }
