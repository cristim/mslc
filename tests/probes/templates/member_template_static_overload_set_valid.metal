// EXPECT: valid
// RUN: grid=64 local=8 cases=3 elems=2048
// Refs #341: a static member template and a static member function of one name are an overload set.
#include <metal_stdlib>
using namespace metal;
struct S {
	template <typename U> static U f(U x) { return x + x; }
	static float f(float x, float y) { return x * y; }
};
kernel void member_template_static_overload_set_valid(device float* out [[buffer(0)]], device const float* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{ out[i] = S::f(in[i]) + S::f(in[i], 2.0f); }
