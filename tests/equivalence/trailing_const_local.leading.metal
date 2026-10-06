#include <metal_stdlib>
using namespace metal;
kernel void f(device float *o [[buffer(0)]], uint i [[thread_position_in_grid]])
{ const float a = 1.0; const float4 v = float4(a); constexpr float c = 2.0; float b = a; b += c; o[i] = b + v.x; }
