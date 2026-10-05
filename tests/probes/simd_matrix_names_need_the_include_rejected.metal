// EXPECT: error undeclared type "matrix_float4x4"
//
// The vector names are Apple's without an include; the matrix names are not, and xcrun metal reports "unknown type name 'matrix_float4x4'".
#include <metal_stdlib>
using namespace metal;
kernel void simd_matrix_names_need_the_include_rejected(device float4* out [[buffer(0)]], constant matrix_float4x4* m [[buffer(1)]], uint i [[thread_position_in_grid]])
{ out[i] = m[0][0]; }
