// EXPECT: error is declared but never defined
// The overload the call names has no body, whatever the other overload does.
#include <metal_stdlib>
using namespace metal;
float f(float3 x);
float f(float x) { return x; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = f(float3(1.0f)); }
