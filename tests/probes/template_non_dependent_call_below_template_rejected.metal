// EXPECT: error arguments do not depend on its parameters
// Refs #341: Apple binds g(s0, 1) where the template is declared (g(S, float), result 1); the end-of-file
// lookup would find g(S, int) declared below and pick it (2), so such a call is refused.
#include <metal_stdlib>
using namespace metal;
struct S { float a; };
float g(S s, float y) { return 1.0f; }
template <typename T> float f(T x) { S s0; s0.a = 1.0f; return g(s0, 1); }
float g(S s, int y) { return 2.0f; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = f(1.0f); }
