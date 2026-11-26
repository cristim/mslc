// EXPECT: error assigning to the pointer "p" is not lowered yet; Metal lets a pointer be reassigned, mslc does not
#include <metal_stdlib>
using namespace metal;
void f(device float* p, device float* q, uint i) { p = q; p[i] = 1.0f; }
kernel void helper_pointer_reassign_rejected(device float* out [[buffer(0)]],
                                             device float* in [[buffer(1)]],
                                             uint i [[thread_position_in_grid]])
{ f(in, out, i); }
