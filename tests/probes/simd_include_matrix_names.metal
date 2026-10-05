// EXPECT: valid
// DISASM: OpTypeMatrix %v4float 4
// DISASM: OpTypeMatrix %v2float 2
//
// <simd/simd.h> declares the matrix_* and simd_* matrix names.
#include <simd/simd.h>
#include <metal_stdlib>
using namespace metal;
kernel void simd_include_matrix_names(device float4* out [[buffer(0)]], constant matrix_float4x4* m [[buffer(1)]], constant simd_float2x2* n [[buffer(2)]], uint i [[thread_position_in_grid]])
{ matrix_float4x4 a = m[0]; simd_float2x2 b = n[0]; out[i] = a[1] + float4(b[0], b[1]); }
