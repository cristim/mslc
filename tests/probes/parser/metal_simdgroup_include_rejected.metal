// EXPECT: error cannot honour #include <metal_simdgroup>
// mslc does not declare what <metal_simdgroup> holds, so it is rejected rather than skipped.
#include <metal_simdgroup>
kernel void metal_simdgroup_include_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
