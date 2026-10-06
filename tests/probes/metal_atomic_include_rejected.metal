// EXPECT: error cannot honour #include <metal_atomic>
// mslc does not declare what <metal_atomic> holds, so it is rejected rather than skipped.
#include <metal_atomic>
kernel void metal_atomic_include_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
