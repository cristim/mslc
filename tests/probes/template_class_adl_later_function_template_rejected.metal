// EXPECT: error function template "h" is declared after the class template
// Refs #341: a function template written below the template is found by argument-dependent lookup at the end of the file.
#include <metal_stdlib>
using namespace metal;
namespace N { struct S { float a; }; float h(S s, float x) { return 1.0f; } }
template <typename T> struct P { T v; float f() const { return h(v, 1); } };
namespace N { template <typename U> float h(S s, U x) { return 2.0f; } }
kernel void k(device float* out [[buffer(0)]]) { P<N::S> p; p.v.a = 1; out[0] = p.f(); }
