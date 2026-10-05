// EXPECT: error cannot honour #include <simd/packed.h>
// Only <metal_stdlib>, <metal_matrix> and <simd/simd.h> are provided.
#include <simd/packed.h>
kernel void pp_include_of_a_system_header_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
