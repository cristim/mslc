// EXPECT: error arguments do not depend on its parameters
// Refs #341: a braced temporary S{x} is not type-dependent (its type is the named struct), so Apple binds the call where the
// template is declared (g(S, float), 10); the end-of-file lookup would find g(S, int) below it (20), so it is refused.
#include <metal_stdlib>
using namespace metal;
struct S { float a; };
float g(S s, float k) { return 10.0f; }
template <typename T> float f(T x) { return g(S{x}, 1); }
float g(S s, int k) { return 20.0f; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = f(1.0f); }
