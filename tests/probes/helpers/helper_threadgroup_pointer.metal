// EXPECT: valid
// DISASM-NOT: OpFunctionParameter %_ptr_Workgroup
// A threadgroup pointer parameter takes a threadgroup local array, as a thread pointer takes a local one. The
// helper names the caller's Workgroup variable itself, once per list of variables, and takes no Workgroup
// pointer: the NVIDIA driver loses stores made through one. On an RTX 3060 thread i writes
// i + (i + 1) % 64 + 200 * i, the same as the kernel with the helpers inlined.
#include <metal_stdlib>
using namespace metal;
inline void put(threadgroup float* s, uint l, float v) { s[l] = v; }
inline float sum2(threadgroup float* s, threadgroup const float* t, uint l) { return s[l] + s[(l + 1) % 64] + t[l] * 100.0f; }
inline float pass(threadgroup float* s, threadgroup float* t, uint l) { return sum2(s, t, l); }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]], uint l [[thread_index_in_threadgroup]]) {
  threadgroup float a[64];
  threadgroup float b[64];
  put(a, l, float(l)); put(b, l, float(l) * 2.0f);
  threadgroup_barrier(mem_flags::mem_threadgroup);
  out[i] = pass(a, b, l);
}
