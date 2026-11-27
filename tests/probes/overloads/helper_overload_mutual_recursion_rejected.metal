// EXPECT: error recursive call
// A cycle through two overloads and a prototype: no single name closes it.
#include <metal_stdlib>
using namespace metal;
float f(float3 x);
float f(float x) { return f(float3(x)); }
float f(float3 x) { return f(x.x); }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = f(1.0f); }
