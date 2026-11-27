// EXPECT: error "vector_float2" is a builtin type name
//
// Apple: "redefinition of 'vector_float2' as different kind of symbol".
#include <metal_stdlib>
using namespace metal;
kernel void vector_float2(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
