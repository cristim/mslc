// EXPECT: error found by argument-dependent lookup where the instance is made
// Refs #341: Apple picks N::h(S, int) (declared below the template) for h(v, 1); mslc does not model that lookup.
#include <metal_stdlib>
using namespace metal;
namespace N { struct S { float a; }; float h(S s, float x) { return 1.0f; } }
template <typename T> struct P { T v; float f() const { return h(v, 1); } };
namespace N { float h(S s, int x) { return 2.0f; } }
kernel void k(device float* out [[buffer(0)]]) { P<N::S> p; p.v.a = 1; out[0] = p.f(); }
