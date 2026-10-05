// EXPECT: valid
//
// <simd/packed.h>, <simd/vector_types.h> and <simd/matrix_types.h> are headers Apple has; the first two declare nothing mslc needs, and the third the matrix names.
#include <simd/packed.h>
#include <simd/vector_types.h>
#include <simd/matrix_types.h>
#include <metal_stdlib>
using namespace metal;
kernel void simd_other_headers_accepted(device float4* out [[buffer(0)]], constant matrix_float4x4* m [[buffer(1)]], uint i [[thread_position_in_grid]])
{ matrix_float4x4 a = m[0]; out[i] = a[0]; }
