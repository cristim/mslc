// EXPECT: valid
// RUN: grid=1 local=1
// Refs #385: a using declaration in N does not reach A::h, an alias of N::I; Apple binds I::h(float) (1.0), as mslc does, so the alias is not over-refused.
#include <metal_stdlib>
using namespace metal;
namespace N { namespace I { float h(float x) { return 1.0f; } } }
namespace M { float h(int x) { return 7.0f; } }
namespace A = N::I;
template <typename T> struct In { T v; float g() const { return A::h(v); } };
namespace N { using M::h; }
kernel void k(device float* o [[buffer(0)]], constant int& s [[buffer(1)]]) { In<int> a; a.v = s; o[0] = a.g(); }
