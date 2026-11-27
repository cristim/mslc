// EXPECT: error has to name a threadgroup local array
#include <metal_stdlib>
using namespace metal;
inline float get(threadgroup float* s, uint l) { return s[l]; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { float a[4] = {1, 2, 3, 4}; out[i] = get(a, i); }
