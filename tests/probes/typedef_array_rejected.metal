// EXPECT: error a typedef of an array type is not supported
//
// Apple accepts a typedef of an array; mslc has no array value type to give it.
#include <metal_stdlib>
using namespace metal;
typedef float four[4];
kernel void typedef_array_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
