// EXPECT: error is first named inside a function template
// Refs #341: slice 5d: an instance first named by a function template's body is not lowered.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; };
template <typename T> float f(T x) { Box<T> b; b.v = x; return float(b.v); }
kernel void k(device float* out [[buffer(0)]]) { out[0] = f(1.0f); }
