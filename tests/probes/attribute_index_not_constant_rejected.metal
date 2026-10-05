// EXPECT: error needs a constant integer argument
//
// An identifier that is not an enumerator is no index.
#include <metal_stdlib>
using namespace metal;
kernel void attribute_index_not_constant_rejected(device uint* out [[buffer(slot)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
