#include <metal_stdlib>
using namespace metal;
kernel void f(device float *o [[buffer(0)]], uint i [[thread_position_in_grid]])
{ float const a = 1.0; float4 const v = float4(a); float constexpr c = 2.0; float b = a; b += c; o[i] = b + v.x; }
