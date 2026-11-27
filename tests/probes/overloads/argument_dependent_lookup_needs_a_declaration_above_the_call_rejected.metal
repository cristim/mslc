// EXPECT: error not a function declared in this file
// A::f is declared below g, so g's lookup does not find it (Apple: undeclared identifier).
#include <metal_stdlib>
using namespace metal;
namespace A { struct S { float a; }; }
float g(A::S s) { return f(s); }
namespace A { float f(S s) { return 1.0f; } }
kernel void k(device float* out [[buffer(0)]]) { A::S s; s.a = 1.0f; out[0] = g(s); }
