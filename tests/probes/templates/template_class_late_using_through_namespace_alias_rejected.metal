// EXPECT: error is made after the class template
// Refs #385: A::h(v) reads N, the target of the alias A; a using declaration written in N below the template makes Apple keep N::h(float) (1.0) where mslc bound the later M::h(int) (7.0).
#include <metal_stdlib>
using namespace metal;
namespace N { float h(float x) { return 1.0f; } }
namespace M { float h(int x) { return 7.0f; } }
namespace A = N;
template <typename T> struct In { T v; float g() const { return A::h(v); } };
namespace N { using M::h; }
kernel void k(device float* o [[buffer(0)]], constant int& s [[buffer(1)]]) { In<int> a; a.v = s; o[0] = a.g(); }
