// EXPECT: valid
// DISASM: OpFunctionCall %float
#include <metal_stdlib>
using namespace metal;
struct Out { float4 position [[position]]; };
float f(float x) { return x * 2.0f; }
vertex Out v(uint vid [[vertex_id]]) { Out o; o.position = float4(f(1.0f)); return o; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = f(2.0f); }
