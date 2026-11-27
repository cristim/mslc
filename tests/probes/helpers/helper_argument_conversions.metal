// EXPECT: valid
#include <metal_stdlib>
using namespace metal;
float f(float x) { return x; }
float3 g(float3 x) { return x; }
uint h(uint x) { return x; }
float3 widen(float x) { return x; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = f(2) + g(1.0f).x + float(h(-1)) + widen(1).y; }
