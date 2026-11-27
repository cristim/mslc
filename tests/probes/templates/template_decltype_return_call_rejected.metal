// EXPECT: error is not a shape mslc can name
// Refs #341: a decltype(...) return type is refused outright: it must not hide the
// template's name, or the call would silently go to the builtin clamp.
#include <metal_stdlib>
using namespace metal;
template <typename T> decltype(T()) clamp(T x, T a, T b) { return a; }
kernel void k(device float* o [[buffer(0)]]) { o[0] = clamp(o[1], 0.0f, 1.0f); }
