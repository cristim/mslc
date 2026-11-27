// EXPECT: error undeclared type "vector_double2"
//
// Apple declares vector_double2 as an incomplete type, so a value of it cannot be made, and a pointer to it is only the name.
#include <metal_stdlib>
using namespace metal;
kernel void simd_vector_double_rejected(device float* out [[buffer(0)]], constant vector_double2* v [[buffer(1)]], uint i [[thread_position_in_grid]])
{ out[i] = 1.0; }
