// EXPECT: valid
// An overload that calls another overload of its own name is not recursion: the by-name
// call graph would have read f -> f.
#include <metal_stdlib>
using namespace metal;
float f(float x) { return x * 2.0f; }
float f(float3 x) { return f(x.x) + f(x.y) + f(x.z); }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = f(float3(1.0f, 2.0f, 3.0f)); }
