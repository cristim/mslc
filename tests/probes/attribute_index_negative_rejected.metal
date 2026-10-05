// EXPECT: error needs a constant integer argument between 0 and 4294967295
//
// A negative enumerator is not an index.
#include <metal_stdlib>
using namespace metal;
enum { Bad = -1 };
kernel void attribute_index_negative_rejected(device uint* out [[buffer(Bad)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
