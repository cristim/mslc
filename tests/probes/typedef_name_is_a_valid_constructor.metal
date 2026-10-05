// EXPECT: valid
//
// A typedef of a scalar or matrix is called like the type.
#include <metal_stdlib>
using namespace metal;
typedef float4x4 mat_t;
typedef half h_t;
kernel void typedef_name_is_a_valid_constructor(device float4* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ mat_t m = mat_t(float4(1.0), float4(2.0), float4(3.0), float4(4.0)); out[i] = m[1] + float4(float(h_t(2.0))); }
