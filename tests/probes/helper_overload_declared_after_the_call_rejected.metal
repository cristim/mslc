// EXPECT: error cannot be converted to the parameter type
// Only the declarations above a call are candidates. f(float3) is below k, so the call
// has f(float) alone, and a float3 does not convert to a float (Apple: no matching function).
#include <metal_stdlib>
using namespace metal;
float f(float x) { return x; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = f(float3(1.0f)); }
float f(float3 x) { return x.x; }
