// EXPECT: error depends on the parameters of a function template
// Refs #341: slice 5d: a parameter Box<T> that deduces T is not lowered; the explicit-argument form is.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; };
template <typename T> T unbox(Box<T> b) { return b.v; }
kernel void k(device float* out [[buffer(0)]]) { Box<float> b{ 4.0f }; out[0] = unbox(b); }
