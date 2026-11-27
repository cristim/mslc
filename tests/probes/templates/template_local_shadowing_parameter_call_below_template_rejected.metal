// EXPECT: error arguments do not depend on its parameters
// Refs #341: a local that shadows the parameter is not type-dependent, so the call binds g(S, int) where the template is declared (Apple: 10); the end-of-file lookup would find
// g(S, float) declared below and pick it, so the call is refused.
#include <metal_stdlib>
using namespace metal;
struct S { float a; };
float g(S s, int y) { return 10.0f; }
template <typename T> float f(T x) { S s0; s0.a = 1.0f; { float x = 1.0f; return g(s0, x); } }
float g(S s, float y) { return 20.0f; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = f(1.0f); }
