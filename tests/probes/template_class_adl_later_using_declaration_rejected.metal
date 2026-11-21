// EXPECT: error using declaration of "h" is declared after the class template
// Refs #341: a using declaration below the template brings another namespace's h into the lookup.
#include <metal_stdlib>
using namespace metal;
namespace N { struct S { float a; }; float h(S s, float x) { return 1.0f; } }
namespace Z { float h(N::S s, int x) { return 2.0f; } }
template <typename T> struct P { T v; float f() const { return h(v, 1); } };
namespace N { using Z::h; }
kernel void k(device float* out [[buffer(0)]]) { P<N::S> p; p.v.a = 1; out[0] = p.f(); }
