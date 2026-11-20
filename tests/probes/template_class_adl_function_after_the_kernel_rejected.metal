// EXPECT: error found by argument-dependent lookup where the instance is made
// Refs #341: clang makes the instance at the end of the file, so N::h(S, int) below the kernel is the one Apple calls (2.0).
#include <metal_stdlib>
using namespace metal;
namespace N { struct S { float a; }; float h(S s, float x) { return 1.0f; } }
template <typename T> struct P { T v; float f() const { return h(v, 1); } };
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { P<N::S> p; p.v.a = 1; out[i] = p.f(); }
namespace N { float h(S s, int x) { return 2.0f; } }
