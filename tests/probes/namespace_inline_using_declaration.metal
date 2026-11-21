// EXPECT: valid
// DISASM: OpFunctionCall
// A using declaration of a function and a constant declared in an inline namespace, and the
// struct reached as A::S.
#include <metal_stdlib>
namespace A { inline namespace I { constant float v = 1.0; float f(float a) { return a + 1.0; } struct S { float x; }; } }
using A::v;
using A::f;
kernel void kern(device float* out [[buffer(0)]]) { A::S s; s.x = f(2.0); out[0] = v + s.x; }
