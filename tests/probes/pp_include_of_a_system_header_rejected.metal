// EXPECT: error cannot honour #include <simd/simd.h>
// Only <metal_stdlib> and <metal_matrix> are provided. A later change supplies the simd types.
#include <simd/simd.h>
kernel void pp_include_of_a_system_header_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
