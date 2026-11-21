// EXPECT: error is ambiguous
// Refs #341: slice 5e: f(Box<T>) beside f(T) is ambiguous here (partial ordering is not lowered; Apple picks the first).
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; };
template <typename T> T f(Box<T> b) { return b.v; }
template <typename T> T f(T b) { return b + T(1); }
kernel void k(device float* out [[buffer(0)]]) { Box<float> b; b.v = 4.0f; out[0] = f(b); }
