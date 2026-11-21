// EXPECT: error is made after the class template
// Refs #385: B is an alias of the alias A of N; the late using in N is still a scope the template reads.
#include <metal_stdlib>
using namespace metal;
namespace N { float h(float x) { return 1.0f; } }
namespace M { float h(int x) { return 7.0f; } }
namespace A = N;
namespace B = A;
template <typename T> struct In { T v; float g() const { return B::h(v); } };
namespace N { using M::h; }
kernel void k(device float* o [[buffer(0)]], constant int& s [[buffer(1)]]) { In<int> a; a.v = s; o[0] = a.g(); }
