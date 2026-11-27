// EXPECT: error <metal_graphics>, <metal_types>, <metal_atomic> and <metal_pack>
// The rejection names every sub-header mslc does read, ending with the last three.
#include <metal_raytracing>
kernel void metal_sub_header_list_in_message(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
