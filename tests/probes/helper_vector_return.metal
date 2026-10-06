// EXPECT: valid
// DISASM: OpFunctionCall %v3float
#include <metal_stdlib>
using namespace metal;
float3 to_srgb(float3 rgb)
{
    rgb.r = rgb.r * 2.0f;
    return rgb + float3(0.5f);
}
kernel void k(device float4* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = float4(to_srgb(float3(0.25f, 0.5f, 0.75f)), 1.0f); }
