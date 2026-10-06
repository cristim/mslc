// EXPECT: valid
// DISASM-NOT: OpFunctionCall
// DISASM-NOT: %v3float
#include <metal_stdlib>
using namespace metal;
float3 unused(float3 x) { return x * 2.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = 1.0f; }
