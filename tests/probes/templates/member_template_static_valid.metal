// EXPECT: valid
// RUN: grid=64 local=8 cases=3 elems=2048
// Refs #341: a static member template is called as S::f(x) or S::f<T>(x); it has no object.
#include <metal_stdlib>
using namespace metal;
struct S {
	float a;
	template <typename U> static U f(U x) { return x + x; }
	template <typename U> static U g(U x) { return x * U(3); }
};
kernel void member_template_static_valid(device float* out [[buffer(0)]], device const float* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{ out[i] = S::f(in[i]) + S::f<float>(1.0f) + float(S::g<int>(2)) + S::f(0.5f); }
