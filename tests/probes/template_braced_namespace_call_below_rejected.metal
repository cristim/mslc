// EXPECT: error arguments do not depend on its parameters
// Refs #341: S{x} in a namespace names the struct too.
#include <metal_stdlib>
using namespace metal;
namespace N { struct S { float a; }; float g(S s, float k) { return 10.0f; }
template <typename T> float f(T x) { return g(S{x}, 1); }
float g(S s, int k) { return 20.0f; } }
kernel void k(device float* out [[buffer(0)]]) { out[0] = N::f(1.0f); }
