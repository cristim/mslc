// EXPECT: error write ".template f<...>(...)"
// Refs #386: Box<T> depends on T, so a bare call on it is refused as Apple does.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; template <typename U> float f(U x) const { return float(v) + float(x); } };
template <typename T> float g(T in) { Box<T> bx{in}; return bx.f<float>(1.0f); }
kernel void k(device float* o [[buffer(0)]]) { o[0] = g(2.0f); }
