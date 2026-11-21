// EXPECT: error depends on the parameters of a function template
// Refs #341: slice 5d: a return type Box<T> is read while T is still being deduced, which is not lowered.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; };
template <typename T> Box<T> make(T x) { Box<T> b; b.v = x; return b; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = make(3.0f).v; }
