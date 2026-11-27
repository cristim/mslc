// EXPECT: error is declared after the class template
// Refs #341: the struct reaches h(s, 1) through a member (implicit this), not a template argument; Apple calls the later h(S, int) (2.0).
#include <metal_stdlib>
using namespace metal;
namespace N { struct S { float a; }; float h(S s, float x) { return 1.0f; } }
template <typename T> struct P { N::S s; T v; float f() const { return h(s, 1); } };
namespace N { float h(S s, int x) { return 2.0f; } }
kernel void k(device float* out [[buffer(0)]]) { P<float> p; p.s.a = 1; out[0] = p.f(); }
