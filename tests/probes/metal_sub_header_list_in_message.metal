// EXPECT: error <metal_graphics> and <metal_types>
// The rejection names every sub-header mslc does read, ending with the last two.
#include <metal_atomic>
kernel void metal_sub_header_list_in_message(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
