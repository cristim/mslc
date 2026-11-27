// EXPECT: error a trailing return type on function template "conv" is not lowered yet
#include <metal_stdlib>
using namespace metal;
template <typename T> auto conv(T c) -> decltype(c) { return c * 2.0f; }
float conv(float3 c) { return c.x; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = conv(1.0f); }
