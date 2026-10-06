// EXPECT: error cannot call the entry point "v2"
// Apple: "call to kernel function v2".
#include <metal_stdlib>
using namespace metal;
struct Out { float4 position [[position]]; };
vertex Out v2(uint vid [[vertex_id]]) { Out o; o.position = float4(0.0f); return o; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ v2(i); out[i] = 1.0f; }
