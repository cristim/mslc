// EXPECT: valid
// A threadgroup local array is an argument of a function template, deduces T in threadgroup T*, and reaches
// only the threadgroup pointer among overloads, as a thread array reaches only the thread one. On an RTX 3060
// thread i writes 2 * (i + 1) + 200 + 10000.
#include <metal_stdlib>
using namespace metal;
template <typename U> inline float run(U scale, threadgroup float* s, uint l) { s[l] = float(l) * scale; threadgroup_barrier(mem_flags::mem_threadgroup); return s[(l + 1) % 64]; }
template <typename T> inline float pick(threadgroup T* s, uint l) { return s[l] + 1000.0f; }
inline float which(thread float* p) { return 1.0f; }
inline float which(threadgroup float* p) { return 2.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]], uint l [[thread_index_in_threadgroup]]) {
  threadgroup float a[64];
  float loc[2] = {0, 0};
  float r = run(2.0f, a, l);
  out[i] = r + pick(a, l) * 0.0f + which(a) * 100.0f + which(loc) * 10000.0f;
}
