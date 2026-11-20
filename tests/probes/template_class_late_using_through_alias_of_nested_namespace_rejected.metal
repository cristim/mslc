// EXPECT: error is made after the class template
// Refs #385: A is an alias of N::I; a using declaration written in N::I below the template is refused.
#include <metal_stdlib>
using namespace metal;
namespace N { namespace I { float h(float x) { return 1.0f; } } }
namespace M { float h(int x) { return 7.0f; } }
namespace A = N::I;
template <typename T> struct In { T v; float g() const { return A::h(v); } };
namespace N { namespace I { using M::h; } }
kernel void k(device float* o [[buffer(0)]], constant int& s [[buffer(1)]]) { In<int> a; a.v = s; o[0] = a.g(); }
