// EXPECT: valid
//
// <simd/simd.h> defines the guard macros Apple's does, and includes <metal_matrix>.
#include <simd/simd.h>
#if !defined(__SIMD_HEADER__) || !defined(__SIMD_MATRIX_TYPES_HEADER__) || !defined(__SIMD_VECTOR_TYPES_HEADER__) || !defined(__SIMD_PACKED_HEADER__) || !defined(__METAL_MATRIX_H)
#error a macro the header defines is missing
#endif
kernel void simd_include_defines_guard_macros(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
