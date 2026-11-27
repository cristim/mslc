// EXPECT: valid
// DISASM: OpFunctionCall %mat3v3float
#include <metal_stdlib>
using namespace metal;
float3x3 twice(float3x3 m) { return m * 2.0f; }
kernel void k(device float3* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    float3x3 m = float3x3(float3(1, 0, 0), float3(0, 1, 0), float3(0, 0, 1));
    out[i] = twice(m) * float3(1, 2, 3);
}
