// EXPECT: valid
// DISASM-NOT: %float_2
// A function template in the argument's namespace is a candidate too (Apple calls it, f(T,int)
// beats f(S,float) here), so the helper's body, which returns 2.0f, is never emitted.
#include <metal_stdlib>
using namespace metal;
namespace A { struct S { float v; }; template <typename T> float f(T x, int y) { return 1.0f; } }
float f(A::S x, float y) { return 2.0f; }
kernel void k(device float* out [[buffer(0)]]) { A::S s; s.v = 1.0f; out[0] = f(s, 1); }
