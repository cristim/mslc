// EXPECT: error a typedef of a type with an address space is not supported
//
// Apple accepts "typedef constant float4 cf4;"; mslc keeps the address space on the declaration that uses the type.
#include <metal_stdlib>
using namespace metal;
typedef constant float4 cf4;
kernel void typedef_address_space_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
