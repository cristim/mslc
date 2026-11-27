// EXPECT: error cannot honour #include <metal_imageblocks>
// mslc does not declare what <metal_imageblocks> holds, so it is rejected rather than skipped.
#include <metal_imageblocks>
kernel void metal_imageblocks_include_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
