// EXPECT: error arguments do not depend on its parameters
// Refs #341: P(x) names the struct P, so it is not type-dependent and Apple binds g(P, float) (10).
#include <metal_stdlib>
using namespace metal;
struct S { float a; };
struct P { int v; P(int a) : v(a) {} };
float g(P p, float k) { return 10.0f; }
template <typename T> float f(T x) { return g(P(x), 1); }
float g(P p, int k) { return 20.0f; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = f(1.0f); }
