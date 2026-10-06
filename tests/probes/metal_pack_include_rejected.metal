// EXPECT: error cannot honour #include <metal_pack>
// mslc does not declare what <metal_pack> holds, so it is rejected rather than skipped.
#include <metal_pack>
kernel void metal_pack_include_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
