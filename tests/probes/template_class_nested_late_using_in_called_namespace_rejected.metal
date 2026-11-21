// EXPECT: error is made after the class template
// Refs #383: the same through a nested instance (Apple 1.0).
#include <metal_stdlib>
using namespace metal;
namespace N { float h(float x) { return 1.0f; } }
namespace M { float h(int x) { return 7.0f; } }
template <typename T> struct In { T v; float g() const { return N::h(v); } };
namespace N { using M::h; }
template <typename T> struct Out { In<T> in; float f() const { return in.g(); } };
kernel void k(device float* o [[buffer(0)]], constant int& s [[buffer(1)]]) {
	Out<int> a; a.in.v = s; o[0] = a.f();
}
