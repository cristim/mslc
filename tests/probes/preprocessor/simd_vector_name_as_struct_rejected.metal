// EXPECT: error "vector_float4" is a builtin type name
//
// Apple: "definition of type 'vector_float4' conflicts with typedef of the same name".
#include <metal_stdlib>
using namespace metal;
struct vector_float4 { float a; };
kernel void simd_vector_name_as_struct_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
