// EXPECT: valid
// DISASM: OpFunctionCall %float
// Refs #341: T* from a const local array deduces T = const float, and a const T* parameter takes T = float.
#include <metal_stdlib>
using namespace metal;
template <typename T> T first(thread T* p) { return p[0]; }
template <typename T> T second(const thread T* p) { return p[1]; }
kernel void k(device float* out [[buffer(0)]]) { const float a[2] = {1.0f, 2.0f}; float b[2] = {3.0f, 4.0f}; out[0] = first(a) + second(b); }
