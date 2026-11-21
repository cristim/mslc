// EXPECT: error unknown type name "P"
// Refs #341: a struct the template names in its signature has to be declared above it (Apple: unknown type name).
#include <metal_stdlib>
using namespace metal;
template <typename T> float f(T x, P y) { return 3.0f; }
struct P { float a; };
kernel void k(device float* out [[buffer(0)]]) { P p; p.a = 1.0f; out[0] = f(1.0f, p); }
