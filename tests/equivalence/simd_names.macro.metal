#include <simd/simd.h>
#include <metal_stdlib>
using namespace metal;

kernel void simd_names(device simd_float4 *out [[buffer(0)]],
                       constant matrix_float4x4 *m [[buffer(1)]],
                       constant vector_uint2 *size [[buffer(2)]],
                       uint i [[thread_position_in_grid]])
{
    matrix_float4x4 a = m[0];
    vector_float2 s = vector_float2(size[0]);
    vector_half3 h = vector_half3(1.0);
    out[i] = a[1] * vector_float4(s.x, s.y, float(h.z), 1.0);
}
