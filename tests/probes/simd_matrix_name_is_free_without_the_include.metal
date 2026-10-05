// EXPECT: valid
//
// matrix_float4x4 is only a typedef once <simd/simd.h> is included, so a struct may have the name before that.
#include <metal_stdlib>
using namespace metal;
struct matrix_float4x4 { float a; };
kernel void simd_matrix_name_is_free_without_the_include(device matrix_float4x4* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i].a = 1.0; }
