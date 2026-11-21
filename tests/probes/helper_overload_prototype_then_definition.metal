// EXPECT: valid
// A prototype and its definition are one function, even with another overload between them,
// and a call below the prototype can name it before the definition appears.
#include <metal_stdlib>
using namespace metal;
float f(float3 x);
float f(float x) { return x; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = f(float3(1.0f)) + f(2.0f); }
float f(float3 x) { return x.x + x.y; }
