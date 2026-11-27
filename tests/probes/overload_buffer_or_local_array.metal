// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// A whole buffer reaches only the device pointer and a local array only the thread one, so each call has one
// viable candidate: thread i writes 21.
#include <metal_stdlib>
using namespace metal;
inline float f(thread float* p) { return 1.0f; }
inline float f(device float* p) { return 2.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { float a[2] = {0, 0}; out[i] = f(out) * 10.0f + f(a); }
