// EXPECT: error undeclared type "vector_float"
//
// Metal's simd header declares vector types of 2 to 4 components only.
#include <metal_stdlib>
using namespace metal;
kernel void simd_vector_name_without_width_rejected(device float* out [[buffer(0)]], constant vector_float* v [[buffer(1)]], uint i [[thread_position_in_grid]])
{ out[i] = 1.0; }
