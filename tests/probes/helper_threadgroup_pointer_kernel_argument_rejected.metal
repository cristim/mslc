// EXPECT: error is a threadgroup argument of the kernel, whose length the pipeline sets
#include <metal_stdlib>
using namespace metal;
inline float get(threadgroup float* s, uint l) { return s[l]; }
kernel void k(device float* out [[buffer(0)]], threadgroup float* t [[threadgroup(0)]], uint i [[thread_position_in_grid]]) { out[i] = get(t, i); }
