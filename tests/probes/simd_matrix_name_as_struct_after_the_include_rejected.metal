// EXPECT: error redefinition of "matrix_float4x4"
//
// Apple: "definition of type 'matrix_float4x4' conflicts with type alias of the same name".
#include <simd/simd.h>
#include <metal_stdlib>
using namespace metal;
struct matrix_float4x4 { float a; };
kernel void simd_matrix_name_as_struct_after_the_include_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
