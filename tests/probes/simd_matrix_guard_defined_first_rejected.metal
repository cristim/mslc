// EXPECT: error undeclared type "matrix_float2x2"
//
// The matrix typedefs sit under their own guard, so defining it first leaves them out, as in Apple's header. Apple: "unknown type name 'matrix_float2x2'".
#define __SIMD_MATRIX_TYPES_HEADER__
#include <simd/simd.h>
#include <metal_stdlib>
using namespace metal;
kernel void simd_matrix_guard_defined_first_rejected(device float2* out [[buffer(0)]], constant matrix_float2x2* m [[buffer(1)]], uint i [[thread_position_in_grid]])
{ out[i] = float2(1.0); }
