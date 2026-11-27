// EXPECT: valid
// Two kernels pass their own threadgroup arrays to the same helpers: each gets its own copy, naming its
// variable (an RTX 3060 gives 3 * (i + 1) for k and 1 for k2).
#include <metal_stdlib>
using namespace metal;
inline void put(threadgroup float* s, uint l, float v) { s[l] = v; }
inline float get(threadgroup float* s, uint l) { return s[(l + 1) % 64]; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]], uint l [[thread_index_in_threadgroup]]) {
  threadgroup float a[64];
  put(a, l, float(l) * 3.0f);
  threadgroup_barrier(mem_flags::mem_threadgroup);
  out[i] = get(a, l);
}
kernel void k2(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]], uint l [[thread_index_in_threadgroup]]) {
  threadgroup float z[64];
  put(z, l, 1.0f);
  threadgroup_barrier(mem_flags::mem_threadgroup);
  out[i] = get(z, l);
}
