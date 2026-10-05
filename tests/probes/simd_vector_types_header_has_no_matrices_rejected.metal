// EXPECT: error undeclared type "matrix_float2x2"
//
// Apple: "unknown type name 'matrix_float2x2'".
#include <simd/vector_types.h>
#include <metal_stdlib>
using namespace metal;
kernel void simd_vector_types_header_has_no_matrices_rejected(device float2* out [[buffer(0)]], constant matrix_float2x2* m [[buffer(1)]], uint i [[thread_position_in_grid]])
{ out[i] = float2(1.0); }
