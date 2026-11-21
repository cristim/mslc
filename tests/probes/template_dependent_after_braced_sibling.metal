// EXPECT: valid
// DISASM: %float_20
// DISASM-NOT: %float_10
// Refs #341: g(s0, x) names the parameter, so it is dependent and binds at the end of the file, where
// g(S, float) (20) beats g(S, int) (10) for a float x, as in Apple.
#include <metal_stdlib>
using namespace metal;
struct S { float a; };
float g(S s, int k) { return 10.0f; }
template <typename T> float f(T x) { S s0; s0.a = 1.0f; return g(s0, x); }
float g(S s, float k) { return 20.0f; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = f(1.0f); }
