// EXPECT: valid
// DISASM-NOT: OpFAdd
// Refs #341: f(Box<T>) beside f(T): Apple picks the first, the more specialized.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; };
template <typename T> T f(Box<T> b) { return b.v; }
template <typename T> T f(T b) { return b + T(1); }
kernel void k(device float* out [[buffer(0)]]) { Box<float> b; b.v = 4.0f; out[0] = f(b); }
