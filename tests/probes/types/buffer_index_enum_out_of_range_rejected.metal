// EXPECT: error index 1000 is out of bounds
//
// The same bound applies to an enumerator.
#include <metal_stdlib>
using namespace metal;
enum { Far = 1000 };
kernel void buffer_index_enum_out_of_range_rejected(device uint* out [[buffer(Far)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
