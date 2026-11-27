// EXPECT: error call to "f" is ambiguous
// Every scalar-to-vector splat ranks alike, whatever the element type: a float reaches
// float3 and int3 equally (Apple: ambiguous).
#include <metal_stdlib>
using namespace metal;
float f(float3 x) { return 1.0f; }
float f(int3 x) { return 2.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = f(1.0f); }
