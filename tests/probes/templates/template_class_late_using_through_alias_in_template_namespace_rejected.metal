// EXPECT: error is made after the class template
// Refs #385: the alias and the template sit in namespace W; the late using is in the alias's target N.
#include <metal_stdlib>
using namespace metal;
namespace N { float h(float x) { return 1.0f; } }
namespace M { float h(int x) { return 7.0f; } }
namespace W { namespace A = N;
template <typename T> struct In { T v; float g() const { return A::h(v); } }; }
namespace N { using M::h; }
kernel void k(device float* o [[buffer(0)]], constant int& s [[buffer(1)]]) { W::In<int> a; a.v = s; o[0] = a.g(); }
