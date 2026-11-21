// EXPECT: valid
// DISASM: %float_20
// DISASM-NOT: %float_10
// Refs #341: arguments that name the parameter (-x, abs(x), x + 1.0f) are type-dependent, so the call is bound
// at the end of the file and finds g(S, float) declared below the template, as Apple does (20 three times).
#include <metal_stdlib>
using namespace metal;
struct S { float a; };
float g(S s, int y) { return 10.0f; }
template <typename T> float f(T x) { S s0; s0.a = 1.0f; return g(s0, -x) + g(s0, abs(x)) + g(s0, x + 1.0f); }
float g(S s, float y) { return 20.0f; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = f(1.0f); }
