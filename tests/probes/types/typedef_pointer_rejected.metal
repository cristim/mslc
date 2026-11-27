// EXPECT: error a typedef of a pointer type is not supported
//
// Apple accepts a typedef of a pointer with an address space; mslc has no place in the type to keep one.
#include <metal_stdlib>
using namespace metal;
typedef device float* fptr;
kernel void typedef_pointer_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
