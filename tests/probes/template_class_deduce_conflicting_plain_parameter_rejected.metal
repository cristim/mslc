// EXPECT: error no matching function for call to "f"
// Refs #341: slice 5e: f(Box<T>, T) given (Box<float>, int): T is float and int; Apple rejects it.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; };
template <typename T> T f(Box<T> a, T x) { return a.v + x; }
kernel void k(device float* out [[buffer(0)]]) { Box<float> a; a.v = 1.0f; out[0] = f(a, 2); }
