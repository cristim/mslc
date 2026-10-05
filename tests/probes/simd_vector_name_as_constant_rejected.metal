// EXPECT: error "simd_float4" is a builtin type name
//
// Apple: "redefinition of 'simd_float4' as different kind of symbol".
#include <metal_stdlib>
using namespace metal;
constant float simd_float4 = 1.0;
kernel void simd_vector_name_as_constant_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
