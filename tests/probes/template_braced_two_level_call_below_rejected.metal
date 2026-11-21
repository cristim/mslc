// EXPECT: error arguments do not depend on its parameters
// Refs #341: the same through a two-level instance: f calls h, and h's call is the one bound at its definition.
#include <metal_stdlib>
using namespace metal;
struct S { float a; };
float g(S s, float k) { return 10.0f; }
template <typename T> float h(T x) { return g(S{x}, 1); }
template <typename T> float f(T x) { return h(x); }
float g(S s, int k) { return 20.0f; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = f(1.0f); }
