// EXPECT: error index 31 is out of bounds
//
// Apple: "'buffer' attribute parameter is out of bounds: must be between 0 and 30".
#include <metal_stdlib>
using namespace metal;
kernel void buffer_index_31_rejected(device uint* out [[buffer(31)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
