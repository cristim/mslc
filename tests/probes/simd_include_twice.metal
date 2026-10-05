// EXPECT: valid
//
// <simd/simd.h> is guarded, so a second include declares nothing again.
#include <simd/simd.h>
#include <simd/simd.h>
#include <metal_stdlib>
using namespace metal;
kernel void simd_include_twice(device float4* out [[buffer(0)]], constant matrix_float4x4* m [[buffer(1)]], uint i [[thread_position_in_grid]])
{ matrix_float4x4 a = m[0]; out[i] = a[0]; }
