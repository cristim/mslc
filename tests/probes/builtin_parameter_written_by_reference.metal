// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// A builtin parameter passed to a helper's thread reference is written through it, so it is copied at entry
// like one the body assigns: thread i writes i + 10.
#include <metal_stdlib>
using namespace metal;
inline void bump(thread uint& x) { x += 10; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { uint j = i; bump(i); out[j] = float(i); }
