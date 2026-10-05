#include <metal_stdlib>
using namespace metal;

kernel void simd_names(device float4 *out [[buffer(0)]],
                       constant float4x4 *m [[buffer(1)]],
                       constant uint2 *size [[buffer(2)]],
                       uint i [[thread_position_in_grid]])
{
    float4x4 a = m[0];
    float2 s = float2(size[0]);
    half3 h = half3(1.0);
    out[i] = a[1] * float4(s.x, s.y, float(h.z), 1.0);
}
