// EXPECT: valid
// Refs #341: a function of another name below the template cannot be what h(v, 1) finds.
#include <metal_stdlib>
using namespace metal;
namespace N { struct S { float a; }; float h(S s, float x) { return 1.0f; } }
template <typename T> struct P { T v; float f() const { return h(v, 1); } };
namespace N { float other(S s, int x) { return 2.0f; } }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { P<N::S> p; p.v.a = 1; out[i] = p.f() + N::other(p.v, 1); }
