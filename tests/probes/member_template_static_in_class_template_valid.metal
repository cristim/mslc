// EXPECT: valid
// RUN: grid=64 local=8 cases=3 elems=2048
// Refs #341: a static member template of a class template instance, called qualified and bare inside the class.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct B {
	T a;
	template <typename U> static U f(U x) { return x + U(1); }
	T h() const { return T(f<float>(2.0f)) + a; }
};
kernel void member_template_static_in_class_template_valid(device float* out [[buffer(0)]], device const float* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{ B<float> b{in[i]}; out[i] = B<float>::f<float>(1.0f) + b.h(); }
