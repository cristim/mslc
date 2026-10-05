// EXPECT: error undeclared type "vector_float8"
//
// Likewise the 8 and 16 wide ones, which are incomplete types in Apple's compiler.
#include <metal_stdlib>
using namespace metal;
kernel void simd_vector_wide_rejected(device float* out [[buffer(0)]], constant vector_float8* v [[buffer(1)]], uint i [[thread_position_in_grid]])
{ out[i] = 1.0; }
